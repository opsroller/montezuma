# montezuma-plants

The jungle plant library's generation and look-dev pipeline, as a queue with events
instead of commands typed in order and watched.

The library (`tools/assets/plants.py`) is 26 species in 59 variants, built as 162
meshes, and it is judged on rendered frames rather than from the code. Running that
loop by hand meant re-deciding the same things every round and then sitting in front
of it: which interpreter has numpy, where the GPU lock is, what resolution counts as
judgeable, whether the import pass had been run since the last mesh change, and
whether twenty minutes of silence was a long capture or a dead one.

## What it is

Three parts, and the division between them is the point:

- **The work** is per subject. One species' meshes, one species' scenes, one page's
  frame. Edit `heliconia` and three frames are stale, not fifty-nine.
- **The queue** decides what runs and when. Celery over redis on a unix socket in
  `.scratch/`, with two queues: `gpu` at a concurrency of one, because a capture is
  one Godot on one GPU, and `cpu` at several, because atlases and builds do not care
  what the GPU is doing. `chain` and `chord` carry what follows what.
- **The events** say what happened. One topic per thing the pipeline actually does,
  published as it happens, and one monitor subscribed to each.

Nothing polls, and nothing waits on anything. A stage finishing is an event; what
follows it is whoever subscribed to that event.

```sh
plants-queue up                  # broker and workers; a SessionStart hook does it too
plants-queue species heliconia   # one plant: build, pages, import, its 3 frames
plants-queue library             # all of them, fanned out
plants-queue status              # what is running and queued
plants-queue down
```

Editing a species submits that species on its own: a `PostToolUse` hook reads which
`@species` block the diff fell inside, publishes `plant.source.changed`, and the
`source-changed` monitor submits that plant's chain.

## Monitors

Each is its own process subscribed to its own routing keys on the broker's topic
exchange. Redis decides who hears what; none of them is a stream with a switch in it.

| Monitor | Subscribes to | Reports |
|---|---|---|
| `run` | `plant.run.*` | a run starting and finishing — and, from a pidfd on its process, a run that was killed and never said so |
| `stages` | the six completion topics | params, atlases, meshes, scenes, imports, sheets |
| `frames` | `plant.frame.captured` | frames landing; publishes `plant.review.wanted` when a species' last one does |
| `faults` | `plant.fault.*` | a bad mesh, and an import left `valid=false` |
| `source-changed` | `plant.source.changed` | submits the edited species' chain |
| `imports` | Godot's own import log | import errors Godot prints once and never again |

Two of them put work back in: `source-changed` submits a chain, `frames` asks for a
review. Adding another consequence means adding another subscriber, not editing an
existing one.

## Topics

The vocabulary is in `tools/assets/events.py`, which refuses to publish a name that
is not in it — a mistyped routing key is a monitor that hears nothing for ever and
looks exactly like a quiet pipeline. They are the pipeline's own events, not the
queue's: `task.succeeded` says that something finished, and every stage of this
pipeline succeeds the same way.

Every event also goes to `.scratch/lookdev-events.jsonl`, appended and flushed, so a
run is readable afterwards and the pipeline still records itself with no broker up.

## Tools

| Tool | Does |
|---|---|
| `params` | writes `tools/assets/themes/plants.json` from the species; must precede `atlases` |
| `atlases` | draws the 36 leaf and bark atlases at 4096 (~3 min changed, instant otherwise) |
| `build` | meshes, materials and the catalogue, under the flake's pinned Blender |
| `pages` | the close-up pages with their empty companions, plus the seven band rows |
| `import` | Godot's import pass — required after any mesh or texture change |
| `check` | degenerate faces, winding, normals, UVs straddling an atlas tile, orphans |
| `capture` | renders pages at 3840×2160 into `.scratch/lookdev` |
| `sheet` | labelled contact sheets of the captures |
| `measure` | reads `docs/reference/foliage-reference.md`'s figures back off a frame |
| `all` | all of the above, in the one order that works |

## What it holds fixed

- **3840×2160** captures. The project's quality floor; not a parameter.
- **`.scratch/`** in the workspace for frames, sheets, the broker's socket and every
  working directory — never the system temp directory.
- **No absolute paths.** The checkout comes from `git rev-parse --show-toplevel` and
  the shared GPU lock from `git rev-parse --git-common-dir`, the one directory every
  worktree agrees on, so nothing here names anyone's home directory.
- **The GPU lock is taken in-process**, not by wrapping commands in `flock(1)`,
  because the process that waits is the only one that knows it is waiting. It stays
  as the backstop for captures started outside the queue; inside it, the `gpu`
  queue's concurrency of one is what serialises them.
- **argv only.** No shell, so nothing to quote, chain or escape, and no loop or
  conditional hidden in a command string.

## Setup

`direnv allow` once per checkout. `.envrc` is `use flake` then `layout pipenv`: the
flake delivers godot, blender, gamescope, redis, python3 and the native libraries the
wheels link against; the Pipfile delivers numpy, Pillow, celery and the redis client.
Inside it nothing takes an interpreter path in front of it.

## Licence

GPL-2.0.
