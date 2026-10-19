# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Check the identity's colour rules against the committed token files.

    python3 design/identity/check_contrast.py              # check, exit 1 on failure
    python3 design/identity/check_contrast.py --markdown   # also print the tables

Reads design/tokens/montezuma.tokens.json (aliases resolved),
design/tokens/tokens.css (light and dark blocks) and the piece colours in
design/tokens/temple-board.json. Checks:

1. WCAG 2.2 contrast for every intended pair: 4.5:1 for text, 3:1 for
   large text, control boundaries, focus rings and meaningful icons.
   Ratios are not rounded: 4.49 fails 4.5.
2. The UI ochre (accent on dark, the south shrine) stays clearly apart
   from the yellow and orange pieces: CIEDE2000 difference of 15 or more.
3. Colour-vision simulation (the physiologically based model in IEEE TVCG 15(6), 2009,
   severity 1.0, in linear RGB) for protanopia, deuteranopia and
   tritanopia: danger must stay apart from success and from warning
   (CIEDE2000 of 10 or more, or a lightness contrast of 1.5:1 or more).
   Other status pairs and the blue and ochre shrine pair are reported
   only: every status carries an icon or a word, and position carries
   the shrines.
Standard library only.
"""
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.dirname(HERE)
TOKENS = os.path.join(DESIGN, "tokens", "montezuma.tokens.json")
CSS = os.path.join(DESIGN, "tokens", "tokens.css")
PIECES = os.path.join(DESIGN, "tokens", "temple-board.json")

TEXT, LARGE, UI = 4.5, 3.0, 3.0


# ------------------------------------------------------------ colour maths
def rgb(h):
    h = h.lstrip("#")
    return [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]


def to_hex(c):
    return "#" + "".join(f"{max(0, min(255, round(v * 255))):02x}" for v in c)


def lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def delin(c):
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def luminance(h):
    r, g, b = (lin(c) for c in rgb(h))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def ratio(a, b):
    la, lb = sorted((luminance(a), luminance(b)))
    return (lb + 0.05) / (la + 0.05)


def over(fg, alpha, bg):
    """Composite a translucent colour over an opaque one (sRGB, as browsers do)."""
    return to_hex([f * alpha + b * (1 - alpha) for f, b in zip(rgb(fg), rgb(bg))])


def lighten(h, amount):
    """Godot's Color.lightened: lerp towards white in sRGB."""
    return to_hex([c + (1 - c) * amount for c in rgb(h)])


def lab(h):
    r, g, b = (lin(c) for c in rgb(h))
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 216 / 24389 else (24389 / 27 * t + 16) / 116
    return 116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z))


def de2000(c1, c2):
    L1, a1, b1 = lab(c1)
    L2, a2, b2 = lab(c2)
    cb = (math.hypot(a1, b1) + math.hypot(a2, b2)) / 2
    g = 0.5 * (1 - math.sqrt(cb ** 7 / (cb ** 7 + 25 ** 7)))
    a1p, a2p = (1 + g) * a1, (1 + g) * a2
    c1p, c2p = math.hypot(a1p, b1), math.hypot(a2p, b2)
    h1p = math.degrees(math.atan2(b1, a1p)) % 360
    h2p = math.degrees(math.atan2(b2, a2p)) % 360
    dl, dc = L2 - L1, c2p - c1p
    dh = 0.0
    if c1p * c2p:
        dh = h2p - h1p
        dh -= 360 if dh > 180 else 0
        dh += 360 if dh < -180 else 0
    dH = 2 * math.sqrt(c1p * c2p) * math.sin(math.radians(dh / 2))
    lbar, cbar = (L1 + L2) / 2, (c1p + c2p) / 2
    hbar = h1p + h2p
    if c1p * c2p:
        hbar = (hbar + (360 if abs(h1p - h2p) > 180 else 0)) / 2
    t = (1 - 0.17 * math.cos(math.radians(hbar - 30)) + 0.24 * math.cos(math.radians(2 * hbar))
         + 0.32 * math.cos(math.radians(3 * hbar + 6)) - 0.20 * math.cos(math.radians(4 * hbar - 63)))
    sl = 1 + 0.015 * (lbar - 50) ** 2 / math.sqrt(20 + (lbar - 50) ** 2)
    sc, sh = 1 + 0.045 * cbar, 1 + 0.015 * cbar * t
    rt = (-2 * math.sqrt(cbar ** 7 / (cbar ** 7 + 25 ** 7))
          * math.sin(math.radians(60 * math.exp(-((hbar - 275) / 25) ** 2))))
    return math.sqrt((dl / sl) ** 2 + (dc / sc) ** 2 + (dH / sh) ** 2 + rt * (dc / sc) * (dH / sh))


CVD = {  # physiologically based model, IEEE TVCG 15(6) 2009; severity 1.0, linear RGB
    "protan": [[0.152286, 1.052583, -0.204868], [0.114503, 0.786281, 0.099216], [-0.003882, -0.048116, 1.051998]],
    "deutan": [[0.367322, 0.860646, -0.227968], [0.280085, 0.672501, 0.047413], [-0.011820, 0.042940, 0.968881]],
    "tritan": [[1.255528, -0.076749, -0.178779], [-0.078411, 0.930809, 0.147602], [0.004733, 0.691367, 0.303900]],
}


def simulate(h, kind):
    v = [lin(c) for c in rgb(h)]
    m = CVD[kind]
    return to_hex([delin(max(0.0, min(1.0, sum(m[i][j] * v[j] for j in range(3))))) for i in range(3)])


# ------------------------------------------------------------ token reading
def load_tokens():
    with open(TOKENS, encoding="utf-8") as f:
        return json.load(f)


def resolve(tokens, path, depth=0):
    if depth > 8:
        raise ValueError("alias loop at " + path)
    node = tokens
    for part in path.split("."):
        node = node[part]
    v = node["$value"] if isinstance(node, dict) and "$value" in node else node
    if isinstance(v, str) and v.startswith("{") and v.endswith("}"):
        return resolve(tokens, v[1:-1], depth + 1)
    return v


def hex_of(tokens, path):
    v = resolve(tokens, path)
    if isinstance(v, dict):
        return v["hex"], v.get("alpha", 1.0)
    return v[:7], (int(v[7:9], 16) / 255 if len(v) == 9 else 1.0)


def load_css():
    with open(CSS, encoding="utf-8") as f:
        text = f.read()
    light_block = text[text.index(":root {"):text.index("@media (prefers-color-scheme: dark)")]
    dark_block = text[text.index("@media (prefers-color-scheme: dark)"):]
    var = re.compile(r"(--[\w-]+):\s*(#[0-9a-fA-F]{6})\s*;")
    light = dict(var.findall(light_block))
    dark = dict(light)
    dark.update(var.findall(dark_block))
    return light, dark


# ------------------------------------------------------------ the checks
def main():
    md = "--markdown" in sys.argv
    tk = load_tokens()
    rows, failures = [], []

    def check(group, fg_name, fg, bg_name, bg, need, use):
        r = ratio(fg, bg)
        ok = r >= need
        rows.append((group, fg_name, fg, bg_name, bg, r, need, use, ok))
        if not ok:
            failures.append(f"{group}: {fg_name} {fg} on {bg_name} {bg} = {r:.2f} < {need}")

    def c(path):
        return hex_of(tk, path)[0]

    # Game UI (top-level keys the game reads; dark).
    g = "game"
    sf, rs = c("color.surface"), c("color.surface-raised")
    hover = lighten(rs, 0.12)
    scrim_hex, scrim_a = hex_of(tk, "color.scrim")
    for bgn, bg in (("surface", sf), ("surface-raised", rs)):
        check(g, "text", c("color.text"), bgn, bg, TEXT, "body text, labels")
        check(g, "text-muted", c("color.text-muted"), bgn, bg, TEXT, "captions")
        check(g, "danger", c("color.danger"), bgn, bg, TEXT, "DangerLabel")
        check(g, "focus", c("color.focus"), bgn, bg, UI, "focus ring")
    # Godot draws a hovered button as surface-raised lightened by 12 percent.
    check(g, "text", c("color.text"), "surface-raised hover", hover, TEXT, "button label on hover")
    check(g, "accent", c("color.accent"), "surface", sf, TEXT, "titles, headings")
    check(g, "accent", c("color.accent"), "surface-raised", rs, UI, "meters, fills")
    check(g, "text-on-accent", c("color.text-on-accent"), "accent", c("color.accent"), TEXT, "primary button")
    check(g, "text-on-accent", c("color.text-on-accent"), "accent-pressed", c("color.accent-pressed"), TEXT, "pressed button")
    for under in ("#ffffff", "#ffcc4d"):
        bg = over(scrim_hex, scrim_a, under)
        check(g, "text", c("color.text"), f"scrim over {under}", bg, TEXT, "modal text over the brightest board")

    # Light and dark roles.
    for theme in ("light", "dark"):
        def r_(name):
            return c(f"color.role.{theme}.{name}")
        grp = "role." + theme
        for s in ("surface", "surface-raised"):
            for t in ("text", "text-muted", "accent", "link", "danger", "success", "warning", "info"):
                check(grp, t, r_(t), s, r_(s), TEXT, "text")
        # Sunken wells hold plain text only (design/README.md).
        for t in ("text", "text-muted"):
            check(grp, t, r_(t), "surface-sunken", r_("surface-sunken"), TEXT, "text in wells")
        for s in ("surface", "surface-raised", "surface-sunken"):
            check(grp, "outline", r_("outline"), s, r_(s), UI, "control boundary")
            check(grp, "focus", r_("focus"), s, r_(s), UI, "focus ring")
        check(grp, "text-on-accent", r_("text-on-accent"), "accent", r_("accent"), TEXT, "primary button")
        check(grp, "text-on-accent", r_("text-on-accent"), "accent-pressed", r_("accent-pressed"), TEXT, "pressed button")
        # Inside the stage, text, button borders and focus rings use stage-text.
        check(grp, "stage-text", r_("stage-text"), "stage", r_("stage"), TEXT, "text, borders and focus on the stage")

    # The site's CSS (every pair site.css uses).
    light, dark = load_css()
    for theme, v in (("css.light", light), ("css.dark", dark)):
        for bgn in ("--color-bg", "--color-surface"):
            for fgn, need, use in (("--color-text", TEXT, "text"), ("--color-muted", TEXT, "help, footer"),
                                   ("--color-accent", TEXT, "links"), ("--color-link", TEXT, "links"),
                                   ("--color-danger", TEXT, "errors"), ("--color-border", UI, "control border"),
                                   ("--color-focus", UI, "focus outline")):
                check(theme, fgn, v[fgn], bgn, v[bgn], need, use)
        check(theme, "--color-accent-text", v["--color-accent-text"], "--color-accent", v["--color-accent"], TEXT, "primary button")
        check(theme, "--color-stage-text", v["--color-stage-text"], "--color-stage", v["--color-stage"], TEXT, "stage text, borders and focus")

    # Brand art: the soot outline must separate every fill from a light ground,
    # and the lime keyline from a dark one.
    soot, lime = c("color.brand.soot"), c("color.brand.lime")
    for name in ("hematite", "ochre", "maya", "lime"):
        fill = c("color.brand." + name)
        if name != "lime":
            check("art", "soot outline", soot, name + " fill", fill, UI, "outline against fill")
    check("art", "soot outline", soot, "lime ground", lime, UI, "mark on light")
    check("art", "lime keyline", lime, "soot ground", soot, UI, "mark on dark")
    check("art", "lime keyline", lime, "app-icon field", c("color.maya.900"), UI, "icon field")

    # Rule 6: UI ochre apart from the yellow and orange pieces.
    with open(PIECES, encoding="utf-8") as f:
        pieces = json.load(f)["kind"]
    sep = []
    for name, path in (("accent (dark)", "color.role.dark.accent"), ("south shrine", "color.role.dark.south"),
                       ("brand ochre", "color.brand.ochre")):
        for k in ("2", "1"):
            p = pieces[k]
            d = de2000(c(path), p["value"])
            sep.append((name, c(path), f'{p["name"]} piece', p["value"], d))
            if d < 15:
                failures.append(f"ochre: {name} {c(path)} vs {p['name']} piece {p['value']}: dE00 {d:.1f} < 15")

    # Colour vision.
    cvd_rows = []
    pair = (("north (maya)", c("color.role.light.north")), ("south (ochre)", c("color.role.light.south")))
    for theme in ("light", "dark"):
        st = [(n, c(f"color.role.{theme}.{n}")) for n in ("danger", "success", "warning", "info")]
        # Enforced: danger against success and warning (the pairs a player must
        # never confuse). Reported: the rest; every status carries an icon or word.
        pairs = [(st[i], st[j], i == 0 and j in (1, 2)) for i in range(4) for j in range(i + 1, 4)]
        if theme == "light":
            pairs.append((pair[0], pair[1], False))
        for (an, a), (bn, b), enforce in pairs:
            for kind in ("none", "protan", "deutan", "tritan"):
                sa = a if kind == "none" else simulate(a, kind)
                sb = b if kind == "none" else simulate(b, kind)
                d, lr = de2000(sa, sb), ratio(sa, sb)
                ok = d >= 10 or lr >= 1.5
                cvd_rows.append((theme, an, bn, kind, sa, sb, d, lr, "pass" if ok else ("FAIL" if enforce else "position")))
                if enforce and not ok:
                    failures.append(f"cvd {theme}: {an} vs {bn} under {kind}: dE00 {d:.1f}, {lr:.2f}:1")

    passed = sum(1 for r in rows if r[-1])
    print(f"contrast: {passed}/{len(rows)} pairs pass; ochre separation: {len(sep)} checks; "
          f"colour vision: {len(cvd_rows)} comparisons")
    if md:
        print("\n| Group | Foreground | Background | Ratio | Needs | Use |\n|---|---|---|---|---|---|")
        seen = set()
        for grp, fn, fg, bn, bg, r, need, use, ok in rows:
            key = (grp, fn, fg, bn, bg)
            if key in seen:
                continue
            seen.add(key)
            print(f"| {grp} | {fn} `{fg}` | {bn} `{bg}` | {r:.2f}:1 | {need:g}:1 | {use}{'' if ok else ' FAIL'} |")
        print("\n| Colour | vs piece | CIEDE2000 |\n|---|---|---|")
        for n, h, pn, ph, d in sep:
            print(f"| {n} `{h}` | {pn} `{ph}` | {d:.1f} |")
        print("\n| Theme | Pair | Vision | Simulated | CIEDE2000 | Lightness ratio | Result |\n|---|---|---|---|---|---|---|")
        for theme, an, bn, kind, sa, sb, d, lr, res in cvd_rows:
            print(f"| {theme} | {an} / {bn} | {kind} | `{sa}` / `{sb}` | {d:.1f} | {lr:.2f}:1 | {res} |")
    if failures:
        print("\nFAILURES:")
        for f_ in failures:
            print("  " + f_)
        return 1
    print("all checks pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
