---
name: work-package
description: Use when you start, hand off or finish a Montezuma 3D work package (WP-NN). Covers branch, owned paths, tests and done criteria.
---

# Working a package

Packages live in `docs/work-packages.md`. Each one names a role, the paths
it owns, what it depends on and when it is done.

## Start

1. Read your package row and `docs/contracts.md`.
2. Load the clean-room and conventional-commits skills.
3. `git fetch origin main && git checkout -b wp/<id>-<slug> origin/main`.
4. If a dependency is not merged yet, build against the contract with a
   stub or placeholder, and say so in the PR.

## While working

- Write only inside your owned paths. A change anywhere else goes through
  the owner (see the coordinate skill). The only shared files everyone may
  append to are `assets/PROVENANCE.md` and `CHANGELOG.md`.
- Keep `core/` free of scene and rendering code.
- Run the headless tests before every push:
  ```sh
  godot --headless --path . --import
  godot --headless --path . --script res://tests/run_tests.gd
  ```
  Add your own tests under `tests/` in a file named for your package
  (`tests/test_totems.gd`), registered in `tests/run_tests.gd`.

## Done

- The package's "done when" line is true.
- Tests and CI pass.
- Draft PR marked ready, titled as a Conventional Commit, body says
  `Refs: WP-NN`, lists anything stubbed, and links the packages that can
  now start.
- Dependants told (coordinate skill).

## If Godot is not installed

Download the version CI uses (see `.github/workflows/tests.yml`):
`Godot_v4.4.1-stable_linux.x86_64.zip` from the godotengine/godot GitHub
releases, unzip, and run it headless.
