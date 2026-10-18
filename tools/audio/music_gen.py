# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Generate the music loops and the jungle ambience.

Usage, from the repository root:

    python3 tools/audio/music_gen.py               # writes game/assets/audio/music/
    python3 tools/audio/music_gen.py --only music_menu

Every piece is composed here, from a seeded random walk over a
pentatonic or modal scale with fixed phrase rules, and played by the
synthesised instruments in synth.py: clay flute, slit drum (two-tone log
drum), hide drum, seed rattle, a low wooden bass and a conch drone. No
melody, rhythm or recording comes from anywhere else.

Each piece renders into a circular buffer (synth.LoopTrack), so note
tails and reverb wrap over the loop point and the loop is seamless with
no crossfade. Output is 48 kHz stereo Ogg Vorbis, levelled to an
integrated loudness target per category.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import synth as s  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "game" / "assets" / "audio" / "music"

MUSIC_LUFS = -22.0      # integrated, per file; sits under gameplay sounds
AMBIENCE_LUFS = -30.0   # a bed under everything, music included


@dataclass
class Piece:
    name: str
    bpm: float
    bars: int
    tonic: int                      # MIDI note of the home pitch (bass octave)
    scale: list[int]                # semitone offsets of the mode
    progression: list[int]          # bass scale degree per 4-bar block, cycled
    intensity: int                  # 0 calm .. 4 driving: how many layers play
    seed: int
    mood: str


MINOR_PENT = [0, 3, 5, 7, 10]
DORIAN_PENT = [0, 2, 3, 7, 9]          # minor with a raised sixth colour
PHRYGIAN = [0, 1, 3, 5, 7, 8, 10]
HIJAZ_PENT = [0, 1, 4, 7, 8]           # bright-dark, for the last episode

PIECES = [
    Piece("music_menu", 80, 32, 38, DORIAN_PENT, [0, 3, 0, 4], 0, 11,
          "calm and open"),
    Piece("music_play_1", 92, 32, 38, MINOR_PENT, [0, 0, 4, 3], 1, 21,
          "curious, a gentle pulse"),
    Piece("music_play_2", 96, 32, 40, PHRYGIAN, [0, 1, 0, 6], 2, 31,
          "shadowed, a darker mode"),
    Piece("music_play_3", 100, 32, 33, MINOR_PENT, [0, 2, 3, 4], 2, 41,
          "steady, more drive"),
    Piece("music_play_4", 104, 32, 36, DORIAN_PENT, [0, 4, 3, 2], 3, 51,
          "bright and busy"),
    Piece("music_play_5", 110, 32, 35, HIJAZ_PENT, [0, 1, 0, 3], 4, 61,
          "urgent, the finale"),
]


def degree(piece: Piece, d: int, octave: int = 0) -> int:
    n = len(piece.scale)
    return piece.tonic + 12 * (octave + d // n) + piece.scale[d % n]


# ---------------------------------------------------------------- composition

RHYTHMS = [   # one bar of 8 eighths: note lengths in eighths, 0 = rest of 1
    [2, 2, 2, 2], [3, 1, 2, 2], [2, 1, 1, 4], [1, 1, 2, 4], [4, 2, 2],
    [3, 3, 2], [2, 2, 4], [1, 1, 1, 1, 4], [6, 2], [0, 2, 2, 3],
]


def compose_melody(piece: Piece, rng: np.random.Generator) -> list[tuple[int, float, float]]:
    """Return (midi, start_beat, length_beats) for a melody of piece.bars bars.

    Form: 2-bar motif A, its answer A', a contrasting B, then A again
    ending home; the second 8 bars repeat the form an octave-step higher
    in places and end on the tonic so the loop resolves into its start.
    """
    n = len(piece.scale)
    home = 2 * n          # melody's tonic degree, two octaves above the bass

    def phrase(start_degree: int, end_degree: int, bars: int) -> list[tuple[int, int]]:
        out = []
        d = start_degree
        for b in range(bars):
            rhythm = RHYTHMS[rng.integers(len(RHYTHMS))]
            for i, length in enumerate(rhythm):
                if length == 0:
                    out.append((None, 1))
                    continue
                last = b == bars - 1 and i == len(rhythm) - 1
                if last:
                    d = end_degree
                else:
                    step = rng.choice([-2, -1, -1, 0, 1, 1, 2], p=[.1, .25, .1, .05, .25, .15, .1])
                    d = int(np.clip(d + step, home - 3, home + n + 2))
                out.append((d, length))
        return out

    motif = phrase(home, home + 2, 2)
    answer = [(d if d is None else d, ln) for d, ln in motif[:-1]] + [(home, motif[-1][1])]
    contrast = phrase(home + 3, home + 1, 2)
    first = motif + answer + contrast + phrase(home + 2, home, 2)
    lift = [(d if d is None else d + 1, ln) for d, ln in motif]
    second = lift + answer + phrase(home + 4, home + 2, 2) + phrase(home + 1, home, 2)

    notes, beat = [], 0.0
    for d, length in first + second:
        beats = length / 2
        if d is not None:
            notes.append((degree(piece, d), beat, beats))
        beat += beats
    return notes


# ---------------------------------------------------------------- arrangement

def render_piece(piece: Piece) -> np.ndarray:
    rng = np.random.default_rng(piece.seed)
    spb = 60.0 / piece.bpm
    beats = piece.bars * 4
    track = s.LoopTrack(beats * spb)

    def at(beat: float) -> float:
        return beat * spb

    # Conch drone: one long swell per 4-bar block on the block's root.
    blocks = [piece.progression[b % len(piece.progression)] for b in range(piece.bars // 4)]
    for block, root in enumerate(blocks):
        note = degree(piece, root, 1)
        track.add(s.conch(note, 16 * spb + 0.6, rng), at(block * 16) - 0.3, 0.10, -0.2)
        track.add(s.clay_flute(note + 7, 16 * spb, rng, vibrato=0.1, breath=0.3) * 0.5,
                  at(block * 16), 0.05, 0.3)

    # Wooden bass: root on beat 1, fifth or octave answers.
    for block, root in enumerate(blocks):
        for bar in range(4):
            b0 = (block * 4 + bar) * 4
            pattern = [(0, 0), (2.5, 2)] if piece.intensity < 2 else [(0, 0), (1.5, 2), (2.5, 0), (3.5, 3)]
            for off, step in pattern:
                note = degree(piece, root + step, 0)
                track.add(s.wood_knock(note, 0.5, rng, hollowness=1.6), at(b0 + off), 0.30)

    # Slit drum ostinato: two tones, a little swing.
    if piece.intensity >= 0:
        cell = [0, 0.75, 1.5, 2, 3, 3.5] if piece.intensity >= 2 else [0, 1.5, 2, 3]
        hi, lo = degree(piece, 0, 2), degree(piece, 0, 2) - 5
        for bar in range(piece.bars):
            if piece.bars > 16 and 16 <= bar < 18:
                continue  # a two-bar breath where the second tune starts
            for i, off in enumerate(cell):
                if piece.intensity == 0 and bar % 2 == 1 and off > 0:
                    continue
                note = hi if i % 2 == 0 else lo
                track.add(s.wood_knock(note, 0.3, rng, hollowness=1.0),
                          at(bar * 4 + off) + rng.normal(0, 0.004), 0.16 * rng.uniform(0.8, 1.0),
                          pan=0.35 if i % 2 else -0.35)

    # Hide drum (huehuetl): grows with intensity.
    if piece.intensity >= 1:
        kicks = {1: [0, 2.5], 2: [0, 1.5, 2.5], 3: [0, 1.5, 2, 3.5], 4: [0, 0.75, 1.5, 2, 2.75, 3.5]}
        for bar in range(piece.bars):
            for off in kicks[piece.intensity]:
                track.add(s.membrane_drum(36 if off == 0 else 40, 0.5, rng, drop=0.5),
                          at(bar * 4 + off), 0.45 if off == 0 else 0.28)

    # Seed rattle on the off-beats.
    if piece.intensity >= 1:
        step = 0.5 if piece.intensity >= 3 else 1.0
        for k in np.arange(0, beats, step):
            track.add(s.rattle(0.12, rng, grains=12, centre=7000), at(k + step / 2), 0.10,
                      pan=rng.uniform(-0.5, 0.5))

    # Clay flute melody, with a quieter echo a beat and a half later. Each
    # 16 bars get their own tune so a long loop does not repeat itself.
    melody = []
    for section in range(piece.bars // 16):
        melody += [(n, b + section * 64, ln) for n, b, ln in compose_melody(piece, rng)]
    for note, start, length in melody:
        dur = length * spb * 0.95
        tone = s.clay_flute(note, dur, rng, vibrato=0.3)
        track.add(tone, at(start), 0.42, pan=0.1)
        track.add(s.lowpass(tone, 2500, 1), at(start + 1.5), 0.10, pan=-0.4)

    # Jade bells mark every 4-bar block (busier pieces only).
    if piece.intensity >= 2:
        for block in range(piece.bars // 4):
            track.add(s.jade_chime(degree(piece, 0, 3), 1.5, rng, bright=0.6), at(block * 16), 0.12, 0.5)

    out = s.circular_reverb(track.buf, rng, seconds=2.2, wet=0.22)
    out -= out.mean(axis=0)
    return out


def render_ambience(seconds: float = 64.0) -> np.ndarray:
    """Jungle bed: leaves and wind, insect drones, distant birds and frogs.
    Distinct calls are sparse so the 64 s loop does not draw attention."""
    rng = np.random.default_rng(71)
    track = s.LoopTrack(seconds)
    n = track.n
    t = np.arange(n) / s.RATE

    # Wind in leaves: two decorrelated pink-noise beds, slow swells that
    # complete a whole number of cycles per loop so the seam is invisible.
    for ch, phase in ((0, 0.0), (1, 1.7)):
        bed = s.lowpass(s.noise(n, rng, "pink"), 1800, 1)
        swell = 0.6 + 0.4 * np.sin(2 * np.pi * 3 * t / seconds + phase) * np.sin(2 * np.pi * 5 * t / seconds)
        track.buf[:, ch] += bed * swell * 0.5

    # Insects: high bands with fast pulsing, fading in and out.
    for centre, rate, cycles, pan in ((4800, 32, 4, -0.6), (6500, 47, 6, 0.5), (3600, 21, 3, 0.1)):
        src = s.bandpass(s.noise(n, rng), centre, 8.0)
        pulse = 0.5 + 0.5 * np.sin(2 * np.pi * rate * t)
        presence = np.clip(np.sin(2 * np.pi * cycles * t / seconds + rng.uniform(0, 6)), 0, 1) ** 2
        track.add(src * pulse * presence, 0.0, 0.08, pan)

    # Birds: short frequency-swept whistles, a few calls per minute.
    for _ in range(16):
        start = rng.uniform(0, seconds)
        base = rng.uniform(1800, 3600)
        calls = rng.integers(2, 5)
        call = []
        for c in range(calls):
            dur = rng.uniform(0.06, 0.16)
            tt = s.t_axis(dur)
            f = base * (1 + rng.uniform(0.2, 0.6) * np.sin(np.pi * tt / dur) * rng.choice([-1, 1]))
            tone = np.sin(2 * np.pi * np.cumsum(f) / s.RATE) * np.sin(np.pi * tt / dur) ** 2
            call.append((tone, c * rng.uniform(0.12, 0.22)))
        sig = s.mix(*call)
        track.add(s.reverb(sig, rng, 1.5, 0.45), start, rng.uniform(0.05, 0.12), rng.uniform(-0.9, 0.9))

    # Frogs: low pulse-train croaks, far away.
    for _ in range(10):
        start = rng.uniform(0, seconds)
        dur = rng.uniform(0.2, 0.4)
        tt = s.t_axis(dur)
        pitch = rng.uniform(180, 320)
        croak = np.sin(2 * np.pi * pitch * tt) * (0.5 + 0.5 * np.sign(np.sin(2 * np.pi * 28 * tt)))
        croak = s.lowpass(croak * np.sin(np.pi * tt / dur), 1200)
        for rep in range(rng.integers(1, 4)):
            track.add(croak, start + rep * 0.45, 0.07, rng.uniform(-0.8, 0.8))

    return track.buf - track.buf.mean(axis=0)


def write(name: str, sig: np.ndarray, target: float) -> dict:
    sig = s.level_to(sig, "integrated_lufs", target, ceiling_dbtp=-1.5)
    path = OUT / f"{name}.ogg"
    s.write_ogg(path, sig, quality=3.0)
    m = s.measure(path)
    return {"seconds": round(len(sig) / s.RATE, 3), "integrated_lufs": round(m["integrated_lufs"], 1),
            "true_peak_dbtp": round(m["true_peak_dbtp"], 1), "bytes": path.stat().st_size}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--only", help="comma-separated names")
    args = ap.parse_args()
    wanted = set(args.only.split(",")) if args.only else None
    report_path = Path(__file__).resolve().parent / "reports" / "music_loudness.json"
    report = {}
    if wanted and report_path.exists():
        import json
        report = json.loads(report_path.read_text())
    OUT.mkdir(parents=True, exist_ok=True)
    for piece in PIECES:
        if wanted and piece.name not in wanted:
            continue
        report[f"{piece.name}.ogg"] = {"bpm": piece.bpm, "bars": piece.bars, "mood": piece.mood,
                                       **write(piece.name, render_piece(piece), MUSIC_LUFS)}
        print(f"{piece.name:16s} ok")
    if not wanted or "ambience_jungle" in wanted:
        report["ambience_jungle.ogg"] = {"mood": "humid jungle, birds and insects",
                                         **write("ambience_jungle", render_ambience(), AMBIENCE_LUFS)}
        print("ambience_jungle  ok")
    report_path.parent.mkdir(exist_ok=True)
    s.write_report(report_path, report)


if __name__ == "__main__":
    main()
