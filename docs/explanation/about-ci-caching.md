<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About CI caching

Before caching, a `tests` run took about nine minutes, and almost all of
it was one step: compiling godot-cpp, the engine bindings, from scratch in
the two `extension` jobs (about six minutes on Ubuntu and eight on
Windows). Everything else, including the core builds on three OSes, the
sanitizer run and the Godot download, took under a minute. godot-cpp is a
pinned submodule that changes rarely, so rebuilding it on every push was
the waste worth removing.

## Why a compiler cache, not a cached build folder

Caching the CMake build folder would also skip the godot-cpp build, but a
build folder restored onto a new checkout is only as correct as CMake's
timestamp checks, and a wrong restore gives a green build of stale code. A
compiler cache (ccache, sccache) keys every object file on a hash of the
preprocessed source, the compiler and its flags. A changed header, flag or
compiler version is a miss, never a stale hit. A cold run is still a
correct run, only slower. That is the property the
`cross-platform-builds` practice asks for.

ccache is the common choice on Linux. On Windows, sccache has the more
established MSVC support, so the Windows job uses it.

## Why Ninja

CMake's Visual Studio generator hands whole projects to MSBuild and
ignores compiler launchers, so the cache would never see a compile. Ninja
runs one compiler call per file, through the launcher, on every OS.

## What is not cached, and why

Downloads that take a second (Godot for the headless load) are left
alone: restoring a cache costs about as much as the download. The core and
sanitizer builds take seconds. Caches that do not pay their way are not
added.

## Cache safety

GitHub scopes caches by branch: a PR can read caches from `main` but
cannot write caches that `main` reads, so a PR cannot poison the default
branch's builds.

The desktop `build` workflow uses the same compiler caches for the
libraries it ships. That is allowed because the cache is content-addressed:
a hit returns the object the same compiler would produce from the same
preprocessed source and flags, so a warm build and a cold build give the
same library. A build folder cache would not meet that bar, which is why
release jobs never restore one.
