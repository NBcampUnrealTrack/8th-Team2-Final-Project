"""HD-2D style VFX renderer: float additive emission layer + straight-alpha translucent layer."""
import math, os, json, random
import numpy as np
from scipy.ndimage import gaussian_filter, map_coordinates
from PIL import Image

f32 = np.float32
GROUND_K = 0.36  # ground-plane ellipse squash (camera looks over the party toward the boss)

# ---------------- colour ramps (linear-ish 0..1, HDR allowed for emission) ----------------
LIFE = [(0.16, 0.55, 0.10), (0.48, 0.92, 0.16), (0.86, 1.00, 0.30), (1.00, 1.00, 0.70), (1.0, 1.0, 0.95)]  # knot / root
EARTH = [(0.70, 0.20, 0.03), (1.00, 0.46, 0.08), (1.00, 0.72, 0.24), (1.00, 0.90, 0.58), (1.0, 1.0, 0.92)]  # stomp / hill
DUST = dict(shadow=(0.40, 0.33, 0.27), mid=(0.58, 0.49, 0.39), light=(0.78, 0.69, 0.55), rim=(0.92, 0.86, 0.72))
LEAF = [(0.20, 0.33, 0.12), (0.36, 0.53, 0.17), (0.56, 0.73, 0.24), (0.76, 0.86, 0.38)]


def ramp(stops, t):
    """t: array 0..1 -> (...,3) colour along stops."""
    t = np.clip(t, 0, 1) * (len(stops) - 1)
    i = np.minimum(t.astype(int), len(stops) - 2)
    fr = (t - i)[..., None]
    s = np.array(stops, f32)
    return s[i] * (1 - fr) + s[i + 1] * fr


def sm(x, w=1.0):
    """coverage from signed distance (positive inside), AA width w px."""
    return np.clip(x / w + 0.5, 0, 1)


def sstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def ease_out(t, p=2):
    t = min(max(t, 0.0), 1.0)
    return 1 - (1 - t) ** p


def ease_in(t, p=2):
    t = min(max(t, 0.0), 1.0)
    return t ** p


# ---------------- tileable noise ----------------
_NOISE = {}


def noise_tex(seed=0, size=256, sigma=6):
    k = (seed, size, sigma)
    if k not in _NOISE:
        rng = np.random.default_rng(seed)
        n = np.zeros((size, size), f32)
        for s, w in ((sigma, .55), (sigma / 2, .3), (sigma / 4, .15)):
            g = gaussian_filter(rng.random((size, size)).astype(f32), s, mode="wrap")
            g = (g - g.min()) / (g.max() - g.min() + 1e-6)
            n += g * w
        _NOISE[k] = (n - n.min()) / (n.max() - n.min())
    return _NOISE[k]


def sample_noise(X, Y, scale=1.0, ox=0.0, oy=0.0, seed=0, sigma=6):
    n = noise_tex(seed, 256, sigma)
    return map_coordinates(n, [(Y * scale + oy) % 256, (X * scale + ox) % 256], order=1, mode="wrap")


# ---------------- frame ----------------
_GRID = {}


def grid(W, H):
    if (W, H) not in _GRID:
        X, Y = np.meshgrid(np.arange(W, dtype=f32) + .5, np.arange(H, dtype=f32) + .5)
        _GRID[(W, H)] = (X, Y)
    return _GRID[(W, H)]


class Frame:
    def __init__(self, W, H):
        self.W, self.H = W, H
        self.X, self.Y = grid(W, H)
        self.add = np.zeros((H, W, 3), f32)   # HDR emission
        self.col = np.zeros((H, W, 3), f32)   # premultiplied colour
        self.a = np.zeros((H, W), f32)

    # -- bounding-box helper: returns slices + local X,Y
    def box(self, x0, y0, x1, y1):
        x0, y0 = max(0, int(x0)), max(0, int(y0))
        x1, y1 = min(self.W, int(math.ceil(x1))), min(self.H, int(math.ceil(y1)))
        if x1 <= x0 or y1 <= y0:
            return None
        sl = (slice(y0, y1), slice(x0, x1))
        return sl, self.X[sl], self.Y[sl]

    def emit(self, sl, rgb, m):
        self.add[sl] += m[..., None] * np.asarray(rgb, f32)

    def over(self, sl, rgb, a):
        a = np.clip(a, 0, 1)
        rgb = np.asarray(rgb, f32)
        self.col[sl] = self.col[sl] * (1 - a[..., None]) + rgb * a[..., None]
        self.a[sl] = self.a[sl] * (1 - a) + a

    def bake_glow(self, k1=0.4, s1=5, k2=0.22, s2=16):
        g = gaussian_filter(self.add, (s1, s1, 0)) * k1 + gaussian_filter(self.add, (s2, s2, 0)) * k2
        self.add = self.add + g

    # -- export
    def edge_mask(self, m=14):
        ex = np.minimum(self.X, self.W - self.X); ey = np.minimum(self.Y, self.H - self.Y)
        return sstep(0, m, ex) * sstep(0, m, ey)

    def add_rgba(self, exposure=1.0):
        tone = 1 - np.exp(-np.maximum(self.add, 0) * exposure)
        tone = tone * self.edge_mask()[..., None]
        a = tone.max(2)
        rgb = tone / np.maximum(a, 1e-4)[..., None]
        return np.dstack([rgb, a])

    def trans_rgba(self):
        rgb = self.col / np.maximum(self.a, 1e-4)[..., None]
        return np.dstack([np.clip(rgb, 0, 1), np.clip(self.a * self.edge_mask(6), 0, 1)])


# ---------------- emission primitives ----------------
def glow(fr, cx, cy, r, rgb, inten=1.0, rx_k=1.0):
    b = fr.box(cx - r * 3 * rx_k, cy - r * 3, cx + r * 3 * rx_k, cy + r * 3)
    if not b:
        return
    sl, X, Y = b
    d2 = ((X - cx) / (r * rx_k)) ** 2 + ((Y - cy) / r) ** 2
    fr.emit(sl, rgb, np.exp(-d2 * 1.6) * inten)


def ellipse_q(X, Y, cx, cy, rx, ry):
    dx, dy = X - cx, Y - cy
    q = np.sqrt((dx / rx) ** 2 + (dy / ry) ** 2) + 1e-6
    gx, gy = dx / (rx * rx * q), dy / (ry * ry * q)
    g = np.sqrt(gx * gx + gy * gy) + 1e-6
    return q, (q - 1) / g  # normalised radius, signed px distance to the ellipse


def ring(fr, cx, cy, rx, ry, thick, stops, inten=1.0, soft=0.0, arc=None, edge_bias=0.0):
    """Crisp ring: colour from ramp across thickness (outer edge hottest when edge_bias>0)."""
    pad = thick + soft * 3 + 3
    b = fr.box(cx - rx - pad, cy - ry - pad, cx + rx + pad, cy + ry + pad)
    if not b or rx < 1:
        return
    sl, X, Y = b
    q, d = ellipse_q(X, Y, cx, cy, rx, ry)
    # ring occupies d in [-thick, 0]
    inside = sm(-d, 1.2) * sm(d + thick, 1.2)
    u = np.clip(-d / max(thick, 1e-3), 0, 1)  # 0 outer edge -> 1 inner edge
    heat = (1 - u) * edge_bias + (1 - edge_bias) * (1 - np.abs(u - .5) * 2)
    m = inside * (0.35 + 0.65 * heat)
    if soft > 0:
        m = m + np.exp(-((d + thick * .5) / (soft + thick * .5)) ** 2) * 0.35
    if arc is not None:
        ang = np.arctan2((Y - cy) / ry, (X - cx) / rx)
        a0, a1, fe = arc
        da = np.angle(np.exp(1j * (ang - (a0 + a1) / 2)))
        half = (a1 - a0) / 2
        m = m * sstep(half, half - fe, np.abs(da))
    col = ramp(stops, 0.3 + 0.62 * heat)
    fr.add[sl] += col * (m * inten)[..., None]


def spike(fr, x0, y0, ang, length, width, stops, inten=1.0, p=1.3):
    """Tapered anime spike from (x0,y0) along angle."""
    if length < 1:
        return
    ca, sa = math.cos(ang), math.sin(ang)
    x1, y1 = x0 + ca * length, y0 + sa * length
    pad = width + 2
    b = fr.box(min(x0, x1) - pad, min(y0, y1) - pad, max(x0, x1) + pad, max(y0, y1) + pad)
    if not b:
        return
    sl, X, Y = b
    u = ((X - x0) * ca + (Y - y0) * sa) / length
    v = (-(X - x0) * sa + (Y - y0) * ca)
    w = width * np.clip(1 - u, 0, 1) ** p * sstep(-0.02, 0.06, u)
    m = sm(w - np.abs(v), 1.0) * (u >= -0.02) * (u <= 1)
    core = np.clip(1 - np.abs(v) / np.maximum(w, 1e-3), 0, 1)
    col = ramp(stops, 0.5 + 0.5 * core * (1 - u * .5))
    fr.add[sl] += col * (m * inten)[..., None]


def streak(fr, x, y, vx, vy, length, width, stops, inten=1.0):
    sp = math.hypot(vx, vy) + 1e-6
    spike(fr, x, y, math.atan2(-vy, -vx), length, width, stops, inten, p=1.0)


def dot(fr, x, y, r, stops, inten=1.0):
    b = fr.box(x - r - 2, y - r - 2, x + r + 2, y + r + 2)
    if not b:
        return
    sl, X, Y = b
    d = np.hypot(X - x, Y - y)
    m = sm(r - d, 1.0)
    fr.add[sl] += ramp(stops, 0.6 + 0.4 * np.clip(1 - d / max(r, .5), 0, 1)) * (m * inten)[..., None]


def star4(fr, x, y, size, stops, inten=1.0, rot=0.0, thin=0.09, h_stretch=1.6):
    """4-point lens glint (horizontal arms longer)."""
    for k in range(4):
        a = rot + k * math.pi / 2
        L = size * (h_stretch if k % 2 == 0 else 1.0)
        spike(fr, x, y, a, L, size * thin, stops, inten, p=1.6)


# ---------------- translucent primitives ----------------
def puff(fr, cx, cy, r, alpha=1.0, erode=0.0, seed=0, ox=0.0, light_dir=(-.35, -.9), dark=0.0):
    """Cel-shaded painterly dust puff with noise dissolve."""
    if r < 1 or alpha <= 0.01:
        return
    b = fr.box(cx - r * 1.4, cy - r * 1.4, cx + r * 1.4, cy + r * 1.4)
    if not b:
        return
    sl, X, Y = b
    dx, dy = (X - cx) / r, (Y - cy) / r
    n = sample_noise(X, Y, 0.9 * 40 / max(r, 8), ox + seed * 37, seed * 53, seed=1, sigma=7)
    d = np.sqrt(dx * dx + dy * dy)
    shape = (0.78 + 0.34 * n) - d
    cov = sm(shape * r, 3.0) * (0.75 + 0.25 * sstep(0.0, 0.25, shape))
    lit = 0.55 - (dx * light_dir[0] + dy * light_dir[1]) * -0.55 + (n - .5) * .35
    lit = lit - dark
    c_sh, c_md, c_lt, c_rm = (np.array(DUST[k], f32) for k in ("shadow", "mid", "light", "rim"))
    t1 = sstep(0.24, 0.44, lit)[..., None]
    t2 = sstep(0.62, 0.80, lit)[..., None]
    col = c_sh * (1 - t1) + c_md * t1
    col = col * (1 - t2) + c_lt * t2
    rim = (sstep(0.62, 0.95, d) * sstep(0.8, 1.0, lit))[..., None]
    col = col * (1 - rim * .6) + c_rm * rim * .6
    dis = sstep(erode - 0.12, erode + 0.12, n) if erode > 0 else 1
    fr.over(sl, col, cov * alpha * dis * (1 - erode * .35))


def sprite_over(fr, spr, x, y, alpha=1.0, rot=0.0, sx=1.0, sy=1.0):
    """Composite a straight-alpha float RGBA sprite centred at (x,y)."""
    im = Image.fromarray((np.clip(spr, 0, 1) * 255).astype(np.uint8), "RGBA")
    if sx != 1 or sy != 1:
        im = im.resize((max(1, int(im.width * sx)), max(1, int(im.height * abs(sy)))), Image.BICUBIC)
        if sy < 0:
            im = im.transpose(Image.FLIP_TOP_BOTTOM)
    if rot:
        im = im.rotate(math.degrees(rot), resample=Image.BICUBIC, expand=True)
    a = np.asarray(im).astype(f32) / 255
    h, w = a.shape[:2]
    x0, y0 = int(round(x - w / 2)), int(round(y - h / 2))
    b = fr.box(x0, y0, x0 + w, y0 + h)
    if not b:
        return
    sl, _, _ = b
    ys, xs = sl
    src = a[ys.start - y0: ys.stop - y0, xs.start - x0: xs.stop - x0]
    fr.over(sl, src[..., :3], src[..., 3] * alpha)


def polyline_decal(fr, pts, width, rgb, alpha=1.0):
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        L = math.hypot(x1 - x0, y1 - y0) + 1e-6
        b = fr.box(min(x0, x1) - width - 2, min(y0, y1) - width - 2, max(x0, x1) + width + 2, max(y0, y1) + width + 2)
        if not b:
            continue
        sl, X, Y = b
        ux, uy = (x1 - x0) / L, (y1 - y0) / L
        t = np.clip((X - x0) * ux + (Y - y0) * uy, 0, L)
        d = np.hypot(X - (x0 + ux * t), Y - (y0 + uy * t))
        fr.over(sl, rgb, sm(width / 2 - d, 1.0) * alpha)


def polyline_emit(fr, pts, width, stops, inten=1.0):
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        L = math.hypot(x1 - x0, y1 - y0) + 1e-6
        b = fr.box(min(x0, x1) - width - 2, min(y0, y1) - width - 2, max(x0, x1) + width + 2, max(y0, y1) + width + 2)
        if not b:
            continue
        sl, X, Y = b
        ux, uy = (x1 - x0) / L, (y1 - y0) / L
        t = np.clip((X - x0) * ux + (Y - y0) * uy, 0, L)
        d = np.hypot(X - (x0 + ux * t), Y - (y0 + uy * t))
        m = sm(width / 2 - d, 1.0)
        fr.add[sl] += ramp(stops, 0.55 + 0.45 * np.clip(1 - d / (width / 2 + 1e-3), 0, 1)) * (m * inten)[..., None]


# ---------------- sprite library (rocks, leaves, flowers) ----------------
def make_rock(R, seed, moss=True):
    """Mossy rock chunk, straight-alpha float RGBA, AA via 2x supersample."""
    S = 2
    n = int(R * 2.8 * S)
    X, Y = np.meshgrid(np.arange(n) + .5, np.arange(n) + .5)
    c = n / 2
    dx, dy = (X - c) / (R * S), (Y - c) / (R * S)
    rng = random.Random(seed)
    harm = [(rng.uniform(.03, .08), rng.randint(2, 4), rng.uniform(0, 6)) for _ in range(3)] + [(rng.uniform(.01, .03), 7, rng.uniform(0, 6))]
    ang = np.arctan2(dy, dx)
    rad = 1 + sum(h * np.sin(k * ang + o) for h, k, o in harm)
    d = np.hypot(dx, dy)
    inside = d < rad
    # faceted normal: quantise angle into facets
    fac = np.round(ang / (math.pi / 4)) * (math.pi / 4)
    nz = np.sqrt(np.clip(1 - (d / rad) ** 2, 0, 1))
    nx, ny = np.cos(fac) * (d / rad), np.sin(fac) * (d / rad)
    lit = np.clip(-nx * .45 - ny * .65 + nz * .6, 0, 1)
    rock = [np.array(c, f32) for c in ((0.25, 0.17, 0.13), (0.42, 0.29, 0.20), (0.60, 0.43, 0.29), (0.76, 0.60, 0.42))]
    t = np.digitize(lit, [0.35, 0.6, 0.82])
    col = np.stack([rock[i] for i in range(4)])[t]
    if moss:
        nmap = noise_tex(seed % 7, 256, 4)
        nn = nmap[(Y.astype(int) * 3) % 256, (X.astype(int) * 3) % 256]
        cap = (dy < -0.15 + (nn - .5) * .6) & inside
        mt = np.digitize(lit, [0.4, 0.7])
        mcol = np.stack([np.array(LEAF[i], f32) for i in (0, 1, 2)])[mt]
        col = np.where(cap[..., None], mcol, col)
    edge = inside & (d > rad - 0.09)
    col = np.where(edge[..., None], np.array((0.13, 0.09, 0.08), f32), col)
    rgba = np.dstack([col, inside.astype(f32)])
    if moss and R >= 14:  # little white flower motif
        fx, fy = int(c - R * S * .25), int(c - R * S * .62)
        rr = max(2, int(R * S * .08))
        for ox, oy in ((0, -rr), (-rr, 0), (rr, 0), (0, rr), (0, 0)):
            yy, xx = np.ogrid[:n, :n]
            m = (xx - fx - ox) ** 2 + (yy - fy - oy) ** 2 <= rr * rr
            rgba[m] = (0.96, 0.82, 0.35, 1) if (ox, oy) == (0, 0) else (0.98, 0.96, 0.90, 1)
    im = Image.fromarray((rgba * 255).astype(np.uint8), "RGBA").resize((n // S, n // S), Image.BOX)
    return np.asarray(im).astype(f32) / 255


def make_leaf(L, seed=0):
    S = 3
    w, h = int(L * S), int(L * .55 * S)
    X, Y = np.meshgrid(np.arange(w) + .5, np.arange(h) + .5)
    u = X / w
    v = (Y - h / 2) / (h / 2)
    half = np.sin(np.pi * np.clip(u, 0, 1)) ** .8 * (1 - .25 * u)
    inside = np.abs(v) < half
    tone = np.where(v < 0, 2, 1)
    col = np.stack([np.array(c, f32) for c in LEAF])[tone]
    rib = (np.abs(v) < 0.09) & (u > .05) & (u < .9)
    col = np.where(rib[..., None], np.array(LEAF[0], f32), col)
    rim = inside & (np.abs(v) > half - 0.22) & (v < 0)
    col = np.where(rim[..., None], np.array(LEAF[3], f32), col)
    rgba = np.dstack([col, inside.astype(f32)])
    im = Image.fromarray((rgba * 255).astype(np.uint8), "RGBA").resize((w // S, max(1, h // S)), Image.BOX)
    return np.asarray(im).astype(f32) / 255


def make_flower(r):
    S = 3
    n = int(r * 2.4 * S)
    X, Y = np.meshgrid(np.arange(n) + .5, np.arange(n) + .5)
    c = n / 2
    dx, dy = (X - c) / (r * S), (Y - c) / (r * S)
    ang = np.arctan2(dy, dx); d = np.hypot(dx, dy)
    pet = d < (0.55 + 0.45 * np.abs(np.cos(ang * 2.5)))
    cen = d < .3
    col = np.where(cen[..., None], np.array((0.96, 0.78, 0.30), f32), np.array((0.98, 0.97, 0.92), f32))
    col = np.where((pet & (dy > .25) & ~cen)[..., None], np.array((0.86, 0.84, 0.78), f32), col)
    rgba = np.dstack([col, pet.astype(f32)])
    im = Image.fromarray((rgba * 255).astype(np.uint8), "RGBA").resize((n // S, n // S), Image.BOX)
    return np.asarray(im).astype(f32) / 255


# ---------------- sheet export ----------------
def to_img(rgba):
    return Image.fromarray((np.clip(rgba, 0, 1) * 255 + .5).astype(np.uint8), "RGBA")


def save_sheet(outdir, name, imgs, fps, pivot, scale, loop, blend, note, cols=None):
    n = len(imgs)
    w, h = imgs[0].size
    cols = cols or int(math.ceil(math.sqrt(n)))
    rows = int(math.ceil(n / cols))
    sheet = Image.new("RGBA", (cols * w, rows * h), (0, 0, 0, 0))
    for i, im in enumerate(imgs):
        sheet.paste(im, ((i % cols) * w, (i // cols) * h))
    os.makedirs(outdir, exist_ok=True)
    sheet.save(os.path.join(outdir, name + ".png"), optimize=True)
    meta = dict(name=name, blend=blend, frame_w=w, frame_h=h, cols=cols, rows=rows, frames=n, fps=fps, loop=loop,
                pivot_px=[int(pivot[0]), int(pivot[1])], pivot_uv=[round(pivot[0] / w, 4), round(pivot[1] / h, 4)],
                screen_scale_1080p=scale, note=note)
    with open(os.path.join(outdir, name + ".json"), "w", encoding="utf-8") as fp:
        json.dump(meta, fp, ensure_ascii=False, indent=2)
    return meta
