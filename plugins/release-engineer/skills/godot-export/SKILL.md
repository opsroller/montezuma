---
name: godot-export
description: How to export Montezuma 3D builds headless and in CI. Use when adding a platform, a CI job, or debugging an export.
---

# Exporting

Godot version is pinned in `.github/workflows/tests.yml` (4.4.1). Export
templates for the same version come from the godotengine/godot GitHub
release (`Godot_v4.4.1-stable_export_templates.tpz`), unpacked to
`~/.local/share/godot/export_templates/4.4.1.stable/`.

```sh
godot --headless --path . --export-release "Linux" build/linux/montezuma3d.x86_64
godot --headless --path . --export-release "Web"   build/web/index.html
```

- Presets in `export_presets.cfg`: `Windows`, `macOS`, `Linux`, `Web`,
  `Android`, `iOS`. Web uses the Compatibility renderer.
- Android needs the SDK and a debug keystore generated in CI; release
  signing waits for the owner's key in repository secrets.
- macOS and iOS need a macOS runner; iOS also needs the owner's Apple
  developer account.
- Releases: tag `vX.Y.Z` on `main`; version and changelog come from
  commit types (`feat` minor, `fix` patch, `!` major).
