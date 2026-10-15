"""1080p HD-2D battle-scene preview (party LEFT, boss RIGHT) with bloom, rendered to MP4."""
import json, math, os, subprocess, sys, random
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy.ndimage import gaussian_filter, zoom

SW, SH, FPS = 1920, 1080, 30
SHEETS = "out/sheets"
UP = "/root/.claude/uploads/1531e53f-362c-592b-aa7c-5f38d2c6e7df/"
FONT_B = ImageFont.truetype("/usr/share/fonts/opentype/noto/NotoSansCJK-Black.ttc", 40, index=1)
FONT_S = ImageFont.truetype("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 24, index=1)
f32 = np.float32


# ---------------------------------------------------------------- assets
def load_sheet(name):
    meta = json.load(open(f"{SHEETS}/{name}.json"))
    sh = Image.open(f"{SHEETS}/{name}.png")
    w, h, sc = meta["frame_w"], meta["frame_h"], meta["screen_scale_1080p"]
    tw, th = int(round(w * sc)), int(round(h * sc))
    frames = []
    for i in range(meta["frames"]):
        c, r = i % meta["cols"], i // meta["cols"]
        im = sh.crop((c * w, r * h, c * w + w, r * h + h)).resize((tw, th), Image.BICUBIC)
        a = np.asarray(im).astype(f32) / 255
        frames.append(a)
    meta["piv_screen"] = (meta["pivot_px"][0] * sc, meta["pivot_px"][1] * sc)
    meta["data"] = frames
    return meta


_SH = {}


def sheet(name):
    if name not in _SH:
        _SH[name] = load_sheet(name)
    return _SH[name]


def make_boss(h_native=260, scale=2):
    im = Image.open(UP + "ec4c2d59-image.png").convert("RGB").crop((0, 0, 767, 584))
    a = np.asarray(im).astype(int)
    H, W, _ = a.shape
    sat = a.max(2) - a.min(2); val = a.mean(2)
    bgish = (sat < 14) & (val > 95) & (val < 175)
    from collections import deque
    mask = np.zeros((H, W), bool)
    q = deque([(0, x) for x in range(W)] + [(H - 1, x) for x in range(W)] + [(y, 0) for y in range(H)] + [(y, W - 1) for y in range(H)])
    while q:
        y, x = q.popleft()
        if 0 <= y < H and 0 <= x < W and not mask[y, x] and bgish[y, x]:
            mask[y, x] = True
            q.extend(((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)))
    rgba = Image.fromarray(np.dstack([a.astype(np.uint8), np.where(mask, 0, 255).astype(np.uint8)]), "RGBA")
    wn = round(W * h_native / H)
    small = rgba.resize((wn, h_native), Image.BOX)
    sa = np.asarray(small).copy()
    sa[..., 3] = np.where(sa[..., 3] > 128, 255, 0)
    rgb = Image.fromarray(sa[..., :3]).quantize(96, method=Image.Quantize.MEDIANCUT).convert("RGB")
    out = Image.fromarray(np.dstack([np.asarray(rgb), sa[..., 3]]), "RGBA")
    out = out.resize((wn * scale, h_native * scale), Image.NEAREST)
    return np.asarray(out).astype(f32) / 255, (W, H)


def make_heroes(scale=2.5):
    g = Image.open(UP + "5fceb674-image.gif")
    frames = []
    for i in range(g.n_frames):
        g.seek(i)
        im = g.convert("RGBA")
        frames.append(np.asarray(im.resize((round(im.width * scale), round(im.height * scale)), Image.NEAREST)).astype(f32) / 255)
    return frames


# ---------------------------------------------------------------- background
def noise(shape, sigma, seed):
    rng = np.random.default_rng(seed)
    n = gaussian_filter(rng.random(shape).astype(f32), sigma)
    return (n - n.min()) / (n.max() - n.min())


def make_bg():
    """Forest clearing seen over the party's shoulders: boss deep in frame, party in the foreground."""
    path = "out/bg_cache.npy"
    if os.path.exists(path):
        return np.load(path)
    rng = random.Random(9)
    Y, X = np.mgrid[0:SH, 0:SW].astype(f32)
    HOR = 360
    img = np.zeros((SH, SW, 3), f32)
    t = np.clip(Y / HOR, 0, 1)[..., None]
    img[:] = np.array((0.14, 0.25, 0.22), f32) * (1 - t) + np.array((0.10, 0.19, 0.15), f32) * t
    img += np.exp(-(((X - 900) / 600) ** 2 + ((Y - 250) / 220) ** 2))[..., None] * np.array((0.24, 0.24, 0.12), f32)
    # far forest wall
    for (n, wmin, wmax, col, blur, base, seed) in ((34, 14, 34, (0.13, 0.22, 0.19), 5, HOR + 8, 11),
                                                   (18, 26, 60, (0.09, 0.16, 0.13), 3, HOR + 22, 12)):
        lay = Image.new("RGBA", (SW, SH), (0, 0, 0, 0)); d = ImageDraw.Draw(lay)
        r = random.Random(seed)
        c = tuple(int(v * 255) for v in col)
        for i in range(n):
            x = r.randint(-60, SW + 60); w = r.randint(wmin, wmax)
            d.polygon([(x, -20), (x + w, -20), (x + w * 1.15, base), (x - w * .15, base)], fill=c + (255,))
            d.polygon([(x - w * .5, base), (x + w * 1.5, base), (x + w * .9, base - w * .7), (x + w * .1, base - w * .7)], fill=c + (255,))
        for i in range(n * 2):
            x, y, rr = r.randint(-100, SW + 100), r.randint(-160, 90), r.randint(90, 200)
            cc = tuple(int(v * r.uniform(.85, 1.2)) for v in c)
            d.ellipse((x - rr, y - rr * .6, x + rr, y + rr * .6), fill=cc + (255,))
        lay = lay.filter(ImageFilter.GaussianBlur(blur))
        la = np.asarray(lay).astype(f32) / 255
        img = img * (1 - la[..., 3:]) + la[..., :3] * la[..., 3:]
    # bushes ringing the clearing
    mid = Image.new("RGBA", (SW, SH), (0, 0, 0, 0)); d = ImageDraw.Draw(mid)
    for i in range(80):
        x = rng.randint(-60, SW + 60); r_ = rng.randint(30, 90); y = HOR + rng.randint(-6, 26)
        d.ellipse((x - r_, y - r_ * .8, x + r_, y + r_ * .3), fill=rng.choice([(28, 54, 32), (36, 66, 36), (22, 44, 26)]) + (255,))
    mid = mid.filter(ImageFilter.GaussianBlur(2))
    ma = np.asarray(mid).astype(f32) / 255
    img = img * (1 - ma[..., 3:]) + ma[..., :3] * ma[..., 3:]
    # perspective ground: sample a planar noise texture with 1/z mapping
    from scipy.ndimage import map_coordinates
    tex1 = noise((512, 512), 3, 21); tex2 = noise((512, 512), 18, 22)
    zz = np.maximum(Y - HOR + 30, 1)
    gv = 9000.0 / zz                      # depth
    gu = (X - SW / 2) * gv / 900.0
    s1 = map_coordinates(tex1, [gv * .9 % 512, (gu * .9 + 256) % 512], order=1, mode="wrap")
    s2 = map_coordinates(tex2, [gv * .35 % 512, (gu * .35 + 256) % 512], order=1, mode="wrap")
    near = np.clip((Y - HOR) / (SH - HOR), 0, 1)
    grass = np.array((0.18, 0.31, 0.14), f32) * (0.78 + 0.4 * s2[..., None]) + (s1[..., None] - .5) * .07
    # clearing (ground-plane ellipse between the boss and the party)
    du, dv = gu / 9.0, (gv - 16.0) / 9.0
    cq = du ** 2 + (dv * 1.0) ** 2
    clear = sstep_np(1.15, .75, cq + (s2 - .5) * .7)[..., None] * (Y > HOR)[..., None]
    dirt = np.array((0.32, 0.27, 0.19), f32) * (0.85 + 0.3 * s2[..., None]) + (s1[..., None] - .5) * .05
    ground = grass * (1 - clear * .8) + dirt * clear * .8
    ground = ground * (0.62 + 0.45 * near[..., None])   # darker far away
    fog = np.clip(1 - near * 2.2, 0, 1)[..., None] * .35
    ground = ground * (1 - fog) + np.array((0.16, 0.25, 0.21), f32) * fog
    gmask = sstep_np(HOR + 2, HOR + 22, Y)[..., None]
    img = img * (1 - gmask) + ground * gmask
    # grass tufts sized by depth
    tuft = Image.new("RGBA", (SW, SH), (0, 0, 0, 0)); d = ImageDraw.Draw(tuft)
    for i in range(3200):
        y = HOR + 12 + (SH - HOR) * rng.random() ** .7
        x = rng.randint(0, SW)
        zz_ = 9000.0 / (y - HOR + 30); uu = (x - SW / 2) * zz_ / 900.0
        if (uu / 9) ** 2 + ((zz_ - 16) / 9) ** 2 < .8 and rng.random() < .9:
            continue
        sz = 1.5 + (y - HOR) / (SH - HOR) * 9
        c = rng.choice([(62, 98, 40), (48, 80, 34), (82, 120, 52)])
        for k in (-1, 0, 1):
            d.line((x, y, x + k * sz * .55, y - sz * 1.7), fill=c + (255,), width=max(1, int(sz / 3)))
    ta = np.asarray(tuft).astype(f32) / 255
    img = img * (1 - ta[..., 3:]) + ta[..., :3] * ta[..., 3:]
    # nearer framing trunks left & right (mid-depth)
    side = Image.new("RGBA", (SW, SH), (0, 0, 0, 0)); d = ImageDraw.Draw(side)
    for (x, w, base) in ((70, 120, 760), (250, 60, 560), (1700, 150, 820), (1560, 56, 540)):
        d.polygon([(x, -20), (x + w, -20), (x + w * 1.1, base), (x - w * .1, base)], fill=(16, 30, 22, 255))
        d.polygon([(x - w * .6, base), (x + w * 1.6, base), (x + w * .95, base - w * .9), (x + w * .05, base - w * .9)], fill=(16, 30, 22, 255))
    side = side.filter(ImageFilter.GaussianBlur(2.5))
    sa_ = np.asarray(side).astype(f32) / 255
    img = img * (1 - sa_[..., 3:]) + sa_[..., :3] * sa_[..., 3:]
    img *= 0.8 + 0.2 * np.clip((Y - 200) / 500, 0, 1)[..., None]
    # light shafts
    rays = np.zeros((SH, SW), f32)
    for i in range(7):
        x0 = rng.randint(200, 1500); w = rng.randint(40, 140)
        u = (X - x0) - Y * 0.38
        rays += np.exp(-(u / w) ** 2) * rng.uniform(.25, .6)
    rays *= np.clip(1 - Y / 1100, 0, 1)
    img += rays[..., None] * np.array((0.20, 0.20, 0.11), f32)
    # foreground foliage (heavy DOF)
    fg = Image.new("RGBA", (SW, SH), (0, 0, 0, 0)); d = ImageDraw.Draw(fg)
    for (cx, cy, n) in ((-30, 1080, 16), (1950, 1070, 16), (-40, -20, 10), (1960, -30, 10)):
        for k in range(n):
            x = cx + rng.randint(-200, 200); y = cy + rng.randint(-130, 80); r_ = rng.randint(50, 130)
            d.ellipse((x - r_, y - r_ * .6, x + r_, y + r_ * .6), fill=rng.choice([(8, 18, 12), (14, 28, 18)]) + (240,))
    fg = fg.filter(ImageFilter.GaussianBlur(14))
    np.save("out/fg_cache.npy", np.asarray(fg).astype(f32) / 255)
    np.save(path, img)
    return img


def sstep_np(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------- compositing helpers
def comp_over(dst, src, x, y, alpha=1.0):
    h, w = src.shape[:2]
    x0, y0 = int(round(x)), int(round(y))
    sx0, sy0 = max(0, -x0), max(0, -y0)
    ex, ey = min(w, SW - x0), min(h, SH - y0)
    if ex <= sx0 or ey <= sy0:
        return
    s = src[sy0:ey, sx0:ex]
    a = s[..., 3:] * alpha
    d = dst[y0 + sy0:y0 + ey, x0 + sx0:x0 + ex]
    d[:] = d * (1 - a) + s[..., :3] * a


def comp_add(dst, src, x, y, gain=1.0):
    h, w = src.shape[:2]
    x0, y0 = int(round(x)), int(round(y))
    sx0, sy0 = max(0, -x0), max(0, -y0)
    ex, ey = min(w, SW - x0), min(h, SH - y0)
    if ex <= sx0 or ey <= sy0:
        return
    s = src[sy0:ey, sx0:ex]
    dst[y0 + sy0:y0 + ey, x0 + sx0:x0 + ex] += s[..., :3] * s[..., 3:] * gain


def shadow(dst, cx, cy, rx, ry, k=.45):
    x0, x1 = max(0, int(cx - rx * 1.4)), min(SW, int(cx + rx * 1.4)); y0, y1 = max(0, int(cy - ry * 1.4)), min(SH, int(cy + ry * 1.4))
    Y, X = np.mgrid[y0:y1, x0:x1].astype(f32)
    m = np.exp(-(((X - cx) / rx) ** 2 + ((Y - cy) / ry) ** 2) * 1.8) * k
    dst[y0:y1, x0:x1] *= (1 - m[..., None])


def post(img, shake=(0, 0)):
    img = img * (1 - FG[..., 3:]) + FG[..., :3] * FG[..., 3:]
    # bloom at quarter res
    small = img[::4, ::4]
    br = np.maximum(small - 0.72, 0)
    b1 = gaussian_filter(br, (3, 3, 0)); b2 = gaussian_filter(br, (10, 10, 0))
    bloom = zoom(b1 * .9 + b2 * .9, (4, 4, 1), order=1)
    out = img + bloom[:SH, :SW]
    # grade: lift shadows to teal, warm highlights
    lum = out.mean(2, keepdims=True)
    out = out + (1 - np.clip(lum * 2, 0, 1)) * np.array((-0.01, 0.015, 0.02), f32) + np.clip(lum - .6, 0, 1) * np.array((0.04, 0.02, -0.03), f32)
    out = 1 - np.exp(-np.maximum(out, 0) * 1.25)  # filmic-ish
    out = out / (1 - math.exp(-1.25))
    out *= VIG
    if shake != (0, 0):
        out = np.roll(out, shake, axis=(0, 1))
    return np.clip(out, 0, 1)


Yv, Xv = np.mgrid[0:SH, 0:SW].astype(f32)
VIG = (1 - 0.38 * (((Xv - SW / 2) / (SW * .62)) ** 2 + ((Yv - SH / 2) / (SH * .7)) ** 2) ** 1.4).clip(.45, 1)[..., None]
del Yv, Xv


# ---------------------------------------------------------------- scene
BG = make_bg()
FG = np.load("out/fg_cache.npy")
BOSS, BOSS_SRC = make_boss()
HEROES = make_heroes()
BOSS_W, BOSS_H = BOSS.shape[1], BOSS.shape[0]
BOSS_FEET = (1090, 650)
BOSS_X0, BOSS_Y0 = BOSS_FEET[0] - BOSS_W // 2, BOSS_FEET[1] - BOSS_H
BS = BOSS_W / BOSS_SRC[0]
KNOT = (BOSS_X0 + 65 * BS, BOSS_Y0 + 292 * BS)
PARTY = [(1150, 924), (860, 960), (570, 922)]   # front row, left->right order of being hit: P0 first
PARTY_C = (860, 934)
CHEST = [(x - 4, y - 132) for x, y in PARTY]

AMB = [dict(x=random.uniform(0, SW), y=random.uniform(150, 1000), s=random.uniform(.2, .6), o=random.uniform(0, 6)) for _ in range(40)]


def draw_scene(t, boss_dy=0.0):
    img = BG.copy()
    shadow(img, BOSS_FEET[0] - 10, BOSS_FEET[1] - 8, 320, 60, .55)
    for (x, y) in PARTY:
        shadow(img, x, y - 2, 60, 16, .55)
    return img


def draw_actors(img, t, boss_dy=0.0, hero_flash=None):
    bob = math.sin(t * 2 * math.pi * .5) * 3 + boss_dy
    comp_over(img, BOSS, BOSS_X0, BOSS_Y0 + bob)
    for i, (x, y) in sorted(enumerate(PARTY), key=lambda e: e[1][1]):
        fr = HEROES[int(t / 0.1 + i * 3) % len(HEROES)]
        h, w = fr.shape[:2]
        comp_over(img, fr, x - w / 2, y - h + 2)
        if hero_flash and hero_flash.get(i, 0) > 0:
            m = fr[..., 3:] * hero_flash[i]
            sub = img[int(y - h + 2):int(y + 2), int(x - w / 2):int(x - w / 2) + w]
            sub += m[:sub.shape[0], :sub.shape[1]] * np.array((1.0, 1.0, .9), f32)


def ambient(img, t):
    for p in AMB:
        x = (p['x'] + t * 12 * p['s']) % SW
        y = p['y'] - (t * 20 * p['s']) % 300 + math.sin(t + p['o']) * 8
        xi, yi = int(x), int(y)
        if 3 <= xi < SW - 3 and 3 <= yi < SH - 3:
            img[yi - 1:yi + 2, xi - 1:xi + 2] += np.array((.35, .38, .22), f32) * (.5 + .5 * math.sin(t * 2 + p['o']))


def fx(img, name, idx, anchor, mode=None, gain=1.0, alpha=1.0):
    sh = sheet(name)
    if idx < 0 or idx >= sh["frames"]:
        return
    fr = sh["data"][idx]
    x, y = anchor[0] - sh["piv_screen"][0], anchor[1] - sh["piv_screen"][1]
    if (mode or sh["blend"]) == "Additive":
        comp_add(img, fr, x, y, gain)
    else:
        comp_over(img, fr, x, y, alpha)


def fidx(name, t, loop=False):
    sh = sheet(name)
    i = int(math.floor(t * sh["fps"] + 1e-6))
    if loop:
        return i % sh["frames"]
    return i if t >= 0 else -1


def ui(pil, title, sub, banner=None, alpha=1.0):
    d = ImageDraw.Draw(pil, "RGBA")
    d.rectangle((0, 40, 760, 112), fill=(10, 14, 12, int(170 * alpha)))
    d.rectangle((0, 40, 8, 112), fill=(236, 196, 92, int(255 * alpha)))
    d.text((34, 46), title, font=FONT_B, fill=(250, 240, 214, int(255 * alpha)))
    w = d.textlength(title, font=FONT_B)
    d.text((34 + w + 24, 62), sub, font=FONT_S, fill=(196, 222, 150, int(255 * alpha)))
    if banner:
        bw = d.textlength(banner, font=FONT_B) + 90
        x0 = (SW - bw) / 2
        d.rectangle((x0, 150, x0 + bw, 222), fill=(80, 16, 12, 200))
        d.rectangle((x0, 150, x0 + bw, 222), outline=(255, 170, 70, 255), width=3)
        d.text((x0 + 45, 156), banner, font=FONT_B, fill=(255, 236, 200, 255))


class Writer:
    def __init__(self, path):
        self.p = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24",
                                   "-s", f"{SW}x{SH}", "-r", str(FPS), "-i", "-", "-c:v", "libx264", "-preset", "medium",
                                   "-crf", "19", "-pix_fmt", "yuv420p", "-movflags", "+faststart", path], stdin=subprocess.PIPE)
        self.stills = []

    def write(self, rgbf, ui_args=None, keep=False):
        im = Image.fromarray((rgbf * 255 + .5).astype(np.uint8))
        if ui_args:
            ui(im, *ui_args)
        self.p.stdin.write(im.tobytes())
        if keep:
            self.stills.append(im.copy())

    def close(self):
        self.p.stdin.close(); self.p.wait()


def shake_at(ts, amp=10):
    if 0 <= ts < .35:
        k = (1 - ts / .35) ** 2
        return (int(math.cos(ts * 95) * amp * .6 * k), int(math.sin(ts * 120) * amp * k))
    return (0, 0)


def frange(dur):
    return [i / FPS for i in range(int(round(dur * FPS)))]


# ---------------------------------------------------------------- pixel debris particles
import boss_debris as BD
_DEB = BD.build_sprites()
DEB = []
for d_ in _DEB:
    im = Image.fromarray(d_["px"], "RGBA")
    big = im.resize((im.width * 2, im.height * 2), Image.NEAREST)          # same 2x density as the boss
    DEB.append([np.asarray(big).astype(f32) / 255, np.asarray(big.transpose(Image.FLIP_LEFT_RIGHT)).astype(f32) / 255])
CLODS, CHUNKS = [0, 1, 2, 3], list(range(4, 16))
GRAV = 2600.0


def draw_deb(img, idx, x, y, flip=0, alpha=1.0):
    spr = DEB[idx][flip]
    comp_over(img, spr, x - spr.shape[1] / 2, y - spr.shape[0] / 2, alpha)


class Debris:
    def __init__(self, seed):
        self.ps = []
        self.rng = random.Random(seed)

    def burst(self, x, y_ground, n, spd=(150, 520), up=(380, 900), sizes=CHUNKS[2:], t0=0.0):
        for _ in range(n):
            a = self.rng.uniform(0, 2 * math.pi)
            self.ps.append(dict(x=x + self.rng.uniform(-20, 20), y=y_ground - 10, vx=math.cos(a) * self.rng.uniform(*spd),
                                vy=-self.rng.uniform(*up), g=y_ground + math.sin(a) * 40 * .38 + self.rng.uniform(-8, 8),
                                idx=self.rng.choice(sizes), flip=self.rng.randint(0, 1), born=t0, bounced=False, rest=None))

    def step_draw(self, img, t, dt):
        for p in self.ps:
            if t < p['born']:
                continue
            if p['rest'] is None:
                p['vy'] += GRAV * dt
                p['x'] += p['vx'] * dt; p['y'] += p['vy'] * dt
                if p['y'] >= p['g'] and p['vy'] > 0:
                    p['y'] = p['g']
                    if not p['bounced'] and p['vy'] > 300:
                        p['vy'] *= -.32; p['vx'] *= .5; p['bounced'] = True
                    else:
                        p['vy'] = 0; p['vx'] = 0; p['rest'] = t
            age_rest = 0 if p['rest'] is None else t - p['rest']
            a = 1 - max(0, age_rest - .6) / .5
            if a > 0:
                draw_deb(img, p['idx'], p['x'], p['y'], p['flip'], a)


def speed_lines(img, x, y_top, w, alpha=.45, length=90):
    for k, ox in enumerate((-w * .3, 0, w * .32)):
        x0 = int(x + ox); y1 = int(y_top - 8 - k * 10); y0 = int(y1 - length * (.7 + .3 * (k % 2)))
        if x0 < 2 or x0 > SW - 4:
            continue
        y0c, y1c = max(0, y0), min(SH, y1)
        if y1c > y0c:
            img[y0c:y1c, x0:x0 + 4] = img[y0c:y1c, x0:x0 + 4] * (1 - alpha) + np.array((0.92, 0.9, 0.8), f32) * alpha


# ---------------------------------------------------------------- sequences
def seq_stomp(w):
    T = "앞발 짓누르기"; S = "강공격 · 위력 100 / 경직 24"
    t0 = 0
    for t in frange(0.6):
        img = draw_scene(t); draw_actors(img, t); ambient(img, t)
        w.write(post(img), (T, S, None), keep=(t == 0.3))
    for t in frange(2.0):
        tt = t + .6
        img = draw_scene(tt)
        fx(img, "T_Boss2_Stomp_Telegraph", fidx("T_Boss2_Stomp_Telegraph", t, True), PARTY_C, gain=1.0)
        draw_actors(img, tt, boss_dy=-ease(t / 2.0) * 14); ambient(img, tt)
        w.write(post(img), (T, "예비 동작 1턴 — 넓은 판정", None), keep=abs(t - 1.0) < 1e-6)
    deb = Debris(3)
    deb.burst(PARTY_C[0], PARTY_C[1], 14, spd=(120, 480), up=(420, 860))
    for t in frange(1.6):
        tt = t + 2.6
        img = draw_scene(tt)
        fx(img, "T_Boss2_Stomp_Impact_Glow", fidx("T_Boss2_Stomp_Impact_Glow", t), PARTY_C, gain=1.0)
        draw_actors(img, tt, boss_dy=-14 * max(0, 1 - t * 8), hero_flash=None)
        deb.step_draw(img, t, 1 / FPS)
        fx(img, "T_Boss2_Stomp_Impact_Dust", fidx("T_Boss2_Stomp_Impact_Dust", t), PARTY_C)
        fx(img, "T_Boss2_Stomp_Impact_Glow", fidx("T_Boss2_Stomp_Impact_Glow", t), PARTY_C, gain=.35)
        ambient(img, tt)
        w.write(post(img, shake_at(t, 14)), (T, S, None), keep=any(abs(t - k) < 1e-6 for k in (2 / 30, 6 / 30, 14 / 30)))


def ease(x):
    x = min(max(x, 0), 1)
    return x * x * (3 - 2 * x)


SW_ANCHOR = (1000, 520)
HIT_FR = (5, 6, 7)   # swing frame when the root passes P0 (right), P1, P2


def _sw_tip(f):
    import fx as F
    a = math.radians(F.SW_HEAD[f])
    return (SW_ANCHOR[0] + F.SW_RX * 2 * math.cos(a), SW_ANCHOR[1] + F.SW_RY * 2 * math.sin(a))


def seq_root(w):
    T = "뿌리 후려치기"
    tip = _sw_tip(4)
    kept = set()
    for rep, (speed, sub) in enumerate(((1.0, "강공격 · 140 / 34 — 패링 신호 → 피격"), (0.35, "슬로우 x0.35 — 패링 타이밍"))):
        for t in frange(0.5):
            img = draw_scene(t); draw_actors(img, t); ambient(img, t)
            w.write(post(img), (T, sub, None))
        for t in frange(0.6 / speed + 0.8):
            tt = t * speed
            img = draw_scene(t + 1)
            hits = [tt - k / 30 for k in HIT_FR]
            draw_actors(img, t + 1, hero_flash={i: max(0, .45 - h * 3) for i, h in enumerate(hits) if 0 <= h < .15})
            hit_t = hits[0]
            fx(img, "T_Boss2_RootLash_Swing", fidx("T_Boss2_RootLash_Swing", tt), SW_ANCHOR)
            fx(img, "T_Boss2_RootLash_Leaves", fidx("T_Boss2_RootLash_Leaves", tt), SW_ANCHOR)
            fx(img, "T_Boss2_ParryCue", fidx("T_Boss2_ParryCue", tt + 1.0 / 30), tip)
            for i, h in enumerate(hits):
                fx(img, "T_Boss2_RootLash_Hit", fidx("T_Boss2_RootLash_Hit", h), CHEST[i], gain=.7)
            ambient(img, t)
            fk = int(tt * 30)
            keep = rep == 1 and fk in (2, 4, 6, 9) and fk not in kept
            if keep:
                kept.add(fk)
            w.write(post(img, shake_at(hit_t, 8)), (T, sub, None), keep=keep)


def seq_knot(w):
    T = "옹이 잔광"; S = "패링 불가 · 회피 / 매직 실드로만 대응"
    for t in frange(2.0):
        img = draw_scene(t); draw_actors(img, t)
        fx(img, "T_Boss2_KnotGlow_Charge", fidx("T_Boss2_KnotGlow_Charge", t, True), KNOT, gain=min(1, t * 2))
        ambient(img, t)
        w.write(post(img), (T, S, None), keep=abs(t - 1.2) < 1e-6)
    for t in frange(1.4):
        img = draw_scene(t + 2); draw_actors(img, t + 2, hero_flash=None)
        if t < .2:
            fx(img, "T_Boss2_KnotGlow_Charge", fidx("T_Boss2_KnotGlow_Charge", t, True), KNOT, gain=1 - t * 5)
        fx(img, "T_Boss2_KnotGlow_Push", fidx("T_Boss2_KnotGlow_Push", t), KNOT)
        ambient(img, t)
        w.write(post(img), (T, "120 / 29", None), keep=any(abs(t - k / 30) < 1e-6 for k in (2, 6, 10)))


def seq_hill(w):
    import fx as F
    T = "언덕 무너뜨리기"
    rng = random.Random(8)
    peb = [dict(x=PARTY_C[0] + rng.uniform(-330, 330), y=PARTY_C[1] + rng.uniform(-80, 80), idx=rng.choice(CHUNKS[5:]),
                flip=rng.randint(0, 1), o=rng.uniform(0, 1)) for _ in range(14)]
    for t in frange(2.0):
        img = draw_scene(t)
        sh_ = (int(math.sin(t * 70) * 2), int(math.cos(t * 50) * 2))
        fx(img, "T_Boss2_HillCollapse_Telegraph1_Dust", fidx("T_Boss2_HillCollapse_Telegraph1_Dust", t, True), PARTY_C)
        fx(img, "T_Boss2_HillCollapse_Telegraph1_Glow", fidx("T_Boss2_HillCollapse_Telegraph1_Glow", t, True), PARTY_C)
        for p in peb:   # pebbles hopping on the trembling ground
            hop = max(0, math.sin((t * 2.2 + p['o']) * 2 * math.pi)) ** 3 * 14
            draw_deb(img, p['idx'], p['x'] + (2 if int(t * 30) % 2 else -2), p['y'] - hop, p['flip'])
        draw_actors(img, t); ambient(img, t)
        w.write(post(img, sh_), (T, "예비 동작 1/2턴", "대지가 흔들린다…"), keep=abs(t - 1.0) < 1e-6)
    falls = [dict(x=PARTY_C[0] + rng.uniform(-420, 420), gy=PARTY_C[1] + rng.uniform(-70, 70), o=rng.uniform(0, 1),
                  sp=rng.uniform(.8, 1.3), idx=rng.choice(CHUNKS[3:]), flip=rng.randint(0, 1)) for _ in range(12)]
    for t in frange(2.2):
        img = draw_scene(t + 2)
        sh_ = (int(math.sin(t * 80) * 4), int(math.cos(t * 60) * 4))
        fx(img, "T_Boss2_HillCollapse_Telegraph2_Dust", fidx("T_Boss2_HillCollapse_Telegraph2_Dust", t, True), PARTY_C)
        fx(img, "T_Boss2_HillCollapse_Telegraph2_Glow", fidx("T_Boss2_HillCollapse_Telegraph2_Glow", t, True), PARTY_C)
        draw_actors(img, t + 2, boss_dy=-ease(t / 2.2) * 20)
        for p in falls:   # pebbles raining down
            ph = (t * p['sp'] + p['o']) % 1
            y = -40 + ph * ph * (p['gy'] + 40)
            if ph < .97:
                draw_deb(img, p['idx'], p['x'], y, p['flip'])
                if ph > .3:
                    speed_lines(img, p['x'], y - 12, 12, alpha=.25, length=40)
        ambient(img, t)
        w.write(post(img, sh_), (T, "예비 동작 2/2턴", "언덕이 무너진다 — 방어하라!"), keep=abs(t - 1.2) < 1e-6)
    # falling hill clods (particles) — timing shared with the flipbook (24 fps landing frames)
    sc = sheet("T_Boss2_HillCollapse_Impact_Dust")["screen_scale_1080p"]
    clods = []
    for i, (xf, rr, lf, ai) in enumerate(F.HILL_SPEC):
        Tland = lf / 24
        gy = PARTY_C[1] + (0 if i == 0 else rng.uniform(-30, 30))
        spr = DEB[ai if ai < 4 else CHUNKS[0]][0]
        clods.append(dict(x=PARTY_C[0] + (xf - 256) * sc, gy=gy, T=Tland, idx=ai if ai < 4 else CHUNKS[rng.randint(0, 1)],
                          flip=rng.randint(0, 1), h=spr.shape[0], wdt=spr.shape[1], big=i == 0, done=False))
    deb = Debris(11)
    crumbs = Debris(12)
    for t in frange(2.0):
        img = draw_scene(t + 4)
        land = clods[0]['T']
        fx(img, "T_Boss2_HillCollapse_Impact_Glow", fidx("T_Boss2_HillCollapse_Impact_Glow", t), PARTY_C, gain=.8)
        draw_actors(img, t + 4, boss_dy=-20 * max(0, 1 - t * 4))
        deb.step_draw(img, t, 1 / FPS)
        crumbs.step_draw(img, t, 1 / FPS)
        for c in clods:
            yl = c['gy'] - c['h'] * .35            # sprite centre when resting on the ground
            if t < c['T']:
                y0 = -c['h'] * .8
                y = y0 + (yl - y0) * (t / c['T']) ** 1.8
                speed_lines(img, c['x'], y - c['h'] * .5, c['wdt'], alpha=.4, length=60 + c['h'] * .6)
                draw_deb(img, c['idx'], c['x'], y, c['flip'])
                if y > 260 and random.random() < .35:
                    crumbs.ps.append(dict(x=c['x'] + random.uniform(-c['wdt'] * .3, c['wdt'] * .3), y=y + c['h'] * .3,
                                          vx=random.uniform(-40, 40), vy=random.uniform(100, 300), g=c['gy'] + random.uniform(-20, 20),
                                          idx=random.choice(CHUNKS[10:]), flip=0, born=t, bounced=True, rest=None))
            else:
                if not c['done']:
                    c['done'] = True
                    n = 16 if c['big'] else 7
                    deb.burst(c['x'], c['gy'], n, spd=(160, 560 if c['big'] else 360), up=(500, 1100 if c['big'] else 760),
                              sizes=CHUNKS[:8] if c['big'] else CHUNKS[4:], t0=t)
                age = t - c['T']
                if age < .25:   # broken clod sinks into the crater, then gone
                    draw_deb(img, c['idx'], c['x'], yl + age * 120, c['flip'], 1 - age / .25)
        fx(img, "T_Boss2_HillCollapse_Impact_Dust", fidx("T_Boss2_HillCollapse_Impact_Dust", t), PARTY_C)
        fx(img, "T_Boss2_HillCollapse_Impact_Glow", fidx("T_Boss2_HillCollapse_Impact_Glow", t), PARTY_C, gain=.2)
        ambient(img, t)
        w.write(post(img, shake_at(t - land, 22)), (T, "강공격 · 200 / 48", None),
                keep=any(abs(t - k / 30) < .5 / 30 for k in (5, 8, 11, 18)))


SEQS = {"1_Stomp": seq_stomp, "2_RootLash": seq_root, "3_KnotGlow": seq_knot, "4_HillCollapse": seq_hill}

if __name__ == "__main__":
    out = sys.argv[1]
    which = sys.argv[2:] or list(SEQS)
    os.makedirs(out, exist_ok=True)
    for k in which:
        w = Writer(f"{out}/Preview_{k}.mp4")
        SEQS[k](w)
        w.close()
        if w.stills:
            th = [s.resize((960, 540), Image.LANCZOS) for s in w.stills[:4]]
            cols = 2 if len(th) > 1 else 1
            rows = math.ceil(len(th) / cols)
            cs = Image.new("RGB", (960 * cols, 540 * rows))
            for i, s in enumerate(th):
                cs.paste(s, ((i % cols) * 960, (i // cols) * 540))
            cs.save(f"{out}/Still_{k}.jpg", quality=90)
        print("done", k, flush=True)
