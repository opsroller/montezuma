# Regenerate or add sounds

Every sound in the game is rendered by a script in `tools/audio/`. Change
the script, re-render, and commit the script and the files together.

## Before you start

Python 3.10+, numpy and ffmpeg (with `libvorbis` for music). Run from the
repository root.

## Regenerate the sound effects

```sh
python3 tools/audio/sfx_gen.py                    # every cue
python3 tools/audio/sfx_gen.py --only match,swap   # just these cues
```

Files land in `game/assets/audio/sfx/`; measured levels land in
`tools/audio/reports/sfx_loudness.json`. Renders are deterministic: each
file has a fixed random seed, so an unchanged script gives identical
files.

## Regenerate the music and ambience

```sh
python3 tools/audio/music_gen.py                       # every piece, about 3 minutes
python3 tools/audio/music_gen.py --only music_play_2   # one piece
```

Files land in `game/assets/audio/music/`, levels in
`tools/audio/reports/music_loudness.json`. Each piece's seed is in its
`Piece(...)` row, so a render is repeatable.

## Change or add a music piece

1. Edit or add a `Piece(...)` row in `PIECES` in
   `tools/audio/music_gen.py`: tempo, bars (a multiple of 16), the bass
   note, a scale, the bass progression per 4-bar block, the intensity
   (0 to 4, which layers play) and a seed. A new seed gives a new tune.
2. Render it with `--only`, then listen to the loop point: play the end
   into the start and check for a gap, click or jump in level.
3. Update the music table in `docs/reference/audio.md`. A new cue also
   needs its contract line and a cue table entry.

## Add a sound effect cue

1. Agree the cue name with the gameplay programmer and add it to the
   audio cues list in `docs/reference/contracts.md`.
2. In `tools/audio/sfx_gen.py`, write a design function `name(r, v)`
   that returns a mono numpy signal, built from the instruments in
   `tools/audio/synth.py`. `r` is the file's random generator, `v` the
   variation index. Give it a one-line docstring saying what the player
   should learn from it.
3. Add it to `CUES` with its variation count (more than one only for
   sounds that fire often) and its category from the levels table in
   [the audio reference](../reference/audio.md#levels).
4. Run `python3 tools/audio/sfx_gen.py --only <name>`. The script fails
   if a file's true peak goes above -1 dBTP.
5. Listen in context: in the game, with music on, on a phone speaker and
   on headphones.
6. Add its row to the sound-effects table in `docs/reference/audio.md`
   and its line to `game/assets/PROVENANCE.md`, in the same PR.
