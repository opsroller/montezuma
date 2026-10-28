---
description: Run the plant look-dev loop end to end - params, atlases, meshes, pages, import, checks, validation, 4K capture, contact sheets, measurement.
argument-hint: "[--species heliconia,ceiba] [--scope species|rows|all]"
allowed-tools: Bash(python3 tools/assets/lookdev.py:*)
---

Run the whole plant pipeline and report what it found.

```
python3 tools/assets/lookdev.py all $ARGUMENTS
```

Then look at the contact sheets in `.scratch/lookdev/` and say what the frames show.
The numbers cannot see folded cardboard: read the measurements, then read the
pictures.
