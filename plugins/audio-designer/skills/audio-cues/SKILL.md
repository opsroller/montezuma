---
name: audio-cues
description: The Montezuma 3D cue names and how game code plays them. Use when adding a sound or calling one from the board or UI.
---

# Cues

Code never names files. It calls `Audio.play("swap")`; the cue table in
`scenes/audio/cues.tres` maps names to streams, bus and volume.

Starting cue list (the contract in `docs/contracts.md` is authoritative):

`swap`, `swap_illegal`, `match`, `cascade_1` … `cascade_8`, `piece_land`,
`bonus_earned`, `bonus_use_<id>`, `totem_charge`, `totem_fire_<id>`,
`quota_done`, `time_warning`, `level_won`, `level_lost`, `ui_click`,
`ui_back`, `trophy`, `music_menu`, `music_play_<episode>`,
`ambience_jungle`.

A missing cue logs a warning and plays nothing, so other packages can call
cues before the sound exists.
