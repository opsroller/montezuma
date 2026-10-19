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

The cache keys name the godot-cpp major version
(`tests-extension-godot-cpp-10-…`). A content-addressed cache cannot
return a wrong object after a godot-cpp bump, but it would carry hundreds
of megabytes of objects for the old bindings that will never hit again,
crowding out the new ones under the size cap. A new key prefix starts the
cache empty after an engine upgrade; change it whenever the submodule
moves to a new major version or Godot API version.

ccache is the common choice on Linux. On Windows, sccache has the more
established MSVC support, so the Windows job uses it.

## Why not a prebuilt godot-cpp package

Building godot-cpp once per version and linking the stored library would
skip even the cache lookups. godot-cpp 10 has no CMake install or export,
so the only ready-made prebuilt is the ConanCenter package
(`godot-cpp/10.0.0`, the same commit as our submodule). Its recipe
rebuilds godot-cpp's usage requirements by hand and does not match ours:
it leaves out the static `libstdc++` link on Linux, always defines
`THREADS_ENABLED` with `-sUSE_PTHREADS` on the web (our web build turns
threads off), and always defines `_HAS_EXCEPTIONS=0` with MSVC (we build
with exceptions on). Linking it would quietly change the libraries we
ship. Using it would mean maintaining our own recipe or overrides, so the
compiler cache stays: it keeps godot-cpp's own CMake target, flags and
all, and a hit is the same object a cold compile gives. Developers use
the same cache locally
([Build and test](../how-to/build-and-test.md#compile-godot-cpp-only-once)).

## Why Ninja

CMake's Visual Studio generator hands whole projects to MSBuild and
ignores compiler launchers, so the cache would never see a compile. Ninja
runs one compiler call per file, through the launcher, on every OS.

## What is not cached, and why

Downloads that take a second (Godot for the headless load) are left
alone: restoring a cache costs about as much as the download. Caches that
do not pay their way are not added.

The core, sanitizer and balance-simulator builds were once left uncached
for that reason. As the core grew they reached 35 to 47 seconds of
compiling on every run, the longest step in their jobs, so they now go
through ccache like the extension builds. The Windows core job is the
exception: it uses the Visual Studio generator, which ignores compiler
launchers, and runs only on `main` and manual runs.

## Why the jobs run on the stock runner images

The web browser check needs Chromium, Firefox and WebKit plus dozens of
system libraries; installing them took about 40 seconds of every `web`
run. Playwright publishes an image with exactly those browsers for each
release, and running the check in it was tried and measured: starting the
container took about 26 seconds, and the browsers ran slower inside it
(WebKit alone took about as long as all three on the runner), so the
check took longer than installing on the runner, even split into one job
per browser. Caching the browser downloads with `actions/cache` was
measured too: the install step still took 40 seconds on a cache hit,
because installing the system libraries is most of it, so that cache was
not added either. The browsers are installed on the runner on each run.

The compile jobs stay on the stock runner images for the same reason.
Those already carry CMake, Ninja and the compilers (the cache action adds
ccache), so a build container would only add an image pull. Godot and
Emscripten come from pinned, checksummed downloads restored from
`actions/cache`, which keeps the version in one file,
`tools/release/godot.env`.

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

## Why Windows is off the PR path

Even with every compile a cache hit, the Windows extension job took about
two minutes, against under 30 seconds for the Linux job. It set the pace
for every PR. Pushes to `main` still run the Windows core tests, the
Windows extension build and the Windows package, so Windows breaks are
caught within one merge rather than before it. That trade favours fast feedback
on the platforms most changes are developed on.

The exception is a change to the build system itself: CMake files,
`.gitmodules` or anything under `thirdparty/`. Those can break only MSVC
with nothing in the C++ looking wrong. The godot-cpp 10 upgrade did:
godot-cpp picks the static MSVC runtime when it is added, after the core
library already had the default DLL runtime, and the Windows link failed
on every `main` push until it was fixed. Such PRs are rare, so the
[`windows` workflow](../reference/ci-pipeline.md#windows-workflow) builds
and tests them on Windows before merge, from the same warm cache, while
code-only PRs keep the faster feedback. It is not a required check,
because a path-filtered check never reports on the PRs it skips and
would block them.
