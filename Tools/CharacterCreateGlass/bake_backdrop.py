# -*- coding: utf-8 -*-
"""Bakes the inn-door stage backdrop for the character creation level.

The pixel stage is assembled from the game's painted 2.5D set (tavern facade, cobble, barrels, bench) on a 512x216 grid,
scaled x10 without filtering, then the night grade, lantern glow and the foot shadow of the approved mock are applied
at full resolution (5120x2160). Same numbers as the mock's CSS, so the middle 16:9 matches it pixel for pixel at 1080p.

The approved mock was 16:9 (384 cells). The stage is 64 cells wider on each side, so a screen up to 2560:1080 (21:9) shows more
street instead of zooming in; 16:9 shows the middle, narrower screens (16:10, 4:3) crop the sides. All coordinates below are in
1080p units of that middle 16:9 frame (x 0..1920), as in the mock.

Run:  python Tools/CharacterCreateGlass/bake_backdrop.py
Needs the web art set (NiaUnbounds/public/assets/world). Output: ArtSource/UI/CharacterCreateGlass_20261013/
"""
import json
import pathlib
import numpy as np
from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[2]
WEB = pathlib.Path('C:/Users/Ndus_072/reddy/projects/NiaUnbounds/public/assets/world')
OUT = ROOT / 'ArtSource/UI/CharacterCreateGlass_20261013'
GRID_W, GRID_H, UP = 384, 216, 10          # the 16:9 frame: 1 grid cell = 5 px at 1080p, 10 px in the texture
MARGIN = 64                                # extra cells on each side (320 px at 1080p)
STAGE_W = GRID_W + 2 * MARGIN
GROUND_Y, FACADE_X, DOOR_BOTTOM = 168, -74, 283

# ---------- pixel stage ----------
tav_a = Image.open(WEB / 'buildings/fac_tavern_a.png').convert('RGB')
tav_b = Image.open(WEB / 'buildings/fac_tavern_b.png').convert('RGB')
cobble = Image.open(WEB / 'tiles/gv_cobble_warm.png').convert('RGB')
hard = Image.open(WEB / 'props/hard.png').convert('RGBA')
props = json.loads((WEB / 'props/hard.json').read_text(encoding='utf-8'))['props']


def prop(name, frame):
    f = props[name]['frames'][frame]
    return hard.crop((f['x'], f['y'], f['x'] + f['w'], f['y'] + f['h']))


stage = Image.new('RGBA', (STAGE_W, GRID_H), (16, 14, 12, 255))
top = GROUND_Y - DOOR_BOTTOM
for k, facade in ((-1, tav_b), (1, tav_b), (0, tav_a)):
    stage.paste(facade, (MARGIN + FACADE_X + 358 * k, top))       # covers cells -368..706, the stage is 0..512
ground_h = GRID_H - GROUND_Y
tile = cobble.resize((512, 64), Image.NEAREST)          # seen at a shallow angle: squashed to half height
strip = Image.new('RGB', (STAGE_W, ground_h))
for x0 in (MARGIN - 512, MARGIN):                      # the tile starts where the 16:9 frame starts, as before; it repeats sideways
    strip.paste(tile, (x0, 0))
px = strip.load()
for y in range(ground_h):
    f = (0.5 + 0.5 * min(1.0, y / 10.0)) * 0.92          # contact shade along the wall
    for x in range(STAGE_W):
        r, g, b = px[x, y]
        px[x, y] = (int(r * f), int(g * f), int(b * f))
stage.paste(strip, (0, GROUND_Y))
for name, frame, x, base in (('barrel', 0, 2, 184), ('barrel', 1, 33, 189), ('bench', 0, 198, 181)):
    p = prop(name, frame)
    x += MARGIN
    shade = Image.new('RGBA', stage.size, (0, 0, 0, 0))
    ImageDraw.Draw(shade).ellipse((x - 3, base - 4, x + p.width + 3, base + 3), fill=(0, 0, 0, 120))
    stage = Image.alpha_composite(stage, shade)
    stage.paste(p, (x, base - p.height), p)
stage = stage.convert('RGB')
OUT.mkdir(parents=True, exist_ok=True)
stage.save(OUT / 'InnDoor_Stage_384x216.png')

# ---------- lighting, in 1080p coordinates ----------
W, H = STAGE_W * UP, GRID_H * UP
UNIT = 1080.0 / H                          # 1080p units per texture pixel
SHIFT = MARGIN * 5                         # where the 16:9 frame starts, in 1080p units
img = np.asarray(stage.resize((W, H), Image.NEAREST), dtype=np.float32) / 255.0
ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
xs = (xs + 0.5) * UNIT - SHIFT
ys = (ys + 0.5) * UNIT


def radial(cx, cy, rx, ry=None):
    ry = rx if ry is None else ry
    return np.sqrt(((xs - cx) / rx) ** 2 + ((ys - cy) / ry) ** 2)


def ramp(t, stops):
    """Piecewise-linear value over normalised distance t, clamped at both ends."""
    pos = [s[0] for s in stops]; val = [s[1] for s in stops]
    return np.interp(t, pos, val).astype(np.float32)


# night: a multiply layer with holes at the lights
mask = (ramp(radial(780, 325, 430), [(0, 0), (.45, .35), (1, 1)])
        * ramp(radial(260, 325, 400), [(0, 0), (.45, .40), (1, 1)])
        * ramp(radial(1145, 400, 330), [(0, 0), (.50, .50), (1, 1)])
        * ramp(radial(-30, 400, 300), [(0, 0), (.50, .50), (1, 1)])
        * ramp(radial(740, 930, 480, 150), [(0, .25), (1, 1)]))
t = (ys / 1080.0)[..., None]
night_rgb = (np.array([9, 15, 34], np.float32) * (1 - t) + np.array([12, 18, 36], np.float32) * t) / 255.0
night_a = ((0.86 * (1 - t[..., 0]) + 0.78 * t[..., 0]) * mask)[..., None]
img = img * (1.0 - night_a + night_a * night_rgb)
del mask, night_rgb, night_a

# glow: the lantern and window light, screen-blended
glows = [  # listed top-most first, as in the CSS
    (radial(780, 318, 120), (255, 190, 110), .62),
    (radial(260, 318, 120), (255, 190, 110), .55),
    (radial(780, 330, 420), (255, 160, 80), .16),
    (radial(260, 330, 380), (255, 160, 80), .13),
    (radial(1145, 395, 210, 190), (255, 200, 120), .20),
]
premul = np.zeros_like(img)
for dist, colour, alpha in reversed(glows):
    a = (np.clip(1.0 - dist, 0.0, 1.0) * alpha)[..., None]
    premul = premul * (1.0 - a) + (np.array(colour, np.float32) / 255.0) * a
img = img + premul * (1.0 - img)
del premul

# The select screen draws its character only when there is one, so it gets the stage without the foot shadow
# and the shadow as a separate picture (same ramp, cut out around the feet).
Image.fromarray(np.clip(img * 255.0 + 0.5, 0, 255).astype(np.uint8), 'RGB').save(OUT / 'T_CCG_InnDoor_Backdrop_NoFoot.png', optimize=True)

# the character's foot shadow (the creation stand never moves, so there it is baked in)
foot = ramp(radial(705, 929, 117, 23), [(0, .62), (.45, .34), (.72, 0), (9, 0)])[..., None]
cut = (int((705 - 130 + SHIFT) / UNIT), int((929 - 30) / UNIT), int((705 + 130 + SHIFT) / UNIT), int((929 + 30) / UNIT))
shadow = np.zeros((cut[3] - cut[1], cut[2] - cut[0], 4), np.float32)
shadow[..., 3] = foot[cut[1]:cut[3], cut[0]:cut[2], 0]
Image.fromarray(np.clip(shadow * 255.0 + 0.5, 0, 255).astype(np.uint8), 'RGBA').save(OUT / 'T_CCG_FootShadow.png', optimize=True)   # 260 x 60 at 1080p
img = img * (1.0 - foot)

out = Image.fromarray(np.clip(img * 255.0 + 0.5, 0, 255).astype(np.uint8), 'RGB')
out.save(OUT / 'T_CCG_InnDoor_Backdrop.png', optimize=True)
out.resize((W // 2, H // 2), Image.BOX).crop((SHIFT, 0, SHIFT + 1920, 1080)).save(OUT / 'InnDoor_Backdrop_preview_1080.png')   # the middle 16:9
print('backdrop', out.size, '->', OUT)
