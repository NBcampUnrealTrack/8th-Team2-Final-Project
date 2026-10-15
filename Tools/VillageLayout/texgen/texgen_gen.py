import numpy as np, os, sys
from PIL import Image, ImageDraw

OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)
L = 64
FINAL = 256
NORMAL_CONVENTION = sys.argv[2] if len(sys.argv) > 2 else "dx"   # "dx" = green down (Unreal), "gl" = green up

yy, xx = np.mgrid[0:L, 0:L]

def tile_noise(L, fx, fy, rng):
    g = rng.random((fy, fx))
    ys = np.arange(L) * fy / L
    xs = np.arange(L) * fx / L
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    ty = (ys - y0)[:, None]; tx = (xs - x0)[None, :]
    ty = ty * ty * (3 - 2 * ty); tx = tx * tx * (3 - 2 * tx)
    y1 = (y0 + 1) % fy; x1 = (x0 + 1) % fx
    a = g[y0][:, x0]; b = g[y0][:, x1]; c = g[y1][:, x0]; d = g[y1][:, x1]
    return a * (1 - tx) * (1 - ty) + b * tx * (1 - ty) + c * (1 - tx) * ty + d * tx * ty

def fbm(L, f0, rng, octaves=3, ax=1.0, ay=1.0, gain=0.5):
    out = np.zeros((L, L)); amp = 1.0; tot = 0.0; f = f0
    for _ in range(octaves):
        fx = max(1, int(round(f * ax))); fy = max(1, int(round(f * ay)))
        out += amp * tile_noise(L, fx, fy, rng); tot += amp; amp *= gain; f *= 2
    return out / tot

def fine(rng):
    return tile_noise(L, L, L, rng)

def quant(v, n):
    return np.clip(np.floor(np.clip(v, 0, 0.999999) * n), 0, n - 1).astype(int)

def wrapd(a, c):
    return (a - c + L / 2) % L - L / 2

def roll(H, dy, dx):
    return np.roll(np.roll(H, dy, axis=0), dx, axis=1)

def sobel_normal(H, strength):
    # gx = right - left ; gy = below - above  (row index grows downward)
    gx = (roll(H, -1, -1) + 2 * roll(H, 0, -1) + roll(H, 1, -1)) - (roll(H, -1, 1) + 2 * roll(H, 0, 1) + roll(H, 1, 1))
    gy = (roll(H, -1, -1) + 2 * roll(H, -1, 0) + roll(H, -1, 1)) - (roll(H, 1, -1) + 2 * roll(H, 1, 0) + roll(H, 1, 1))
    gx /= 8.0; gy /= 8.0
    nx = -gx * strength
    ny = (-gy if NORMAL_CONVENTION == "dx" else gy) * strength
    nz = np.ones_like(H)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx, ny, nz = nx / ln, ny / ln, nz / ln
    rgb = np.stack([128 + nx * 127, 128 + ny * 127, 128 + nz * 127], axis=-1)
    return np.clip(np.round(rgb), 0, 255).astype(np.uint8)

def ao_from_height(H, radius=2, k=1.6, floor=0.72):
    acc = np.zeros_like(H); n = 0
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            acc += roll(H, dy, dx); n += 1
    mean = acc / n
    occ = np.clip(mean - H, 0, None) * k
    return np.clip(1.0 - occ, floor, 1.0)

def colorize(idx, pal):
    pal = np.array(pal, dtype=np.uint8)
    return pal[np.clip(idx, 0, len(pal) - 1)]

# ---------------------------------------------------------------- materials
def gen_plaster(rng):
    base = fbm(L, 4, rng, 3)
    f = fine(rng)
    H = 0.5 + (base - 0.5) * 0.25 + (f - 0.5) * 0.06
    pits = f < 0.035
    bumps = f > 0.975
    H[pits] -= 0.12; H[bumps] += 0.06
    pal = [(150, 132, 102), (180, 162, 130), (204, 188, 154), (218, 204, 172), (232, 220, 190)]
    v = 0.55 + (base - 0.5) * 0.5 + (f - 0.5) * 0.28
    idx = quant(v, 5)
    idx[pits] = np.minimum(idx[pits], 1)
    idx[bumps] = 4
    return H, colorize(idx, pal), 0.85, 4.0

def gen_planks(rng):
    PH = 16
    plank = yy // PH; ly = yy % PH
    f = fine(rng)
    slow = fbm(L, 1, rng, 2, ax=3, ay=2)
    idx = np.zeros((L, L), int); H = np.full((L, L), 0.6)
    shade = [-0.06, 0.05, -0.02, 0.08]
    for p in range(4):
        m = plank == p
        lines = tile_noise(L, 3, 26, rng)
        v = 0.55 + shade[p] + (slow - 0.5) * 0.45 + (f - 0.5) * 0.1
        i = quant(v, 5)
        i[lines > 0.74] -= 1
        i[lines < 0.16] += 1
        idx[m] = np.clip(i, 0, 4)[m]
        H[m] = (0.6 + (lines - 0.5) * 0.08 + (f - 0.5) * 0.02)[m]
    seam = ly == 0
    idx[seam] = 0; H[seam] = 0.25
    hl = ly == 1
    idx[hl] = np.minimum(idx[hl] + 1, 4)
    ends = [9, 41, 25, 55]
    for p in range(4):
        m = (plank == p) & (xx == ends[p]) & (ly > 0)
        idx[m] = 0; H[m] = 0.3
        m2 = (plank == p) & (xx == (ends[p] + 1) % L) & (ly > 0)
        idx[m2] = np.minimum(idx[m2] + 1, 4)
    for (cx, cy, rx, ry) in [(20, 8, 3.6, 2.6), (50, 40, 3.2, 2.2), (34, 57, 2.6, 1.8)]:
        dx = wrapd(xx, cx); dy = wrapd(yy, cy)
        d = np.sqrt((dx / rx) ** 2 + (dy / ry) ** 2)
        idx[(d < 1.0) & (d >= 0.7)] = 1
        idx[(d < 0.7) & (d >= 0.35)] = 2
        idx[d < 0.35] = 0
        H[d < 1.0] -= 0.06 * (1 - d[d < 1.0])
    pal = [(54, 36, 22), (88, 60, 36), (118, 84, 50), (144, 106, 64), (168, 130, 84)]
    return H, colorize(idx, pal), 0.7, 3.0

def gen_timber(rng):
    f = fine(rng)
    slow = fbm(L, 1, rng, 2, ax=2, ay=2)
    lines = tile_noise(L, 22, 2, rng)
    lines2 = tile_noise(L, 30, 3, rng)
    streak = tile_noise(L, 14, 1, rng)
    v = 0.48 + (slow - 0.5) * 0.4 + (f - 0.5) * 0.1
    idx = quant(v, 5)
    idx[lines > 0.74] -= 1
    idx[lines2 > 0.8] -= 1
    idx[lines < 0.16] += 1
    idx[streak > 0.86] = 0
    idx = np.clip(idx, 0, 4)
    H = 0.5 + (lines - 0.5) * 0.14 + (lines2 - 0.5) * 0.06
    H[streak > 0.86] -= 0.1
    pal = [(34, 23, 15), (52, 37, 25), (72, 52, 34), (92, 68, 45), (110, 84, 56)]
    return H, colorize(idx, pal), 0.75, 3.0

def gen_shingle(rng):
    RH = 8; SW = 8
    row = yy // RH; ly = yy % RH
    off = (row % 2) * (SW // 2)
    col = (xx + off) // SW; lx = (xx + off) % SW
    nrows = L // RH; ncols = L // SW
    tab = rng.random((nrows, ncols))
    shade = tab[row, col % ncols]
    f = fine(rng)
    v = 0.3 + shade * 0.4 + (f - 0.5) * 0.2
    idx = quant(v, 5)
    idx = np.clip(idx, 1, 3)
    moss = fbm(L, 3, rng, 3) + (f - 0.5) * 0.1
    mossm = moss > 0.6
    mossidx = np.where(moss > 0.68, 6, 5)
    idx = np.where(mossm, mossidx, idx)
    H = 0.35 + 0.3 * (ly / (RH - 1)) + (shade - 0.5) * 0.06 + (f - 0.5) * 0.02
    H[mossm] += 0.04
    # subtle vertical seams
    vs = (lx == 0) & (ly > 0) & ~mossm
    idx[vs] = np.maximum(idx[vs] - 1, 0); H[vs] -= 0.08
    # rim highlight along the bottom edge, rounded corners
    rim = (ly == RH - 1) & (lx >= 2) & (lx <= SW - 2) & ~mossm
    idx[rim] = np.minimum(idx[rim] + 1, 4)
    corner = (ly == RH - 1) & ((lx == 1) | (lx == SW - 1) | (lx == 0))
    idx[corner] = 0; H[corner] -= 0.15
    # shadow cast by the row above
    shadow = ly == 0
    idx[shadow] = 0; H[shadow] -= 0.05
    pal = [(40, 38, 28), (62, 60, 44), (82, 80, 58), (102, 98, 70), (120, 116, 84), (66, 86, 40), (92, 116, 52)]
    return H, colorize(idx, pal), 0.9, 3.0

def gen_stone(rng):
    bounds = np.array([0, 11, 21, 32, 43, 53, 64])
    row = np.searchsorted(bounds, yy, side='right') - 1
    rtop = bounds[row]; rh = bounds[row + 1] - rtop
    ly = yy - rtop
    BW = 16
    off = (row % 2) * 8
    col = (xx + off) // BW; lx = (xx + off) % BW
    tab = rng.random((6, 4))
    shade = tab[row, col % 4]
    f = fine(rng)
    v = 0.25 + shade * 0.6 + (f - 0.5) * 0.3
    idx = quant(v, 5) + 1   # 1..5 stone shades
    H = 0.55 + (shade - 0.5) * 0.15 + (f - 0.5) * 0.04
    mortar = (ly == 0) | (lx == 0)
    hl = ((ly == 1) | (lx == 1)) & ~mortar
    sh = ((ly == rh - 1) | (lx == BW - 1)) & ~mortar
    idx[hl] = np.minimum(idx[hl] + 1, 5)
    idx[sh] = np.maximum(idx[sh] - 1, 1)
    idx[mortar] = 0; H[mortar] = 0.2
    # chips
    chip = (f < 0.02) & ~mortar
    idx[chip] = np.maximum(idx[chip] - 2, 1); H[chip] -= 0.1
    pal = [(50, 44, 36), (96, 88, 72), (120, 112, 94), (142, 134, 114), (162, 154, 132), (180, 172, 150)]
    return H, colorize(idx, pal), 0.8, 3.0

def gen_leaf(rng):
    H = np.zeros((L, L)); S = np.zeros((L, L)); cov = np.zeros((L, L), bool)
    for i in range(130):
        cx, cy = rng.integers(0, L, 2); r = rng.uniform(2.2, 4.6)
        ph = rng.uniform(0, 6.28); z = rng.uniform(0.2, 1.0)
        dx = wrapd(xx, cx); dy = wrapd(yy, cy)
        th = np.arctan2(dy, dx)
        reff = r * (1 + 0.22 * np.sin(3 * th + ph))
        d = np.sqrt(dx * dx + dy * dy) / reff
        inside = d < 1.0
        dome = np.sqrt(np.clip(1 - d * d, 0, None))
        h = z * 0.45 + dome * 0.55
        win = inside & (h > H)
        lit = np.clip(-(dx + dy) / (reff * 1.414), -1, 1)
        S[win] = (0.28 + 0.4 * z + 0.32 * lit)[win]
        H[win] = h[win]; cov |= win
    f = fine(rng)
    v = S + (f - 0.5) * 0.14
    idx = quant(v, 5)
    idx[~cov] = 0
    pal = [(28, 40, 22), (48, 70, 30), (74, 102, 42), (102, 130, 56), (132, 156, 74)]
    return H, colorize(idx, pal), 0.8, 2.5

def gen_bark(rng):
    ridges = fbm(L, 1, rng, 4, ax=11, ay=1.5)
    warp = (fbm(L, 2, rng, 2) - 0.5) * 8
    xi = ((xx + np.round(warp).astype(int)) % L)
    ridges = ridges[yy, xi]
    f = fine(rng)
    v = 0.12 + ridges * 0.9 + (f - 0.5) * 0.16
    idx = quant(v, 5)
    cracks = (f < 0.012)
    idx[cracks] = 0
    H = ridges * 0.8 + (f - 0.5) * 0.03
    H[cracks] -= 0.15
    pal = [(28, 20, 13), (46, 34, 22), (66, 50, 33), (86, 66, 45), (106, 84, 58)]
    return H, colorize(idx, pal), 0.85, 3.0

def gen_cloth(rng):
    f = fine(rng)
    weave = ((xx + yy) % 2) * 0.5 + f * 0.4
    fold = fbm(L, 2, rng, 2, ax=3, ay=1)
    band = (yy >= 24) & (yy < 40)
    v = 0.45 + (fold - 0.5) * 0.7 + (weave - 0.45) * 0.35
    idx = quant(v, 4)
    edge = (yy == 24) | (yy == 39)
    idx[edge] = np.maximum(idx[edge] - 1, 0)
    blue = [(24, 34, 72), (36, 50, 98), (48, 66, 120), (62, 84, 140)]
    cream = [(178, 162, 126), (206, 192, 154), (226, 212, 176), (240, 228, 194)]
    cb = colorize(idx, blue); cc = colorize(idx, cream)
    bc = np.where(band[..., None], cc, cb)
    H = 0.5 + (fold - 0.5) * 0.25 + (weave - 0.45) * 0.05
    return H, bc, 0.6, 3.0

def gen_soil(rng):
    RH = 16
    ly = yy % RH
    ridge = 0.5 - 0.5 * np.cos(2 * np.pi * (ly + 0.5) / RH)
    clumps = fbm(L, 8, rng, 3)
    f = fine(rng)
    H = ridge * 0.55 + (clumps - 0.5) * 0.3 + (f - 0.5) * 0.08
    v = 0.18 + ridge * 0.58 + (clumps - 0.5) * 0.35 + (f - 0.5) * 0.28
    idx = quant(v, 5)
    peb = f > 0.99
    idx[peb] = 5; H[peb] += 0.08
    pal = [(28, 20, 14), (44, 32, 22), (60, 46, 32), (76, 60, 42), (92, 74, 52), (124, 110, 90)]
    return H, colorize(idx, pal), 0.95, 3.0

def gen_metal(rng):
    base = fbm(L, 4, rng, 3)
    f = fine(rng)
    H = 0.5 + (base - 0.5) * 0.1 + (f - 0.5) * 0.03
    v = 0.4 + (base - 0.5) * 0.45 + (f - 0.5) * 0.25
    idx = quant(v, 5)
    for i in range(9):
        x0, y0 = rng.integers(0, L, 2)
        ang = rng.uniform(0, np.pi)
        ln = rng.integers(4, 14)
        ca, sa = np.cos(ang), np.sin(ang)
        for t in range(ln):
            px = int(round(x0 + ca * t)) % L; py = int(round(y0 + sa * t)) % L
            idx[py, px] = 3; H[py, px] -= 0.05
    pits = f < 0.015
    idx[pits] = 0; H[pits] -= 0.08
    pal = [(26, 26, 28), (40, 40, 44), (54, 55, 58), (68, 69, 72), (88, 90, 92)]
    return H, colorize(idx, pal), 0.45, 4.0

MATS = [
    ("T_Prop_Plaster_Pixel", gen_plaster, 101),
    ("T_Prop_WoodPlanks_Pixel", gen_planks, 102),
    ("T_Prop_Timber_Pixel", gen_timber, 103),
    ("T_Prop_RoofShingle_Pixel", gen_shingle, 104),
    ("T_Prop_StoneBrick_Pixel", gen_stone, 105),
    ("T_Prop_Leaf_Pixel", gen_leaf, 106),
    ("T_Prop_Bark_Pixel", gen_bark, 107),
    ("T_Prop_Cloth_Pixel", gen_cloth, 108),
    ("T_Prop_Soil_Pixel", gen_soil, 109),
    ("T_Prop_Metal_Pixel", gen_metal, 110),
]

def up(arr):
    return Image.fromarray(arr).resize((FINAL, FINAL), Image.NEAREST)

results = []
for name, fn, seed in MATS:
    rng = np.random.default_rng(seed)
    H, bc, rough, nstr = fn(rng)
    H = np.clip(H, 0, 1)
    n = sobel_normal(H, nstr)
    ao = ao_from_height(H)
    rr = np.clip(rough + (tile_noise(L, L, L, np.random.default_rng(seed + 500)) - 0.5) * 0.06, 0, 1)
    orm = np.stack([ao * 255, rr * 255, np.zeros_like(ao)], axis=-1)
    orm = np.clip(np.round(orm), 0, 255).astype(np.uint8)
    bci = up(bc.astype(np.uint8)); ni = up(n); oi = up(orm)
    bci.save(os.path.join(OUT, name + "_BC.png"))
    ni.save(os.path.join(OUT, name + "_N.png"))
    oi.save(os.path.join(OUT, name + "_ORM.png"))
    results.append((name, bci, ni, oi))
    print(name, "H range", round(float(H.min()), 3), round(float(H.max()), 3), "colors", len(np.unique(bc.reshape(-1, 3), axis=0)), "AO min", round(float(ao.min()), 3))

# contact sheet: BC | BC tiled 2x2 | N | ORM
C = 192; PAD = 8; LBL = 170
W = LBL + 4 * C + 5 * PAD; Hh = 30 + len(results) * (C + PAD)
sheet = Image.new("RGB", (W, Hh), (24, 24, 28))
d = ImageDraw.Draw(sheet)
d.text((10, 8), "TEXGEN prop textures - BC | BC tiled 2x2 | N | ORM", fill=(230, 230, 230))
for i, (name, bci, ni, oi) in enumerate(results):
    y = 30 + i * (C + PAD)
    d.text((10, y + C // 2 - 6), name.replace("T_Prop_", "").replace("_Pixel", ""), fill=(230, 230, 230))
    x = LBL + PAD
    sheet.paste(bci.resize((C, C), Image.NEAREST), (x, y)); x += C + PAD
    t = Image.new("RGB", (FINAL * 2, FINAL * 2))
    for ty in (0, FINAL):
        for tx in (0, FINAL):
            t.paste(bci, (tx, ty))
    sheet.paste(t.resize((C, C), Image.NEAREST), (x, y)); x += C + PAD
    sheet.paste(ni.resize((C, C), Image.NEAREST), (x, y)); x += C + PAD
    sheet.paste(oi.resize((C, C), Image.NEAREST), (x, y))
sheet.save(os.path.join(OUT, "_contact.png"))
print("sheet", sheet.size)
