"""Debris sprites cut from the boss art itself (mossy back = 'hill', bark legs), pixelised exactly like the boss sprite."""
import math, random
from collections import deque
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

UP = "/root/.claude/uploads/1531e53f-362c-592b-aa7c-5f38d2c6e7df/"
NATIVE = 260 / 584          # boss illustration px -> boss native pixel (scene draws native at 2x nearest)
OUTLINE = (66, 38, 34)


def _boss_rgba():
    im = Image.open(UP + "ec4c2d59-image.png").convert("RGB").crop((0, 0, 767, 584))
    a = np.asarray(im).astype(int)
    H, W, _ = a.shape
    sat = a.max(2) - a.min(2); val = a.mean(2)
    bgish = (sat < 14) & (val > 95) & (val < 175)
    mask = np.zeros((H, W), bool)
    q = deque([(0, x) for x in range(W)] + [(H - 1, x) for x in range(W)] + [(y, 0) for y in range(H)] + [(y, W - 1) for y in range(H)])
    while q:
        y, x = q.popleft()
        if 0 <= y < H and 0 <= x < W and not mask[y, x] and bgish[y, x]:
            mask[y, x] = True
            q.extend(((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)))
    from scipy.ndimage import label
    lab, nlab = label(bgish & ~mask)
    if nlab:
        sizes = np.bincount(lab.ravel())
        big = np.where(sizes > 120)[0]
        big = big[big != 0]
        mask |= np.isin(lab, big)
    return im, ~mask


BOSS_IM, BOSS_MASK = _boss_rgba()
# palette identical to the in-scene boss sprite
_small = BOSS_IM.resize((round(767 * NATIVE), 260), Image.BOX)
PAL = _small.quantize(96, method=Image.Quantize.MEDIANCUT)

MOSS_SRC = [(262, 112, 520, 200), (122, 176, 300, 256)]
BARK_SRC = [(302, 302, 468, 468), (562, 382, 698, 498)]


def _tex(box, w, h, rng, flip=True):
    """1:1 texture from the boss art (mirrored tiling when the crop is too small) — no rescaling."""
    t = BOSS_IM.crop(box)
    if flip and rng.random() < .5:
        t = t.transpose(Image.FLIP_LEFT_RIGHT)
    tw, th = t.width, t.height
    big = Image.new("RGB", (tw * 3, th * 3))
    for i in range(3):
        for j in range(3):
            tt = t
            if i % 2: tt = tt.transpose(Image.FLIP_LEFT_RIGHT)
            if j % 2: tt = tt.transpose(Image.FLIP_TOP_BOTTOM)
            big.paste(tt, (i * tw, j * th))
    ox, oy = rng.randint(0, max(0, big.width - w)), rng.randint(0, max(0, big.height - h))
    return big.crop((ox, oy, ox + w, oy + h))


def _scallop_mask(w, h, y0, rng, r=11):
    """Mask of the moss cap: everything above a scalloped (leaf-edged) line around y0."""
    m = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(m)
    d.rectangle((0, 0, w, y0), fill=255)
    x = -r
    while x < w + r:
        rr = r * rng.uniform(.75, 1.25)
        cy = y0 + rng.uniform(-r * .3, r * .3)
        # rounded leaf lobe with a soft tip (boss moss edge)
        d.ellipse((x - rr, cy - rr, x + rr, cy + rr * .8), fill=255)
        d.polygon([(x - rr * .55, cy + rr * .3), (x + rr * .55, cy + rr * .3), (x + rng.uniform(-2, 2), cy + rr * 1.15)], fill=255)
        x += rr * rng.uniform(1.4, 1.9)
    return m


def clod(width, seed, kind="clod"):
    """Illustration-scale clod (RGBA PIL) built from the boss's own moss & bark."""
    rng = random.Random(seed)
    w = int(width * 1.15); h = int(width * (1.25 if kind == "clod" else .95))
    pad = 10
    W, H = w + pad * 2, h + pad * 2
    cx, top = W / 2, pad + h * .12
    R = width / 2
    if kind == "clod":
        tilt = rng.uniform(-.08, .08)
        base = [(-1.0, -.0 + tilt), (-.5, -.04), (0, -.06), (.5, -.04), (1.0, 0 - tilt),
                (.92, .38), (.66, .78), (.32, 1.02), (.04, 1.3), (-.26, 1.06), (-.6, .9), (-.88, .5)]
        pts = [(cx + (x + rng.uniform(-.05, .05)) * R, top + (y + (rng.uniform(-.02, .02) if y < .1 else rng.uniform(-.08, .08))) * R)
               for x, y in base]
    else:
        k = rng.randint(5, 7); a0 = rng.uniform(0, 6.28)
        pts = [(cx + math.cos(a) * rng.uniform(.75, 1.0) * R, H / 2 + math.sin(a) * rng.uniform(.7, 1.0) * R)
               for a in (a0 + i * 2 * math.pi / k + rng.uniform(-.3, .3) for i in range(k))]
    shape = Image.new("L", (W, H), 0)
    ImageDraw.Draw(shape).polygon(pts, fill=255)
    # textures
    bark = _tex(BARK_SRC[rng.randrange(len(BARK_SRC))], W, H, rng)
    if rng.random() < .5:
        bark = bark.rotate(rng.choice([90, 270]), expand=False)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(bark, (0, 0), shape)
    if kind == "clod":
        moss = _tex(MOSS_SRC[rng.randrange(len(MOSS_SRC))], W, H, rng)
        cap = _scallop_mask(W, H, int(top + R * rng.uniform(.34, .46)), rng, r=max(7, int(R * .2)))
        cap_in = Image.fromarray((np.asarray(cap) > 0) & (np.asarray(shape) > 0)).convert("L")
        # moss also bulges slightly above the cut line (soft dome)
        dome = Image.new("L", (W, H), 0)
        # outline under the moss lobes (boss style)
        edge = cap_in.filter(ImageFilter.MaxFilter(7))
        ol = Image.new("RGBA", (W, H), OUTLINE + (255,))
        shape_np = np.asarray(shape) > 0
        edge_m = Image.fromarray((np.asarray(edge) > 0) & (shape_np | (np.asarray(dome) > 0))).convert("L")
        img.paste(ol, (0, 0), edge_m)
        img.paste(moss, (0, 0), cap_in)
        shape = Image.fromarray(shape_np | (np.asarray(cap_in) > 0)).convert("L")
        # little sprouting leaves on top, like the boss's back
        d = ImageDraw.Draw(img)
        for i in range(rng.randint(1, 3)):
            x = cx + rng.uniform(-.7, .7) * R
            y = top + rng.uniform(-.05, .05) * R
            L = R * rng.uniform(.18, .3)
            ang = rng.uniform(-2.2, -0.9)
            ex, ey = x + math.cos(ang) * L, y + math.sin(ang) * L
            d.line((x, y, ex, ey), fill=(110, 70, 50, 255), width=3)
            lw = L * .45
            nx, ny = -math.sin(ang), math.cos(ang)
            leaf = [(ex, ey), (ex + math.cos(ang) * lw + nx * lw * .5, ey + math.sin(ang) * lw + ny * lw * .5),
                    (ex + math.cos(ang) * lw * 2, ey + math.sin(ang) * lw * 2),
                    (ex + math.cos(ang) * lw - nx * lw * .5, ey + math.sin(ang) * lw - ny * lw * .5)]
            d.polygon(leaf, fill=(150, 176, 74, 255), outline=OUTLINE + (255,))
        # hanging roots
        for i in range(rng.randint(2, 4)):
            x = cx + rng.uniform(-.45, .4) * R
            col = np.where(np.asarray(shape)[:, int(x)] > 0)[0]
            if not len(col):
                continue
            y = col.max() - 3
            L = R * rng.uniform(.25, .55)
            p = [(x, y)]
            ph = rng.uniform(0, 6)
            for j in range(1, 8):
                p.append((x + math.sin(j * .9 + ph) * R * .05, y + L * j / 7))
            for j in range(len(p) - 1):
                wdt = max(2, int(6 * (1 - j / len(p))))
                d.line([p[j], p[j + 1]], fill=OUTLINE + (255,), width=wdt + 3)
                d.line([p[j], p[j + 1]], fill=(150, 96, 74, 255), width=wdt)
            shape = Image.fromarray(np.asarray(img)[..., 3] > 0).convert("L")
    # boss-style cel shadow: hard diagonal cut on the lower-right
    a = np.asarray(img).astype(np.float32)
    Y, X = np.mgrid[0:H, 0:W]
    cut = (X - cx) * .8 + (Y - (top + R * .5)) * 1.0 > R * .35 + np.sin(Y * .08 + seed) * 4
    a[..., :3] = np.where(cut[..., None], a[..., :3] * np.array([.78, .74, .82]), a[..., :3])
    img = Image.fromarray(a.astype(np.uint8), "RGBA")
    # outer outline (boss line colour)
    al = np.asarray(img)[..., 3] > 0
    ring = Image.fromarray(al).convert("L").filter(ImageFilter.MaxFilter(7))
    outl = Image.new("RGBA", (W, H), OUTLINE + (255,))
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    out.paste(outl, (0, 0), ring)
    out.alpha_composite(img)
    return out


def pixelise(im):
    """Same treatment as the boss: BOX downscale to native, boss palette, hard alpha."""
    w, h = max(1, round(im.width * NATIVE)), max(1, round(im.height * NATIVE))
    s = im.resize((w, h), Image.BOX)
    a = np.asarray(s).copy()
    alpha = a[..., 3] > 120
    rgb = Image.fromarray(a[..., :3]).quantize(palette=PAL, dither=Image.Dither.NONE).convert("RGB")
    return np.dstack([np.asarray(rgb), (alpha * 255).astype(np.uint8)])


SPECS = [("clod", 120, 1), ("clod", 70, 2), ("clod", 58, 3), ("clod", 46, 4),
         ("chunk", 30, 11), ("chunk", 27, 12), ("chunk", 24, 13), ("chunk", 22, 14),
         ("chunk", 19, 15), ("chunk", 17, 16), ("chunk", 15, 17), ("chunk", 13, 18),
         ("chunk", 11, 19), ("chunk", 9, 20), ("chunk", 8, 21), ("chunk", 7, 22)]


def build_sprites():
    out = []
    for k, w, s in SPECS:
        a = pixelise(clod(w / NATIVE, s, k))   # widths are in native px
        ys, xs = np.where(a[..., 3] > 0)
        out.append(dict(kind=k, px=a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]))
    return out


def atlas(path, cell=144):
    spr = build_sprites()
    sheet = Image.new("RGBA", (cell * 4, cell * 4), (0, 0, 0, 0))
    rects = []
    for i, s in enumerate(spr):
        im = Image.fromarray(s["px"], "RGBA")
        ox, oy = (i % 4) * cell + (cell - im.width) // 2, (i // 4) * cell + (cell - im.height) // 2
        sheet.paste(im, (ox, oy))
        rects.append(dict(kind=s["kind"], x=ox, y=oy, w=im.width, h=im.height))
    sheet.save(path)
    return rects


# =============================================================================
# Hand-built clods in the boss's own drawing language (cel shading, outlined bark plates,
# lobed moss with lit rims, white flowers), then pixelised with the boss palette.
# =============================================================================
BK = [(74, 48, 46), (98, 68, 60), (122, 84, 68), (146, 100, 78), (172, 124, 92)]
MS = [(84, 92, 52), (108, 112, 58), (130, 128, 58), (152, 150, 62), (186, 188, 90)]


def clod2(width, seed, kind="clod"):
    rng = random.Random(seed)
    R = width / 2
    W = int(width * 1.5) + 20; H = int(width * 1.7) + 20
    cx, top = W / 2, 18 + R * .25
    if kind == "clod":
        tilt = rng.uniform(-.06, .06)
        base = [(-1.0, tilt), (-.5, -.03), (0, -.05), (.5, -.03), (1.0, -tilt),
                (.93, .4), (.68, .8), (.34, 1.04), (.06, 1.32), (-.24, 1.08), (-.6, .92), (-.88, .52)]
        pts = [(cx + (x + rng.uniform(-.05, .05)) * R, top + (y + (rng.uniform(-.02, .02) if y < .1 else rng.uniform(-.08, .08))) * R)
               for x, y in base]
    else:
        k = rng.randint(5, 7); a0 = rng.uniform(0, 6.28)
        cyc = H / 2
        pts = [(cx + math.cos(a) * rng.uniform(.78, 1.0) * R, cyc + math.sin(a) * rng.uniform(.72, 1.0) * R)
               for a in (a0 + i * 2 * math.pi / k + rng.uniform(-.3, .3) for i in range(k))]
    sh = Image.new("L", (W, H), 0); ImageDraw.Draw(sh).polygon(pts, fill=255)
    shape = np.asarray(sh) > 0
    Y, X = np.mgrid[0:H, 0:W].astype(np.float32)
    # --- bark plates: vertically elongated voronoi cells
    nP = max(3, int(R / 9)) if kind == "clod" else max(2, int(R / 10))
    ys_ = np.where(shape.any(1))[0]; xs_ = np.where(shape.any(0))[0]
    px = np.array([rng.uniform(xs_.min(), xs_.max()) for _ in range(nP * 2)])
    py = np.array([rng.uniform(ys_.min(), ys_.max()) for _ in range(nP * 2)])
    Dd = ((X[..., None] - px) / 1.0) ** 2 + ((Y[..., None] - py) / 2.6) ** 2
    o = np.argsort(Dd, -1)
    cell = o[..., 0]
    d1 = np.sqrt(np.take_along_axis(Dd, o[..., :1], -1)[..., 0]); d2 = np.sqrt(np.take_along_axis(Dd, o[..., 1:2], -1)[..., 0])
    lvl_cell = np.array([rng.choice([2, 2, 3, 3, 4]) for _ in range(len(px))])
    lvl = lvl_cell[cell].copy()
    # cel shadow inside each plate: right side one step darker
    rel_x = X - px[cell]
    lvl = np.where(rel_x > R * .12, lvl - 1, lvl)
    # global form shadow lower-right
    lvl = np.where((X - cx) * .7 + (Y - (top + R * .6)) > R * .45, lvl - 1, lvl)
    lvl = np.clip(lvl, 1, 4)
    bk = np.array(BK, np.uint8)
    rgb = bk[lvl]
    # plate seams (outline colour) + bark grain strokes
    seam = (d2 - d1 < 2.2)
    rgb[seam] = BK[0]
    grain = (np.sin((X + np.sin(Y * .05 + seed) * 6) * .22 + cell * 1.7) > .93) & ~seam
    rgb[grain] = bk[np.clip(lvl - 1, 0, 4)][grain]
    a = shape.copy()
    img = np.dstack([rgb, a.astype(np.uint8) * 255])
    im = Image.fromarray(img, "RGBA")
    d = ImageDraw.Draw(im)
    if kind == "clod":
        # --- moss cap: overlapping cel-shaded clumps (like the moss on the boss's back)
        blobs = []
        x = pts[0][0] + R * .05
        while x < pts[4][0] - R * .02:
            rr = R * rng.uniform(.2, .32)
            blobs.append((x, top + rng.uniform(-.02, .16) * R, rr))
            x += rr * rng.uniform(1.0, 1.4)
        for i in range(max(1, int(R / 25))):   # a second, lower row hanging over the bark
            blobs.append((cx + rng.uniform(-.75, .65) * R, top + R * rng.uniform(.26, .36), R * rng.uniform(.14, .2)))
        blobs.sort(key=lambda b_: b_[1])
        for (bx, by, rr) in blobs:
            e = lambda k, ox=0, oy=0: (bx + ox - rr * k, by + oy - rr * k * .8, bx + ox + rr * k, by + oy + rr * k * .8)
            d.ellipse(e(1.0), fill=OUTLINE + (255,))
            d.ellipse(e(.9), fill=MS[1] + (255,))
            d.ellipse(e(.74, -rr * .1, -rr * .14), fill=MS[2] + (255,))
            d.ellipse(e(.42, -rr * .3, -rr * .34), fill=MS[3] + (255,))
            d.ellipse(e(.14, -rr * .45, -rr * .5), fill=MS[4] + (255,))
            if rng.random() < .5:   # leaf tip hanging from the clump
                d.polygon([(bx - rr * .3, by + rr * .55), (bx + rr * .2, by + rr * .55), (bx - rr * .05, by + rr * 1.05)],
                          fill=MS[1] + (255,), outline=OUTLINE + (255,))
        # small white flowers & a sprout leaf (boss motifs)
        for i in range(rng.randint(1, 3) if R > 40 else rng.randint(0, 1)):
            fx_, fy_ = cx + rng.uniform(-.7, .6) * R, top + rng.uniform(.05, .3) * R
            rr = max(2.5, R * .04)
            for ox, oy in ((0, -rr), (-rr, 0), (rr, 0), (0, rr)):
                d.ellipse((fx_ + ox - rr * .75, fy_ + oy - rr * .75, fx_ + ox + rr * .75, fy_ + oy + rr * .75), fill=(236, 230, 210, 255))
            d.ellipse((fx_ - rr * .45, fy_ - rr * .45, fx_ + rr * .45, fy_ + rr * .45), fill=(222, 180, 90, 255))
        if R > 30:
            x0, y0 = cx + rng.uniform(-.5, .5) * R, top - R * .02
            L = R * .28; ang = rng.uniform(-2.0, -1.1)
            ex, ey = x0 + math.cos(ang) * L, y0 + math.sin(ang) * L
            d.line((x0, y0, ex, ey), fill=OUTLINE + (255,), width=5); d.line((x0, y0, ex, ey), fill=BK[3] + (255,), width=2)
            lw = L * .55; nx, ny = -math.sin(ang), math.cos(ang)
            leaf = [(ex, ey), (ex + math.cos(ang) * lw * .5 + nx * lw * .45, ey + math.sin(ang) * lw * .5 + ny * lw * .45),
                    (ex + math.cos(ang) * lw * 1.3, ey + math.sin(ang) * lw * 1.3),
                    (ex + math.cos(ang) * lw * .5 - nx * lw * .45, ey + math.sin(ang) * lw * .5 - ny * lw * .45)]
            d.polygon(leaf, fill=MS[3] + (255,), outline=OUTLINE + (255,))
        # hanging roots
        al = np.asarray(im)[..., 3] > 0
        for i in range(rng.randint(2, 4)):
            x = cx + rng.uniform(-.4, .35) * R
            col = np.where(al[:, int(x)])[0]
            if not len(col):
                continue
            y = col.max() - 4
            L = R * rng.uniform(.25, .5); ph = rng.uniform(0, 6)
            p = [(x + math.sin(j * .9 + ph) * R * .05, y + L * j / 7) for j in range(8)]
            for j in range(len(p) - 1):
                wdt = max(2, int(R * .07 * (1 - j / len(p))) + 1)
                d.line([p[j], p[j + 1]], fill=OUTLINE + (255,), width=wdt + 4)
            for j in range(len(p) - 1):
                wdt = max(1, int(R * .07 * (1 - j / len(p))))
                d.line([p[j], p[j + 1]], fill=BK[3] + (255,), width=wdt)
    # outer outline
    al = np.asarray(im)[..., 3] > 0
    ring = Image.fromarray(al.astype(np.uint8) * 255).filter(ImageFilter.MaxFilter(7))
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    out.paste(Image.new("RGBA", (W, H), OUTLINE + (255,)), (0, 0), ring)
    out.alpha_composite(im)
    return out


def build_sprites():
    out = []
    for k, w, s in SPECS:
        a = pixelise(clod2(w / NATIVE, s, k))
        ys, xs = np.where(a[..., 3] > 0)
        out.append(dict(kind=k, px=a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]))
    return out


if __name__ == "__main__":
    import sys, json
    p = sys.argv[1] if len(sys.argv) > 1 else "out/inspect/debris.png"
    r = atlas(p)
    json.dump(dict(name="T_Boss2_DebrisAtlas", filter="Nearest", screen_scale_1080p=2, cells=r), open(p[:-4] + ".json", "w"), indent=1)


