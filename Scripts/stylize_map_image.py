# 지도 스타일화 --- bake_map_image.py 가 캡처 직후 시스템 파이썬으로 호출한다 (에디터 파이썬에는 numpy/PIL 이 없음).
# 사용: py -3 stylize_map_image.py <바탕색.png> <건물.png> <최종색.png> <물뺀최종색.png> <나무.json> <출력.png>
# 톤 기준: 2026-09-21 확정본 --- 양피지 바탕, 영역 재칠, 지붕은 배경에 톤을 맞춘 탁한 갈색.
# 10/15 HD-2D 맵용으로 다시 짬: 땅은 바탕색을 군집으로 나눠 맵마다 땅 색을 살리고(색 규칙은 맵 조명·땅 색에 따라 틀렸다),
#   물은 물 있이/없이 최종색 차이, 숲은 나무 카드 자리마다 둥근 수관, 흙길은 초록 땅 맵에서만 갈색으로 찾는다.
import json
import random
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter

SIZE = 1024
PARCH = np.array([0.83, 0.78, 0.63])
GRASS = np.array([0.66, 0.74, 0.47])
FOREST = np.array([0.40, 0.54, 0.33])
DEAD = np.array([0.55, 0.51, 0.41])
ROAD = np.array([0.88, 0.81, 0.62])
WATER = np.array([0.62, 0.78, 0.84])
ROOF = np.array([0.62, 0.47, 0.36])
LINE = (94, 74, 48)

base_p, mask_p, lit_p, dry_p, trees_p, out_p = sys.argv[1:7]


def load(p):
    return np.asarray(Image.open(p).convert("RGB").resize((SIZE, SIZE), Image.LANCZOS)).astype(np.float32) / 255.0


def blur(mm, radius):
    return np.asarray(Image.fromarray((np.clip(mm, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius))).astype(np.float32) / 255.0


def smooth_mask(mm, radius=2, thresh=0.5):
    return blur(mm.astype(np.float32), radius) >= thresh


def close_mask(mm, radius):
    size = radius * 2 + 1
    im = Image.fromarray((mm * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(size)).filter(ImageFilter.MinFilter(size))
    return np.asarray(im) > 127


def open_mask(mm, radius):
    size = radius * 2 + 1
    im = Image.fromarray((mm * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(size)).filter(ImageFilter.MaxFilter(size))
    return np.asarray(im) > 127


a = load(base_p)
m = load(mask_p)
lit = load(lit_p)
dry = load(dry_p)
trees = json.load(open(trees_p))
scale = SIZE / float(trees["size"])

void = a.max(-1) < 0.015                       # 캡처 범위 안이지만 아무것도 없는 곳 (맵 밖)
void = open_mask(void, 2)
building = smooth_mask(m.max(-1) > 0.03, 1, 0.4)
water = np.abs(lit - dry).sum(-1) > 0.10
water = close_mask(open_mask(water, 1), 3) & ~building
# 다리: 물길을 가로질러 끊긴 좁은 틈 (물 있이/없이 차이에서 빠진 곳) --- 길 색으로 칠한다
bridge = close_mask(water, 10) & ~water & ~building

# --- 땅: 바탕색을 뭉개고 몇 가지 색으로 나눈다 (맵마다 땅 색이 달라 색 규칙 대신 군집)
small = np.asarray(Image.fromarray((a * 255).astype(np.uint8)).resize((256, 256), Image.BOX).filter(ImageFilter.MedianFilter(5))).astype(np.float32) / 255.0
valid_small = np.asarray(Image.fromarray((~void).astype(np.uint8) * 255).resize((256, 256), Image.NEAREST)) > 127
X = small[valid_small]
K = 6
rng = np.random.default_rng(1)
C = X[rng.choice(len(X), K, replace=False)] if len(X) >= K else np.tile(PARCH, (K, 1))
for _ in range(25):
    lab = ((X[:, None, :] - C[None]) ** 2).sum(-1).argmin(1)
    C = np.array([X[lab == i].mean(0) if (lab == i).any() else C[i] for i in range(K)])
# 큰 그림에서 군집 번호: 중간 크기에서 뭉갠 색으로 가까운 군집, 경계는 군집별 확률을 흐려서 매끈하게
mid = np.asarray(Image.fromarray((a * 255).astype(np.uint8)).resize((512, 512), Image.BOX).filter(ImageFilter.MedianFilter(7))).astype(np.float32) / 255.0
d = ((mid[..., None, :] - C[None, None]) ** 2).sum(-1)
lab_mid = d.argmin(-1)
score = np.zeros((SIZE, SIZE, K), np.float32)
for i in range(K):
    li = Image.fromarray(((lab_mid == i) * 255).astype(np.uint8)).resize((SIZE, SIZE), Image.BILINEAR).filter(ImageFilter.GaussianBlur(7))
    score[..., i] = np.asarray(li)
label = score.argmax(-1)


def tone(c):
    # 군집 평균색을 지도 색으로: 초록은 풀색 쪽, 나머지는 양피지 쪽으로 당기고 밝기 차이는 조금 남긴다
    r, g, b = c
    v = c.max()
    lum = 0.3 * r + 0.59 * g + 0.11 * b
    if g > r * 1.02 and g > b * 1.15:
        target = GRASS * (0.95 + 0.3 * (lum - 0.45))
        return np.clip(target * 0.85 + c * 0.15, 0, 1)
    if b > r * 1.15 and b > g:
        return WATER * 0.9 + c * 0.1
    gray = np.array([lum, lum, lum])
    desat = c * 0.45 + gray * 0.55
    return np.clip(desat * 0.45 + PARCH * (0.62 + 0.45 * (lum - 0.45)), 0, 1)


palette = np.array([tone(c) for c in C])
out = palette[label]
out[void] = PARCH

# --- 흙길: 초록 땅 맵에서 갈색은 좁은 길이라 군집에 묻힌다 => 따로 찾아 길 색으로. 갈색이 땅의 대부분인 맵(사막·붉은 땅)은 건너뛴다
med = np.asarray(Image.fromarray((a * 255).astype(np.uint8)).filter(ImageFilter.MedianFilter(5))).astype(np.float32) / 255.0
mr, mg, mb = med[..., 0], med[..., 1], med[..., 2]
mv = med.max(-1)
msat = (mv - med.min(-1)) / np.maximum(mv, 1e-6)
hue = np.degrees(np.arctan2(np.sqrt(3) * (mg - mb), 2 * mr - mg - mb)) % 360
brown = (hue > 12) & (hue < 45) & (msat > 0.3) & (mv > 0.18) & (mv < 0.75) & ~void
path = np.zeros_like(brown)
if brown.mean() < 0.15:
    path = smooth_mask(open_mask(close_mask(brown, 3), 2), 1.5) & ~building & ~water   # 자갈길 점들은 잇고, 외딴 흙 얼룩은 버린다
    out[path] = ROAD * 0.95

# --- 숲: 나무 자리마다 둥근 수관. 아래쪽 나무가 위에 오게 y 순으로 그린다
forest_im = Image.new("L", (SIZE, SIZE), 0)
dead_im = Image.new("L", (SIZE, SIZE), 0)
hi_im = Image.new("L", (SIZE, SIZE), 0)
fd = ImageDraw.Draw(forest_im)
dd = ImageDraw.Draw(dead_im)
hd = ImageDraw.Draw(hi_im)
for t in sorted(trees["trees"], key=lambda t: t[1]):
    x, y, r = t[0] * scale, t[1] * scale, max(t[2] * scale, 2.0)
    (dd if len(t) > 3 and t[3] else fd).ellipse([x - r, y - r, x + r, y + r], fill=255)
    hr = r * 0.55
    hd.ellipse([x - r * 0.3 - hr, y - r * 0.3 - hr, x - r * 0.3 + hr, y - r * 0.3 + hr], fill=255)
forest = smooth_mask(np.asarray(forest_im) > 127, 1.2) & ~building & ~water & ~bridge
dead = smooth_mask(np.asarray(dead_im) > 127, 1.2) & ~building & ~water & ~bridge & ~forest
out[forest] = FOREST
out[dead] = DEAD
hi = blur(np.asarray(hi_im) / 255.0, 2.0)[..., None] * (forest | dead)[..., None]
out = out * (1 - hi * 0.22) + np.minimum(out * 1.18, 1) * hi * 0.22
out[water] = WATER
out[bridge] = ROAD * 0.92
mask_v = m.max(-1)[..., None]
roof = ROOF * (0.82 + mask_v * 0.35)
roof = roof * 0.75 + PARCH * 0.25
out[building] = np.clip(roof, 0, 1)[building]
styled = Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.SMOOTH_MORE)


def edges(mm):
    e = np.zeros_like(mm, bool)
    e[1:, :] |= mm[1:, :] != mm[:-1, :]
    e[:, 1:] |= mm[:, 1:] != mm[:, :-1]
    return e


def stroke(img, e, color, alpha, width=3, soft=0.6):
    em = Image.fromarray((e * 255).astype(np.uint8))
    if width > 1:                                  # MaxFilter(1)은 PIL 이 0 나누기로 죽는다
        em = em.filter(ImageFilter.MaxFilter(width))
    em = em.filter(ImageFilter.GaussianBlur(soft))
    return Image.composite(Image.new("RGB", img.size, color), img, em.point(lambda p: int(p * alpha)))


# 땅 군집 경계는 옅게, 물·숲·건물·맵 밖 경계는 진하게
ground_lab = np.where(forest | dead | water | building | void, -1, label)
styled = stroke(styled, edges(ground_lab) & ~edges(forest | dead | water | building | void), LINE, 0.22, 1, 0.6)
styled = stroke(styled, edges(path & ~forest & ~dead), LINE, 0.3, 1, 0.6)
styled = stroke(styled, edges(water) | edges(forest | dead) | edges(void) | edges(bridge), LINE, 0.5)
styled = stroke(styled, edges(building), (86, 64, 44), 0.55, 3, 0.4)

# 숲 얼룩 질감
random.seed(3)
blob = Image.new("L", (128, 128))
pb = blob.load()
for y in range(128):
    for x in range(128):
        pb[x, y] = 128 + random.randint(-60, 60)
blob = blob.resize(styled.size, Image.BICUBIC).filter(ImageFilter.GaussianBlur(6))
dark = ImageEnhance.Brightness(styled).enhance(0.9)
fm = Image.fromarray(((forest | dead) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2))
styled = Image.composite(dark, styled, ImageChops.multiply(blob.point(lambda p: 255 if p > 150 else 0), fm).point(lambda p: int(p * 0.9)))

# 종이 질감 + 가장자리 비네트
random.seed(7)
paper = Image.new("L", (96, 96))
pp = paper.load()
for y in range(96):
    for x in range(96):
        pp[x, y] = 235 + random.randint(-9, 9)
paper = paper.resize(styled.size, Image.BICUBIC).filter(ImageFilter.GaussianBlur(2))
styled = ImageChops.multiply(styled, Image.merge("RGB", [paper] * 3))
w, h = styled.size
vmask = Image.new("L", (w, h), 0)
dv = ImageDraw.Draw(vmask)
for i in range(20):
    dv.rectangle([i * 7, i * 7, w - i * 7, h - i * 7], outline=int(255 * (1 - i / 20) * 0.3), width=7)
vmask = vmask.filter(ImageFilter.GaussianBlur(22))
styled = Image.composite(ImageEnhance.Brightness(styled).enhance(0.78), styled, vmask)
styled.save(out_p)
print("stylized:", out_p, "clusters", [tuple(np.round(c, 2)) for c in C])
