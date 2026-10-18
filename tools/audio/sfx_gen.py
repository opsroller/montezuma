# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Generate every sound effect cue in the audio contract.

Usage, from the repository root:

    python3 tools/audio/sfx_gen.py            # writes game/assets/audio/sfx/
    python3 tools/audio/sfx_gen.py --only match,swap

Each sound is built from the instruments in synth.py (jade chimes, wood
and slit drums, hide drums, rattles, clay flute, conch) so the palette
sounds like one temple. Output is 48 kHz 16-bit mono WAV, levelled per
category (see LEVELS) and checked against a -1 dBTP ceiling. A loudness
report is written next to the files.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import synth as s  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "game" / "assets" / "audio" / "sfx"

# D minor pentatonic, the palette's home key (MIDI note numbers).
SCALE = [62, 65, 67, 69, 72, 74, 77, 79, 81, 84, 86, 89, 91, 93, 96]

# Category -> (meter, target LUFS). Short sounds use the loudest 400 ms
# momentary value; longer stingers use integrated loudness.
LEVELS = {
    "frequent": ("max_momentary_lufs", -24.0),   # swap, land, ui ticks
    "feedback": ("max_momentary_lufs", -19.0),   # match, cascade, bonus use
    "alert":    ("max_momentary_lufs", -17.0),   # time warning, quota done
    "stinger":  ("max_momentary_lufs", -16.0),   # won, lost, trophy, totem fire
}


def rng_for(name: str, variation: int) -> np.random.Generator:
    """One fixed seed per file so renders are identical on every run."""
    seed = sum(ord(c) * (i + 1) for i, c in enumerate(name)) * 100 + variation
    return np.random.default_rng(seed)


# ---------------------------------------------------------------- designs
# Each design returns a mono signal. `v` is the variation index (0-based).

def swap(r, v):
    """Two stone pieces sliding past each other: short gritty slide plus a
    soft wooden settle at the end."""
    slide = s.sweep(0.16, r, 900 + 150 * v, 2600 + 200 * v, q=2.0)
    slide *= s.env_ad(len(slide), 0.02, 0.06)
    tick = s.wood_knock(67 + [0, 2, -2][v], 0.12, r, hollowness=0.3) * 0.5
    return s.mix((slide, 0.0), (tick, 0.11))


def swap_illegal(r, v):
    """Pieces bump and go back: two dull knocks, the second lower."""
    a = s.wood_knock(50, 0.18, r, hollowness=0.5)
    b = s.wood_knock(46, 0.22, r, hollowness=0.6)
    thud = s.membrane_drum(38, 0.2, r, drop=0.2) * 0.4
    return s.mix((a, 0.0), (thud, 0.0), (b, 0.11))


def match(r, v):
    """A clean jade chime: the core reward sound, always the same pitch
    family so cascades can climb from it."""
    note = SCALE[5] + [0, 0.05, -0.05, 0.1][v]
    chime = s.jade_chime(note, 0.6, r)
    fifth = s.jade_chime(note + 7, 0.45, r, bright=0.6) * 0.35
    dust = s.rattle(0.18, r, grains=14, centre=5000) * 0.25
    return s.reverb(s.mix((chime, 0.0), (fifth, 0.005), (dust, 0.0)), r, 0.8, 0.18)


def cascade(step):
    def design(r, v):
        """Each cascade step climbs the pentatonic scale and adds sparkle."""
        base = SCALE[min(5 + step, len(SCALE) - 1)]
        parts = [(s.jade_chime(base, 0.7, r), 0.0),
                 (s.jade_chime(base + 12, 0.5, r, bright=0.5) * 0.3, 0.01)]
        if step >= 3:
            parts.append((s.shimmer(0.3 + 0.03 * step, r, base + 5, base + 24, count=4 + step) * 0.4, 0.03))
        if step >= 6:
            parts.append((s.membrane_drum(43, 0.4, r, drop=0.4) * 0.5, 0.0))
        return s.reverb(s.mix(*parts), r, 1.0, 0.2)
    return design


def piece_land(r, v):
    """A piece settles into its slot: a small low stone tap."""
    tap = s.wood_knock(55 + [0, 1, -1, 2][v], 0.1, r, hollowness=0.15)
    grit = s.bandpass(s.noise(int(0.05 * s.RATE), r), 1800, 1.0) * s.env_ad(int(0.05 * s.RATE), 0.001, 0.008) * 0.4
    return s.lowpass(s.mix((tap, 0.0), (grit, 0.0)), 5000)


def bonus_earned(r, v):
    """A power-up appears on the board: rising gold shimmer over a wooden
    pickup."""
    up = s.shimmer(0.5, r, 74, 98, count=10)
    hit = s.wood_knock(62, 0.2, r, hollowness=0.8) * 0.6
    return s.reverb(s.mix((hit, 0.0), (up, 0.02)), r, 1.0, 0.2)


def bonus_use(kind):
    """A power-up on the board goes off when its piece is cleared."""
    def design(r, v):
        if kind == "tnt":
            n = int(1.4 * s.RATE)
            fizz = s.highpass(s.noise(int(0.3 * s.RATE), r), 4000) * 0.15
            boom = s.lowpass(s.noise(n, r, "brown"), 900) * s.env_ad(n, 0.003, 0.3)
            body = s.membrane_drum(28, 1.2, r, drop=1.5)
            debris = s.rattle(0.9, r, grains=70, centre=2500) * 0.4
            return s.reverb(s.mix((fizz, 0.0), (boom, 0.3), (body, 0.3), (debris, 0.4)), r, 1.6, 0.22)
        if kind == "color_clear":
            n = int(0.9 * s.RATE)
            whoomp = s.lowpass(s.noise(n, r, "brown"), 600) * s.env_ad(n, 0.01, 0.2) * 0.8
            return s.reverb(s.mix((whoomp, 0.0), (s.shimmer(0.7, r, 70, 100, count=16) * 0.8, 0.05)), r, 1.4, 0.25)
        if kind == "lightning":
            n = int(0.7 * s.RATE)
            zap = s.noise(n, r) * (s.rattle(0.7, r, grains=60, centre=3000) != 0)
            buzz = np.sign(np.sin(2 * np.pi * 110 * s.t_axis(0.7))) * 0.3
            body = s.bandpass(zap + buzz * s.noise(n, r) * 0.2, 2500, 0.7) * s.env_ad(n, 0.002, 0.18)
            return s.reverb(body + s.membrane_drum(33, 0.7, r, 1.0) * 0.5, r, 1.4, 0.25)
        if kind == "clock":
            ticks = [(s.wood_knock(79 if i % 2 == 0 else 74, 0.1, r, 0.3) * 0.6, i * 0.09) for i in range(4)]
            rise = s.shimmer(0.4, r, 79, 96, count=6)
            return s.reverb(s.mix(*ticks, (rise, 0.34)), r, 1.0, 0.2)
        if kind == "multiplier":
            chord = [(s.jade_chime(nn, 0.8, r) * 0.6, i * 0.06) for i, nn in enumerate((62, 69, 74, 81))]
            return s.reverb(s.mix(*chord, (s.wood_knock(62, 0.3, r, 1.0) * 0.6, 0.0)), r, 1.2, 0.22)
        if kind == "hint":
            # Quiet on purpose: a hint should guide the eye, not reward.
            return s.reverb(s.mix((s.jade_chime(93, 0.5, r, bright=0.5) * 0.6, 0.0),
                                  (s.jade_chime(98, 0.4, r, bright=0.4) * 0.3, 0.08)), r, 1.0, 0.3)
        raise ValueError(kind)
    return design


def gem_collect(r, v):
    """A gem piece is cleared and counts toward the goal: a bright glint
    above the match chime, so gems are heard apart from plain matches."""
    glint = s.jade_chime(SCALE[10] + [0, 0.1, -0.1][v], 0.5, r, bright=1.4)
    sparkle = s.shimmer(0.18, r, 96, 104, count=3) * 0.5
    return s.reverb(s.mix((glint, 0.0), (sparkle, 0.03)), r, 0.8, 0.2)


def steel_break(r, v):
    """A steel piece is destroyed: hard metallic crack, inharmonic ring."""
    n = int(0.6 * s.RATE)
    t = s.t_axis(0.6)
    ring = sum(np.sin(2 * np.pi * f * t) * np.exp(-t / d) for f, d in
               ((1230 + 40 * v, 0.25), (2710, 0.12), (4190, 0.07), (6330, 0.04)))
    crack = s.highpass(s.noise(n, r), 2000) * s.env_ad(n, 0.0005, 0.02)
    return s.reverb(s.mix((ring * 0.4 + crack, 0.0), (s.wood_knock(45, 0.2, r, 0.2) * 0.5, 0.0)), r, 0.9, 0.18)


def chain_break(r, v):
    """Chains on a cell snap: a few small metal clinks falling away."""
    parts = []
    for i in range(4):
        tt = s.t_axis(0.12)
        f = r.uniform(2500, 4200)
        clink = np.sin(2 * np.pi * f * tt) * np.exp(-tt / 0.03) + s.noise(len(tt), r) * np.exp(-tt / 0.002) * 0.4
        parts.append((clink * (1 - i * 0.18), i * r.uniform(0.04, 0.07) + i * 0.02))
    return s.reverb(s.mix(*parts), r, 0.7, 0.15)


def totem_charge(r, v):
    """A totem fills a notch: short rising hum with a soft chime on top."""
    t = s.t_axis(0.45)
    f = 110 * 2 ** (t / 0.45)
    hum = np.sin(2 * np.pi * np.cumsum(f) / s.RATE) * s.env_ad(len(t), 0.15, 0.2) * 0.6
    chime = s.jade_chime(SCALE[7], 0.4, r, bright=0.7) * 0.5
    return s.reverb(s.mix((hum, 0.0), (chime, 0.25)), r, 0.9, 0.2)


def totem_fire(kind):
    """A Power Totem fires: conch and drum say "totem", the layer on top
    says which one."""
    def design(r, v):
        horn = s.conch(50, 0.9, r) * 0.6
        drum = s.membrane_drum(33, 0.8, r, drop=0.8)
        if kind == "fire":        # fireballs clear random pieces
            fx = s.mix(*[(s.sweep(0.35, r, 400, 2500, q=1.5) * s.env_ad(int(0.35 * s.RATE), 0.05, 0.1)
                          + s.membrane_drum(40 + 3 * i, 0.35, r, 1.0) * 0.6, 0.15 + i * 0.12) for i in range(4)])
        elif kind == "time":      # adds time: rising log-drum ticks
            fx = s.mix(*[(s.wood_knock(nn, 0.25, r, 1.0) * 0.6, 0.1 + i * 0.12)
                         for i, nn in enumerate((62, 69, 74, 81))])
        elif kind == "gold":      # points: a falling shower of gold
            fx = s.shimmer(0.9, r, 76, 104, count=24, rising=False) * 0.8
        elif kind == "harvest":   # gathers every gem: fast rising glints
            fx = s.mix(*[(s.jade_chime(SCALE[i % len(SCALE)] + 12, 0.4, r, bright=1.3) * 0.5, 0.1 + i * 0.05)
                         for i in range(10)])
        elif kind == "ward":      # breaks steel: cracks and ring
            fx = s.mix(*[(steel_break(r, i % 2) * 0.7, 0.15 + i * 0.14) for i in range(3)])
        elif kind == "gift":      # places power-ups: wooden pops with sparkle
            fx = s.mix(*[(s.wood_knock(74 + 5 * i, 0.3, r, 0.8) * 0.6 + s.jade_chime(86 + 5 * i, 0.3, r) * 0.3,
                          0.15 + i * 0.15) for i in range(3)])
        elif kind == "paint":     # repaints pieces: a swirl and soft bells
            fx = s.mix((s.sweep(0.8, r, 500, 4000, q=2.5) * s.env_ad(int(0.8 * s.RATE), 0.3, 0.2) * 0.6, 0.1),
                       (s.shimmer(0.6, r, 79, 91, count=8) * 0.5, 0.3))
        else:
            raise ValueError(kind)
        return s.reverb(s.mix((horn, 0.0), (drum, 0.0), (fx, 0.0)), r, 1.8, 0.25)
    return design


def quota_done(r, v):
    """A colour's quota is filled: two-note wooden call, then a high chime."""
    return s.reverb(s.mix((s.wood_knock(69, 0.25, r, 1.0), 0.0),
                          (s.wood_knock(74, 0.3, r, 1.0), 0.12),
                          (s.jade_chime(86, 0.6, r) * 0.7, 0.12)), r, 1.0, 0.2)


def time_warning(r, v):
    """Time running low: an urgent slit-drum pair that cuts through music.
    Played once per warning by the game, so it is short and dry."""
    return s.mix((s.wood_knock(74, 0.15, r, 1.0), 0.0), (s.wood_knock(69, 0.18, r, 1.0), 0.14),
                 (s.membrane_drum(45, 0.25, r, 0.3) * 0.5, 0.0))


def level_won(r, v):
    """Level complete: a conch call, then a rising pentatonic phrase on
    clay flute over hide drums, ending on the home note."""
    phrase = [(62, 0.0, 0.2), (65, 0.2, 0.2), (69, 0.4, 0.2), (72, 0.6, 0.25), (74, 0.85, 1.1)]
    flute = s.mix(*[(s.clay_flute(n + 12, d, r, vibrato=0.4), st + 0.5) for n, st, d in phrase])
    drums = s.mix(*[(s.membrane_drum(40 if i % 2 == 0 else 45, 0.4, r, 0.4) * 0.6, 0.5 + i * 0.2) for i in range(5)])
    horn = s.conch(50, 0.7, r) * 0.6
    sparkle = s.shimmer(0.9, r, 84, 100, count=10) * 0.4
    return s.reverb(s.mix((horn, 0.0), (flute, 0.0), (drums, 0.0), (sparkle, 1.35),
                          (s.jade_chime(86, 1.2, r) * 0.5, 1.35)), r, 2.0, 0.25)


def level_lost(r, v):
    """Out of time: a falling flute phrase over a slow low drum."""
    phrase = [(74, 0.0, 0.3), (72, 0.3, 0.3), (69, 0.6, 0.35), (65, 0.95, 0.9)]
    flute = s.mix(*[(s.clay_flute(n, d, r, vibrato=0.5), st) for n, st, d in phrase])
    drums = s.mix((s.membrane_drum(36, 0.8, r, 0.5) * 0.7, 0.0), (s.membrane_drum(33, 1.2, r, 0.5) * 0.8, 0.95))
    return s.reverb(s.mix((flute, 0.0), (drums, 0.0)), r, 2.0, 0.28)


def ui_click(r, v):
    """Menu confirm: small dry wooden tick, slightly bright."""
    return s.wood_knock(84 + [0, 0.3, -0.3][v], 0.06, r, hollowness=0.1)


def ui_back(r, v):
    """Menu back: the same tick pitched down, a fourth below, doubled."""
    return s.mix((s.wood_knock(79, 0.05, r, 0.1), 0.0), (s.wood_knock(74, 0.07, r, 0.1), 0.05))


def trophy(r, v):
    """Trophy unlocked: conch call under a long gold shimmer and a chord."""
    chord = s.mix(*[(s.jade_chime(nn, 1.4, r) * 0.45, i * 0.05) for i, nn in enumerate((74, 79, 81, 86))])
    return s.reverb(s.mix((s.conch(62, 1.0, r) * 0.5, 0.0), (chord, 0.15),
                          (s.shimmer(1.2, r, 80, 104, count=20) * 0.5, 0.2)), r, 2.2, 0.28)


# Ids from the contract (WP-03): one totem per kind, six power-ups.
TOTEMS = ["fire", "time", "gold", "harvest", "ward", "gift", "paint"]
POWER_UPS = ["tnt", "color_clear", "lightning", "clock", "multiplier", "hint"]

# cue name -> (design, variations, category). Variations exist only for
# sounds that fire often enough for repetition to show.
CUES = {
    "swap": (swap, 3, "frequent"),
    "swap_illegal": (swap_illegal, 1, "feedback"),
    "match": (match, 4, "feedback"),
    **{f"cascade_{i}": (cascade(i), 1, "feedback") for i in range(1, 9)},
    "piece_land": (piece_land, 4, "frequent"),
    "bonus_earned": (bonus_earned, 1, "feedback"),
    **{f"bonus_use_{b}": (bonus_use(b), 1, "frequent" if b == "hint" else "feedback") for b in POWER_UPS},
    "gem_collect": (gem_collect, 3, "feedback"),
    "steel_break": (steel_break, 2, "feedback"),
    "chain_break": (chain_break, 2, "feedback"),
    "totem_charge": (totem_charge, 1, "feedback"),
    **{f"totem_fire_{t}": (totem_fire(t), 1, "stinger") for t in TOTEMS},
    "quota_done": (quota_done, 1, "alert"),
    "time_warning": (time_warning, 1, "alert"),
    "level_won": (level_won, 1, "stinger"),
    "level_lost": (level_lost, 1, "stinger"),
    "ui_click": (ui_click, 3, "frequent"),
    "ui_back": (ui_back, 1, "frequent"),
    "trophy": (trophy, 1, "stinger"),
}


def file_names(cue: str, variations: int) -> list[str]:
    if variations == 1:
        return [f"{cue}.wav"]
    return [f"{cue}_{i + 1:02d}.wav" for i in range(variations)]


def render(cue: str) -> dict:
    design, variations, category = CUES[cue]
    key, target = LEVELS[category]
    report = {}
    for i, fname in enumerate(file_names(cue, variations)):
        sig = design(rng_for(cue, i), i)
        sig = s.highpass(sig - np.mean(sig), 30, 2)       # no DC, no rumble
        sig = s.level_to(sig, key, target, ceiling_dbtp=-1.5)  # margin under -1 dBTP
        sig = s.fade(sig, 0.001, 0.015)
        # Trim the silent end of reverb tails.
        loud = np.nonzero(np.abs(sig) > 10 ** (-60 / 20))[0]
        sig = s.fade(sig[:loud[-1] + 1 + int(0.01 * s.RATE)], 0.0005, 0.02)
        path = OUT / fname
        s.write_wav(path, sig)
        m = s.measure(path)
        report[fname] = {"cue": cue, "category": category, "seconds": round(len(sig) / s.RATE, 3),
                         key.replace("max_", ""): round(s.loudness_of(sig, key), 1),
                         "true_peak_dbtp": round(m["true_peak_dbtp"], 1)}
        if m["true_peak_dbtp"] > -1.0:
            raise SystemExit(f"{fname}: true peak {m['true_peak_dbtp']} dBTP above -1")
    return report


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--only", help="comma-separated cue names")
    args = ap.parse_args()
    cues = args.only.split(",") if args.only else list(CUES)
    unknown = [c for c in cues if c not in CUES]
    if unknown:
        raise SystemExit(f"unknown cue(s): {', '.join(unknown)}. Known: {', '.join(CUES)}")
    report_path = Path(__file__).resolve().parent / "reports" / "sfx_loudness.json"
    report = {}
    if args.only and report_path.exists():
        import json
        report = json.loads(report_path.read_text())
    for cue in cues:
        report.update(render(cue))
        print(f"{cue:24s} ok")
    report_path.parent.mkdir(exist_ok=True)
    s.write_report(report_path, report)


if __name__ == "__main__":
    main()
