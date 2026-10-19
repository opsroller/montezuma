# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Small synthesis kit shared by the sound and music generators.

Everything here is plain numpy so the game's audio can be rebuilt from
source on any machine: no samples, no presets, no recordings. Signals are
float64 arrays at RATE, mono unless a function says otherwise.
"""

from __future__ import annotations

import json
import re
import subprocess
import wave
from pathlib import Path

import numpy as np

RATE = 48000


# ---------------------------------------------------------------- basics

def t_axis(seconds: float) -> np.ndarray:
    return np.arange(int(round(seconds * RATE))) / RATE


def midi_hz(note: float) -> float:
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def env_ad(n: int, attack: float, decay: float, curve: float = 1.0) -> np.ndarray:
    """Attack then exponential-ish decay over n samples (times in seconds)."""
    t = np.arange(n) / RATE
    a = np.clip(t / max(attack, 1e-4), 0.0, 1.0)
    d = np.exp(-np.maximum(t - attack, 0.0) / max(decay, 1e-4)) ** curve
    return a * d


def fade(sig: np.ndarray, fade_in: float = 0.002, fade_out: float = 0.01) -> np.ndarray:
    """Short raised-cosine fades so no file starts or ends with a click."""
    out = sig.copy()
    for length, rev in ((fade_in, False), (fade_out, True)):
        n = min(int(length * RATE), len(out) // 2)
        if n <= 0:
            continue
        ramp = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, n))
        if rev:
            out[-n:] *= ramp[::-1, None] if out.ndim == 2 else ramp[::-1]
        else:
            out[:n] *= ramp[:, None] if out.ndim == 2 else ramp
    return out


def pad(sig: np.ndarray, n: int) -> np.ndarray:
    if len(sig) >= n:
        return sig[:n]
    shape = (n - len(sig),) + sig.shape[1:]
    return np.concatenate([sig, np.zeros(shape)])


def mix(*parts: tuple[np.ndarray, float]) -> np.ndarray:
    """Sum (signal, start_seconds) pairs into one buffer."""
    end = max(int(round(s * RATE)) + len(x) for x, s in parts)
    out = np.zeros(end)
    for x, s in parts:
        i = int(round(s * RATE))
        out[i:i + len(x)] += x
    return out


# ---------------------------------------------------------------- filters

def one_pole_lp(sig: np.ndarray, cutoff_hz: float | np.ndarray) -> np.ndarray:
    """One-pole low-pass in the frequency domain for fixed cutoffs, or a
    sample loop for a time-varying cutoff."""
    if np.isscalar(cutoff_hz):
        return fft_filter(sig, lambda f: 1.0 / np.sqrt(1.0 + (f / cutoff_hz) ** 2))
    a = np.exp(-2 * np.pi * np.asarray(cutoff_hz) / RATE)
    out = np.empty_like(sig)
    y = 0.0
    for i, (x, k) in enumerate(zip(sig, a)):
        y = (1 - k) * x + k * y
        out[i] = y
    return out


def fft_filter(sig: np.ndarray, gain_of_hz) -> np.ndarray:
    """Zero-phase filter: multiply the spectrum by gain_of_hz(frequencies)."""
    n = len(sig)
    spec = np.fft.rfft(sig, n)
    f = np.fft.rfftfreq(n, 1 / RATE)
    return np.fft.irfft(spec * gain_of_hz(f), n)


def bandpass(sig: np.ndarray, centre: float, q: float) -> np.ndarray:
    return fft_filter(sig, lambda f: 1.0 / (1.0 + (q * (f / centre - centre / np.maximum(f, 1e-3))) ** 2))


def highpass(sig: np.ndarray, cutoff: float, order: int = 2) -> np.ndarray:
    return fft_filter(sig, lambda f: 1.0 / np.sqrt(1.0 + (cutoff / np.maximum(f, 1e-3)) ** (2 * order)))


def lowpass(sig: np.ndarray, cutoff: float, order: int = 2) -> np.ndarray:
    return fft_filter(sig, lambda f: 1.0 / np.sqrt(1.0 + (f / cutoff) ** (2 * order)))


def noise(n: int, rng: np.random.Generator, colour: str = "white") -> np.ndarray:
    w = rng.standard_normal(n)
    if colour == "white":
        return w
    slope = {"pink": 0.5, "brown": 1.0}[colour]
    out = fft_filter(w, lambda f: 1.0 / np.maximum(f, 20.0) ** slope)
    return out / (np.std(out) + 1e-12)


def reverb(sig: np.ndarray, rng: np.random.Generator, seconds: float = 1.2,
           wet: float = 0.25, brightness: float = 5000.0) -> np.ndarray:
    """Convolution with a synthetic stone-room tail (decaying filtered noise)."""
    n = int(seconds * RATE)
    ir = noise(n, rng) * np.exp(-np.arange(n) / (RATE * seconds / 6.9))
    ir = lowpass(ir, brightness, 1)
    ir /= np.sqrt(np.sum(ir ** 2)) + 1e-12
    m = len(sig) + n
    tail = np.fft.irfft(np.fft.rfft(sig, m) * np.fft.rfft(ir, m), m)
    return pad(sig, m) * (1 - wet) + tail * wet


# ---------------------------------------------------------------- voices

def jade_chime(note: float, length: float, rng: np.random.Generator,
               bright: float = 1.0) -> np.ndarray:
    """Struck jade or stone bar: inharmonic partials, fast bright decay."""
    t = t_axis(length)
    f0 = midi_hz(note)
    out = np.zeros_like(t)
    for ratio, amp, dec in ((1.0, 1.0, 0.9), (2.76, 0.45 * bright, 0.35),
                            (5.40, 0.25 * bright, 0.15), (8.93, 0.12 * bright, 0.07)):
        if f0 * ratio > RATE * 0.45:
            continue
        ph = rng.uniform(0, 2 * np.pi)
        out += amp * np.sin(2 * np.pi * f0 * ratio * t + ph) * np.exp(-t / (dec * length))
    click = noise(len(t), rng) * np.exp(-t / 0.002) * 0.3 * bright
    return fade(out + highpass(click, 2000), 0.0005, 0.02)


def wood_knock(note: float, length: float, rng: np.random.Generator,
               hollowness: float = 1.0) -> np.ndarray:
    """Slit drum or wooden block: a pitched body plus a short noisy strike."""
    t = t_axis(length)
    f0 = midi_hz(note)
    body = (np.sin(2 * np.pi * f0 * t) + 0.35 * np.sin(2 * np.pi * f0 * 2.02 * t)
            + 0.15 * np.sin(2 * np.pi * f0 * 3.9 * t))
    body *= np.exp(-t / (0.06 + 0.12 * hollowness))
    strike = bandpass(noise(len(t), rng), f0 * 4, 1.5) * np.exp(-t / 0.004)
    return fade(body + 0.6 * strike, 0.0005, 0.01)


def membrane_drum(note: float, length: float, rng: np.random.Generator,
                  drop: float = 0.5) -> np.ndarray:
    """Low hide drum: a pitch that sags after the hit, plus a skin slap."""
    t = t_axis(length)
    f = midi_hz(note) * (1 + drop * np.exp(-t / 0.03))
    phase = 2 * np.pi * np.cumsum(f) / RATE
    body = np.sin(phase) * np.exp(-t / (length * 0.35))
    slap = lowpass(noise(len(t), rng), 1800) * np.exp(-t / 0.012)
    return fade(body + 0.5 * slap, 0.0005, 0.02)


def rattle(length: float, rng: np.random.Generator, grains: int = 40,
           centre: float = 6000.0) -> np.ndarray:
    """Seed-pod rattle: a burst of tiny filtered clicks."""
    n = int(length * RATE)
    out = np.zeros(n)
    times = np.sort(rng.beta(1.5, 3.0, grains)) * (n - 400)
    for i in times.astype(int):
        g = noise(300, rng) * np.exp(-np.arange(300) / 40.0) * rng.uniform(0.3, 1.0)
        out[i:i + 300] += g
    return fade(bandpass(out, centre, 1.2), 0.001, 0.01)


def clay_flute(note: float, length: float, rng: np.random.Generator,
               vibrato: float = 0.25, breath: float = 0.12, glide_from: float | None = None) -> np.ndarray:
    """Clay flute or ocarina: near-sine tone, gentle vibrato, breath noise."""
    t = t_axis(length)
    start = midi_hz(glide_from) if glide_from is not None else midi_hz(note)
    f = midi_hz(note) + (start - midi_hz(note)) * np.exp(-t / 0.04)
    vib_depth = vibrato * (1 - np.exp(-t / 0.25))
    f = f * 2 ** (vib_depth * np.sin(2 * np.pi * 5.2 * t + rng.uniform(0, 6)) / 12 * 0.25)
    phase = 2 * np.pi * np.cumsum(f) / RATE
    tone = np.sin(phase) + 0.12 * np.sin(2 * phase) + 0.05 * np.sin(3 * phase)
    air = bandpass(noise(len(t), rng), midi_hz(note) * 2, 2.0) * breath
    env = np.minimum(t / 0.03, 1.0) * np.minimum((length - t) / 0.08, 1.0)
    return fade((tone + air) * np.clip(env, 0, 1), 0.001, 0.01)


def conch(note: float, length: float, rng: np.random.Generator) -> np.ndarray:
    """Conch-shell horn: buzzy, slightly rising, slow swell."""
    t = t_axis(length)
    f = midi_hz(note) * (1 - 0.03 * np.exp(-t / 0.15))
    phase = 2 * np.pi * np.cumsum(f) / RATE
    tone = sum(np.sin(k * phase) / k ** 1.3 for k in range(1, 9))
    tone = lowpass(tone, midi_hz(note) * 6, 1)
    env = np.minimum(t / 0.18, 1.0) * np.minimum((length - t) / 0.3, 1.0)
    air = lowpass(noise(len(t), rng), 2500) * 0.05
    return fade((tone + air) * np.clip(env, 0, 1), 0.002, 0.02)


def shimmer(length: float, rng: np.random.Generator, low: float = 72, high: float = 100,
            count: int = 18, rising: bool = True) -> np.ndarray:
    """Scattered tiny gold chimes, for rewards and magic."""
    parts = []
    for i in range(count):
        frac = i / max(count - 1, 1)
        note = low + (high - low) * (frac if rising else 1 - frac) + rng.uniform(-1, 1)
        parts.append((jade_chime(note, 0.35, rng, bright=1.2) * rng.uniform(0.25, 0.6),
                      frac * length * 0.8 + rng.uniform(0, 0.02)))
    return mix(*parts)


def sweep(length: float, rng: np.random.Generator, f_start: float, f_end: float,
          q: float = 3.0) -> np.ndarray:
    """Band of noise swept between two centre frequencies (whoosh, slide)."""
    n = int(length * RATE)
    src = noise(n, rng, "pink")
    hops = 64
    out = np.zeros(n)
    win = np.hanning(2 * n // hops + 2)
    step = len(win) // 2
    for k in range(0, n, step):
        seg = src[k:k + len(win)]
        if len(seg) < 32:
            break
        frac = k / n
        c = f_start * (f_end / f_start) ** frac
        out[k:k + len(seg)] += bandpass(seg * win[:len(seg)], c, q)
    return out


# ---------------------------------------------------------------- physical models
# Contact sounds (stone, metal, debris) are modelled the way they happen:
# a short contact force excites the resonant modes of the struck object,
# each mode a decaying sine whose frequency, level and decay come from the
# material. Friction is a train of tiny stick-slip impacts. These give the
# dense, slightly irregular spectra of real impacts that a few fixed
# sines cannot.

# Material -> (mode frequency spread exponent, decay of the lowest mode in
# seconds, how fast decay shortens with frequency, roughness of the
# contact). Stone damps fast and evenly; metal rings long and dense.
MATERIALS = {
    "stone": (1.35, 0.030, 0.9, 0.35),
    "jade":  (1.30, 0.080, 0.7, 0.20),
    "metal": (1.12, 0.900, 0.45, 0.10),
    "wood":  (1.50, 0.045, 1.0, 0.45),
}


def object_modes(f0: float, material: str, rng: np.random.Generator,
                 count: int = 24) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Mode frequencies, amplitudes and decay times for an object whose
    lowest mode is f0. Ratios follow a stretched series with random
    detuning, like a solid block or plate rather than a string."""
    spread, decay0, decay_slope, _ = MATERIALS[material]
    k = np.arange(1, count + 1)
    ratios = k ** spread * (1 + rng.uniform(-0.06, 0.06, count))
    ratios[0] = 1.0
    freqs = f0 * ratios
    keep = freqs < RATE * 0.45
    freqs = freqs[keep]
    amps = rng.uniform(0.4, 1.0, len(freqs)) / np.sqrt(np.arange(1, len(freqs) + 1))
    decays = decay0 * (freqs / f0) ** -decay_slope * rng.uniform(0.7, 1.3, len(freqs))
    return freqs, amps, decays


def contact_force(ms: float, rng: np.random.Generator, roughness: float = 0.2) -> np.ndarray:
    """Hertz-like contact pulse: a half sine whose length sets the
    brightness (harder, smaller contacts are shorter and brighter), with a
    little surface noise."""
    n = max(int(ms * 1e-3 * RATE), 2)
    pulse = np.sin(np.pi * np.arange(n) / n) ** 1.5
    return pulse * (1 + roughness * rng.standard_normal(n))


def resonate(excitation: np.ndarray, freqs: np.ndarray, amps: np.ndarray,
             decays: np.ndarray, rng: np.random.Generator) -> np.ndarray:
    """Drive a bank of damped sinusoid modes with an excitation signal."""
    length = float(min(np.max(decays) * 6.9, 4.0))
    t = t_axis(length)
    ir = np.zeros_like(t)
    for f, a, d in zip(freqs, amps, decays):
        ir += a * np.sin(2 * np.pi * f * t + rng.uniform(0, 0.3)) * np.exp(-t / d)
    m = len(excitation) + len(ir)
    out = np.fft.irfft(np.fft.rfft(excitation, m) * np.fft.rfft(ir, m), m)
    return out / (np.max(np.abs(out)) + 1e-12)


def impact(f0: float, material: str, rng: np.random.Generator, contact_ms: float = 0.4,
           count: int = 24, crack: float = 0.0, damping: float = 1.0) -> np.ndarray:
    """One strike on an object: contact pulse through its modes, plus an
    optional broadband crack for brittle fracture. damping scales every
    decay time (below 1 for small or held objects)."""
    roughness = MATERIALS[material][3]
    force = contact_force(contact_ms, rng, roughness)
    freqs, amps, decays = object_modes(f0, material, rng, count)
    out = resonate(force, freqs, amps, decays * damping, rng)
    # Rough, brittle materials also crunch: a short noise burst from the
    # contact area, coloured around the object's upper modes.
    n = int((contact_ms * 1e-3 * 12 + 0.004) * RATE)
    grit = bandpass(noise(n, rng), min(f0 * 3, RATE * 0.4), 0.6) * np.exp(-np.arange(n) / (n / 4))
    out = mix((out, 0.0), (grit * roughness * 0.8, 0.0))
    if crack > 0:
        n = int(0.025 * RATE)
        c = highpass(noise(n, rng), 1500) * np.exp(-np.arange(n) / (0.004 * RATE))
        out = mix((out, 0.0), (c * crack, 0.0))
    return out


def bounces(drop_seconds: float, count: int, restitution: float, rng: np.random.Generator) -> list[tuple[float, float]]:
    """(time, strength) of an object bouncing to rest: each gap and each
    hit shrinks by the coefficient of restitution."""
    out, t, gap, strength = [], 0.0, drop_seconds, 1.0
    for _ in range(count):
        out.append((t, strength))
        gap *= restitution * rng.uniform(0.85, 1.1)
        strength *= restitution * rng.uniform(0.7, 1.0)
        t += gap
    return out


def debris(length: float, rng: np.random.Generator, pieces: int = 30, material: str = "stone",
           f_low: float = 1500.0, f_high: float = 6000.0, start_spread: float = 0.5,
           damping: float = 1.0) -> np.ndarray:
    """Fragments raining down and bouncing to rest on a hard floor. Small
    pieces ring higher and bounce faster."""
    parts = []
    for _ in range(pieces):
        size = rng.uniform(0, 1)                       # 0 small .. 1 large
        f0 = f_high * (f_low / f_high) ** size
        start = rng.gamma(2.0, start_spread / 4)
        gain = 0.25 + 0.75 * size * rng.uniform(0.5, 1.0)
        for bt, bs in bounces(rng.uniform(0.05, 0.16) * (0.6 + size), int(rng.integers(2, 5)), 0.55, rng):
            if start + bt >= length:
                break
            hit = impact(f0 * rng.uniform(0.97, 1.03), material, rng,
                         contact_ms=0.15 + 0.35 * size, count=10, damping=damping)
            parts.append((hit * gain * bs, start + bt))
    out = mix(*parts) if parts else np.zeros(int(length * RATE))
    return pad(out, int(length * RATE))


def friction(length: float, rng: np.random.Generator, f0: float = 2400.0, material: str = "stone",
             rate: float = 900.0, velocity: np.ndarray | None = None) -> np.ndarray:
    """Surface sliding on surface: stick-slip micro-impacts at a rate that
    follows the sliding velocity, each exciting the surface modes, over a
    bed of fine scraping noise."""
    n = int(length * RATE)
    v = np.ones(n) if velocity is None else np.interp(np.linspace(0, 1, n), np.linspace(0, 1, len(velocity)), velocity)
    p = np.clip(v * rate / RATE, 0, 1)
    hits = (rng.uniform(0, 1, n) < p) * np.minimum(rng.pareto(2.5, n), 3.0) * v
    # The contact point moves, so the surface modes it excites change:
    # render in short overlapping segments, each with its own modes.
    grains = np.zeros(n)
    seg = int(0.03 * RATE)
    win = np.hanning(2 * seg)
    for i0 in range(0, n, seg):
        part = hits[i0:i0 + 2 * seg] * win[:len(hits[i0:i0 + 2 * seg])]
        freqs, amps, decays = object_modes(f0 * rng.uniform(0.85, 1.15), material, rng, 24)
        g = resonate(part, freqs, amps, decays * 0.25, rng) * np.max(np.abs(part))
        end = min(n, i0 + len(g))
        grains[i0:end] += g[:end - i0]
    grains /= np.max(np.abs(grains)) + 1e-12
    scrape = bandpass(noise(n, rng, "pink"), f0 * 1.5, 0.6) * v
    return grains + 0.7 * scrape / (np.max(np.abs(scrape)) + 1e-12)


def fit(sig: np.ndarray, seconds: float, fade_out: float = 0.04) -> np.ndarray:
    """Cut a sound to its length budget with a smooth fade, so long modal
    and reverb tails never run past what the cue may take."""
    n = int(seconds * RATE)
    if len(sig) <= n:
        return sig
    return fade(sig[:n], 0.0, fade_out)


def soft_limit(sig: np.ndarray, headroom_db: float = 6.0) -> np.ndarray:
    """Round off the few sharpest transient peaks (as a recording chain
    would) so a short sound can reach its loudness under the true-peak
    ceiling. Peaks are first scaled to headroom_db above the knee."""
    x = sig / (np.max(np.abs(sig)) + 1e-12) * 10 ** (headroom_db / 20)
    return np.tanh(x)


def stone_room(sig: np.ndarray, rng: np.random.Generator, seconds: float = 1.2, wet: float = 0.2,
               size: float = 1.0) -> np.ndarray:
    """Reverb of a stone hall: discrete early reflections off near walls,
    then a dense tail whose highs die faster than its lows, as air and
    porous stone absorb them."""
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    ir = np.zeros(n)
    for _ in range(10):                                  # early reflections
        i = int(rng.uniform(0.004, 0.045) * size * RATE)
        if i < n:
            ir[i] += rng.uniform(0.3, 0.8) * rng.choice((-1, 1))
    tail = np.zeros(n)
    for lo, hi, rt in ((20, 500, 1.0), (500, 2000, 0.8), (2000, 6000, 0.55), (6000, 20000, 0.3)):
        band = fft_filter(noise(n, rng), lambda f, lo=lo, hi=hi: ((f >= lo) & (f < hi)).astype(float))
        tail += band * np.exp(-t / (seconds * rt / 6.9))
    onset = np.clip((t - 0.012 * size) / 0.03, 0, 1)
    ir += tail * onset * 0.25
    ir /= np.sqrt(np.sum(ir ** 2)) + 1e-12
    m = len(sig) + n
    wet_sig = np.fft.irfft(np.fft.rfft(sig, m) * np.fft.rfft(ir, m), m)
    return pad(sig, m) * (1 - wet) + wet_sig * wet


# ---------------------------------------------------------------- loops

class LoopTrack:
    """A loop buffer: anything placed past the end wraps to the start, so
    tails ring across the seam and the loop is seamless by construction."""

    def __init__(self, seconds: float, channels: int = 2):
        self.n = int(round(seconds * RATE))
        self.buf = np.zeros((self.n, channels))

    def add(self, sig: np.ndarray, start: float, gain: float = 1.0, pan: float = 0.0) -> None:
        i0 = int(round(start * RATE)) % self.n
        left = np.cos((pan + 1) * np.pi / 4) * gain
        right = np.sin((pan + 1) * np.pi / 4) * gain
        idx = (i0 + np.arange(len(sig))) % self.n
        np.add.at(self.buf[:, 0], idx, sig * left)
        np.add.at(self.buf[:, 1], idx, sig * right)

    def add_stereo(self, sig: np.ndarray, start: float = 0.0, gain: float = 1.0) -> None:
        idx = (int(round(start * RATE)) + np.arange(len(sig))) % self.n
        np.add.at(self.buf[:, 0], idx, sig[:, 0] * gain)
        np.add.at(self.buf[:, 1], idx, sig[:, 1] * gain)


def circular_reverb(buf: np.ndarray, rng: np.random.Generator, seconds: float = 2.0,
                    wet: float = 0.2) -> np.ndarray:
    """Reverb whose tail wraps around the loop (circular convolution)."""
    n = len(buf)
    m = int(seconds * RATE)
    out = buf * (1 - wet)
    for ch in range(buf.shape[1]):
        ir = noise(m, rng) * np.exp(-np.arange(m) / (RATE * seconds / 6.9))
        ir = lowpass(ir, 4500, 1)
        ir /= np.sqrt(np.sum(ir ** 2)) + 1e-12
        full = np.zeros(n)
        full[:min(m, n)] = ir[:n]
        out[:, ch] += np.fft.irfft(np.fft.rfft(buf[:, ch]) * np.fft.rfft(full), n) * wet
    return out


# ---------------------------------------------------------------- level and files

def true_peak_db(sig: np.ndarray) -> float:
    """4x oversampled peak, an estimate of BS.1770 true peak."""
    x = sig if sig.ndim == 2 else sig[:, None]
    peak = 0.0
    for ch in range(x.shape[1]):
        n = len(x)
        spec = np.fft.rfft(x[:, ch])
        up = np.fft.irfft(spec, 4 * n) * 4
        peak = max(peak, float(np.max(np.abs(up))))
    return 20 * np.log10(max(peak, 1e-12))


def write_wav(path: Path, sig: np.ndarray) -> None:
    """16-bit PCM WAV, mono or stereo."""
    path.parent.mkdir(parents=True, exist_ok=True)
    x = sig if sig.ndim == 2 else sig[:, None]
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(x.shape[1])
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())


def write_ogg(path: Path, sig: np.ndarray, quality: float = 4.0) -> None:
    """Ogg Vorbis through ffmpeg, from a temporary WAV beside the output."""
    tmp = path.with_suffix(".tmp.wav")
    write_wav(tmp, sig)
    try:
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(tmp),
                        "-c:a", "libvorbis", "-q:a", str(quality),
                        "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact",
                        str(path)], check=True)
    finally:
        tmp.unlink(missing_ok=True)


def measure(path: Path) -> dict:
    """BS.1770 loudness of a file with ffmpeg's ebur128 meter: integrated
    loudness, loudest momentary (400 ms) value and true peak."""
    proc = subprocess.run(["ffmpeg", "-nostats", "-v", "verbose", "-i", str(path), "-af",
                           "ebur128=peak=true:framelog=verbose", "-f", "null", "-"],
                          capture_output=True, text=True, check=True)
    log = proc.stderr
    moments = [float(m) for m in re.findall(r"M:\s*(-?[\d.]+)", log)]
    summary = log[log.rfind("Summary:"):]
    integrated = float(re.search(r"I:\s*(-?[\d.]+) LUFS", summary).group(1))
    peak = float(re.search(r"Peak:\s*(-?[\d.inf]+) dBFS", summary).group(1))
    return {"integrated_lufs": integrated,
            "max_momentary_lufs": max(moments) if moments else float("nan"),
            "true_peak_dbtp": peak}


def loudness_of(sig: np.ndarray, key: str) -> float:
    """Measure an in-memory signal: key is a field of measure(). Short
    sounds are padded with silence so the 400 ms meter window fits."""
    import tempfile
    x = sig / (np.max(np.abs(sig)) + 1e-12) * 0.5
    x = pad(x, max(len(x), int(0.6 * RATE)))
    with tempfile.TemporaryDirectory() as d:
        f = Path(d) / "m.wav"
        write_wav(f, x)
        value = measure(f)[key]
    # Undo the 0.5 peak scaling so the number belongs to sig itself.
    return value + 20 * np.log10((np.max(np.abs(sig)) + 1e-12) / 0.5)


def level_to(sig: np.ndarray, key: str, target: float, ceiling_dbtp: float = -1.0) -> np.ndarray:
    """Scale sig so its loudness (key of measure()) hits target LUFS, then
    pull it down further if the true peak would pass the ceiling."""
    current = loudness_of(sig, key)
    out = sig * 10 ** ((target - current) / 20)
    tp = true_peak_db(out)
    if tp > ceiling_dbtp:
        out *= 10 ** ((ceiling_dbtp - tp) / 20)
    return out


def write_report(path: Path, rows: dict) -> None:
    path.write_text(json.dumps(rows, indent=2, sort_keys=True) + "\n")
