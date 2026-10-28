# montezuma-plants

The Montezuma jungle plant library's generation and look-dev pipeline, as tools
instead of hand-typed commands.

The library (`tools/assets/plants.py`) generates 26 species in 59 variants and is
judged on rendered frames, never from the code. Running that loop by hand meant
re-deciding the same things every round — which interpreter has numpy, where the GPU
lock is, what resolution counts as judgeable, where scratch goes, whether the import
pass had been run since the last mesh change. Each has one right answer, so none of
them belongs in a command line. This plugin fixes them.

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
| `lookdev` | all of the above, in the one order that works |

## What it holds fixed

- **argv only**, through `$.process.run`. No shell, so nothing to quote, chain or
  escape, and no loop or conditional hidden in a command string.
- **3840×2160** captures. The project's quality floor; not a parameter.
- **`.scratch/`** in the workspace for frames, sheets and working directories —
  never the system temp directory, which is shared with every other process on the
  machine and sits outside the checkout the work belongs to.
- **No absolute paths.** The shared GPU lock is derived from
  `git rev-parse --git-common-dir`, the one directory every worktree of the checkout
  agrees on, so nothing here names anyone's home directory.
- **The interpreter is probed, once**, rather than prefixed onto every command. If
  neither `python3` nor the workspace venv can import numpy and Pillow it says so and
  what to do about it.
- **Progress is pushed**, not polled: the status line reports each step and each page
  as it finishes, because the call awaits the process.

## Setup

Needs, on PATH: `git`, `godot`, `gamescope`, `flock`, `blender` (the flake's pinned
one — generated meshes do not reproduce byte for byte across Blender versions), and a
Python with numpy and Pillow.

## Licence

GPL-2.0-or-later, as the repository it serves.
