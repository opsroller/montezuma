# Montezuma 3D

A cross-platform 3D remake of the match-3 gameplay of *The Treasures of
Montezuma*, built with [Godot 4](https://godotengine.org). It is modernized,
but it keeps the original's feel.

This is an independent fan project. It has no connection to Alawar
Entertainment. Nothing in it comes from the original game: no code, art,
audio or text. The gameplay rules are written up in
[docs/rules-spec.md](docs/rules-spec.md) and reimplemented from scratch.

## Layout

- `core/` holds the board, move resolution and level state. It is plain
  GDScript with no rendering dependencies.
- `tests/` holds the headless test runner.
- `docs/` holds the rules spec and design notes.

## Running the tests

```sh
godot --headless --path . --import
godot --headless --path . --script res://tests/run_tests.gd
```

## Targets

Windows, macOS, Linux, web and Android come first. iOS follows.
