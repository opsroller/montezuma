---
name: sound-synthesis
description: How to generate Montezuma 3D sound effects and loops from code. Use when creating audio files.
---

# Generating sound

Python with numpy is available in cloud sessions, and ffmpeg for encoding.

```sh
python3 tools/audio/make_sfx.py --out assets/audio/sfx   # writes .wav
ffmpeg -i in.wav -c:a libvorbis -q:a 5 out.ogg            # loops and music as .ogg
```

- Effects: additive or FM synthesis with short envelopes; stone clacks from
  filtered noise bursts; chimes from inharmonic partials.
- Cascade chimes: one base chime pitched up a scale step per cascade
  level (`cascade_1` to `cascade_8`).
- Music: percussion patterns (log drums, shakers) plus a pentatonic flute or
  marimba line, rendered to seamless `.ogg` loops at 44.1 kHz.
- Effects as 16-bit `.wav` (low latency), music and ambience as `.ogg`.
- Normalize effects to -6 dBFS peak, music to about -16 LUFS.
