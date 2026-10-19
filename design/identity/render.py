# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Render the identity's PNG previews with headless Chromium.

    python3 design/identity/build.py      # first: writes the SVGs and tokens
    python3 design/identity/render.py     # then: writes design/previews/*.png

Needs `chromium` on PATH (any recent version; checked with 155). The
applications sheet and type specimen use Archivo and Atkinson Hyperlegible
Next. They are not committed: this script downloads them from google/fonts
at the pinned commit below into a cache directory (MONTEZUMA_FONT_CACHE,
default ~/.cache/montezuma/fonts), checks each sha256, and never copies
them into the repository. Without network, pass --no-fonts to render with
system fallbacks (the logo previews do not use fonts at all).

Every page is built here as HTML and screenshotted with:
    chromium --headless --disable-gpu --no-sandbox --hide-scrollbars
             --window-size=W,H --screenshot=out.png page.html
"""
import hashlib
import os
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import build  # noqa: E402  (same folder)

OUT = os.path.join(DESIGN, "previews")

# google/fonts at this commit (main on 2026-10-19).
FONTS_COMMIT = "47d54ca1ebb14fd864695f1d39dc734f39017c33"
FONTS = {
    "Archivo": ("ofl/archivo/Archivo[wdth,wght].ttf",
                "0e094a7d3c7c4c25cf1310c4b30014f1dae9332220b1c2c88f4fa996f0b05053"),
    "Atkinson Hyperlegible Next": ("ofl/atkinsonhyperlegiblenext/AtkinsonHyperlegibleNext[wght].ttf",
                                   "5a455d1cfa099b601ab70751bb9673e8fe1854dc4500c80e1a220d0d75e31745"),
}


def font_cache():
    return os.environ.get("MONTEZUMA_FONT_CACHE",
                          os.path.join(os.path.expanduser("~"), ".cache", "montezuma", "fonts"))


def fetch_fonts():
    """Download and verify the two fonts; return {family: file path}."""
    cache = font_cache()
    os.makedirs(cache, exist_ok=True)
    out = {}
    for fam, (path, sha) in FONTS.items():
        local = os.path.join(cache, os.path.basename(path))
        if not os.path.exists(local) or _sha(local) != sha:
            url = (f"https://raw.githubusercontent.com/google/fonts/{FONTS_COMMIT}/"
                   + urllib.parse.quote(path))
            with urllib.request.urlopen(url, timeout=60) as r, open(local, "wb") as f:
                f.write(r.read())
        if _sha(local) != sha:
            raise SystemExit(f"sha256 mismatch for {local}")
        out[fam] = local
    return out


def _sha(p):
    with open(p, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def font_css(fonts):
    if not fonts:
        return ""
    return "".join(
        f'@font-face{{font-family:"{fam}";src:url("file://{p}") format("truetype");'
        f'font-weight:100 900;font-stretch:62% 125%;}}' for fam, p in fonts.items())


def svg_inline(rel):
    """An SVG file's markup without its comment header."""
    with open(os.path.join(DESIGN, rel), encoding="utf-8") as f:
        text = f.read()
    return text[text.index("<svg"):]


def shoot(html, name, w, h):
    os.makedirs(OUT, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", suffix=".html", delete=False, encoding="utf-8") as t:
        t.write(html)
    out = os.path.join(OUT, name)
    subprocess.run(["chromium", "--headless", "--disable-gpu", "--no-sandbox", "--hide-scrollbars",
                    f"--window-size={w},{h}", "--default-background-color=00000000",
                    "--force-device-scale-factor=1", f"--screenshot={out}", "file://" + t.name],
                   check=True, capture_output=True)
    os.unlink(t.name)
    print("wrote design/previews/" + name)


def page(body, css="", bg="transparent"):
    return ("<!doctype html><html><head><meta charset='utf-8'><style>"
            f"html,body{{margin:0;padding:0;background:{bg};overflow:hidden}}"
            "svg{display:block}" + css + "</style></head><body>" + body + "</body></html>")


def sized(rel, w, h, extra=""):
    s = svg_inline(rel)
    return s.replace("<svg ", f'<svg style="width:{w}px;height:{h}px;{extra}" ', 1)


def svg_w(rel, w, extra=""):
    """An SVG at width w, height from its viewBox."""
    s = svg_inline(rel)
    vb = s[s.index('viewBox="') + 9:]
    _, _, vw, vh = (float(v) for v in vb[:vb.index('"')].split())
    return sized(rel, w, round(w * vh / vw, 2), extra), w * vh / vw


B = build.BRAND
R = {k: build.colour(v[1]) for k, v in build.ROLES.items()}      # dark roles
RL = {k: build.colour(v[0]) for k, v in build.ROLES.items()}     # light roles


def band_css(height):
    """CSS for a strip of the step-fret band at a given height (px)."""
    w = build.BAND_TILE_W * height / build.BAND_TILE_H
    return (f"height:{height}px;background:url('file://{os.path.join(DESIGN, 'patterns', 'step-fret-band.svg')}') "
            f"0 0/{w:g}px {height}px repeat-x;")


# ---------------------------------------------------------------- icons
def icons():
    # The master holds from 48 px up; 16 and 32 px use the pixel variant at
    # exactly 1x and 2x so every edge lands on a pixel.
    for s in (1024, 180, 48):
        shoot(page(sized("icons/app-icon.svg", s, s)), f"icon-{s}.png", s, s)
    for s in (32, 16):
        shoot(page(sized("icons/app-icon-16.svg", s, s)), f"icon-{s}.png", s, s)


# ---------------------------------------------------------------- lockups
def lockups():
    for theme, bg, sfx in (("light", B["lime"], ""), ("dark", B["soot"], "-reversed")):
        h, _ = svg_w(f"logo/lockup-horizontal{sfx}.svg", 760)
        st, _ = svg_w(f"logo/lockup-stacked{sfx}.svg", 300)
        mk, _ = svg_w("logo/mark.svg" if theme == "light" else "logo/mark-on-dark.svg", 150)
        mono, _ = svg_w("logo/mark-mono.svg" if theme == "light" else "logo/mark-reversed.svg", 150)
        wm, _ = svg_w(f"logo/wordmark{sfx}.svg", 420)
        small = sized("logo/mark-small.svg", 32, 32) + sized("logo/mark-small.svg", 16, 16) if theme == "light" \
            else sized("icons/app-icon-16.svg", 32, 32) + sized("icons/app-icon-16.svg", 16, 16)
        body = (f'<div style="padding:48px;display:grid;gap:40px">{h}'
                f'<div style="display:flex;gap:48px;align-items:flex-end">{st}{mk}{mono}'
                f'<div style="display:flex;gap:16px;align-items:flex-end">{small}</div></div>{wm}</div>')
        shoot(page(body, bg=bg), f"lockups-{theme}.png", 920, 480)


# ---------------------------------------------------------------- checks
FILTERS = """<svg width="0" height="0" style="position:absolute"><defs>
<filter id="protan" color-interpolation-filters="linearRGB"><feColorMatrix type="matrix" values="0.152286 1.052583 -0.204868 0 0 0.114503 0.786281 0.099216 0 0 -0.003882 -0.048116 1.051998 0 0 0 0 0 1 0"/></filter>
<filter id="deutan" color-interpolation-filters="linearRGB"><feColorMatrix type="matrix" values="0.367322 0.860646 -0.227968 0 0 0.280085 0.672501 0.047413 0 0 -0.011820 0.042940 0.968881 0 0 0 0 0 1 0"/></filter>
<filter id="tritan" color-interpolation-filters="linearRGB"><feColorMatrix type="matrix" values="1.255528 -0.076749 -0.178779 0 0 -0.078411 0.930809 0.147602 0 0 0.004733 0.691367 0.303900 0 0 0 0 0 1 0"/></filter>
</defs></svg>"""


def checks():
    """Mark, icon and status colours: normal, greyscale and simulated
    protanopia, deuteranopia and tritanopia (IEEE TVCG 15(6), 2009 model)."""
    cols = [("normal", "none"), ("greyscale", "grayscale(1)"), ("protan", "url(#protan)"),
            ("deutan", "url(#deutan)"), ("tritan", "url(#tritan)")]
    status = "".join(
        f'<div style="display:flex;align-items:center;gap:6px;margin:3px 0;color:{RL[k]};font:700 15px sans-serif">'
        f'<span style="display:inline-block;width:14px;height:14px;background:{RL[k]}"></span>{k}</div>'
        for k in ("danger", "success", "warning", "info"))
    status_d = "".join(
        f'<div style="display:flex;align-items:center;gap:6px;margin:3px 0;color:{R[k]};font:700 15px sans-serif">'
        f'<span style="display:inline-block;width:14px;height:14px;background:{R[k]}"></span>{k}</div>'
        for k in ("danger", "success", "warning", "info"))
    cells = ""
    for label, flt in cols:
        mk, _ = svg_w("logo/mark.svg", 170)
        cells += (f'<div style="filter:{flt};display:grid;gap:14px;justify-items:center">'
                  f'<div style="font:700 14px sans-serif;color:{B["soot"]}">{label}</div>{mk}'
                  f'{sized("icons/app-icon.svg", 96, 96)}'
                  f'<div style="display:flex;gap:10px">{sized("icons/app-icon.svg", 48, 48)}{sized("icons/app-icon-16.svg", 16, 16)}</div>'
                  f'<div style="background:{RL["surface"]};padding:8px 12px">{status}</div>'
                  f'<div style="background:{R["surface"]};padding:8px 12px">{status_d}</div></div>')
    body = FILTERS + f'<div style="display:flex;gap:28px;padding:28px">{cells}</div>'
    shoot(page(body, bg=B["lime"]), "colour-checks.png", 1110, 620)


# ---------------------------------------------------------------- type
SPECIMEN_DISPLAY = ["MONTEZUMA’S · NIVEL 12 · TENOCHTITLAN",
                    "MOTECUHZOMA · HUITZILOPOCHTLI · XICALCOLIUHQUI",
                    "ÉPOCA · CHÂTEAU · AÇÃO · GRÖSSE · ¿LISTO? ¡SÍ!"]
SPECIMEN_BODY = [
    ("en", "Match three or more stones to fill the temple's quota before the sun sets."),
    ("es", "Combina tres o más piedras antes de que se ponga el sol. ¿Listo? ¡Sí!"),
    ("fr", "Associez trois pierres ou plus avant le coucher du soleil. Niveau terminé !"),
    ("pt", "Combine três ou mais pedras antes do pôr do sol. Ação, coração."),
    ("de", "Verbinde drei oder mehr Steine, bevor die Sonne untergeht. Größe, Übung."),
    ("nah", "Motecuhzoma Xocoyotzin, Tenochtitlan, Tlatelolco, Huitzilopochtli, Tlaloc, xicalcoliuhqui."),
]


def type_specimen(fonts):
    disp = ("font-family:'Archivo',sans-serif;font-weight:800;font-stretch:125%;"
            f'letter-spacing:0.02em;text-transform:uppercase;color:{R["accent"]}')
    body_ff = "font-family:'Atkinson Hyperlegible Next',sans-serif"
    rows = "".join(f'<div style="{disp};font-size:{s}px;line-height:1.1;margin:0 0 10px">{t}</div>'
                   for t, s in zip(SPECIMEN_DISPLAY, (40, 30, 30)))
    rows += f'<div style="{disp.replace("800", "700").replace("125%", "112%")};font-size:32px;text-transform:none;color:{R["text"]};margin:18px 0 8px">Heading 32 · Archivo 700, width 112</div>'
    for lang, t in SPECIMEN_BODY:
        rows += (f'<div style="{body_ff};font-size:22px;line-height:32px;color:{R["text"]}">'
                 f'<span style="color:{R["text-muted"]};display:inline-block;width:46px;font-size:16px">{lang}</span>{t}</div>')
    rows += (f'<div style="{body_ff};font-weight:700;font-size:22px;font-variant-numeric:tabular-nums;'
             f'color:{R["accent"]};margin-top:12px">Score 1 234 567 · 0:59 · 1/l/I 0/O · 3 of 12</div>'
             f'<div style="{body_ff};font-size:16px;line-height:22px;color:{R["text-muted"]};margin-top:8px">'
             f'Caption 16 · Atkinson Hyperlegible Next 400 · the smallest UI text on a phone</div>')
    shoot(page(f'<div style="padding:40px">{rows}</div>', font_css(fonts), bg=R["surface"]),
          "type.png", 1280, 540)


# ---------------------------------------------------------------- applications
# The applications use the 3D key art (design/keyart/renders): the identity is
# shown as the realistic lit temple and the bevelled gold wordmark, not as the
# flat vector mark, which stays for favicons, one-colour and small sizes.
KEYART = os.path.join(DESIGN, "keyart", "renders")
# Lift for the gold title over photographic grounds (the render is lit for soot).
GOLD = "filter:brightness(1.45) contrast(1.08) drop-shadow(0 3px 8px rgba(0,0,0,.75));"


def ka(name):
    return "file://" + os.path.join(KEYART, name)


GLASS = ("background:rgba(20,16,13,0.62);backdrop-filter:blur(18px) saturate(1.2);"
         "-webkit-backdrop-filter:blur(18px) saturate(1.2);"
         "box-shadow:0 30px 80px rgba(0,0,0,0.55), inset 0 0 0 1px rgba(232,200,128,0.28);")


def app_home(fonts, dark):
    wall = "#111214" if dark else "#e8e6ef"
    label = "#f2f2f2" if dark else "#1b1b1f"
    tiles = ["#c7c9d1", "#9aa0ad", "#d5d0c4", "#aab4bf", "#bfc6b8", "#c9c0cf", "#b8bcc6", "#d0c9bd",
             "#a9adb7", "#c4bdb0", "#b4c0c9"] if not dark else \
        ["#3a3d44", "#4c515c", "#44403a", "#3d4650", "#434a3e", "#4a4250", "#3f434b", "#4b453c",
         "#383c45", "#47423a", "#3e4a52"]
    cells = []
    for i in range(12):
        if i == 5:
            icon = (f'<img src="{ka("app-icon-3d.png")}" style="width:60px;height:60px;border-radius:13.4px;'
                    f'display:block;box-shadow:0 1px 3px rgba(0,0,0,.35)">')
            name = "Montezuma's"
        else:
            t = tiles[i if i < 5 else i - 1]
            icon = f'<div style="width:60px;height:60px;border-radius:13.4px;background:{t}"></div>'
            name = "App"
        cells.append(f'<div style="display:grid;justify-items:center;gap:5px">{icon}'
                     f'<div style="font:500 11px system-ui,sans-serif;color:{label}">{name}</div></div>')
    return (f'<div style="width:330px;height:300px;background:{wall};border-radius:28px;padding:28px 22px;'
            f'box-sizing:border-box;display:grid;grid-template-columns:repeat(4,1fr);gap:18px 10px">'
            + "".join(cells) + "</div>")


def app_capsule():
    return (f'<div style="width:460px;height:215px;position:relative;overflow:hidden;'
            f'background:url({ka("mark-on-plaza.png")}) 50% 38%/120% auto no-repeat">'
            f'<div style="position:absolute;inset:0;background:linear-gradient(180deg,rgba(0,0,0,0) 30%,rgba(10,8,6,.9) 100%)"></div>'
            f'<img src="{ka("title-wordmark.png")}" style="position:absolute;left:50%;bottom:6px;width:380px;'
            f'transform:translateX(-50%);{GOLD}"></div>')


def app_site(width, fonts):
    mobile = width < 600
    h = 560
    pad = 18 if mobile else 64
    body = "font-family:'Atkinson Hyperlegible Next',sans-serif"
    nav = "" if mobile else (f'<nav style="display:flex;gap:30px;{body};font-weight:700;font-size:16px">'
                             f'<a style="color:#f4ecdc">Play</a><a style="color:#f4ecdc">Download</a>'
                             f'<a style="color:#f4ecdc">Credits</a></nav>')
    header = (f'<header style="position:absolute;left:0;right:0;top:0;display:flex;justify-content:space-between;'
              f'align-items:center;padding:{14 if mobile else 20}px {pad}px;'
              f'background:linear-gradient(180deg,rgba(10,8,6,.88),rgba(10,8,6,0))">'
              f'<img src="{ka("title-wordmark.png")}" style="height:{46 if mobile else 60}px;margin:-6px 0;display:block;{GOLD}">{nav}</header>')
    btn = (f'display:inline-block;{body};font-weight:700;font-size:{16 if mobile else 18}px;padding:13px 24px;border-radius:6px')
    copy = (f'<div style="position:absolute;left:{pad}px;right:{pad}px;bottom:{28 if mobile else 56}px;max-width:620px">'
            f'<h1 style="font-family:\'Archivo\',sans-serif;font-weight:800;font-stretch:112%;color:#f4ecdc;'
            f'font-size:{30 if mobile else 54}px;line-height:1.04;margin:0 0 14px;text-shadow:0 2px 18px rgba(0,0,0,.5)">'
            f'Match the treasures of Tenochtitlan</h1>'
            f'<p style="{body};font-size:{16 if mobile else 19}px;line-height:1.55;color:#e8dcc6;margin:0 0 22px;'
            f'text-shadow:0 1px 10px rgba(0,0,0,.6)">A free, open-source match-three, set in the city of Motecuhzoma.</p>'
            f'<span style="{btn};background:linear-gradient(180deg,#e9a63a,#c9821b);color:#1e1916;'
            f'box-shadow:0 8px 24px rgba(0,0,0,.35), inset 0 1px 0 rgba(255,240,200,.6)">Play the first level</span> '
            f'<span style="{btn};margin-left:10px;color:#f4ecdc;box-shadow:inset 0 0 0 1.5px rgba(244,236,220,.7)">Download</span></div>')
    bgpos = "52% 30%/auto 118%" if mobile else "50% 40%/cover"
    return (f'<div style="width:{width}px;height:{h}px;position:relative;overflow:hidden;'
            f'background:url({ka("mark-on-plaza.png")}) {bgpos} no-repeat">'
            f'<div style="position:absolute;inset:0;background:linear-gradient(180deg,rgba(10,8,6,0) 30%,rgba(10,8,6,.85) 100%)"></div>'
            f'{header}{copy}</div>')


def app_menu(fonts):
    """Game title menu: glass panel over the key art."""
    ff = "font-family:'Atkinson Hyperlegible Next',sans-serif"

    def button(text, primary=False, focus=False):
        if primary:
            look = ("background:linear-gradient(180deg,#e9a63a,#c9821b);color:#1e1916;"
                    "box-shadow:0 6px 18px rgba(0,0,0,.4), inset 0 1px 0 rgba(255,240,200,.6)")
        else:
            look = ("background:rgba(244,236,220,.06);color:#f4ecdc;"
                    "box-shadow:inset 0 0 0 1px rgba(244,236,220,.18)")
        ring = f"outline:3px solid {R['focus']};outline-offset:4px;" if focus else ""
        return (f'<div style="{look};{ring}border-radius:6px;padding:13px 16px;'
                f'{ff};font-weight:700;font-size:21px;text-align:center;letter-spacing:.01em">{text}</div>')
    panel = (f'<div style="position:absolute;right:96px;top:50%;transform:translateY(-50%);width:400px;'
             f'{GLASS}border-radius:14px;padding:30px 34px 30px;display:grid;gap:12px">'
             f'{button("Play", True)}{button("Level map", focus=True)}{button("Settings")}{button("Credits")}'
             f'<div style="{ff};font-size:16px;color:#c9c1b6;text-align:center;margin-top:8px">'
             f'Inspired by The Treasures of Montezuma (Visual Shape, Alawar)</div></div>')
    title = (f'<img src="{ka("title-wordmark.png")}" style="position:absolute;left:60px;bottom:52px;width:660px;{GOLD}">')
    return (f'<div style="width:1280px;height:720px;position:relative;overflow:hidden;'
            f'background:url({ka("mark-on-plaza.png")}) 30% 40%/cover no-repeat">'
            f'<div style="position:absolute;inset:0;background:linear-gradient(90deg,rgba(10,8,6,.05) 0%,'
            f'rgba(10,8,6,.25) 55%,rgba(10,8,6,.6) 100%),linear-gradient(180deg,rgba(0,0,0,0) 50%,rgba(10,8,6,.9) 100%)"></div>'
            f'{title}{panel}</div>')


def applications(fonts):
    css = font_css(fonts)
    parts = {
        "app-home-screens.png": (f'<div style="display:flex;gap:24px;padding:20px">{app_home(fonts, False)}{app_home(fonts, True)}</div>', 724, 340),
        "app-store-capsule.png": (app_capsule(), 460, 215),
        "app-site-mobile.png": (app_site(360, fonts), 360, 560),
        "app-site-desktop.png": (app_site(1280, fonts), 1280, 560),
        "app-game-menu.png": (app_menu(fonts), 1280, 720),
    }
    for name, (html, w, h) in parts.items():
        shoot(page(html, css, bg=B["lime"] if "home" in name else "transparent"), name, w, h)
    img = lambda n, w: f'<img src="file://{os.path.join(OUT, n)}" style="width:{w}px;display:block">'  # noqa: E731
    lab = lambda t: f'<div style="font:700 15px sans-serif;color:{B["soot"]};margin:0 0 8px">{t}</div>'  # noqa: E731
    sheet = (f'<div style="padding:32px;display:grid;gap:28px;grid-template-columns:auto auto;align-items:start">'
             f'<div>{lab("App icon on light and dark home screens")}{img("app-home-screens.png", 724)}</div>'
             f'<div>{lab("Store capsule, 460 x 215")}{img("app-store-capsule.png", 460)}</div>'
             f'<div>{lab("Website at 1280 px")}{img("app-site-desktop.png", 900)}</div>'
             f'<div>{lab("Website at 360 px")}{img("app-site-mobile.png", 360)}</div>'
             f'<div style="grid-column:1 / span 2">{lab("Game title menu: glass panel over the key art")}'
             f'{img("app-game-menu.png", 1100)}</div></div>')
    shoot(page(sheet, bg=B["lime"]), "applications.png", 1460, 1720)


def band_preview():
    shoot(page(f'<div style="{band_css(72)}width:960px"></div><div style="{band_css(36)}width:960px;margin-top:16px"></div>'
               f'<div style="{band_css(18)}width:960px;margin-top:16px"></div>', bg=B["lime"]),
          "band.png", 960, 160)


# ---------------------------------------------------------------- rules
def construction():
    """Clear space: x is one shrine's width with its outline (21 of the
    mark's 87 units). Shown round the horizontal lockup and the mark."""
    def boxed(rel, w, x_frac, label):
        svg, h = svg_w(rel, w)
        x = w * x_frac
        return (f'<div style="display:grid;gap:10px;justify-items:start">'
                f'<div style="font:700 14px sans-serif;color:{B["soot"]}">{label}</div>'
                f'<div style="padding:{x:.1f}px;outline:1px dashed {B["hematite"]};background:'
                f'repeating-linear-gradient(45deg,transparent 0 6px,{RL["surface-sunken"]} 6px 8px)">'
                f'<div style="outline:1px solid {B["maya"]}">{svg}</div></div>'
                f'<div style="font:400 13px sans-serif;color:{RL["text-muted"]}">dashed: clear space, 1 x = {x:.0f} px here</div></div>')
    lock_w = 640
    x_lock = build.SHRINE_UNIT / 439.35          # x as a fraction of the lockup width
    x_mark = build.SHRINE_UNIT / 87
    body = (f'<div style="padding:40px;display:flex;gap:56px;align-items:flex-start">'
            f'{boxed("logo/lockup-horizontal.svg", lock_w, x_lock, "Horizontal lockup")}'
            f'{boxed("logo/mark.svg", 200, x_mark, "Mark")}</div>')
    shoot(page(body, bg=B["lime"]), "construction.png", 1160, 380)


def misuse():
    """Six or more things never to do, drawn from the real files."""
    mk = lambda extra="", w=150: svg_w("logo/mark.svg", w, extra)[0]  # noqa: E731
    lock, _ = svg_w("logo/lockup-horizontal.svg", 300)
    recol = svg_inline("logo/mark.svg").replace(B["maya"], "#7a3cc8").replace(B["ochre"], "#2e9a57")
    recol = recol.replace("<svg ", '<svg style="width:150px;height:119px" ', 1)
    bare = svg_inline("logo/mark.svg").replace(f'stroke="{B["soot"]}"', 'stroke="none"')
    bare = bare.replace("<svg ", '<svg style="width:150px;height:119px" ', 1)
    typed = (f'<div style="display:flex;align-items:flex-end;gap:10px">{mk(w=70)}'
             f'<span style="font:700 34px Georgia,serif;color:{B["soot"]}">Montezuma\'s</span></div>')
    items = [
        ("Mirrored: blue must be on the left", mk("transform:scaleX(-1)")),
        ("Soot outline removed", bare),
        ("Recoloured", recol),
        ("Stretched", mk("width:210px;height:100px")),
        ("Effects: shadow, glow, gradient", mk(f"filter:drop-shadow(6px 6px 4px rgba(0,0,0,.6))")),
        ("Rotated", mk("transform:rotate(-12deg)")),
        ("On a busy or low-contrast ground", f'<div style="background:{B["hematite"]};padding:10px">{mk(w=130)}</div>'),
        ("Wordmark retyped in a font", typed),
        ("Lockup rearranged", f'<div style="display:flex;flex-direction:row-reverse;align-items:flex-end;gap:10px">'
                              f'{mk(w=60)}{svg_w("logo/wordmark.svg", 200)[0]}</div>'),
    ]
    cells = "".join(
        f'<div style="display:grid;gap:10px;justify-items:center;align-content:start;padding:16px;'
        f'background:{RL["surface-raised"]};border:2px solid {RL["danger"]};border-radius:4px">'
        f'<div style="height:130px;display:grid;place-items:center">{art}</div>'
        f'<div style="font:700 14px sans-serif;color:{RL["danger"]}">&#x2715; {t}</div></div>'
        for t, art in items)
    body = f'<div style="padding:32px;display:grid;grid-template-columns:repeat(3,300px);gap:20px">{cells}</div>'
    del lock
    shoot(page(body, bg=B["lime"]), "misuse.png", 1000, 690)


def main():
    fonts = {} if "--no-fonts" in sys.argv else fetch_fonts()
    icons()
    lockups()
    checks()
    band_preview()
    construction()
    misuse()
    type_specimen(fonts)
    applications(fonts)


if __name__ == "__main__":
    main()
