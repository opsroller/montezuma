# Montezuma's

*Montezuma's* is a cross-platform 3D match-3 game inspired by the gameplay of
*The Treasures of Montezuma*. It is modernized, but it keeps the original's
feel. The game code is native C++20, and the game uses
[Godot 4](https://godotengine.org) through GDExtension for 3D, input and
platform export.

This is an independent fan project. It has no connection to Alawar
Entertainment. Nothing in it comes from the original game: no code, art,
audio or text. The gameplay rules are written up in
[docs/reference/rules-spec.md](docs/reference/rules-spec.md) and reimplemented from scratch.

The name is deliberately broad. Later, the game can host mini-games that put
different graphics and rule variants on the same core.

## Layout

- `core/` holds the game rules (board, cascades, level state) in plain C++20
  with no engine dependency, along with their tests.
- `extension/` holds the GDExtension that exposes the core to Godot
  (`Match3Board`) and draws the 3D board (`BoardView3D`).
- `game/` holds the Godot project. It has no GDScript; game logic lives in
  C++.
- `thirdparty/godot-cpp` is the official C++ binding for Godot (10.0, built
  against the Godot 4.7 API), included as a git submodule.
- `docs/` holds the rules spec and design notes.

## Building and testing

You need CMake 3.22 or newer, Ninja, a C++20 compiler (GCC, Clang or MSVC),
Python 3 (godot-cpp generates its bindings with it) and Godot 4.7.2.

```sh
git submodule update --init --recursive
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
godot --path game        # play; the extension is built into game/bin/
```

To build and test only the core, without godot-cpp, pass
`-DMONTEZUMA_BUILD_EXTENSION=OFF`.

## Targets

Windows, macOS, Linux, Android and iOS, all as native builds. The website
will run a playable intro level, built from the same C++ as WebAssembly, and
offer the download once the player finishes it.

## License

Montezuma's is free software, licensed under the GNU General Public License,
version 2 or (at your option) any later version. See [LICENSE](LICENSE).
Each source file carries an `SPDX-License-Identifier` header. The Godot Engine
and godot-cpp are separate projects under the MIT license, which is compatible
with the GPL.
