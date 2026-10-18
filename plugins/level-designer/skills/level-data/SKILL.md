---
name: level-data
description: Format and authoring rules for Montezuma 3D level files in data/levels/. Use when writing or editing levels.
---

# Level files

One JSON file per level: `data/levels/e<episode>/l<NN>.json`. The schema is
the LevelData contract in `docs/contracts.md`; the loader in `core/`
rejects files that break it.

```json
{
  "id": "e1-l03",
  "episode": 1,
  "index": 3,
  "board": { "width": 8, "height": 8, "kinds": 5 },
  "quotas": { "red": 12, "blue": 10, "green": 8 },
  "time_limit": { "relaxed": 240, "normal": 150 },
  "totems_unlocked": ["time", "lightning"],
  "blocked_cells": [],
  "seed": 1003
}
```

## Authoring

- Episodes introduce one idea each: a new artifact kind, a totem, a
  blocked-cell shape. Note it in `data/levels/e<N>/README.md`.
- Raise difficulty with quota size and kind count first, time second.
- Every level gets a fixed seed so the simulator and tests are repeatable;
  the game reseeds at play time.
