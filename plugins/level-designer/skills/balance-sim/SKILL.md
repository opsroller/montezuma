---
name: balance-sim
description: Run the headless Montezuma 3D balance simulator to check a level's difficulty. Use after writing or tuning levels.
---

# Balance simulator

`tools/sim/simulate.gd` plays each level many times with simple bots
(greedy: biggest immediate clear; quota-aware: prefers kinds still needed)
at a fixed moves-per-second rate, and reports win rate and median time
left per difficulty.

```sh
godot --headless --path . --script res://tools/sim/simulate.gd -- \
  --levels data/levels/e1 --runs 200 --mps 0.5
```

## Targets

| Episode | Relaxed win rate (quota-aware bot) | Normal |
|---|---|---|
| 1 | 95%+ | 85%+ |
| 2-3 | 90%+ | 70-85% |
| 4-5 | 85%+ | 55-75% |

These are starting targets. Tighten them once the spec records how hard
the original felt. Commit the sim report summary in the PR description.
