# Montezuma 3D

A cross-platform 3D remake of the match-3 gameplay of *The Treasures of
Montezuma*. It is modernized, but it keeps the original's feel. The game code
is native C++20, and the game uses [Godot 4](https://godotengine.org) through
GDExtension for 3D, input and platform export.

This is an independent fan project. It has no connection to Alawar
Entertainment. Nothing in it comes from the original game: no code, art,
audio or text. The gameplay rules are written up in
[docs/rules-spec.md](docs/rules-spec.md) and reimplemented from scratch.

## Layout

- `core/` holds the game rules (board, cascades, level state) in plain C++20
  with no engine dependency, along with their tests.
- `docs/` holds the rules spec and design notes.
- The Godot project and the GDExtension binding are coming next.

## Building and testing

You need CMake 3.22 or newer and a C++20 compiler (GCC, Clang or MSVC).

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Targets

Windows, macOS, Linux, Android and iOS, all as native builds.

## License

Montezuma 3D is free software, licensed under the GNU General Public License,
version 2 or (at your option) any later version. See [LICENSE](LICENSE).
Each source file carries an `SPDX-License-Identifier` header. The Godot Engine
and godot-cpp are separate projects under the MIT license, which is compatible
with the GPL.
