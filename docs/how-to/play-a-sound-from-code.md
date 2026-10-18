# Play a sound from code

Game code never plays files. It names what happened, and the cue table
decides what that sounds like.

## From C++

```cpp
#include "audio/game_audio.hpp"

using montezuma::godot_binding::GameAudio;

if (GameAudio* audio = GameAudio::get_singleton()) {
    audio->play("match");
}
```

The singleton is null only outside the running game (for example in the
editor), so always check it.

## From a scene or the UI

Call the autoload: `/root/GameAudio`, method `play` with the cue name,
for example from a signal connection in a `.tscn`.

## Start and stop music

`play("music_play_2")` cross-fades from whatever music is playing;
`stop_music(1.0)` fades it out over one second. Ambience works the same
on its own player: `play("ambience_jungle")`, `stop("ambience_jungle")`.

## Add a cue

1. Add the name to the audio cues list in `docs/reference/contracts.md`
   (the sound designer owns it).
2. Make the sound: see [regenerate or add sounds](regenerate-or-add-sounds.md).
3. Add an entry to `game/assets/audio/cues.json` with its files, bus,
   priority and voice limit (fields in
   [the audio reference](../reference/audio.md#cue-table)).
4. Run the self-test:
   `godot --headless --path game --quit-after 120 -- --audio-selftest`.
