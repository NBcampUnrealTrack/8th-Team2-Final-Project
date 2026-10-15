"""HD-2D sky textures (painted clouds + horizon ridge) and a rebuilt roof shingle set."""
import os
import numpy as np
from PIL import Image

SP = r"C:\Users\Ndus_072\AppData\Local\Temp\claude\C--Users-Ndus-072-RevDevTool-RevStudio-ProjectM\2d851b1b-c6b6-4a48-a0e8-226d39210886\scratchpad"
OUT = os.path.join(SP, "skytex")
os.makedirs(OUT, exist_ok=True)


def value_noise(w, h, freq_x, freq_y, seed, tile_x=True):
    """Bilinear value noise, tileable along X."""
    rs = np.random.RandomState(seed)
    g = rs.rand(freq_y + 1, freq_x + 1).astype(np.float32)
    if tile_x:
        g[:, -1] = g[:, 0]
    ys = np.linspace(0, freq_y, h, endpoint=False)
    xs = np.linspace(0, freq_x, w, endpoint=False)
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    fy = (ys - y0)[:, None]; fx = (xs - x0)[None, :]
    fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
    y1 = np.minimum(y0 + 1, freq_y); x1 = np.minimum(x0 + 1, freq_x)
    a = g[np.ix_(y0, x0)]; b = g[np.ix_(y0, x1)]
    c = g[np.ix_(y1, x0)]; d = g[np.ix_(y1, x1)]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm(w, h, base_fx, base_fy, octaves, seed):
    out = np.zeros((h, w), np.float32); amp = 1.0; norm = 0.0
    for o in range(octaves):
        out += amp * value_noise(w, h, base_fx * 2 ** o, max(1, base_fy * 2 ** o), seed + o * 17)
        norm += amp; amp *= 0.5
    return out / norm


# ================= sky: R = cloud layer 1, G = cloud layer 2, B = horizon ridge =================
W, H = 1024, 512
# layer 1: big soft cumulus
n1 = fbm(W, H, 6, 3, 5, 11)
n1 = (n1 - 0.42) / 0.28
c1 = np.clip(n1, 0, 1)
c1 = c1 ** 1.4
# layer 2: small high wisps
n2 = fbm(W, H, 14, 6, 4, 77)
c2 = np.clip((n2 - 0.52) / 0.22, 0, 1) ** 1.8
# quantize for a painted look
c1 = np.round(c1 * 8) / 8
c2 = np.round(c2 * 6) / 6

# ridge: layered hills, only in the lower band of V
ridge = np.zeros((H, W), np.float32)
xs = np.arange(W)
for k, (freq, amp, base, seed) in enumerate(((3, 0.030, 0.535, 5), (7, 0.018, 0.548, 23), (13, 0.010, 0.556, 41))):
    prof = value_noise(W, 1, freq, 1, seed)[0]
    prof = base - prof * amp
    yy = (np.arange(H)[:, None] / H)
    ridge = np.maximum(ridge, (yy >= prof[None, :]).astype(np.float32) * (0.45 + 0.28 * k))
ridge = np.clip(ridge, 0, 1)

sky_rgb = np.stack([c1, c2, ridge], axis=2)
Image.fromarray((sky_rgb * 255).round().astype(np.uint8)).save(os.path.join(OUT, "T_Sky_Clouds.png"))
print("sky clouds/ridge ok")

# preview of what the clouds look like composited
prev = np.zeros((H, W, 3), np.float32)
prev[:] = np.array([0.32, 0.48, 0.72])
prev = prev * (1 - c1[..., None] * 0.9) + np.array([0.96, 0.95, 0.92]) * (c1[..., None] * 0.9)
prev = prev * (1 - c2[..., None] * 0.5) + np.array([1.0, 0.98, 0.94]) * (c2[..., None] * 0.5)
prev = prev * (1 - ridge[..., None]) + np.array([0.24, 0.28, 0.38]) * ridge[..., None]
Image.fromarray((np.clip(prev, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT, "_sky_preview.png"))


# ================= roof shingles: readable rows, dark slate-green, restrained moss =================
N = 256; S = 4; G = N // S  # 64 logical pixels
rs = np.random.RandomState(2026)
BASE = [(52, 60, 48), (64, 74, 56), (78, 88, 64), (92, 100, 72)]
GAP = (26, 30, 24)
EDGE = (110, 118, 86)
MOSS = [(70, 92, 44), (88, 110, 54)]

a = np.zeros((G, G, 3), np.uint8)
a[:, :] = BASE[1]
ROWS = 8
RH = G // ROWS          # 8 logical px per row
TW = 8                  # tile width in logical px
for r in range(ROWS):
    y0 = r * RH
    off = (r % 2) * (TW // 2)
    for t in range(G // TW + 1):
        x0 = (t * TW + off) % G
        shade = BASE[rs.randint(0, len(BASE))]
        for dy in range(RH):
            y = y0 + dy
            if y >= G:
                break
            # scalloped bottom: the last row of the tile is narrower
            trim = 1 if dy == RH - 1 else 0
            for dx in range(trim, TW - trim):
                x = (x0 + dx) % G
                v = shade
                if dy == 0:
                    v = tuple(min(255, c + 14) for c in shade)     # lit top edge of each tile
                elif dy == RH - 1:
                    v = GAP                                          # dark shadow gap under the row
                a[y, x] = v
        # vertical seam between tiles
        for dy in range(RH - 1):
            y = y0 + dy
            if y < G:
                a[y, (x0) % G] = tuple(max(0, c - 16) for c in shade)
# highlight the ridge-side edge of every second row so rows read at distance
for r in range(0, ROWS, 2):
    y = r * RH
    a[y, :] = np.clip(np.asarray(a[y, :], np.int16) + 10, 0, 255).astype(np.uint8)

# moss: two low-frequency drifts, ~12% coverage
m = fbm(G, G, 2, 2, 3, 91)
m = (m - m.min()) / (m.max() - m.min())
moss_mask = m > 0.74
idx = np.argwhere(moss_mask)
for y, x in idx:
    col = MOSS[rs.randint(0, 2)]
    a[y, x] = tuple(np.clip(np.array(col) + rs.randint(-8, 9, 3), 0, 255))
print("moss coverage %.1f%%" % (moss_mask.mean() * 100))

roof_bc = Image.fromarray(a).resize((N, N), Image.NEAREST)
roof_bc.save(os.path.join(OUT, "T_Prop_RoofShingle_Pixel_BC.png"))

# height from the row structure -> normal + ORM
hgt = np.zeros((G, G), np.float32)
for r in range(ROWS):
    for dy in range(RH):
        y = r * RH + dy
        if y < G:
            hgt[y, :] = 1.0 - dy / (RH - 1) * 0.85
hgt[moss_mask] += 0.08
hgt = np.clip(hgt, 0, 1)
hb = np.asarray(Image.fromarray((hgt * 255).astype(np.uint8)).resize((N, N), Image.NEAREST), np.float32) / 255
gy, gx = np.gradient(hb)
nz = np.ones_like(hb) * 0.35
nx, ny = -gx * 3.0, gy * 3.0    # DirectX green-down, matches the rest of the pack
ln = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2)
nrm = np.stack([nx / ln, ny / ln, nz / ln], axis=2)
Image.fromarray((((nrm + 1) / 2) * 255).round().clip(0, 255).astype(np.uint8)).save(os.path.join(OUT, "T_Prop_RoofShingle_Pixel_N.png"))

ao = 0.72 + 0.28 * hb
rough = np.full_like(hb, 0.90) - np.asarray(Image.fromarray((moss_mask * 255).astype(np.uint8)).resize((N, N), Image.NEAREST), np.float32) / 255 * 0.05
orm = np.stack([ao, np.clip(rough, 0, 1), np.zeros_like(hb)], axis=2)
Image.fromarray((orm * 255).round().clip(0, 255).astype(np.uint8)).save(os.path.join(OUT, "T_Prop_RoofShingle_Pixel_ORM.png"))
print("roof set ok")

# contact sheet
sheet = Image.new("RGB", (256 * 4 + 30, 256), (18, 18, 18))
sheet.paste(roof_bc, (0, 0))
tiled = Image.new("RGB", (256, 256))
half = roof_bc.resize((128, 128), Image.NEAREST)
for i in range(2):
    for j in range(2):
        tiled.paste(half, (i * 128, j * 128))
sheet.paste(tiled, (266, 0))
sheet.paste(Image.open(os.path.join(OUT, "T_Prop_RoofShingle_Pixel_N.png")), (532, 0))
sheet.paste(Image.open(os.path.join(OUT, "_sky_preview.png")).resize((256, 128)), (798, 64))
sheet.save(os.path.join(OUT, "_contact.png"))
print("files:", sorted(os.listdir(OUT)))
