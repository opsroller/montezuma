---
name: godot-ui
description: Conventions for Montezuma 3D screens and HUD in Godot 4 Control nodes. Use when building anything in ui/.
---

# UI conventions

- One scene per screen in `ui/screens/`, reusable widgets in
  `ui/widgets/`, one shared `Theme` resource in `ui/theme/main.tres`.
- Layout with containers and anchors only, no absolute positions; test at
  720×1280 portrait and 1920×1080 landscape.
- Screens change through a `Screens` autoload (`Screens.go("episode_map")`)
  so navigation lives in one place.
- Read game state from signals the game scene emits (`quota_changed`,
  `time_changed`, `score_changed`, `totem_charge_changed`); the exact names
  are in `docs/contracts.md`.
- Fonts: OFL-licensed only, recorded in `assets/PROVENANCE.md`.
- Play `ui_click` / `ui_back` cues on every button.
