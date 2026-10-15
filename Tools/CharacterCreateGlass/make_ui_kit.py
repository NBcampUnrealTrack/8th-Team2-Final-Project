# -*- coding: utf-8 -*-
"""Renders the glass-sheet UI kit (PNG with alpha) from the same CSS the approved mock used.

Pictures are 2x, except the 9-slices (sheet, its shadow, the create button): Slate sizes a box brush's corners from the
texture's own pixels, so a 2x texture would draw every corner twice as large.

Run with any Python that has Playwright:  python Tools/CharacterCreateGlass/make_ui_kit.py
Output: ArtSource/UI/CharacterCreateGlass_20261013/kit/T_CCG_*.png  (+ kit.json with the sizes the importer needs)
Simple flat pills, circles and hairlines are not here: the widget draws those with rounded-box brushes.
"""
import json
import pathlib
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'ArtSource/UI/CharacterCreateGlass_20261013/kit'
SCALE = 2
NINE_SLICE = ('Sheet', 'SheetShadow', 'Cta', 'CtaShadow')      # rendered at 1x, see above

ICONS = {
    'Left': '<path d="M15 5l-7 7 7 7"/>',
    'Right': '<path d="M9 5l7 7-7 7"/>',
    'Arrow': '<path d="M5 12h14M13 6l6 6-6 6"/>',
    'Check': '<path d="M5 12.5l4.5 4.5L19 7.5"/>',
    'Lock': '<rect x="5" y="11" width="14" height="9" rx="2"/><path d="M8 11V8a4 4 0 018 0v3"/>',
    'Dice': '<rect x="4" y="4" width="16" height="16" rx="3.5"/><circle cx="9" cy="9" r="1" fill="#fff"/><circle cx="15" cy="15" r="1" fill="#fff"/><circle cx="15" cy="9" r="1" fill="#fff"/><circle cx="9" cy="15" r="1" fill="#fff"/>',
    'Reset': '<path d="M5 13a7 7 0 107-7H8"/><path d="M11 3L8 6l3 3"/>',
    'Female': '<circle cx="12" cy="9" r="5"/><path d="M12 14v7M8.5 18h7"/>',
    'Male': '<circle cx="10" cy="14" r="5"/><path d="M14 10l6-6M15 4h5v5"/>',
    'Plus': '<path d="M12 5v14M5 12h14"/>',
}


def svg(body, size=24, stroke='#fff', width=2):
    return ('<svg width="%d" height="%d" viewBox="0 0 24 24" fill="none" stroke="%s" stroke-width="%s" stroke-linecap="round" '
            'stroke-linejoin="round" style="display:block">%s</svg>') % (size, size, stroke, width, body)


# name -> (canvas w, canvas h, inner html)
ASSETS = {
    # glass sheet: 9-slice, the vertical gradient lives in the stretched middle
    'Sheet': (128, 256, '<div style="position:absolute;inset:0;box-sizing:border-box;border-radius:30px;'
                        'background:linear-gradient(180deg,rgba(46,58,80,.40),rgba(16,23,36,.58));'
                        'border:1px solid rgba(255,255,255,.16);box-shadow:inset 0 1px 0 rgba(255,255,255,.14)"></div>'),
    # drop shadow of the sheet: the box itself stays empty, like a CSS box-shadow
    'SheetShadow': (520, 520, '<div style="position:absolute;left:160px;top:130px;width:200px;height:200px;border-radius:30px;'
                              'box-shadow:0 30px 80px rgba(0,0,0,.5)"></div>'),
    'Cta': (96, 64, '<div style="position:absolute;inset:0;border-radius:32px;background:linear-gradient(180deg,#f8f2e4,#dfd2b3);'
                    'box-shadow:inset 0 1px 0 #fff"></div>'),
    # drop shadow of the create button (box-shadow: 0 10px 30px), the pill itself stays empty
    'CtaShadow': (216, 164, '<div style="position:absolute;left:60px;top:40px;width:96px;height:64px;border-radius:32px;'
                            'box-shadow:0 10px 30px rgba(0,0,0,.4)"></div>'),
    # thumbnail chip: the fill sits under the picture, the rings sit over it (the ring is outside the 48 px chip, like a CSS box-shadow)
    'ThumbFill': (48, 48, '<div style="position:absolute;inset:0;border-radius:14px;background:#0c1722"></div>'),
    'ThumbRing': (62, 62, '<div style="position:absolute;left:7px;top:7px;width:48px;height:48px;border-radius:14px;'
                          'box-shadow:0 0 0 2.5px #f1ede6,0 0 0 7px rgba(241,237,230,.14)"></div>'),
    'ThumbRingIdle': (50, 50, '<div style="position:absolute;left:1px;top:1px;width:48px;height:48px;border-radius:14px;'
                              'box-shadow:0 0 0 1px rgba(255,255,255,.16)"></div>'),
    # hair-colour swatch: the fill is a plain circle the C++ colours; these go over it
    'SwatchRim': (22, 22, '<div style="position:absolute;inset:0;border-radius:50%;'
                          'box-shadow:inset 0 0 0 1px rgba(255,255,255,.3),inset 0 -4px 5px rgba(0,0,0,.2),inset 0 3px 4px rgba(255,255,255,.14)"></div>'),
    'SwatchRing': (34, 34, '<div style="position:absolute;left:6px;top:6px;width:22px;height:22px;border-radius:50%;'
                           'box-shadow:0 0 0 2.5px rgba(14,21,33,.92),0 0 0 4.5px #f1ede6"></div>'),
    # select screen: the "new character" row is a dashed outline at its real size (the sheet's content width is fixed)
    'DashedRow': (426, 64, '<div style="position:absolute;inset:0;box-sizing:border-box;border-radius:18px;'
                           'border:1px dashed rgba(255,255,255,.28)"></div>'),
    # "준비 중": Slate cannot dash an outline, so the dashed pill is a picture at its real size
    'SoonPill': (71, 27, '<div style="position:absolute;inset:0;box-sizing:border-box;border-radius:99px;'
                         'border:1px dashed rgba(255,255,255,.3)"></div>'),
    'RaceRing': (72, 72, '<div style="position:absolute;left:8px;top:8px;width:56px;height:56px;border-radius:50%;'
                         'box-shadow:0 0 0 2.5px #f1ede6,0 0 0 7px rgba(241,237,230,.15)"></div>'),
    'RaceRingIdle': (72, 72, '<div style="position:absolute;left:8px;top:8px;width:56px;height:56px;border-radius:50%;'
                             'box-shadow:0 0 0 1px rgba(255,255,255,.2)"></div>'),
    'LockBadge': (22, 22, '<div style="position:absolute;inset:0;box-sizing:border-box;border-radius:50%;background:#0f1d2c;'
                          'border:1px solid rgba(255,255,255,.3);display:grid;place-items:center">' + svg(ICONS['Lock'], 11, '#c8d0d6') + '</div>'),
    'FadeDown': (8, 128, '<div style="position:absolute;inset:0;background:linear-gradient(180deg,#fff,rgba(255,255,255,0))"></div>'),
    'FadeUp': (8, 128, '<div style="position:absolute;inset:0;background:linear-gradient(0deg,#fff,rgba(255,255,255,0))"></div>'),
}
for key, body in ICONS.items():
    ASSETS['Icon_' + key] = (24, 24, svg(body))

html = ['<!doctype html><meta charset="utf-8"><style>html,body{margin:0;background:transparent}.a{position:relative;overflow:hidden;margin:8px}</style>']
for name, (w, h, inner) in ASSETS.items():
    html.append('<div class="a" id="%s" style="width:%dpx;height:%dpx">%s</div>' % (name, w, h, inner))

OUT.mkdir(parents=True, exist_ok=True)
page_file = OUT / '_kit.html'
page_file.write_text('\n'.join(html), encoding='utf-8')
meta = {}
with sync_playwright() as p:
    browser = p.chromium.launch()
    for scale in (SCALE, 1):
        page = browser.new_page(viewport={'width': 800, 'height': 600}, device_scale_factor=scale)
        page.goto(page_file.as_uri())
        for name, (w, h, _inner) in ASSETS.items():
            if (name in NINE_SLICE) != (scale == 1):
                continue
            target = OUT / ('T_CCG_%s.png' % name)
            page.locator('#' + name).screenshot(path=str(target), omit_background=True)
            meta['T_CCG_' + name] = {'w': w, 'h': h, 'px': [w * scale, h * scale]}
        page.close()
    browser.close()
# Pixel-art twinkles for the look change: drawn cell by cell, one cell = one art pixel = 5 px at 1080p (the stage's grid).
# Imported with nearest filtering so they stay as sharp as the character.
SPARKLES = {
    'Sparkle': ['...a...', '...a...', '..aba..', 'aabcbaa', '..aba..', '...a...', '...a...'],
    'SparkleSmall': ['..a..', '.aba.', 'abcba', '.aba.', '..a..'],
}
INK = {'a': (242, 212, 154, 235), 'b': (255, 243, 214, 255), 'c': (255, 255, 255, 255)}
from PIL import Image
for name, rows in SPARKLES.items():
    cells = len(rows)
    img = Image.new('RGBA', (cells * 5, cells * 5), (0, 0, 0, 0))
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in INK:
                img.paste(INK[ch], (x * 5, y * 5, x * 5 + 5, y * 5 + 5))
    img.save(OUT / ('T_CCG_%s.png' % name))
    meta['T_CCG_' + name] = {'w': cells * 5, 'h': cells * 5, 'px': [cells * 5, cells * 5]}

for stale in OUT.glob('T_CCG_*.png'):       # pictures a previous version made and this one no longer does
    if stale.stem not in meta:
        stale.unlink()
(OUT / 'kit.json').write_text(json.dumps(meta, indent=1), encoding='utf-8')
print('kit written:', len(meta), 'images ->', OUT)
