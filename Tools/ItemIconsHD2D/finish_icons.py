# -*- coding: utf-8 -*-
"""Turns the raw GPT pictures (ArtSource/ItemIcons_HD2D/_gen, ~1254 px pixel-art look on magenta) into game icons:

  1. background: flood the magenta from the edges -> transparent (pink items inside the black outline stay)
  2. the drawing's real pixel size: the period of the colour edges across columns / rows (autocorrelation)
  3. one sample per drawn pixel (median of the cell centre) -> the native pixel art (~26-30 px)
  4. near-identical colours merged (distance 22, most frequent first --- median cut eats highlights and shades), alpha 0 / 255
  5. centred 1:1 on a 48 x 48 grid -> every icon keeps the generator's dot size (~1/47 of the picture), written x3
     (144 px, nearest) as the game icon. The generator ignores "32 x 32"; squeezing its ~40-cell drawings into 32 drops rows.

Output: ArtSource/ItemIcons_HD2D/<T_Item_...>.png (144), _native/<name>.png (48), contact sheet _sheet.png.
Run: python Tools/ItemIconsHD2D/finish_icons.py [names...]
"""
import pathlib
import sys

import numpy as np
from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / 'ArtSource' / 'ItemIcons_HD2D' / '_gen'
OUT = ROOT / 'ArtSource' / 'ItemIcons_HD2D'
GRID = 48
SCALE = 3
KEY = (255, 0, 255)


def background_mask(rgb):
    """True where the picture is the magenta background reachable from the edges."""
    im = Image.fromarray(rgb)
    marker = (1, 254, 3)
    w, h = im.size
    for seed in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1), (w // 2, 0), (w // 2, h - 1), (0, h // 2), (w - 1, h // 2)):
        px = im.getpixel(seed)
        if sum(abs(a - b) for a, b in zip(px, KEY)) < 200:
            ImageDraw.floodfill(im, seed, marker, thresh=90)
    a = np.asarray(im)
    bg = np.all(a == marker, axis=-1)
    # magenta fringe left by the generator's soft edge
    d = np.abs(rgb.astype(int) - np.array(KEY)).sum(-1)
    return bg | (d < 120)


def period(profile, lo=12, hi=80):
    """Best repeat distance of a 1-D edge profile (autocorrelation peak)."""
    p = profile - profile.mean()
    best, best_v = None, -1e18
    n = len(p)
    for t in range(lo, min(hi, n // 3)):
        v = float(np.dot(p[:-t], p[t:])) / (n - t)
        if v > best_v:
            best, best_v = t, v
    # refine to a fractional period around the integer peak
    cand = np.arange(best - 1.0, best + 1.0, 0.05)
    xs = np.arange(n)
    scores = []
    for c in cand:
        phase = (xs % c) / c
        scores.append(abs(np.sum(p * np.exp(2j * np.pi * phase))))
    return float(cand[int(np.argmax(scores))])


def phase_of(profile, b):
    xs = np.arange(len(profile))
    z = np.sum((profile - profile.mean()) * np.exp(2j * np.pi * xs / b))
    return (-np.angle(z) / (2 * np.pi) * b) % b       # where the edges (cell boundaries) sit


def run_mode(rgb, bg, lo=10, hi=70):
    """Most common length of one-colour runs inside the drawing (rows and columns) = the drawn pixel size.
    A plain autocorrelation often locks onto twice the size; single-pixel runs are the most frequent ones."""
    runs = []
    a = rgb.astype(int)
    for arr, mask in ((a, bg), (a.transpose(1, 0, 2), bg.T)):
        for y in range(0, arr.shape[0], 5):
            row, m = arr[y], mask[y]
            change = np.flatnonzero(np.abs(np.diff(row, axis=0)).sum(-1) > 30) + 1
            bounds = np.concatenate(([0], change, [row.shape[0]]))
            for s, e in zip(bounds[:-1], bounds[1:]):
                if not m[s:e].any():
                    runs.append(e - s)
    h = np.bincount([r for r in runs if lo <= r <= hi], minlength=hi + 1).astype(float)
    h = np.convolve(h, [1, 2, 1], 'same')
    return int(np.argmax(h))


def refine(profile, b0):
    xs = np.arange(len(profile))
    p = profile - profile.mean()
    cand = np.arange(b0 - 1.5, b0 + 1.5, 0.05)
    return float(cand[int(np.argmax([abs(np.sum(p * np.exp(2j * np.pi * xs / c))) for c in cand]))])


def native(rgb, bg):
    edges_x = (np.abs(np.diff(rgb.astype(int), axis=1)).sum(-1) > 40) & ~bg[:, 1:]
    edges_y = (np.abs(np.diff(rgb.astype(int), axis=0)).sum(-1) > 40) & ~bg[1:, :]
    px, py = edges_x.sum(0).astype(float), edges_y.sum(1).astype(float)
    b0 = run_mode(rgb, bg)
    b = (refine(px, b0) + refine(py, b0)) / 2
    ox, oy = phase_of(px, b) + 1, phase_of(py, b) + 1
    ys, xs = np.where(~bg)
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    # first cell boundary at or before the content
    cx0 = ox + np.floor((x0 - ox) / b) * b
    cy0 = oy + np.floor((y0 - oy) / b) * b
    nx = int(np.ceil((x1 - cx0 + 1) / b))
    ny = int(np.ceil((y1 - cy0 + 1) / b))
    out = np.zeros((ny, nx, 4), np.uint8)
    r = max(1, int(b * 0.22))
    for j in range(ny):
        for i in range(nx):
            cx, cy = int(cx0 + (i + 0.5) * b), int(cy0 + (j + 0.5) * b)
            sl = (slice(max(cy - r, 0), cy + r + 1), slice(max(cx - r, 0), cx + r + 1))
            cell_bg = bg[sl]
            if cell_bg.size == 0 or cell_bg.mean() > 0.5:
                continue
            c = rgb[sl][~cell_bg]
            m = np.median(c, axis=0)
            if m[0] > 170 and m[1] < 110 and m[2] > 170:      # magenta fringe the flood missed
                continue
            out[j, i, :3] = m
            out[j, i, 3] = 255
    return out, b


def tidy(px, dist=22):
    """Merge near-identical colours (the generator varies a flat area by a few values). Most frequent colours win,
    so small highlights and shades that differ by more than `dist` survive (a median cut would fold them away)."""
    a = px.copy()
    solid = a[..., 3] > 0
    cols, counts = np.unique(a[solid][:, :3], axis=0, return_counts=True)
    order = np.argsort(-counts)
    keep = []
    remap = {}
    for k in order:
        c = cols[k].astype(int)
        for kc in keep:
            if np.abs(c - kc).sum() <= dist:
                remap[tuple(cols[k])] = tuple(kc)
                break
        else:
            keep.append(c)
            remap[tuple(cols[k])] = tuple(c)
    flat = a[solid][:, :3]
    a[solid, :3] = np.array([remap[tuple(c)] for c in flat], np.uint8)
    return Image.fromarray(a, 'RGBA')


def shrink_mode(im, w, h):
    """Pixel-art friendly shrink: each target pixel takes the most common opaque colour of its source area (dark
    outline colours win ties so silhouettes stay closed); it is transparent if most of the area is."""
    a = np.asarray(im)
    H, W = a.shape[:2]
    out = np.zeros((h, w, 4), np.uint8)
    for j in range(h):
        y0, y1 = int(j * H / h), max(int((j + 1) * H / h), int(j * H / h) + 1)
        for i in range(w):
            x0, x1 = int(i * W / w), max(int((i + 1) * W / w), int(i * W / w) + 1)
            blk = a[y0:y1, x0:x1].reshape(-1, 4)
            solid = blk[blk[:, 3] > 0]
            if len(solid) * 2 < len(blk):
                continue
            cols, counts = np.unique(solid[:, :3], axis=0, return_counts=True)
            dark = cols.sum(1) < 90
            score = counts + dark * 0.5
            out[j, i, :3] = cols[int(np.argmax(score))]
            out[j, i, 3] = 255
    return Image.fromarray(out, 'RGBA')


def fit(native_im):
    w, h = native_im.size
    box = native_im.getbbox() or (0, 0, w, h)
    crop = native_im.crop(box)
    if max(crop.size) > GRID - 2:          # too big for the grid (long swords, bows): shrink by majority colour per cell
        s = (GRID - 2) / max(crop.size)
        crop = shrink_mode(crop, max(1, round(crop.width * s)), max(1, round(crop.height * s)))
    canvas = Image.new('RGBA', (GRID, GRID), (0, 0, 0, 0))
    canvas.alpha_composite(crop, ((GRID - crop.width) // 2, (GRID - crop.height) // 2))
    return canvas


def finish(name):
    raw = np.asarray(Image.open(SRC / (name + '.png')).convert('RGB'))
    bg = background_mask(raw)
    px, b = native(raw, bg)
    icon = fit(tidy(px))
    (OUT / '_native').mkdir(parents=True, exist_ok=True)
    icon.save(OUT / '_native' / (name + '.png'))
    icon.resize((GRID * SCALE, GRID * SCALE), Image.NEAREST).save(OUT / (name + '.png'))
    w, h = icon.getbbox()[2] - icon.getbbox()[0], icon.getbbox()[3] - icon.getbbox()[1]
    return b, (w, h), px.shape[1::-1]


def sheet(names):
    cols = 10
    cell = 96
    rows = (len(names) + cols - 1) // cols
    im = Image.new('RGB', (cols * cell, rows * cell), (14, 26, 40))
    d = ImageDraw.Draw(im)
    for k, n in enumerate(names):
        p = OUT / (n + '.png')
        if not p.exists():
            continue
        ic = Image.open(p).convert('RGBA').resize((72, 72), Image.LANCZOS)
        x, y = (k % cols) * cell + 12, (k // cols) * cell + 6
        d.rectangle([x - 2, y - 2, x + 74, y + 74], fill=(22, 41, 61))
        im.paste(ic, (x, y), ic)
        d.text((x - 8, y + 76), n[7:][:14], fill=(170, 182, 191))
    im.save(OUT / '_sheet.png')


def main(argv):
    names = argv or sorted(p.stem for p in SRC.glob('T_Item_*.png'))
    for n in names:
        try:
            b, size, nat = finish(n)
            print('%-24s cell %.2f px  native %s  icon %s' % (n, b, nat, size))
        except Exception as e:      # keep going --- the sheet shows what is missing
            print('%-24s FAILED %s' % (n, e))
    sheet(sorted(p.stem for p in SRC.glob('T_Item_*.png')))


if __name__ == '__main__':
    main(sys.argv[1:])
