# SPDX-License-Identifier: GPL-2.0
"""The plant pipeline as queued work: one task per *thing*, not one task per stage.

    plants-queue up                     the broker and the workers
    plants-queue species heliconia      rebuild one plant and reshoot its frames
    plants-queue library                all of them, fanned out across the queues

The unit of work is one subject - one species' meshes, one species' look-dev scenes,
one page's frame - and that is the whole point of putting this on a queue. An agent
edits one species in plants.py and three frames are stale, not fifty-nine; the queue
is handed the three. A batch task that rebuilds everything is a twenty-minute job
whatever changed, which is the thing the queue exists to stop.

Two queues, because the resources are different:

- `gpu`, run at a concurrency of one. A capture is one Godot on one GPU, and so is
  Godot's import pass. The queue's limit is what serialises them; the advisory lock
  in lookdev.py stays as the backstop for captures started outside the queue, in
  another worktree or by hand.
- `cpu`, run at several. Atlases are numpy and builds are Blender, and neither cares
  what the GPU is doing.

Nothing in this file waits for anything. `chain` and `chord` are Celery's, so "build
this species, write its scenes, import, shoot each of its pages, and make the sheets
when the last one lands" is a declared shape, and the sheet task runs because the
chord's header finished rather than because something sat watching for it.

The broker is redis over a unix socket inside `.scratch/`: no port, nothing on the
network, and it goes away with the directory.
"""

import os
import sys
from pathlib import Path
from types import SimpleNamespace

from celery import Celery, chain, chord

sys.path.insert(0, str(Path(__file__).resolve().parent))

import events                                            # noqa: E402
import lookdev                                           # noqa: E402

ROOT = lookdev.ROOT
SOCKET = ROOT / ".scratch/redis.sock"
BROKER = f"redis+socket://{SOCKET}"

GPU, CPU = "gpu", "cpu"
# Held back from every core so a 4K capture is never fighting its own atlases.
CPU_SLOTS = max(1, (os.cpu_count() or 4) // 3)

app = Celery("plants", broker=BROKER, backend=BROKER)
app.conf.update(
    task_default_queue=CPU,
    task_routes={
        "plants.import_assets": {"queue": GPU},
        "plants.capture": {"queue": GPU},
    },
    # Celery's own task events, which the queue's workers use among themselves. The
    # events this pipeline's monitors subscribe to are the plant topics in
    # events.py, published by the work itself.
    worker_send_task_events=True,
    task_send_sent_event=True,
    # A stage is minutes long and is not safe to run twice at once: a worker takes
    # one message at a time and acknowledges it when the work is done rather than
    # when it is picked up, so a worker killed mid-capture leaves the job for the
    # next one instead of losing it.
    worker_prefetch_multiplier=1,
    task_acks_late=True,
    task_reject_on_worker_lost=True,
    task_track_started=True,
    result_expires=3600,
)


def _args(**over):
    """lookdev.py's steps read an argparse namespace; this is that namespace."""
    return SimpleNamespace(**{"species": None, "scope": "species", "per": 4, **over})


def _stream():
    """A worker publishes to the same topics a hand-run pipeline does - to the file
    the monitors follow, and to the broker the topic monitors subscribe to - so there
    is one account of what happened whichever way the work was started."""
    if lookdev.EV.handle is None:
        lookdev.EV = events.Stream(lookdev.EVENTS, broker=BROKER)


# --- one subject each ---------------------------------------------------------

@app.task(name="plants.params")
def params(_=None):
    """themes/plants.json from the species. One file, and every atlas reads it, so
    it is the one stage that is genuinely not per-subject."""
    _stream()
    return lookdev.do("params", _args())


@app.task(name="plants.atlases")
def atlases(_=None):
    """The leaf and bark atlases. Whole-set by the generator's own design: it hashes
    each atlas's inputs and redraws only what changed, so handing it one species'
    colours costs the same as handing it one species."""
    _stream()
    return lookdev.do("atlases", _args())


@app.task(name="plants.build")
def build(_=None, species=None):
    """One species' meshes, materials and catalogue entries."""
    _stream()
    return lookdev.do("build", _args(species=species))


@app.task(name="plants.pages")
def pages(_=None, species=None):
    """One species' look-dev scenes, each with its empty companion for masking."""
    _stream()
    return lookdev.do("pages", _args(species=species))


@app.task(name="plants.import_assets")
def import_assets(_=None):
    """Godot's import pass. Whole-project because Godot's importer is: it walks the
    tree. On the GPU queue, so it never runs beside a capture."""
    _stream()
    return lookdev.do("import", _args())


@app.task(name="plants.capture")
def capture(page):
    """One page at 3840x2160. Fifty-nine of these are fifty-nine jobs the queue can
    place, retry and report on one at a time, rather than one job that is twenty
    minutes of silence ending in a number."""
    _stream()
    return lookdev.capture_page(page)


@app.task(name="plants.check")
def check(_=None):
    """Mesh faults a frame cannot show, across the built library."""
    _stream()
    return lookdev.do("check", _args())


@app.task(name="plants.sheets")
def sheets(_=None, per=4):
    """The contact sheets. Whole-set by nature - the point of a sheet is the
    comparison between plants."""
    _stream()
    return lookdev.do("sheet", _args(per=per))


# --- the shapes ---------------------------------------------------------------

def species_pages(key):
    """The look-dev pages that belong to one species, which are the only frames an
    edit to that species makes stale."""
    return lookdev.page_names("species", key)


def species_work(key):
    """One plant, from source to frames: build it, write its scenes, import, shoot
    each of its pages, and make the sheets once the last of them lands.

    This is what a monitor submits when it hears that one species' source changed.
    Three or four jobs, not fifty-nine.
    """
    frames = species_pages(key)
    if not frames:
        raise ValueError(f"no look-dev page belongs to species {key!r}")
    return chain(
        build.si(species=key),
        pages.si(species=key),
        import_assets.si(),
        chord([capture.si(page) for page in frames], sheets.si()),
    )


def library(scope="all"):
    """Every plant, as one job per plant rather than one job for all of them.

    `build` and `pages` can each take a species, so there is no reason for the
    library to go through them in one lump: twenty-six builds are twenty-six items
    the cpu queue runs ten at a time, each finishing and publishing on its own, and a
    single species that fails is one red item rather than a whole stage. A chord
    gates the import on all of them, because Godot's import pass walks the tree and
    there is no point running it twice.
    """
    frames = lookdev.page_names(scope, None)
    if not frames:
        raise ValueError(f"no look-dev page matches scope={scope}")
    plants = lookdev.species_keys()
    return chain(
        params.si(),
        atlases.si(),
        # One build per species, then one scene-writing job per species, each gated
        # on the whole previous round.
        chord([build.si(species=key) for key in plants],
              chord([pages.si(species=key) for key in plants],
                    chain(
                        import_assets.si(),
                        check.si(),
                        chord([capture.si(page) for page in frames], sheets.si()),
                    ))),
    )
