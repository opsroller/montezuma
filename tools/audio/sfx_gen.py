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
    """Two carved stone pieces slide past each other: stick-slip grit that
    speeds up and slows down, then a small stone settle as they stop."""
    slide_len = 0.17 + 0.01 * v
    vel = np.sin(np.linspace(0, np.pi, 32)) ** 0.8
    a = s.friction(slide_len, r, f0=1900 + 180 * v, rate=700, velocity=vel)
    b = s.friction(slide_len, r, f0=2600 - 150 * v, rate=500, velocity=vel) * 0.6
    settle = s.impact(900 + 70 * v, "stone", r, contact_ms=0.5, count=16) * 0.7
    body = s.impact(240, "stone", r, contact_ms=1.6, count=8) * 0.35
    dry = s.mix((a, 0.0), (b, 0.005), (settle, slide_len - 0.01), (body, slide_len - 0.01))
    return s.fit(s.stone_room(dry, r, 0.35, 0.08, size=0.5), 0.3)


def swap_illegal(r, v):
    """The pieces bump into each other and slide back: a short shove, a
    heavy stone knock, the scrape back and a dull settle."""
    vel = np.sin(np.linspace(0, np.pi, 16))
    push = s.friction(0.07, r, f0=1600, rate=600, velocity=vel) * 0.6
    bump = s.impact(620, "stone", r, contact_ms=0.9, count=18, crack=0.15)
    thud = s.impact(170, "stone", r, contact_ms=2.5, count=8) * 0.6
    back = s.friction(0.11, r, f0=1400, rate=500, velocity=vel) * 0.5
    settle = s.impact(520, "stone", r, contact_ms=1.1, count=14) * 0.6
    dry = s.mix((push, 0.0), (bump, 0.065), (thud, 0.065), (back, 0.09), (settle, 0.2), (thud * 0.5, 0.2))
    return s.soft_limit(s.fit(s.stone_room(dry, r, 0.4, 0.1, size=0.5), 0.4), 4.0)


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
    """A falling stone piece drops into its slot: a hard stone click with
    a short ringing body, and the board's low knock under it. Variations
    differ in size, so a cascade's landings do not sound identical."""
    f0 = [1150, 1320, 980, 1500][v]
    click = s.impact(f0, "stone", r, contact_ms=[0.35, 0.3, 0.45, 0.25][v], count=20)
    board = s.impact(210 + 20 * v, "stone", r, contact_ms=1.8, count=8) * 0.45
    return s.fit(s.stone_room(s.mix((click, 0.0), (board, 0.0)), r, 0.2, 0.06, size=0.4), 0.15, 0.05)


def bonus_earned(r, v):
    """A power-up appears on the board: rising gold shimmer over a wooden
    pickup."""
    up = s.shimmer(0.5, r, 74, 98, count=10)
    hit = s.wood_knock(62, 0.2, r, hollowness=0.8) * 0.6
    return s.reverb(s.mix((hit, 0.0), (up, 0.02)), r, 1.0, 0.2)


def tnt(r):
    """A charge goes off on the board: a crackling fuse, the blast's
    pressure wave and low body, then stone debris raining down in the
    temple hall."""
    fuse_len = 0.28
    n = int(fuse_len * s.RATE)
    sparks = (r.uniform(0, 1, n) < 0.05) * np.minimum(r.pareto(2.0, n), 3.0)
    fuse = s.highpass(sparks, 2500) * 0.25 + s.bandpass(s.noise(n, r), 6000, 0.7) * 0.03
    fuse *= np.linspace(0.4, 1.0, n)
    # Friedlander pressure wave: sharp rise, decay through a negative phase.
    t = s.t_axis(0.12)
    wave = (1 - t / 0.018) * np.exp(-2.2 * t / 0.018)
    wave = s.lowpass(wave, 3500)
    m = int(1.2 * s.RATE)
    crack = s.noise(int(0.08 * s.RATE), r) * s.env_ad(int(0.08 * s.RATE), 0.0003, 0.015)
    body = s.lowpass(s.noise(m, r, "brown"), 160, 2) * s.env_ad(m, 0.004, 0.32)
    rumble = s.lowpass(s.noise(m, r, "brown"), 450, 2) * s.env_ad(m, 0.01, 0.18)
    blast = s.mix((wave * 1.2, 0.0), (crack * 0.6, 0.0), (body * 0.9, 0.0), (rumble * 0.5, 0.0))
    blast = np.tanh(blast * 2.0) / 2.0          # the air and the mic both saturate
    rain = s.debris(1.6, r, pieces=40, material="stone", f_low=700, f_high=5000, start_spread=0.9) * 0.3
    dry = s.mix((fuse, 0.0), (blast, fuse_len), (rain, fuse_len + 0.08))
    return s.soft_limit(s.fit(s.stone_room(dry, r, 1.8, 0.3, size=1.6), 2.2, 0.4), 6.0)


def bonus_use(kind):
    """A power-up on the board goes off when its piece is cleared."""
    def design(r, v):
        if kind == "tnt":
            return tnt(r)
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
    """A steel band is smashed: a heavy blow on dense ringing metal, the
    crack of the band failing, and fragments skittering over stone."""
    blow = s.impact(380 + 45 * v, "metal", r, contact_ms=0.7, count=40, crack=0.5, damping=0.55)
    snap = s.impact(1150 + 90 * v, "metal", r, contact_ms=0.2, count=24, crack=0.8, damping=0.3)
    thud = s.impact(150, "stone", r, contact_ms=3.0, count=8) * 0.7
    bits = s.debris(1.0, r, pieces=9, material="metal", f_low=2200, f_high=6500,
                    start_spread=0.25, damping=0.12) * 0.35
    dry = s.mix((blow, 0.0), (thud, 0.0), (snap, 0.012), (bits, 0.06))
    return s.soft_limit(s.fit(s.stone_room(dry, r, 1.0, 0.18), 1.5, 0.5), 4.0)


def chain_break(r, v):
    """Chains on a cell snap: a link parts under tension with a sharp
    crack, then the loose links rattle down and settle on stone."""
    snap = s.impact(1700 + 150 * v, "metal", r, contact_ms=0.15, count=24, crack=0.9, damping=0.15)
    parts = [(snap, 0.0)]
    times = np.sort(r.beta(1.4, 2.6, 16)) * 0.5 + 0.03
    for i, tt in enumerate(times):
        link = s.impact(r.uniform(2400, 5200), "metal", r, contact_ms=r.uniform(0.1, 0.25),
                        count=12, damping=0.08)
        parts.append((link * r.uniform(0.2, 0.6) * (1 - 0.03 * i), float(tt)))
    for tt in (0.18, 0.31, 0.45):
        parts.append((s.impact(r.uniform(500, 700), "stone", r, contact_ms=0.8, count=10) * 0.25,
                      tt + r.uniform(-0.02, 0.02)))
    return s.fit(s.stone_room(s.mix(*parts), r, 0.6, 0.12, size=0.6), 0.9, 0.15)


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
