---
description: Import and capture the plant look-dev pages at 3840x2160, then lay them out as contact sheets.
argument-hint: "[--scope species|rows|all] [--species heliconia,ceiba]"
allowed-tools: Bash(python3 tools/assets/lookdev.py:*)
---

```
python3 tools/assets/lookdev.py import
python3 tools/assets/lookdev.py capture $ARGUMENTS
python3 tools/assets/lookdev.py sheet
```

`import` first, always: Godot serves meshes from `.godot/imported`, so a rebuilt mesh
does not reach a frame until it has run. Frames land in `.scratch/lookdev/`.

Then look at the sheets. Judge on a close-up page, never a band row — a row is as
wide as the sum of its spreads, so its camera stands tens of metres back.
