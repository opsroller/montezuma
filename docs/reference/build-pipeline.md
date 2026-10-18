# Build pipeline

Facts about the desktop build pipeline in `.github/workflows/build.yml`.
Tests run separately in `.github/workflows/tests.yml`; the web demo is
built by `.github/workflows/web.yml`.

## Triggers

| Event | Runs |
|---|---|
| Push to `main` | Every time |
| Pull request | When it changes `build.yml`, `tools/release/`, `CMakeLists.txt`, `core/`, `extension/`, `game/` or `thirdparty/` (docs-only PRs skip it) |
| Manual (`workflow_dispatch`) | On demand, from the Actions tab |

The workflow token is read-only (`contents: read`). No job uses secrets.

## Jobs

| Job | Runner | What it does | Red means |
|---|---|---|---|
| `extension (linux)` | `ubuntu-22.04` | Builds `libmontezuma.linux.template_release.x86_64.so` | The extension does not compile on Linux (GCC) |
| `extension (windows)` | `windows-2022` | Builds `libmontezuma.windows.template_release.x86_64.dll` (MSVC) | The extension does not compile with MSVC |
| `extension (macos)` | `macos-14` | Builds `libmontezuma.macos.template_release.universal.dylib` (arm64 + x86_64, macOS 10.15+) | The extension does not compile with Apple Clang for both architectures |
| `export` | `ubuntu-24.04` | Fetches the pinned Godot, exports the three presets and packages them | A preset is wrong, a library is missing, or ad-hoc signing of the macOS app failed. The `export-logs` artifact has Godot's output |
| `smoke (<platform>)` | Same runner as the matching `extension` job | Verifies the checksum, unpacks the package and runs the game headless for 120 frames | The packaged game does not start or logs an engine error, for example the extension not loading |

All extension builds use `-DGODOTCPP_TARGET=template_release
-DMONTEZUMA_BUILD_EXTENSION=ON -DMONTEZUMA_BUILD_TESTS=OFF
-DMONTEZUMA_WERROR=ON`.

## Artifacts

| Artifact | Contents | Kept |
|---|---|---|
| `montezuma-linux` | `montezuma-<version>-linux-x86_64.tar.gz` and its `.sha256` | 90 days (repository default) |
| `montezuma-windows` | `montezuma-<version>-windows-x86_64.zip` and its `.sha256` | 90 days |
| `montezuma-macos` | `montezuma-<version>-macos-universal.zip` and its `.sha256` | 90 days |
| `export-logs` | Godot's import and export output per platform | 7 days |
| `extension-<platform>` | The compiled libraries, passed from `extension` to `export` | 3 days |

Each package holds one folder named after the archive, containing the
game, the extension library, `LICENSE.txt` (GPL-2.0), and two files
written by `tools/release/notices.py`: `THIRD-PARTY-NOTICES.txt` (Godot's
license and `COPYRIGHT.txt`, godot-cpp's and nlohmann/json's licenses) and
`SOURCE.txt` (the commit and where to get its source). The web demo ships
the same two files from the same script.

The same files are also inside the game, readable from `res://`:

| `res://` path | From |
|---|---|
| `CREDITS.md` | root `CREDITS.md` (the credits screen shows it) |
| `LICENSE.txt` | root `LICENSE` |
| `THIRD-PARTY-NOTICES.txt` | `notices.py` |
| `SOURCE.txt` | `notices.py` |

`export-desktop.sh` stages them into `game/` with `game-files.py stage`
before exporting, the presets' `include_filter` packs them, and
`game-files.py check` fails the export if any of them is missing from the
exported game. They are generated and ignored by git.

| Platform | Game inside the folder |
|---|---|
| Linux | `montezuma.x86_64` (data embedded) and the `.so` |
| Windows | `montezuma.exe` (data embedded) and the `.dll` |
| macOS | `Montezuma's.app`, ad-hoc signed, not notarized |

Archives are packed by `tools/release/package.py` with sorted entries,
no owners, and every timestamp set to the commit time
(`SOURCE_DATE_EPOCH`).

## Versions

`tools/release/version.sh` prints the version of the checked-out commit:

| Commit | Version |
|---|---|
| Tagged `vX.Y.Z` | `X.Y.Z` |
| Anything else | `<last tag, or 0.0.0>-dev.<commits on the branch>.g<short sha>`, e.g. `0.0.0-dev.8.gb98fcf7` |

## Pinned inputs

| Input | Pin | Where |
|---|---|---|
| Godot editor and export templates | 4.4.1-stable, SHA-512 verified | `tools/release/godot.env` |
| godot-cpp | Submodule commit | `thirdparty/godot-cpp` |
| GitHub Actions | Full commit SHA, version in a comment | `build.yml` |
| Runner images | Named versions (`ubuntu-22.04`, `windows-2022`, `macos-14`, `ubuntu-24.04`), not `-latest` | `build.yml` |

The Godot downloads are cached under the key
`godot-<hash of godot.env>`; changing the pin changes the key.

## Scripts in `tools/release/`

| Script | Use |
|---|---|
| `godot.env` | Godot version, file names and SHA-512 checksums |
| `fetch-godot.sh <dir> <template>...` | Downloads and verifies the editor and templates, installs the named templates |
| `version.sh` | Prints the version, as above |
| `export-desktop.sh <linux\|windows\|macos> <version> <outdir>` | Exports one preset and writes the archive and `.sha256` |
| `package.py <tar.gz\|zip> <root> <name> <archive>` | Packs a folder with stable bytes; refuses symlinks |
| `notices.py <outdir>` | Writes `THIRD-PARTY-NOTICES.txt` and `SOURCE.txt`; add a row to its `COMPONENTS` for every library linked into the game |
| `game-files.py stage <gamedir>` | Copies `CREDITS.md` and `LICENSE` into the Godot project and runs `notices.py` there |
| `game-files.py check <gamedir> <export>` | Exits 1 unless every staged file's bytes are inside the exported game (a file or a `.app` folder) |
| `notices/` | Verbatim upstream notice texts (Godot 4.4.1 `LICENSE.txt`, `COPYRIGHT.txt`) |
| `smoke-test.sh <archive>` | Verifies, unpacks and runs a package on its own platform |

## Export presets

`game/export_presets.cfg` has three presets, all `x86_64` except macOS:

| Preset | Platform | Notable options |
|---|---|---|
| `Linux` | Linux | PCK embedded |
| `Windows` | Windows Desktop | PCK embedded; `application/modify_resources=false` (no rcedit, so no icon or version info in the `.exe` yet) |
| `macOS` | macOS | Universal; bundle id `io.github.opsroller.montezuma`; built-in ad-hoc signing; no notarization; minimum macOS 10.15 (x86_64) and 11.0 (arm64) |

## Not yet covered

- Real code signing and notarization (needs the owner's Apple Developer
  and Windows signing accounts; see the store work packages).
- Linux arm64 and Windows arm64 packages.
- Version numbers inside the binaries (Windows file version, macOS
  bundle version).
- Bit-for-bit reproducibility across runners: archives are stable, but
  two runners have not yet been compared.
