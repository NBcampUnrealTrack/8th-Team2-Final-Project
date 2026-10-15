# -*- coding: utf-8 -*-
"""픽셀 스프라이트용 노멀맵 생성기 (HD-2D 캐릭터 라이팅).

옥토패스 레퍼런스 조사 결과를 반영한 기본값:
  --detail 0.0    아트 명도로 높이를 만들지 않는다. 아트에는 이미 화가가 그린 음영이
                  들어 있어서, 명도를 높이로 쓰면 같은 음영이 지오메트리로 한 번 더
                  들어가 흰 옷에 얼룩이 생긴다.
  --maxtilt 22    기울기 상한(도). 실루엣 거리장은 가장자리에서 법선을 눕혀버려
                  측면광에서 테두리가 통째로 검어진다.
  --nblur 0.14    네이티브 해상도 블러 시그마 = 짧은변 x 이 값. 텍셀 단위 요철을 지우고
                  몸 전체 볼륨만 남긴다. 옥토패스의 스프라이트 음영은 저주파다.
  --green dx      DirectX 규약(green down). Paper2D 스프라이트에서 검증된 방향.

입력:  <스크립트폴더>/nm2_src/**/*.png  (AssetDiscoveryService.export_texture 로 뽑은 알베도)
출력:  <스크립트폴더>/nm2_final/<에셋경로를 __ 로 이은 이름>_N.png
"""

import os, sys, glob, math
import numpy as np
from PIL import Image

SP = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(SP, "nm2_src")
DST = os.path.join(SP, "nm2_final")
os.makedirs(DST, exist_ok=True)

def arg(name, default, cast=float):
    if name in sys.argv:
        return cast(sys.argv[sys.argv.index(name) + 1])
    return default

SLOPE   = arg("--slope", 1.00)          # tan(angle) reached at the 98th pct gradient
GREEN   = arg("--green", "dx", str)     # 'dx' = green down (G=-dh/drow), 'gl' = green up
DETAIL  = arg("--detail", 0.25)         # weight of the luminance detail term
SS      = int(arg("--ss", 8))           # supersample factor
MAXTILT  = arg("--maxtilt", 22.0)        # 기울기 상한(도)
NBLUR    = arg("--nblur", 0.14)          # 네이티브 블러 시그마 = 짧은변 x 이 값
MAXSLOPE = math.tan(math.radians(MAXTILT))

# ---------- helpers ----------
def gaussian1d(sigma):
    r = max(1, int(math.ceil(sigma * 3.0)))
    x = np.arange(-r, r + 1, dtype=np.float64)
    k = np.exp(-(x * x) / (2.0 * sigma * sigma))
    return k / k.sum()

def blur(img, sigma):
    """separable gaussian, edge-reflect."""
    if sigma <= 0: return img
    k = gaussian1d(sigma); r = (len(k) - 1) // 2
    p = np.pad(img, ((r, r), (r, r)), mode="reflect")
    out = np.zeros_like(img, dtype=np.float64)
    tmp = np.zeros((p.shape[0], img.shape[1]), dtype=np.float64)
    for i, w in enumerate(k):                       # horizontal
        tmp += w * p[:, i:i + img.shape[1]]
    for i, w in enumerate(k):                       # vertical
        out += w * tmp[i:i + img.shape[0], :]
    return out

def edt(mask):
    """Euclidean distance from each True texel to the nearest False texel.
    scipy when available, else exact brute force per row-band (images are small)."""
    try:
        from scipy.ndimage import distance_transform_edt
        return distance_transform_edt(mask)
    except Exception:
        h, w = mask.shape
        ys, xs = np.nonzero(~mask)
        if len(ys) == 0:
            return np.full(mask.shape, float(max(h, w)))
        gy, gx = np.mgrid[0:h, 0:w]
        d = np.full(mask.shape, np.inf)
        for i in range(0, len(ys), 4096):
            yy = ys[i:i + 4096][None, None, :]; xx = xs[i:i + 4096][None, None, :]
            dd = (gy[..., None] - yy) ** 2 + (gx[..., None] - xx) ** 2
            d = np.minimum(d, dd.min(axis=2))
        return np.sqrt(d) * mask

def box_down(a, f):
    h, w = a.shape
    return a.reshape(h // f, f, w // f, f).mean(axis=(1, 3))

# ---------- core ----------
def make_normal(path):
    im = Image.open(path).convert("RGBA")
    W, H = im.size
    a = np.asarray(im, dtype=np.float64) / 255.0
    alpha = a[..., 3] > 0.5

    # supersample (nearest -> keeps hard pixel-art edges)
    big = np.asarray(im.resize((W * SS, H * SS), Image.NEAREST), dtype=np.float64) / 255.0
    m = big[..., 3] > 0.5
    m = np.pad(m, 1, mode="constant", constant_values=False)   # guarantee a background ring

    # (b) silhouette height: distance-to-edge, normalised by the LOCAL body radius
    #     (max distance inside the same horizontal run) so a thin arm and a wide
    #     torso both read as full rounded cylinders instead of the torso going flat.
    d = edt(m)[1:-1, 1:-1]
    inside0 = m[1:-1, 1:-1]
    R = np.zeros_like(d)
    for y in range(d.shape[0]):
        row = inside0[y]
        if not row.any(): continue
        x = 0
        while x < len(row):
            if row[x]:
                x2 = x
                while x2 < len(row) and row[x2]: x2 += 1
                R[y, x:x2] = d[y, x:x2].max()
                x = x2
            else:
                x += 1
    # smooth the radius field across rows (mask-weighted) so runs blend
    wm = inside0.astype(np.float64)
    num = blur(R * wm, SS * 1.2); den = blur(wm, SS * 1.2)
    R = np.where(den > 1e-6, num / np.maximum(den, 1e-6), R)
    R = np.maximum(R, SS * 0.5)
    h_sil = np.sqrt(np.clip(d / R, 0.0, 1.0))

    # (c) art detail: luminance, low weight, mean-filled outside the mask
    lum = 0.299 * big[..., 0] + 0.587 * big[..., 1] + 0.114 * big[..., 2]
    inside = inside0
    mean_l = lum[inside].mean() if inside.any() else 0.5
    lum = np.where(inside, lum, mean_l)
    lum = blur(lum, SS * 0.75)
    lo, hi = lum[inside].min(), lum[inside].max()
    h_det = (lum - lo) / (hi - lo) if hi > lo else np.zeros_like(lum)

    # (d) combine, smooth at supersampled res, resample to native
    hgt = (1.0 - DETAIL) * h_sil + DETAIL * h_det
    hgt = hgt * inside                      # zero outside the silhouette
    hgt = blur(hgt, SS * 0.5)
    hgt = box_down(hgt, SS)
    sig = NBLUR * min(hgt.shape[0], hgt.shape[1])
    if sig > 0.05:
        wa = alpha.astype(np.float64)
        num = blur(hgt * wa, sig); den = blur(wa, sig)
        hgt = np.where(den > 1e-6, num / np.maximum(den, 1e-6), hgt)

    # (e) gradients -> normal
    grow, gcol = np.gradient(hgt)           # d/drow , d/dcol   (native texel units)
    mag = np.sqrt(grow ** 2 + gcol ** 2)
    ref = np.percentile(mag[alpha], 98) if alpha.any() else 0.0
    k = (SLOPE / ref) if ref > 1e-9 else 0.0
    nx = -gcol * k
    ny = (grow if GREEN == "gl" else -grow) * k
    s = np.sqrt(nx ** 2 + ny ** 2)
    over = s > MAXSLOPE
    if over.any():
        sc = np.where(over, MAXSLOPE / np.maximum(s, 1e-9), 1.0)
        nx *= sc; ny *= sc
    nz = np.ones_like(nx)
    ln = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2)
    nx, ny, nz = nx / ln, ny / ln, nz / ln

    # (f) flat where masked out
    nx = np.where(alpha, nx, 0.0); ny = np.where(alpha, ny, 0.0); nz = np.where(alpha, nz, 1.0)

    enc = np.stack([nx, ny, nz], axis=-1) * 0.5 + 0.5
    enc = np.floor(enc * 255.0 + 0.5).clip(0, 255).astype(np.uint8)
    maxang = math.degrees(math.atan(np.sqrt(nx ** 2 + ny ** 2).max() / 1.0)) if alpha.any() else 0.0
    return Image.fromarray(enc, "RGB"), (W, H), maxang, hgt

# ---------- run ----------
files = sorted(p for p in glob.glob(os.path.join(SRC, "**", "*"), recursive=True)
         if p.lower().endswith(".png"))
rows = []
print("slope=%.2f green=%s detail=%.2f ss=%d maxtilt=%.1f nblur=%.2f" % (SLOPE, GREEN, DETAIL, SS, MAXTILT, NBLUR))
for f in files:
    rel = os.path.relpath(f, SRC).replace(os.sep, "/")
    name = os.path.splitext(rel)[0].replace("/", "__")
    nm, size, maxang, hgt = make_normal(f)
    out = os.path.join(DST, name + "_N.png")
    nm.save(out)
    arr = np.asarray(nm)
    print("%-38s %dx%d  maxTilt=%4.1fdeg  R[%d..%d] G[%d..%d] B[%d..%d]" %
          (name, size[0], size[1], maxang, arr[..., 0].min(), arr[..., 0].max(),
           arr[..., 1].min(), arr[..., 1].max(), arr[..., 2].min(), arr[..., 2].max()))
    rows.append((name, Image.open(f).convert("RGBA"), nm))

# contact sheet: 4 columns of (source | normal) pairs
CELL_H = 200
pad = 8
rows = rows[:24]
cells = []
for name, src, nm in rows:
    sc = CELL_H / src.size[1]
    tw = max(1, int(src.size[0] * sc))
    bg = Image.new("RGB", (tw, CELL_H), (60, 60, 66))
    s2 = src.resize((tw, CELL_H), Image.NEAREST)
    bg.paste(s2, (0, 0), s2)
    n2 = nm.resize((tw, CELL_H), Image.NEAREST)
    cell = Image.new("RGB", (tw * 2 + pad, CELL_H), (25, 25, 28))
    cell.paste(bg, (0, 0)); cell.paste(n2, (tw + pad, 0))
    cells.append(cell)
cols = 4
rows_n = (len(cells) + cols - 1) // cols
cw = max(c.size[0] for c in cells) + pad * 2
sheet = Image.new("RGB", (cw * cols, (CELL_H + pad * 2) * rows_n), (18, 18, 20))
for i, c in enumerate(cells):
    x = (i % cols) * cw + pad; y = (i // cols) * (CELL_H + pad * 2) + pad
    sheet.paste(c, (x, y))
sheet.save(os.path.join(DST, "_contact.png"))
print("contact sheet ->", os.path.join(DST, "_contact.png"), sheet.size)
