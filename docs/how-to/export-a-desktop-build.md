# Export a desktop build

How to produce the same Windows, macOS or Linux package that CI builds,
on a Linux machine. Run from the repository root. For what each file is,
see [the build pipeline reference](../reference/build-pipeline.md).

## Get a CI build instead

Every push to `main` builds all three. Open the latest **build** run in
the repository's Actions tab and download `montezuma-windows`,
`montezuma-macos` or `montezuma-linux` from its artifacts. Check the
download with the `.sha256` file beside it:

```sh
sha256sum -c montezuma-*.sha256
```

To build a branch, run the **build** workflow from the Actions tab with
**Run workflow** and pick the branch.

## Build it yourself (Linux)

1. Build the extension's release library for Linux (the other platforms'
   libraries must be built on Windows and macOS, or downloaded from a CI
   run's `extension-windows` and `extension-macos` artifacts into
   `game/bin/`):

   ```sh
   cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON \
     -DMONTEZUMA_BUILD_TESTS=OFF -DMONTEZUMA_WERROR=ON -DGODOTCPP_TARGET=template_release
   cmake --build build/release -j
   ```

2. Fetch the pinned Godot and the templates you need (the templates
   archive is 1.2 GB; the script reuses it once downloaded):

   ```sh
   tools/release/fetch-godot.sh ~/godot-dl linux_release.x86_64
   ```

   For Windows add `windows_release_x86_64.exe windows_release_x86_64_console.exe`,
   for macOS add `macos.zip`.

3. Export and package:

   ```sh
   GODOT=~/godot-dl/godot tools/release/export-desktop.sh linux "$(tools/release/version.sh)" dist/linux
   ```

4. Check it runs:

   ```sh
   tools/release/smoke-test.sh dist/linux/montezuma-*.tar.gz
   ```

If the export fails, Godot's output is in `dist/linux/logs/`.

## Build for macOS

On a Mac, build the universal library with
`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15`
added to the configure line in step 1, then copy it into `game/bin/` on
the Linux machine and export with `macos`. The app is ad-hoc signed only:
macOS will warn that it is from an unidentified developer until signing
and notarization are set up.
