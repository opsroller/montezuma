---
name: headless-tests
description: How to write and run Montezuma 3D tests headless in Godot. Use when adding tests or debugging a CI failure in the tests job.
---

# Headless tests

Runner: `tests/run_tests.gd` extends `SceneTree`, calls each test, prints
`N checks, M failures`, and exits non-zero on failure. CI runs it on
Godot 4.4.1 (`.github/workflows/tests.yml`).

```sh
godot --headless --path . --import          # once, builds class cache
godot --headless --path . --script res://tests/run_tests.gd
```

## Adding tests

- Put a package's tests in `tests/test_<area>.gd` as a class with a
  `run(t)` method that calls `t.check(cond, label)`; register it in
  `run_tests.gd`. This keeps parallel packages from editing the same lines.
- Build boards from strings with `board_from(["RGB..", ...])`-style helpers
  so the test shows the position.
- Seed every random source. A test that depends on luck is a bug.
- Test the result data (cells cleared, order, score), not internals.

## When CI fails

Run the exact commands above locally first. If `--import` reports missing
classes, a new `class_name` file was not imported; run `--import` again.
