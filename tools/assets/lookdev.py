# SPDX-License-Identifier: GPL-2.0
"""The plant library's look-dev pipeline: one program, in the same language as the rest.

    python3 tools/assets/lookdev.py all --species heliconia
    python3 tools/assets/lookdev.py capture --scope rows
    python3 tools/assets/lookdev.py check

Steps, in the order that works:

    params    write themes/plants.json from the species in plants.py
    atlases   draw the 36 leaf and bark atlases at 4096 from it
    build     meshes, materials and the catalogue, under the pinned Blender
    pages     a close-up page per specimen with its empty companion, plus band rows
    import    Godot's import pass - without it a capture shows the previous assets
    check     mesh faults a frame cannot show
    validate  the project's asset rules (validate_assets.py)
    capture   render pages at 3840x2160 into .scratch/lookdev
    sheet     lay the captures out as labelled contact sheets
    all       every step above

What it holds fixed, because each has one right answer and was got wrong by hand:

- 3840x2160 captures. The project's quality floor; not a flag.
- Scratch is `.scratch/` in the workspace, never the system temp directory.
- The GPU lock is derived from `git rev-parse --git-common-dir`, the one directory
  every worktree of this checkout agrees on, so no path to a home directory is
  written down anywhere. It is taken here rather than by wrapping each command in
  `flock(1)`, because the process that waits is the only one that knows it is
  waiting: waiting and acquiring are events, not something to be discovered later by
  reading /proc/locks on a timer.
- `import` runs before any capture. Godot serves meshes from `.godot/imported`, so
  a rebuilt mesh does not reach a frame until it has.
- Every subprocess is an argument vector. There is no shell, so there is nothing to
  quote, chain or escape and no stderr goes missing.
- `--events` writes the run as a stream of JSON lines, one per event, flushed as it
  happens: a step starting and ending, every page captured with an ETA from this
  run's own pages, every page that wrote no frame, and every fault line a step
  printed and then exited 0 over. `plants-watch` turns that stream into
  notifications, so a half-hour run reports itself instead of being polled.

`python3`, `blender`, `godot` and `gamescope` come from the project's flake. If one is
missing the step says so and stops; nothing probes for alternatives or falls back to
a virtual environment.
"""

import argparse
import contextlib
import fcntl
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import events                                            # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SCRATCH = ROOT / ".scratch"
SHOTS = SCRATCH / "lookdev"
LOOKDEV = ROOT / "game/assets/lookdev"
PLANTS = "tools/assets/plants.py"
PARAMS = "tools/assets/themes/plants.json"
# The quality floor (CLAUDE.md): nothing is judged below this. Not a flag.
WIDTH, HEIGHT, FRAMES = 3840, 2160, 4
# Where `--events` writes when it is given no path. One file per checkout, truncated
# at the start of a run, so a monitor following it is never reading the last run's.
EVENTS = SCRATCH / "lookdev-events.jsonl"

# A child's line worth an event of its own. Every one of these was a fault that cost
# a round of look-dev to find, and all of them were printed by a step that then
# exited 0: `check` and `validate` are advisory, Godot imports a broken scene
# happily, and Blender reports a failed build on stdout. Nothing here is a guess
# about what a tool might print - these are the strings the tools do print:
# check_plant_meshes.py's per-mesh fault columns and ORPHAN, validate_assets.py's
# `error:`/`warning:` lines, Godot's ERROR/SCRIPT ERROR, Blender's `Error:`, and a
# Python traceback from any of them.
FAULT = re.compile(
    r"\b(ZERO|FLIP|NORMAL|STRADDLE|EMPTY|ORPHAN)\b"
    r"|^\s*(error|warning):"
    r"|\b(SCRIPT ERROR|ERROR:|Error:|FAILED|Traceback)\b"
)


class Missing(RuntimeError):
    """A tool the pipeline needs is not on PATH."""


# The run's event stream. Silent until `--events` opens one, so importing this
# module - which `tasks.py` does, in every worker - publishes nothing by itself.
EV = events.Stream(None)

# What each step means when it finishes. The step names are this file's business; the
# topics are the pipeline's, and a subscriber to `plant.mesh.built` wants to hear
# about meshes, not about step 3 of 8 exiting 0.
FINISHED = {
    "params": "plant.params.written",
    "atlases": "plant.atlases.drawn",
    "build": "plant.mesh.built",
    "pages": "plant.pages.written",
    "import": "plant.assets.imported",
    "sheet": "plant.sheets.ready",
}


def need(*names):
    for name in names:
        if shutil.which(name) is None:
            raise Missing(
                f"{name} is not on PATH. The project's flake provides it: enter the "
                f"environment (direnv) before running this."
            )


def run(argv, timeout=None):
    """One subprocess, as an argument vector, its output read a line at a time.

    The line goes straight back out to our own stdout, so running this by hand still
    shows a long step's progress, and it is also matched against FAULT so that a
    fault a step prints and then exits 0 over becomes an event the moment it is
    printed rather than something to be found in a log afterwards.

    stderr is merged into stdout rather than left to its own pipe: not to hide it -
    it is printed and matched like every other line - but so that the order the tool
    printed things in survives, and so a tool that reports its failures on stderr
    (Blender, Godot, and every traceback) reaches the fault filter.
    """
    print(f"$ {' '.join(argv)}", flush=True)
    child = subprocess.Popen(
        argv, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1,
    )
    with child.stdout:
        for line in child.stdout:
            line = line.rstrip("\n")
            print(line, flush=True)
            if FAULT.search(line):
                EV.publish("plant.fault.mesh", line=line.strip()[:200])
    return child.wait(timeout=timeout)


def gpu_lock():
    """Beside the directory every worktree of this checkout shares."""
    out = subprocess.run(
        ["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
        cwd=ROOT, capture_output=True, text=True, check=True,
    ).stdout.strip()
    if not out:
        raise RuntimeError("not a git repository: cannot place the shared GPU lock")
    return str(Path(out.removesuffix("/.git")) / ".godot-gpu.lock")


@contextlib.contextmanager
def gpu(path):
    """Hold the shared GPU lock, and say so.

    The same advisory lock `flock(1)` takes, taken here instead of by wrapping the
    command in `flock` - because the process that waits is the only one that knows it
    is waiting. Asking the kernel afterwards who holds a lock means parsing
    /proc/locks on a timer, which is a poll invented to recover something this
    process had all along.

    A flock belongs to the open file description, and a child inherits it across
    fork, so holding it here for the length of the child is exactly what wrapping the
    child in `flock` did - and a worktree still using `flock(1)` on the same file
    still serialises against this one, because it is the same lock on the same inode.

    Taken per page rather than per run: twenty minutes of exclusive GPU would starve
    every other worktree of the checkout.
    """
    handle = open(path, "a+")
    waited = 0.0
    try:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            began = time.monotonic()
            EV.publish("plant.gpu.waiting", lock=Path(path).name)
            fcntl.flock(handle, fcntl.LOCK_EX)
            waited = round(time.monotonic() - began, 1)
            EV.publish("plant.gpu.held", lock=Path(path).name, waited=waited)
        yield waited
    finally:
        fcntl.flock(handle, fcntl.LOCK_UN)
        handle.close()


def only_args(species):
    return ["--only", species] if species else []


def species_keys():
    """Every species in the library, asked of the library.

    plants.py imports bpy inside the functions that need Blender, so importing it
    here costs nothing and nothing has to keep a second list of species in step with
    the first - which is the sort of list that is wrong for a month before anybody
    notices.
    """
    import plants                                        # noqa: PLC0415
    return sorted(plants.SPECIES)


def page_names(scope, species):
    """The look-dev scenes to render. The _Empty companions are for masking a
    measurement, not for looking at, so they are never in this list."""
    stems = sorted(p.stem for p in LOOKDEV.glob("*.tscn"))
    rows = [n for n in stems if n.startswith("LD_Plants_")]
    specimens = [n for n in stems
                 if n.startswith("LD_Plant_") and not n.endswith("_Empty")]
    if scope == "rows":
        return rows
    if species:
        keys = ["".join(w.capitalize() for w in k.strip().split("_"))
                for k in species.split(",") if k.strip()]
        specimens = [n for n in specimens
                     if any(n.startswith(f"LD_Plant_{k}") for k in keys)]
    return rows + specimens if scope == "all" else specimens


# --- the steps -------------------------------------------------------------

def step_params(args):
    need("python3")
    return run([sys.executable, PLANTS, "textures"])


def step_atlases(args):
    need("python3")
    return run([sys.executable, "tools/assets/gen_theme_textures.py", PARAMS])


def step_build(args):
    # Blender is pinned by the flake and is part of the asset: two committed meshes
    # were found not to reproduce byte for byte under a different version.
    need("blender")
    return run(["blender", "-b", "--factory-startup", "-P", PLANTS, "--", "build",
                *only_args(args.species)])


def step_pages(args):
    need("python3")
    code = run([sys.executable, PLANTS, "closeups", "--empty", *only_args(args.species)])
    return code or run([sys.executable, PLANTS, "lookdev"])


def step_import(args):
    need("godot")
    with gpu(gpu_lock()):
        code = run(["godot", "--path", "game", "--import", "--headless"])
    # Godot reports a failed import on its way past and then exits 0. The asset is
    # left with valid=false in its .import file and is skipped, silently, by every
    # later pass - which is how a plant comes to render white. Finding them is two
    # lines here and was a lost afternoon every time it was not.
    poisoned = sorted(
        path.name for path in (ROOT / "game/assets").rglob("*.import")
        if "valid=false" in path.read_text(errors="replace")
    )
    for name in poisoned:
        EV.publish("plant.fault.import", asset=name,
                   why="valid=false: this asset did not import, and every later pass "
                       "will skip it without a word")
    if poisoned:
        print(f"\n{len(poisoned)} asset(s) failed to import: "
              f"{', '.join(poisoned[:5])}{' ...' if len(poisoned) > 5 else ''}")
    return code


def step_check(args):
    need("python3")
    return run([sys.executable, "tools/assets/check_plant_meshes.py"])


def step_validate(args):
    """The project's asset rules: paths, budgets, texture channels, import settings,
    provenance, SPDX headers. Run by hand every round until it was put here."""
    need("python3")
    return run([sys.executable, "tools/assets/validate_assets.py"])


def capture_page(page):
    """One page: the unit of capture work.

    A page is the smallest thing worth rendering and the smallest thing worth
    reprocessing. Edit one species and three frames are stale, not fifty-nine - so
    this takes one page and nothing else, and a queue can place fifty-nine of them,
    or three, or the one that failed last time, without any of them knowing about
    the others. The GPU lock is taken for the length of this page alone.
    """
    need("godot", "gamescope")
    SHOTS.mkdir(parents=True, exist_ok=True)
    work = SCRATCH / f"cap-{page}"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    began = time.monotonic()
    try:
        with gpu(gpu_lock()):
            run(["gamescope", "--backend", "headless",
                 "-W", str(WIDTH), "-H", str(HEIGHT), "--",
                 "godot", "--path", "game", "--rendering-driver", "vulkan",
                 f"res://assets/lookdev/{page}.tscn",
                 "--write-movie", str(work / "f.png"),
                 "--fixed-fps", "30", "--quit-after", str(FRAMES),
                 "--", "--start-screen="])
        # Movie Maker numbers its frames; the last one is the settled image.
        frames = sorted(work.glob("f*.png"))
        took = round(time.monotonic() - began, 1)
        if not frames:
            print("    FAILED: no frame written")
            EV.publish("plant.frame.missing", page=page, seconds=took,
                       why="Godot wrote no movie frames")
            return 1
        shot = SHOTS / f"{page}.png"
        shutil.copy2(frames[-1], shot)
        print(f"    {shot.relative_to(ROOT)}")
        EV.publish("plant.frame.captured", page=page, seconds=took,
                   frame=str(shot.relative_to(ROOT)), width=WIDTH, height=HEIGHT)
        return 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


def step_capture(args):
    """Every page the scope asks for, one at a time. The queue runs `capture_page`
    directly and in parallel across its GPU slot; this is the same work for somebody
    at a terminal who wants the lot."""
    pages = page_names(args.scope, args.species)
    if not pages:
        print("capture: no pages matched")
        EV.publish("plant.frame.missing", page="", scope=args.scope,
                   species=args.species or "", why="no look-dev page matched")
        return 1
    failed = 0
    for index, page in enumerate(pages, 1):
        print(f"[{index}/{len(pages)}] {page}", flush=True)
        failed += 1 if capture_page(page) else 0
    print(f"\ncaptured {len(pages) - failed}/{len(pages)} page(s) at {WIDTH}x{HEIGHT} "
          f"into {SHOTS.relative_to(ROOT)}")
    return 1 if failed else 0


def step_sheet(args):
    need("python3")
    return run([sys.executable, "tools/assets/contact_sheet.py",
                str(SHOTS), str(SHOTS / "sheet.png"), "--per", str(args.per)])


def step_all(args):
    # `check` and `validate` reporting faults is information, not a reason to abandon
    # the run: the frames are still worth looking at, and the faults are in the report.
    advisory = ("check", "validate")
    for name in ("params", "atlases", "build", "pages", "import", "check", "validate"):
        code = do(name, args)
        if code and name not in advisory:
            print(f"\nstopped: {name} exited {code}")
            return code
    code = do("capture", args)
    do("sheet", args)
    return code


def do(name, args):
    """One step, and the event for what it produced.

    Every step goes through here, including the ones `all` runs, so the topic is
    published whichever way the step was started - by hand, by `all`, or by a worker
    off the queue. A step that produced nothing publishes nothing: a subscriber to
    `plant.mesh.built` is told that meshes exist, and is never told that a build was
    attempted.
    """
    began = time.monotonic()
    code = STEPS[name](args)
    topic = FINISHED.get(name)
    if topic and code == 0:
        EV.publish(topic, step=name, species=getattr(args, "species", None) or "",
                   seconds=round(time.monotonic() - began, 1))
    return code


STEPS = {
    "params": step_params, "atlases": step_atlases, "build": step_build,
    "pages": step_pages, "import": step_import, "check": step_check,
    "validate": step_validate, "capture": step_capture, "sheet": step_sheet,
    "all": step_all,
}


def main():
    global EV
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("step", choices=sorted(STEPS))
    ap.add_argument("--species", help='comma-separated keys, e.g. "heliconia,ceiba"')
    ap.add_argument("--scope", choices=("species", "rows", "all"), default="species")
    ap.add_argument("--per", type=int, default=9, help="frames per contact sheet")
    # On by default. A monitor that reports nothing because somebody left a flag off
    # is worse than no monitor: it reads as a quiet, healthy run.
    ap.add_argument("--events", default=str(EVENTS), metavar="PATH",
                    help=f"where to write the run's event stream as JSON lines, which "
                         f"the plugin's monitors follow (default "
                         f"{EVENTS.relative_to(ROOT)})")
    ap.add_argument("--no-events", dest="events", action="store_const", const="",
                    help="do not write an event stream")
    ap.add_argument("--broker", default=os.environ.get("PLANTS_BROKER", ""),
                    help="publish the same events to this broker as well, so a "
                         "monitor subscribed to one topic hears them live")
    args = ap.parse_args()
    if args.events:
        EV = events.Stream(Path(args.events), broker=args.broker or None)
    EV.publish("plant.run.started", step=args.step, scope=args.scope,
               species=args.species or "")
    try:
        code = do(args.step, args)
    except Missing as missing:
        print(f"error: {missing}", file=sys.stderr)
        EV.publish("plant.run.finished", step=args.step, code=2, why=str(missing))
        return 2
    except KeyboardInterrupt:
        EV.publish("plant.run.finished", step=args.step, code=130, why="interrupted")
        return 130
    except Exception as broke:                        # noqa: BLE001
        EV.publish("plant.run.finished", step=args.step, code=1,
                   why=f"{type(broke).__name__}: {broke}")
        raise
    EV.publish("plant.run.finished", step=args.step, code=code)
    return code


if __name__ == "__main__":
    sys.exit(main())
