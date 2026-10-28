---
description: Regenerate the plant assets - parameters, atlases, meshes, materials, catalogue - without rendering anything.
argument-hint: "[--species heliconia,ceiba]"
allowed-tools: Bash(python3 tools/assets/lookdev.py:*)
---

```
python3 tools/assets/lookdev.py params
python3 tools/assets/lookdev.py atlases
python3 tools/assets/lookdev.py build $ARGUMENTS
python3 tools/assets/lookdev.py check
python3 tools/assets/lookdev.py validate
```

`params` must precede `atlases`, which read what it writes. The atlases take about
three minutes when the colours changed and are skipped when they did not. `build`
runs under the flake's pinned Blender, because the version is part of the asset.
