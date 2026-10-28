# SPDX-License-Identifier: GPL-2.0-or-later
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
  written down anywhere.
- `import` runs before any capture. Godot serves meshes from `.godot/imported`, so
  a rebuilt mesh does not reach a frame until it has.
- Every subprocess is an argument vector. There is no shell, so there is nothing to
  quote, chain or escape and no stderr goes missing.

`python3`, `blender`, `godot`, `gamescope` and `flock` come from the project's flake.
If one is missing the step says so and stops; nothing probes for alternatives or
falls back to a virtual environment.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRATCH = ROOT / ".scratch"
SHOTS = SCRATCH / "lookdev"
LOOKDEV = ROOT / "game/assets/lookdev"
PLANTS = "tools/assets/plants.py"
PARAMS = "tools/assets/themes/plants.json"
# The quality floor (CLAUDE.md): nothing is judged below this. Not a flag.
WIDTH, HEIGHT, FRAMES = 3840, 2160, 4


class Missing(RuntimeError):
    """A tool the pipeline needs is not on PATH."""


def need(*names):
    for name in names:
        if shutil.which(name) is None:
            raise Missing(
                f"{name} is not on PATH. The project's flake provides it: enter the "
                f"environment (direnv) before running this."
            )


def run(argv, timeout=None):
    """One subprocess, as an argument vector. Output is streamed, not captured, so a
    long step shows its progress rather than going quiet for three minutes."""
    print(f"$ {' '.join(argv)}", flush=True)
    return subprocess.run(argv, cwd=ROOT, timeout=timeout, check=False).returncode


def gpu_lock():
    """Beside the directory every worktree of this checkout shares."""
    out = subprocess.run(
        ["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
        cwd=ROOT, capture_output=True, text=True, check=True,
    ).stdout.strip()
    if not out:
        raise RuntimeError("not a git repository: cannot place the shared GPU lock")
    return str(Path(out.removesuffix("/.git")) / ".godot-gpu.lock")


def only_args(species):
    return ["--only", species] if species else []


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
    need("godot", "flock")
    return run(["flock", gpu_lock(), "godot", "--path", "game", "--import", "--headless"])


def step_check(args):
    need("python3")
    return run([sys.executable, "tools/assets/check_plant_meshes.py"])


def step_validate(args):
    """The project's asset rules: paths, budgets, texture channels, import settings,
    provenance, SPDX headers. Run by hand every round until it was put here."""
    need("python3")
    return run([sys.executable, "tools/assets/validate_assets.py"])


def step_capture(args):
    need("godot", "gamescope", "flock")
    lock = gpu_lock()
    pages = page_names(args.scope, args.species)
    if not pages:
        print("capture: no pages matched")
        return 1
    SHOTS.mkdir(parents=True, exist_ok=True)
    failed = 0
    for index, page in enumerate(pages, 1):
        print(f"[{index}/{len(pages)}] {page}", flush=True)
        work = SCRATCH / f"cap-{page}"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True, exist_ok=True)
        run(["flock", lock,
             "gamescope", "--backend", "headless", "-W", str(WIDTH), "-H", str(HEIGHT), "--",
             "godot", "--path", "game", "--rendering-driver", "vulkan",
             f"res://assets/lookdev/{page}.tscn",
             "--write-movie", str(work / "f.png"),
             "--fixed-fps", "30", "--quit-after", str(FRAMES),
             "--", "--start-screen="])
        # Movie Maker numbers its frames; the last one is the settled image.
        frames = sorted(work.glob("f*.png"))
        if frames:
            shutil.copy2(frames[-1], SHOTS / f"{page}.png")
            print(f"    {(SHOTS / f'{page}.png').relative_to(ROOT)}")
        else:
            failed += 1
            print(f"    FAILED: no frame written")
        shutil.rmtree(work, ignore_errors=True)
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
        code = STEPS[name](args)
        if code and name not in advisory:
            print(f"\nstopped: {name} exited {code}")
            return code
    code = step_capture(args)
    step_sheet(args)
    return code


STEPS = {
    "params": step_params, "atlases": step_atlases, "build": step_build,
    "pages": step_pages, "import": step_import, "check": step_check,
    "validate": step_validate, "capture": step_capture, "sheet": step_sheet,
    "all": step_all,
}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("step", choices=sorted(STEPS))
    ap.add_argument("--species", help='comma-separated keys, e.g. "heliconia,ceiba"')
    ap.add_argument("--scope", choices=("species", "rows", "all"), default="species")
    ap.add_argument("--per", type=int, default=9, help="frames per contact sheet")
    args = ap.parse_args()
    try:
        return STEPS[args.step](args)
    except Missing as missing:
        print(f"error: {missing}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
