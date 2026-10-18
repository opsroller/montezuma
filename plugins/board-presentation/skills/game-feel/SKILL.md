---
name: game-feel
description: Timing and juice targets that keep Montezuma 3D feeling like the original. Use when tuning animation, camera or effects.
---

# Feel targets

Starting values, in `scenes/board/feel.gd`. Adjust against reference video
from the level designer's research, then record the final numbers in the
spec's "Feel targets" section via the level designer.

| Beat | Target |
|---|---|
| Swap | 0.12-0.15 s, ease-out |
| Illegal bounce | 0.10 s out, 0.10 s back |
| Clear | 0.15 s scale-down with flash and particles |
| Fall | ~0.06 s per cell, slight bounce on land |
| Cascade gap | under 0.05 s between steps |
| Cascade audio | rising pitch per step (cue `cascade_<n>`) |

## Rules

- Readability first: pieces never overlap from the camera's view, colors
  and silhouettes stay distinct at phone size.
- 3D adds depth (lighting, gloss, a slight tilt, particles) but no camera
  moves during play beyond a tiny shake on big cascades.
- 60 fps on mid-range Android with the Mobile renderer.
