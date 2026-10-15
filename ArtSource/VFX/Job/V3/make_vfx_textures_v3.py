"""직업 스킬 VFX V3 용 마스크 텍스처 (가산 머티리얼 + User.EffectColor 틴트).

V2(`../make_vfx_textures.py`)와 같은 규칙:
- RGBA 이고 R=G=B=A=밝기 — `M_NU_Sprite(Front)` 가 불투명도를 R 로 읽어도 같은 모양
- 배경 투명, 방향이 있는 모양은 머리가 텍스처 위쪽

만드는 것:
- T_VFX_Crescent   512x512  두꺼운 초승달. 좌우 대칭(임팩트는 방향을 모름)이라 양 끝이 위로 올라간 호로 그린다.
                            안쪽(위) 가장자리가 가장 밝고 바깥(아래)으로 부드럽게 빠진다
- T_VFX_LightShaft 128x512  세로 빛기둥. 가운데 좁은 흰 코어 + 가로로 퍼지는 글로우, 위아래 끝은 페이드
- T_VFX_ShardChip  128x128  각진 흰 조각 한 장(회전은 파티클이 준다). 물체라 가장자리를 또렷하게
- T_VFX_ThickRing  512x512  굵은 충격파 링. 안쪽 가장자리가 가장 밝고 바깥으로 부드럽게 빠진다
                            (V2 의 T_Shockwave 는 압축이 TC_Grayscale 이라 Color 샘플러에서 빨강만 나와 쓰지 않는다)
- 10/8 필살기 3차(D3_TEXTURES): 빛살 · 가로 플레어 · 날 선 충격파 · 번개 3 · 룬 띠 · 보케 · 할퀴기 2 · 카드 무늬 4 는 마스크,
  바위 가시 A/B/C · 흙먼지 · 나뭇잎 · 금화는 색이 있는 RGBA(알파 블렌드 재질 M_NU_SpriteAlphaFront 용, sRGB 켬)

사용법:  python ArtSource/VFX/Job/V3/make_vfx_textures_v3.py        (전부)
         python ArtSource/VFX/Job/V3/make_vfx_textures_v3.py d3     (10/8 필살기 3차 텍스처만)
출력:    같은 폴더의 T_VFX_*.png
"""

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

OUT_DIR = Path(__file__).resolve().parent


def smoothstep(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def save_mask(values, w, h, name, glow=0.0, glow_gain=0.0):
    """values: list of 0..1 floats row-major. 글로우는 본 모양 둘레에만 max 로 얹는다."""
    img = Image.new("L", (w, h))
    img.putdata([int(round(max(0.0, min(1.0, v)) * 255)) for v in values])
    if glow > 0:
        blurred = img.filter(ImageFilter.GaussianBlur(max(w, h) * glow)).point(lambda v: int(v * glow_gain))
        out = Image.new("L", (w, h))
        out.putdata([max(a, b) for a, b in zip(img.get_flattened_data(), blurred.get_flattened_data())])
        img = out
    Image.merge("RGBA", (img, img, img, img)).save(OUT_DIR / name)
    print("저장", name, (w, h))


def crescent(size=512):
    # 위로 열린 호: 바깥 원(아래로 큼)에서 안쪽 원(위로 올린 원)을 뺀 영역
    ocx, ocy, orad = 0.5, 0.36, 0.46   # 바깥 원
    icx, icy, irad = 0.5, 0.06, 0.48   # 안쪽 원(위로 올려 두께가 가운데에서 가장 두껍다)
    vals = []
    feather = 1.5 / size
    for j in range(size):
        y = (j + 0.5) / size
        for i in range(size):
            x = (i + 0.5) / size
            d_out = orad - math.hypot(x - ocx, y - ocy)    # >0 바깥 원 안
            d_in = math.hypot(x - icx, y - icy) - irad      # >0 안쪽 원 밖
            inside = smoothstep(-feather, feather, d_out) * smoothstep(-feather, feather, d_in)
            if inside <= 0:
                vals.append(0.0)
                continue
            # 띠 두께에 대한 안쪽 가장자리에서의 거리 비율 → 안쪽이 가장 밝다
            band = max(1e-4, d_in + d_out)
            k = min(1.0, max(0.0, d_in / band))
            v = (1.0 - k) ** 1.15 * 0.85 + 0.15 * (1.0 - k)
            # 끝으로 갈수록(띠가 얇을수록) 조금 어둡게
            v *= smoothstep(0.0, 0.10, band)
            vals.append(v * inside)
    save_mask(vals, size, size, "T_VFX_Crescent.png", glow=0.02, glow_gain=0.55)


def light_shaft(w=128, h=512):
    vals = []
    for j in range(h):
        y = (j + 0.5) / h
        # 위 25% 와 아래 8% 에서 페이드
        vfade = smoothstep(0.0, 0.25, y) * (1.0 - smoothstep(0.92, 1.0, y))
        for i in range(w):
            x = (i + 0.5) / w
            dx = abs(x - 0.5)
            core = math.exp(-(dx / 0.06) ** 2)
            glow = 0.6 * math.exp(-(dx / 0.2) ** 2)
            vals.append(min(1.0, core + glow) * vfade)
    save_mask(vals, w, h, "T_VFX_LightShaft.png")


def shard_chip(size=128):
    # 큰 그림에 각진 다각형을 그리고 줄여서 가장자리만 매끈하게(물체라 글로우는 아주 약하게)
    s = 4
    big = Image.new("L", (size * s, size * s), 0)
    d = ImageDraw.Draw(big)
    pts = [(0.50, 0.08), (0.80, 0.30), (0.88, 0.62), (0.58, 0.92), (0.22, 0.78), (0.14, 0.40)]
    d.polygon([(x * size * s, y * size * s) for x, y in pts], fill=200)
    # 한쪽 면을 밝게 — 결정 조각처럼 각이 읽히게
    d.polygon([(x * size * s, y * size * s) for x, y in [(0.50, 0.08), (0.80, 0.30), (0.52, 0.52), (0.14, 0.40)]], fill=255)
    small = big.resize((size, size), Image.LANCZOS)
    vals = [v / 255.0 for v in small.get_flattened_data()]
    save_mask(vals, size, size, "T_VFX_ShardChip.png", glow=0.03, glow_gain=0.35)


def thick_ring(size=512):
    r0, width = 0.33, 0.13          # 안쪽 가장자리 반지름, 띠 두께
    vals = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            if r < r0:
                v = math.exp(-((r0 - r) / 0.012) ** 2)              # 안쪽은 날카롭게 끊김
            else:
                k = (r - r0) / width
                v = math.exp(-(k / 0.55) ** 2) * (1.0 - smoothstep(0.85, 1.0, r / 0.5))
            vals.append(v)
    save_mask(vals, size, size, "T_VFX_ThickRing.png")


def slash_arc(size=512, name="T_VFX_SlashArc.png", width=0.095, power=0.8, glow=0.012, glow_gain=0.45, flat=False, inset=0.0):
    # 베기 자국: 원호를 따라 가운데가 가장 두껍고 양 끝이 바늘처럼 가늘어진다(V2/V3 초승달은 두껍고 바깥이
    # 깃털처럼 퍼져 "그릇" 으로 읽혔다). 웃는 입 모양(아래로 볼록)으로 그리고, 대각선은 스프라이트 회전으로 준다.
    # 띠 안에서는 안쪽(원 중심 쪽) 가장자리가 가장 밝아 칼날이 지나간 앞선으로 읽힌다
    cx, cy, rad = 0.5, 0.10, 0.42 - inset   # inset: 코어 호를 색 호의 밝은 안쪽 가장자리로 당긴다
    a0, a1 = math.radians(18), math.radians(162)      # 아래쪽 호 144°
    vals = []
    for j in range(size):
        y = (j + 0.5) / size
        for i in range(size):
            x = (i + 0.5) / size
            ang = math.atan2(y - cy, x - cx)
            if ang < a0 or ang > a1:
                vals.append(0.0)
                continue
            t = (ang - a0) / (a1 - a0)                 # 0..1 호를 따라
            half = width * (math.sin(math.pi * t) ** power) * 0.5
            if half <= 1e-5:
                vals.append(0.0)
                continue
            r = math.hypot(x - cx, y - cy)
            k = (r - (rad - half)) / (2 * half)        # 0 = 안쪽 가장자리, 1 = 바깥 가장자리
            if k < -0.05 or k > 1.05:
                vals.append(0.0)
                continue
            edge = smoothstep(-0.05, 0.08, k) * (1.0 - smoothstep(0.92, 1.05, k))
            kc = min(1.0, max(0.0, k))
            v = 1.0 if flat else (1.0 - kc) ** 1.4 * 0.8 + 0.2
            # 양 끝은 조금 어둡게 — 가운데에서 힘이 실린 것처럼
            v *= 0.55 + 0.45 * math.sin(math.pi * t)
            vals.append(v * edge)
    save_mask(vals, size, size, name, glow=glow, glow_gain=glow_gain)


def arrow_bolt(size=512, name="T_VFX_ArrowBolt.png"):
    # 빛 화살: 위쪽에 날카로운 마름모 촉, 아래로 가늘어지며 사라지는 화살대. 정사각 칸 안에 세로로 그려
    # 균등 크기 스프라이트 + 비행 방향 정렬로 쓴다(V2 T_VFX_Arrow 는 표지판 같은 단순 화살표였다)
    vals = []
    head_top, head_mid, head_end = 0.06, 0.20, 0.30   # 촉: 위 끝 · 가장 넓은 곳 · 화살대와 만나는 곳
    head_w = 0.075                                      # 촉 반폭
    for j in range(size):
        y = (j + 0.5) / size
        for i in range(size):
            x = (i + 0.5) / size
            dx = abs(x - 0.5)
            v = 0.0
            if head_top <= y <= head_end:
                if y <= head_mid:
                    hw = head_w * (y - head_top) / (head_mid - head_top)
                else:
                    hw = head_w * (1.0 - 0.75 * (y - head_mid) / (head_end - head_mid))
                if hw > 0:
                    edge = 1.0 - smoothstep(hw * 0.75, hw * 1.05, dx)
                    core = math.exp(-(dx / (hw * 0.45 + 1e-4)) ** 2)
                    v = max(v, edge * (0.65 + 0.35 * core))
            if y >= head_mid:
                t = (y - head_mid) / (0.98 - head_mid)          # 0 = 촉 쪽, 1 = 꼬리 끝
                sw = 0.018 * (1.0 - 0.6 * t)
                shaft = math.exp(-(dx / sw) ** 2) * (1.0 - smoothstep(0.55, 1.0, t)) * (1.0 - 0.35 * t)
                glow = 0.25 * math.exp(-(dx / (sw * 3.5)) ** 2) * (1.0 - smoothstep(0.3, 1.0, t))
                v = max(v, min(1.0, shaft + glow))
            vals.append(v)
    save_mask(vals, size, size, name, glow=0.01, glow_gain=0.5)


def speed_line(size=256, name="T_VFX_SpeedLine.png"):
    # 곧고 가는 속도선(꼬리용): 가로로는 좁은 가우시안, 세로로는 양 끝만 부드럽게 빠진다
    vals = []
    for j in range(size):
        y = (j + 0.5) / size
        vf = smoothstep(0.0, 0.15, y) * (1.0 - smoothstep(0.85, 1.0, y))
        for i in range(size):
            x = (i + 0.5) / size
            dx = abs(x - 0.5)
            vals.append((math.exp(-(dx / 0.05) ** 2) + 0.3 * math.exp(-(dx / 0.14) ** 2)) * vf)
    save_mask(vals, size, size, name)


def card(size=512, name="T_VFX_CardV3.png"):
    # 갬블러 카드: 밝은 테두리 + 은은한 면 + 가운데 다이아 문양 + 테두리 안쪽 가는 선. 세로로 긴 카드(비 2:3)를
    # 정사각 칸 가운데에 세운다 — 비행 방향 정렬 시 카드 긴 축이 날아가는 방향을 향한다
    hw, hh, r = 0.22, 0.33, 0.04                       # 반폭 · 반높이 · 모서리 반지름
    vals = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            # 둥근 사각형 SDF
            qx, qy = abs(x) - (hw - r), abs(y) - (hh - r)
            d = math.hypot(max(qx, 0.0), max(qy, 0.0)) + min(max(qx, qy), 0.0) - r
            if d > 0.03:
                vals.append(0.0)
                continue
            inside = 1.0 - smoothstep(-0.003, 0.003, d)
            border = math.exp(-((d + 0.008) / 0.007) ** 2)                    # 바깥 테두리
            inner = math.exp(-((d + 0.040) / 0.004) ** 2) * 0.55              # 안쪽 가는 선
            face = 0.22 * inside
            # 가운데 다이아(마름모)
            dia = abs(x) / 0.10 + abs(y) / 0.15
            diamond = (1.0 - smoothstep(0.92, 1.0, dia)) * 0.95
            halo = 0.25 * math.exp(-max(d, 0.0) / 0.012) * (d > 0)          # 테두리 밖 은은한 빛
            vals.append(min(1.0, max(face, border, inner * inside, diamond * inside) + halo))
    save_mask(vals, size, size, name)


def claw(size=512, name="T_VFX_Claw.png"):
    # 드루이드 짐승 발톱: 나란한 호 세 줄(가운데가 가장 길고 굵다). slash_arc 와 같은 방식으로 그리고 줄마다 반지름 · 길이를 달리한다
    cx, cy = 0.5, 0.05
    marks = [(0.36, 0.060, 48, 132), (0.44, 0.075, 40, 140), (0.52, 0.060, 50, 130)]
    vals = [0.0] * (size * size)
    for rad, width, d0, d1 in marks:
        a0, a1 = math.radians(d0), math.radians(d1)
        for j in range(size):
            y = (j + 0.5) / size
            for i in range(size):
                x = (i + 0.5) / size
                ang = math.atan2(y - cy, x - cx)
                if ang < a0 or ang > a1:
                    continue
                t = (ang - a0) / (a1 - a0)
                half = width * (math.sin(math.pi * t) ** 0.7) * 0.5
                if half <= 1e-5:
                    continue
                r = math.hypot(x - cx, y - cy)
                k = (r - (rad - half)) / (2 * half)
                if k < -0.05 or k > 1.05:
                    continue
                edge = smoothstep(-0.05, 0.1, k) * (1.0 - smoothstep(0.9, 1.05, k))
                kc = min(1.0, max(0.0, k))
                v = ((1.0 - kc) ** 1.2 * 0.75 + 0.25) * (0.6 + 0.4 * math.sin(math.pi * t)) * edge
                idx = j * size + i
                vals[idx] = max(vals[idx], v)
    save_mask(vals, size, size, name, glow=0.01, glow_gain=0.45)


def stripe_aura(w=256, h=512, name="T_VFX_StripeAura.png"):
    # 가속 버프: 세로 줄무늬 원통 오라. 원통 가장자리가 밝고(림) 위아래는 페이드, 줄무늬 세기는 고정 시드로 고르지 않게
    rnd = random.Random(11)
    stripes = [(rnd.uniform(0.08, 0.92), rnd.uniform(0.006, 0.016), rnd.uniform(0.45, 1.0)) for _ in range(14)]
    vals = []
    for j in range(h):
        v = (j + 0.5) / h
        fade = smoothstep(0.0, 0.25, v) * (1.0 - smoothstep(0.55, 1.0, v))
        for i in range(w):
            u = (i + 0.5) / w
            c = abs(u - 0.5) * 2.0                       # 0 가운데 ~ 1 가장자리
            rim = 0.25 + 0.75 * c ** 3
            side = 1.0 - smoothstep(0.92, 1.0, c)
            s = max(a * math.exp(-((u - x) / wd) ** 2) for x, wd, a in stripes)
            vals.append(min(1.0, (0.12 * rim + 0.8 * s * (0.4 + 0.6 * rim)) * fade * side))
    save_mask(vals, w, h, name)


def guard_panel(size=512, name="T_VFX_GuardPanel.png"):
    # 방어 버프 결계 패널: 육각형, 가장자리가 밝고 안은 0.12 이하, 안쪽에 가는 두 번째 선
    R = 0.44
    vals = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            # 꼭짓점이 위아래인 정육각형 SDF
            ax, ay = abs(x), abs(y)
            d = max(ax * 0.8660254 + ay * 0.5, ay) - R * 0.8660254
            inside = 1.0 - smoothstep(-0.004, 0.004, d)
            edge = math.exp(-((d + 0.010) / 0.009) ** 2)
            inner = math.exp(-((d + 0.045) / 0.004) ** 2) * 0.45
            face = 0.10 * inside
            halo = 0.3 * math.exp(-max(d, 0.0) / 0.015) * (d > 0)
            vals.append(min(1.0, max(face, edge, inner * inside) + halo))
    save_mask(vals, size, size, name)


# ---------------------------------------------------------------------------
# 10/8 블록 D 3차 — 필살기 폴리싱. 절정(빛살 · 플레어 · 충격파) · 번개 · 룬 띠 · 할퀴기 · 카드 무늬는 가산용 마스크,
# 바위 · 흙먼지 · 나뭇잎 · 동전은 색이 있는 RGBA(알파 블렌드 재질 M_NU_SpriteAlphaFront 용 — 어두운 색을 낼 수 있다)
# ---------------------------------------------------------------------------

def _to_mask_img(img_l, name):
    Image.merge("RGBA", (img_l, img_l, img_l, img_l)).save(OUT_DIR / name)
    print("저장", name, img_l.size)


def _lighter(a, b):
    from PIL import ImageChops
    return ImageChops.lighter(a, b)


def _scale(img_l, k):
    return img_l.point(lambda v: int(min(255, v * k)))


def light_rays(size=512, name="T_VFX_LightRays.png", count=46, seed=21):
    # 절정의 방사 빛살: 가운데에서 바깥으로 길이 · 굵기 · 밝기가 제각각인 가는 삼각형 빛줄기. 천천히 돌려 쓴다
    rnd = random.Random(seed)
    s = 4
    S = size * s
    c = S / 2
    rays = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(rays)
    for i in range(count):
        a = (i + rnd.uniform(-0.35, 0.35)) / count * 2 * math.pi
        L = rnd.uniform(0.30, 0.50) * S
        hw = math.radians(rnd.uniform(0.9, 3.2))
        b = int(255 * rnd.uniform(0.45, 1.0))
        base = 0.02 * S
        pts = [(c + math.cos(a - hw * 2.5) * base, c + math.sin(a - hw * 2.5) * base),
               (c + math.cos(a) * L, c + math.sin(a) * L),
               (c + math.cos(a + hw * 2.5) * base, c + math.sin(a + hw * 2.5) * base)]
        layer = Image.new("L", (S, S), 0)
        ImageDraw.Draw(layer).polygon(pts, fill=b)
        rays = _lighter(rays, layer)
    wide = Image.new("L", (S, S), 0)
    for i in range(10):
        a = rnd.uniform(0, 2 * math.pi)
        L = rnd.uniform(0.32, 0.46) * S
        hw = math.radians(rnd.uniform(4.0, 8.0))
        layer = Image.new("L", (S, S), 0)
        ImageDraw.Draw(layer).polygon([(c, c), (c + math.cos(a - hw) * L * 0.4, c + math.sin(a - hw) * L * 0.4), (c + math.cos(a) * L, c + math.sin(a) * L),
                                       (c + math.cos(a + hw) * L * 0.4, c + math.sin(a + hw) * L * 0.4)], fill=int(255 * rnd.uniform(0.22, 0.34)))
        wide = _lighter(wide, layer)
    rays = _lighter(rays, wide.filter(ImageFilter.GaussianBlur(6 * s)))
    rays = rays.resize((size, size), Image.LANCZOS)
    # 바깥으로 갈수록 어두워지는 방사 감쇠 + 가운데 작은 글로우
    rad = Image.new("L", (size, size))
    vals = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y) / 0.5
            vals.append(int(255 * (1.0 - smoothstep(0.15, 1.0, r)) ** 0.8))
    rad.putdata(vals)
    from PIL import ImageChops
    out = ImageChops.multiply(rays, rad).filter(ImageFilter.GaussianBlur(0.8))
    glow = Image.new("L", (size, size))
    gv = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            gv.append(int(255 * 0.55 * math.exp(-(math.hypot(x, y) / 0.07) ** 2)))
    glow.putdata(gv)
    out = _lighter(out, glow)
    out = _lighter(out, _scale(out.filter(ImageFilter.GaussianBlur(5)), 0.6))
    _to_mask_img(out, name)


def flare_streak(size=512, name="T_VFX_FlareStreak.png"):
    # 가로 렌즈 플레어: 아주 얇은 밝은 선 + 조금 두꺼운 옅은 띠, 양 끝으로 갈수록 가늘고 어둡게
    vals = []
    for j in range(size):
        dy = abs((j + 0.5) / size - 0.5)
        for i in range(size):
            dx = abs((i + 0.5) / size - 0.5) / 0.5
            along = max(0.0, 1.0 - dx)
            core = math.exp(-(dy / (0.004 + 0.006 * along)) ** 2) * along ** 1.6
            band = 0.30 * math.exp(-(dy / (0.02 + 0.03 * along)) ** 2) * along ** 3
            center = 0.5 * math.exp(-(math.hypot(dx * 0.5, dy) / 0.05) ** 2)
            vals.append(min(1.0, core + band + center))
    save_mask(vals, size, size, name)


def shock_ring(size=512, name="T_VFX_ShockRing.png"):
    # 날 선 충격파: 바깥 가장자리가 칼처럼 밝게 끊기고 안쪽으로 옅게 빠진다(ThickRing 은 안쪽이 밝고 양쪽이 부드럽다)
    r_out, r_in = 0.465, 0.30
    vals = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            if r > r_out + 0.03 or r < r_in - 0.01:
                vals.append(0.0)
                continue
            k = max(0.0, min(1.0, (r - r_in) / (r_out - r_in)))
            body = k ** 2.4 * (1.0 - smoothstep(r_out - 0.004, r_out + 0.004, r))
            edge = math.exp(-((r - (r_out - 0.006)) / 0.006) ** 2) * 0.9
            outer = 0.25 * math.exp(-max(0.0, r - r_out) / 0.01) * (r > r_out)
            vals.append(min(1.0, body * 0.75 + edge + outer))
    save_mask(vals, size, size, name)


def _bolt_points(rnd, x0, y0, x1, y1, depth, amp, xmin, xmax):
    # 픽셀 좌표에서 가운데점 흔들기(가로 · 세로 비가 다른 텍스처라 정규화 좌표로 흔들면 찌그러진다)
    pts = [(x0, y0), (x1, y1)]
    for _ in range(depth):
        nxt = [pts[0]]
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            seg = math.hypot(bx - ax, by - ay)
            mx, my = (ax + bx) / 2, (ay + by) / 2
            nx, ny = -(by - ay) / (seg + 1e-6), (bx - ax) / (seg + 1e-6)
            off = rnd.uniform(-amp, amp) * seg
            nxt += [(min(xmax, max(xmin, mx + nx * off)), my + ny * off), (bx, by)]
        pts = nxt
    return pts


def bolt(w=256, h=1024, name="T_VFX_Bolt1.png", seed=1):
    # 지그재그 번개: 위에서 아래로 내리꽂는 본줄기 + 갈래 2~3. 흰 코어 + 두 겹 글로우. 머리(시작)는 텍스처 위쪽
    rnd = random.Random(seed)
    s = 2
    W, H = w * s, h * s
    xmin, xmax = 0.16 * W, 0.84 * W
    main = _bolt_points(rnd, 0.5 * W, 0.01 * H, (0.5 + rnd.uniform(-0.1, 0.1)) * W, 0.99 * H, 7, 0.17, xmin, xmax)
    branches = []
    for _ in range(rnd.randint(2, 3)):
        k = rnd.randint(len(main) // 8, len(main) * 6 // 10)
        bx, by = main[k]
        side = rnd.choice([-1, 1])
        L = rnd.uniform(0.10, 0.22) * H
        ex, ey = bx + side * rnd.uniform(0.15, 0.3) * W, by + L
        branches.append(_bolt_points(rnd, bx, by, min(xmax, max(xmin, ex)), ey, 5, 0.2, xmin, xmax))

    def draw(width_px, items):
        im = Image.new("L", (W, H), 0)
        dd = ImageDraw.Draw(im)
        for pts, wscale, b in items:
            dd.line(pts, fill=int(255 * b), width=max(1, int(width_px * wscale)), joint="curve")
        return im

    items = [(main, 1.0, 1.0)] + [(b, 0.6, 0.75) for b in branches]
    core = draw(5 * s, items)
    mid = draw(11 * s, items).filter(ImageFilter.GaussianBlur(4 * s))
    glow = draw(26 * s, items).filter(ImageFilter.GaussianBlur(14 * s))
    out = _lighter(_lighter(core, _scale(mid, 0.9)), _scale(glow, 0.55))
    out = out.resize((w, h), Image.LANCZOS)
    _to_mask_img(out, name)


def rune_ring(size=512, name="T_VFX_RuneRing.png", seed=5):
    # 룬 띠: 두 겹 가는 원 사이에 각진 문자(획 2~4)를 둘레로 늘어놓은 띠 + 안쪽 눈금 원. 룬 원(RuneCircle) 바깥에 겹쳐 반대로 돌린다
    rnd = random.Random(seed)
    s = 4
    S = size * s
    c = S / 2
    im = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(im)
    lw = int(0.0045 * S)
    for r in (0.470, 0.400):
        R = r * S
        d.ellipse([c - R, c - R, c + R, c + R], outline=235, width=lw)
    R = 0.372 * S
    d.ellipse([c - R, c - R, c + R, c + R], outline=150, width=max(1, lw // 2))
    for k in range(72):                      # 안쪽 눈금
        a = k / 72 * 2 * math.pi
        r0, r1 = 0.372 * S, (0.386 if k % 3 == 0 else 0.380) * S
        d.line([(c + math.cos(a) * r0, c + math.sin(a) * r0), (c + math.cos(a) * r1, c + math.sin(a) * r1)], fill=170, width=max(1, lw // 2))
    n = 26
    for k in range(n):                       # 문자
        a = (k + 0.5) / n * 2 * math.pi
        rc = 0.435 * S
        cx, cy = c + math.cos(a) * rc, c + math.sin(a) * rc
        tx, ty = -math.sin(a), math.cos(a)  # 접선
        nx, ny = math.cos(a), math.sin(a)   # 법선
        gw, gh = 0.022 * S, 0.026 * S
        corners = [(-1, -1), (1, -1), (1, 1), (-1, 1), (0, -1), (0, 1), (-1, 0), (1, 0), (0, 0)]
        for _ in range(rnd.randint(2, 4)):
            p, q = rnd.sample(corners, 2)
            P = (cx + tx * p[0] * gw + nx * p[1] * gh, cy + ty * p[0] * gw + ny * p[1] * gh)
            Q = (cx + tx * q[0] * gw + nx * q[1] * gh, cy + ty * q[0] * gw + ny * q[1] * gh)
            d.line([P, Q], fill=245, width=lw)
    out = im.resize((size, size), Image.LANCZOS)
    out = _lighter(out, _scale(out.filter(ImageFilter.GaussianBlur(3)), 0.7))
    _to_mask_img(out, name)


def bokeh_soft(size=256, name="T_VFX_BokehSoft.png"):
    # 보케 원판(부드러운 판): BokehDisc 는 테두리가 밝아 비눗방울처럼 보였다 — 테두리는 아주 약하게, 가장자리는 넓게 번진다
    vals = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y) / 0.46
            body = 1.0 - smoothstep(0.78, 1.0, r)
            rim = 0.10 * math.exp(-((r - 0.86) / 0.07) ** 2)
            vals.append(min(1.0, body * (0.62 + 0.12 * r) + rim))
    save_mask(vals, size, size, name)


def claw_rake(size=512, name="T_VFX_ClawRake.png", width=0.085, flat=False, glow=0.012, glow_gain=0.5):
    # 큰 발톱 할퀴기: 세로로 긴 세 줄(살짝 휘고, 위 1/3 에서 가장 굵고 양 끝은 바늘). 회전은 스프라이트가 준다
    strokes = [(-0.13, 0.12, 0.84, 0.85), (0.0, 0.06, 0.94, 1.0), (0.13, 0.14, 0.86, 0.85)]
    vals = []
    for j in range(size):
        y = (j + 0.5) / size
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            v = 0.0
            for x0, y0, y1, b in strokes:
                if y < y0 or y > y1:
                    continue
                t = (y - y0) / (y1 - y0)
                cx = x0 + 0.06 * (t - 0.5) ** 2 - 0.015 + 0.03 * t   # 살짝 휜 획
                half = width * 0.5 * (math.sin(math.pi * min(1.0, t * 1.25 if t < 0.4 else 0.5 + (t - 0.4) / 1.2)) ** 0.7)
                if half <= 1e-5:
                    continue
                dx = abs(x - cx)
                if dx > half * 1.15:
                    continue
                k = dx / half
                edge = 1.0 - smoothstep(0.85, 1.12, k)
                body = 1.0 if flat else 0.55 + 0.45 * math.exp(-(k / 0.45) ** 2)
                v = max(v, body * edge * b * (0.6 + 0.4 * math.sin(math.pi * t)))
            vals.append(v)
    save_mask(vals, size, size, name, glow=glow, glow_gain=glow_gain)


def _suit_img(kind, S):
    im = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(im)
    f = 255
    if kind == "Diamond":
        d.polygon([(0.5 * S, 0.08 * S), (0.80 * S, 0.5 * S), (0.5 * S, 0.92 * S), (0.20 * S, 0.5 * S)], fill=f)
    elif kind == "Heart":
        r = 0.20 * S
        d.ellipse([0.30 * S - r, 0.36 * S - r, 0.30 * S + r, 0.36 * S + r], fill=f)
        d.ellipse([0.70 * S - r, 0.36 * S - r, 0.70 * S + r, 0.36 * S + r], fill=f)
        d.polygon([(0.115 * S, 0.43 * S), (0.885 * S, 0.43 * S), (0.5 * S, 0.88 * S)], fill=f)
        d.polygon([(0.30 * S, 0.30 * S), (0.70 * S, 0.30 * S), (0.70 * S, 0.50 * S), (0.30 * S, 0.50 * S)], fill=f)
    elif kind == "Spade":
        r = 0.19 * S
        d.ellipse([0.31 * S - r, 0.56 * S - r, 0.31 * S + r, 0.56 * S + r], fill=f)
        d.ellipse([0.69 * S - r, 0.56 * S - r, 0.69 * S + r, 0.56 * S + r], fill=f)
        d.polygon([(0.13 * S, 0.50 * S), (0.87 * S, 0.50 * S), (0.5 * S, 0.08 * S)], fill=f)
        d.polygon([(0.31 * S, 0.50 * S), (0.69 * S, 0.50 * S), (0.69 * S, 0.66 * S), (0.31 * S, 0.66 * S)], fill=f)
        d.polygon([(0.5 * S, 0.58 * S), (0.64 * S, 0.92 * S), (0.36 * S, 0.92 * S)], fill=f)
    elif kind == "Club":
        r = 0.16 * S
        for cx, cy in ((0.5, 0.28), (0.29, 0.56), (0.71, 0.56)):
            d.ellipse([cx * S - r, cy * S - r, cx * S + r, cy * S + r], fill=f)
        d.polygon([(0.5 * S, 0.45 * S), (0.64 * S, 0.92 * S), (0.36 * S, 0.92 * S)], fill=f)
        d.rectangle([0.42 * S, 0.40 * S, 0.58 * S, 0.62 * S], fill=f)
    return im


def suit(kind, size=256):
    # 카드 무늬(가산 마스크): 면은 0.75, 가장자리 안쪽 선이 밝고 둘레에 옅은 빛
    s = 4
    S = size * s
    full = _suit_img(kind, S)
    inner = full.filter(ImageFilter.MinFilter(int(0.05 * S) | 1))
    from PIL import ImageChops
    edge = ImageChops.subtract(full, inner)
    body = _lighter(_scale(full, 0.70), edge)
    body = body.resize((size, size), Image.LANCZOS)
    body = _lighter(body, _scale(body.filter(ImageFilter.GaussianBlur(6)), 0.6))
    _to_mask_img(body, "T_VFX_Suit%s.png" % kind)


# ---------- 색이 있는 RGBA (알파 블렌드) ----------

def _save_rgba(img, name):
    img.save(OUT_DIR / name)
    print("저장", name, img.size)


def _lerp_c(a, b, t):
    return tuple(int(round(a[k] + (b[k] - a[k]) * t)) for k in range(len(a)))


def rock_spike_color(w=256, h=512, name="T_VFX_RockSpikeA.png", lean=0.0, seed=3, mirror=False):
    # 드루이드 바위 가시(색): 밝은 왼쪽 면 · 가운데 면 · 어두운 오른쪽 면 + 능선 하이라이트 + 금 + 바닥 그늘 + 어두운 외곽선.
    # lean 은 끝이 옆으로 기운 정도(텍스처에 기울기를 구워 둔다 — 스프라이트 회전이 안 되는 이미터라서)
    rnd = random.Random(seed)
    s = 2
    W, H = w * s, h * s
    tipx = 0.5 + lean
    base_l, base_r = 0.08, 0.92
    # 외곽 꺾임점(바닥 → 끝) — 왼쪽 · 오른쪽 가장자리
    def edge_pts(side):
        pts = []
        n = 6
        for k in range(n + 1):
            t = k / n
            xb = base_l if side < 0 else base_r
            x = xb + (tipx - xb) * t + side * rnd.uniform(-0.03, 0.035) * (1 - t) * (k not in (0, n))
            y = 0.97 - t * 0.93
            pts.append((x, y))
        return pts
    L = edge_pts(-1)
    R = edge_pts(1)
    sil = L + list(reversed(R))
    ridge_base = 0.40 + lean * 0.5 + rnd.uniform(-0.04, 0.04)
    ridge2_base = 0.66 + lean * 0.3
    tip = (tipx, 0.04)
    light = (176, 160, 132)
    mid = (128, 114, 94)
    dark = (78, 68, 58)
    hi = (224, 212, 186)
    line = (46, 40, 34)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    P = lambda pts: [(x * W, y * H) for x, y in pts]
    d.polygon(P(sil), fill=mid + (255,))
    # 왼쪽 면(밝게): 왼쪽 가장자리 ~ 능선
    d.polygon(P(L + [tip, (ridge_base, 0.97)]), fill=light + (255,))
    # 오른쪽 면(어둡게): 둘째 능선 ~ 오른쪽 가장자리
    d.polygon(P([(ridge2_base, 0.97), tip] + list(reversed(R))), fill=dark + (255,))
    # 아래쪽 깨진 작은 면
    d.polygon(P([(0.18, 0.97), (0.30, 0.70), (ridge_base, 0.97)]), fill=_lerp_c(light, mid, 0.5) + (255,))
    # 능선 하이라이트와 금
    d.line(P([tip, (ridge_base, 0.97)]), fill=hi + (255,), width=3 * s)
    d.line(P(L), fill=_lerp_c(hi, light, 0.4) + (255,), width=2 * s)
    for _ in range(4):
        y0 = rnd.uniform(0.35, 0.85)
        x0 = rnd.uniform(0.25, 0.7)
        pts = [(x0, y0)]
        for _ in range(3):
            x0 += rnd.uniform(-0.05, 0.05)
            y0 += rnd.uniform(0.03, 0.07)
            pts.append((x0, y0))
        d.line(P(pts), fill=line + (255,), width=max(1, s))
    d.line(P(sil + [sil[0]]), fill=line + (255,), width=2 * s)
    img = img.resize((w, h), Image.LANCZOS)
    # 바닥 그늘(아래 18% 를 어둡게) + 아래 5% 알파 페이드(흙에 묻힌 것처럼)
    px = img.load()
    for j in range(h):
        v = (j + 0.5) / h
        shade = 1.0 - 0.45 * smoothstep(0.80, 0.98, v)
        af = 1.0 - smoothstep(0.93, 0.995, v)
        for i in range(w):
            r, g, b, a = px[i, j]
            if a:
                px[i, j] = (int(r * shade), int(g * shade), int(b * shade), int(a * af))
    if mirror:
        img = img.transpose(Image.FLIP_LEFT_RIGHT)
    _save_rgba(img, name)


def dust_puff(size=256, name="T_VFX_DustPuff.png", seed=9):
    # 흙먼지 덩어리(색): 부드러운 원 여러 개를 겹친 구름, 위쪽이 밝고 아래가 어둡다. 알파는 가장자리로 부드럽게
    rnd = random.Random(seed)
    blobs = [(0.5, 0.56, 0.24)] + [(0.5 + rnd.uniform(-0.2, 0.2), 0.52 + rnd.uniform(-0.14, 0.1), rnd.uniform(0.11, 0.18)) for _ in range(7)]
    top, bot = (198, 180, 150), (122, 104, 84)
    img = Image.new("RGBA", (size, size))
    px = img.load()
    for j in range(size):
        y = (j + 0.5) / size
        for i in range(size):
            x = (i + 0.5) / size
            a = 0.0
            for bx, by, br in blobs:
                dd = math.hypot(x - bx, y - by) / br
                a = max(a, 1.0 - smoothstep(0.45, 1.0, dd))
            if a <= 0:
                px[i, j] = (0, 0, 0, 0)
                continue
            col = _lerp_c(top, bot, smoothstep(0.3, 0.8, y))
            px[i, j] = col + (int(255 * 0.85 * a),)
    img = img.filter(ImageFilter.GaussianBlur(1.5))
    _save_rgba(img, name)


def leaf(size=128, name="T_VFX_Leaf.png"):
    # 나뭇잎(색): 끝이 뾰족한 잎 + 밝은 잎맥 + 어두운 외곽선. 머리(끝)가 위쪽
    s = 4
    S = size * s
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pts = []
    for k in range(41):
        t = k / 40
        y = 0.08 + 0.84 * t
        wdt = 0.30 * math.sin(math.pi * t) ** 0.8
        pts.append((0.5 - wdt, y))
    pts += [(0.5 + (0.5 - x), y) for x, y in reversed(pts)]
    base, tipc, rib, outline = (82, 156, 62), (138, 204, 92), (176, 228, 128), (36, 82, 32)
    d.polygon([(x * S, y * S) for x, y in pts], fill=base + (255,))
    d.polygon([(x * S, y * S) for x, y in pts[:41]] + [(0.5 * S, 0.92 * S)], fill=tipc + (255,))
    d.line([(0.5 * S, 0.10 * S), (0.5 * S, 0.97 * S)], fill=rib + (255,), width=3 * s)
    for k in range(1, 5):
        y = 0.2 + k * 0.14
        for sgn in (-1, 1):
            d.line([(0.5 * S, y * S), ((0.5 + sgn * 0.18) * S, (y - 0.08) * S)], fill=rib + (220,), width=2 * s)
    d.line([(x * S, y * S) for x, y in pts + [pts[0]]], fill=outline + (255,), width=2 * s)
    img = img.resize((size, size), Image.LANCZOS)
    _save_rgba(img, name)


def coin(size=128, name="T_VFX_Coin.png"):
    # 금화(색): 밝은 테두리 · 안쪽 면 그라데이션 · 가운데 다이아 각인 · 왼쪽 위 하이라이트 · 어두운 외곽선
    img = Image.new("RGBA", (size, size))
    px = img.load()
    rim_c, face_hi, face_lo, emb, hl, out_c = (255, 222, 116), (246, 196, 70), (196, 132, 30), (255, 238, 160), (255, 252, 222), (120, 76, 14)
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y) / 0.46
            if r > 1.03:
                px[i, j] = (0, 0, 0, 0)
                continue
            a = 1.0 - smoothstep(0.97, 1.03, r)
            if r > 0.90:
                col = out_c if r > 0.97 else rim_c
            elif r > 0.80:
                col = _lerp_c(face_lo, rim_c, 0.3)
            else:
                col = _lerp_c(face_hi, face_lo, smoothstep(-0.4, 0.5, x + y))
                if abs(x) / 0.12 + abs(y) / 0.17 < 1.0:
                    col = emb if (x + y) < 0 else _lerp_c(emb, face_lo, 0.5)
            hlv = math.exp(-((x + 0.17) ** 2 + (y + 0.19) ** 2) / 0.006) * 0.8 * (r < 0.9)
            col = _lerp_c(col, hl, hlv)
            px[i, j] = col + (int(255 * a),)
    _save_rgba(img, name)


D3_TEXTURES = [
    light_rays, flare_streak, shock_ring,
    lambda: bolt(name="T_VFX_Bolt1.png", seed=1), lambda: bolt(name="T_VFX_Bolt2.png", seed=4), lambda: bolt(name="T_VFX_Bolt3.png", seed=12),
    rune_ring, bokeh_soft,
    claw_rake, lambda: claw_rake(name="T_VFX_ClawRakeCore.png", width=0.030, flat=True, glow=0.006, glow_gain=0.3),
    lambda: suit("Diamond"), lambda: suit("Heart"), lambda: suit("Spade"), lambda: suit("Club"),
    rock_spike_color, lambda: rock_spike_color(name="T_VFX_RockSpikeB.png", lean=0.12, seed=8),
    lambda: rock_spike_color(name="T_VFX_RockSpikeC.png", lean=0.12, seed=8, mirror=True),
    dust_puff, leaf, coin,
]


# ---------- 10/8 블록 F — 나이아 속성 임팩트 ----------

def splash_crown(size=512, name="T_VFX_SplashCrown.png", seed=17):
    # 물보라 왕관: 거품 띠(아래 타원)에서 바깥으로 휘며 솟는 물줄기 11개 + 떨어져 나간 물방울. 면은 반투명(0.55),
    # 물줄기 바깥 가장자리만 밝게 — 반듯한 왕관 아이콘처럼 보이지 않게 높이 · 굵기 · 휨을 제각각
    rnd = random.Random(seed)
    s = 4
    S = size * s
    body = Image.new("L", (S, S), 0)
    rim = Image.new("L", (S, S), 0)
    db, dr = ImageDraw.Draw(body), ImageDraw.Draw(rim)
    cx, by = 0.5, 0.80
    for k in range(26):                       # 거품 띠
        a = k / 26 * 2 * math.pi
        x, y = cx + math.cos(a) * 0.34, by + math.sin(a) * 0.06
        r = rnd.uniform(0.02, 0.04)
        db.ellipse([(x - r) * S, (y - r * 0.6) * S, (x + r) * S, (y + r * 0.6) * S], fill=140)
    n = 11
    for k in range(n):
        u = (k + 0.5) / n + rnd.uniform(-0.02, 0.02)
        x0 = cx + (u - 0.5) * 0.64
        y0 = by - 0.03 * math.sin(math.pi * u)
        h = rnd.uniform(0.16, 0.46) * (0.55 + 0.45 * math.sin(math.pi * u))
        out = (u - 0.5) * rnd.uniform(0.6, 1.0)
        w0 = rnd.uniform(0.018, 0.032)
        left, right = [], []
        steps = 14
        for i in range(steps + 1):
            t = i / steps
            x = x0 + out * h * (t * t) * 0.9          # 끝으로 갈수록 바깥으로 휜다
            y = y0 - h * t
            w = w0 * (1.0 - t) ** 0.8 + 0.002
            dx = out * h * 2 * t * 0.9
            dy = -h
            ln = math.hypot(dx, dy) + 1e-6
            nx, ny = -dy / ln, dx / ln
            left.append(((x + nx * w) * S, (y + ny * w) * S))
            right.append(((x - nx * w) * S, (y - ny * w) * S))
        poly = left + list(reversed(right))
        db.polygon(poly, fill=150)
        dr.line(left if out > 0 else right, fill=235, width=max(2, int(0.006 * S)))
        tx, ty = (left[-1][0] + right[-1][0]) / 2 / S, (left[-1][1] + right[-1][1]) / 2 / S
        for m in range(rnd.randint(1, 2)):        # 떨어져 나간 물방울
            r = rnd.uniform(0.008, 0.018)
            px = tx + out * rnd.uniform(0.02, 0.07)
            py = ty - rnd.uniform(0.02, 0.08)
            dr.ellipse([(px - r) * S, (py - r) * S, (px + r) * S, (py + r) * S], fill=240)
    out_img = _lighter(body.filter(ImageFilter.GaussianBlur(2 * s)), rim)
    out_img = out_img.resize((size, size), Image.LANCZOS)
    out_img = _lighter(out_img, _scale(out_img.filter(ImageFilter.GaussianBlur(4)), 0.5))
    _to_mask_img(out_img, name)


def droplet(size=128, name="T_VFX_Droplet.png"):
    # 물방울(속도 정렬용): 둥근 머리가 위(진행 방향), 아래로 가늘어지는 꼬리. 머리 왼쪽 위에 밝은 반점
    vals = []
    hx, hy, hr = 0.5, 0.30, 0.17
    for j in range(size):
        y = (j + 0.5) / size
        for i in range(size):
            x = (i + 0.5) / size
            dh = math.hypot(x - hx, y - hy) / hr
            v = 0.0
            if dh < 1.08:
                v = (1.0 - smoothstep(0.9, 1.08, dh)) * (0.75 + 0.25 * dh)
            if y > hy:
                t = (y - hy) / (0.94 - hy)
                if t <= 1.0:
                    hw = hr * (1.0 - t) ** 1.3
                    if hw > 1e-4:
                        e = 1.0 - smoothstep(hw * 0.8, hw * 1.05, abs(x - hx))
                        v = max(v, e * (0.8 - 0.4 * t))
            spec = 0.6 * math.exp(-(((x - 0.45) ** 2 + (y - 0.25) ** 2) / 0.0012))
            vals.append(min(1.0, v + spec))
    save_mask(vals, size, size, name, glow=0.02, glow_gain=0.4)


def wind_arc(size=512, name="T_VFX_WindArc.png", strands=((0.42, 1.0, 0.018), (0.37, 0.55, 0.008), (0.46, 0.45, 0.006))):
    # 바람결 호(회오리 띠용): 원을 따라 도는 가는 바람결 두 갈래(180° 마주 봄). 머리(반시계 방향 끝)가 굵고 밝고,
    # 꼬리로 갈수록 가늘고 옅어진다. 가닥마다 반지름 · 밝기 · 굵기가 달라 여러 줄 바람결로 읽힌다. 바닥면에 눕혀 돌린다
    vals = []
    span = math.radians(150)
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            a = math.atan2(y, x)
            v = 0.0
            for arm in (0.0, math.pi):
                d = (a - arm) % (2 * math.pi)      # 머리(0)에서 꼬리 방향으로 잰 각
                if d > span:
                    continue
                t = d / span                       # 0 = 머리, 1 = 꼬리 끝
                for rad, b, w in strands:
                    hw = w * min(1.0, t / 0.12) ** 0.6 * (1.0 - t) ** 0.9 + 0.0008   # 머리는 뾰족하게 시작해 곧 가장 굵다
                    k = abs(r - rad) / hw
                    if k < 1.3:
                        v = max(v, b * (1.0 - smoothstep(0.7, 1.3, k)) * (1.0 - t) ** 1.2)
            vals.append(v)
    save_mask(vals, size, size, name, glow=0.01, glow_gain=0.5)


def flame_lick(size=256, name="T_VFX_FlameLick.png"):
    # 갈라진 불꽃(만화풍): 넓은 밑동에서 가운데 큰 혀 + 양옆 작은 혀가 바깥으로 휘며 뾰족하게 솟는다. 심(心)이 밝다. 위가 머리
    s = 4
    S = size * s
    def tongue(img, base_x, base_w, tip_x, tip_y, bend, fill):
        pts_l, pts_r = [], []
        steps = 24
        for i in range(steps + 1):
            t = i / steps
            y = 0.90 - (0.90 - tip_y) * t
            cx = base_x + (tip_x - base_x) * t + bend * math.sin(math.pi * t)
            w = base_w * (1.0 - t) ** 1.15 * (1.0 + 0.25 * math.sin(math.pi * t * 0.8))
            pts_l.append(((cx - w) * S, y * S))
            pts_r.append(((cx + w) * S, y * S))
        ImageDraw.Draw(img).polygon(pts_l + list(reversed(pts_r)), fill=fill)
    outer = Image.new("L", (S, S), 0)
    ImageDraw.Draw(outer).ellipse([0.30 * S, 0.74 * S, 0.70 * S, 0.96 * S], fill=170)
    tongue(outer, 0.50, 0.17, 0.52, 0.06, 0.05, 170)
    tongue(outer, 0.38, 0.09, 0.24, 0.30, -0.04, 170)
    tongue(outer, 0.62, 0.09, 0.78, 0.36, 0.04, 170)
    core = Image.new("L", (S, S), 0)
    ImageDraw.Draw(core).ellipse([0.38 * S, 0.76 * S, 0.62 * S, 0.93 * S], fill=255)
    tongue(core, 0.50, 0.09, 0.51, 0.30, 0.03, 255)
    img = _lighter(outer.filter(ImageFilter.GaussianBlur(1.5 * s)), core.filter(ImageFilter.GaussianBlur(3 * s)))
    img = img.resize((size, size), Image.LANCZOS)
    img = _lighter(img, _scale(img.filter(ImageFilter.GaussianBlur(5)), 0.5))
    _to_mask_img(img, name)


def heal_arc(size=512, name="T_VFX_HealArc.png"):
    # 회복 빛줄기: 캐릭터를 감싸 도는 가는 빛 한 가닥(240°). 머리가 뾰족하게 시작해 곧 가장 굵고 밝으며 꼬리로 가늘고 옅어진다.
    # 안쪽에 아주 옅은 둘째 가닥(160°). 바닥면에 눕혀 돌린다 — 바람결(WindArc)보다 훨씬 가늘고 부드럽다
    vals = []
    strands = ((0.43, 1.0, 0.010, math.radians(240)), (0.395, 0.35, 0.005, math.radians(160)))
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            d = (math.atan2(y, x)) % (2 * math.pi)
            v = 0.0
            for rad, b, w, span in strands:
                if d > span:
                    continue
                t = d / span
                hw = w * min(1.0, t / 0.08) ** 0.6 * (1.0 - t) ** 0.8 + 0.0006
                k = abs(r - rad) / hw
                if k < 1.4:
                    v = max(v, b * (1.0 - smoothstep(0.6, 1.4, k)) * (1.0 - t) ** 1.4)
            vals.append(v)
    save_mask(vals, size, size, name, glow=0.012, glow_gain=0.6)


F_TEXTURES = [splash_crown, droplet, wind_arc, flame_lick, heal_arc]


def laser_streak(w=128, h=1024, name="T_VFX_LaserStreak.png"):
    # 10/13 피격 빛줄기 (파이널 판타지 레조넌스 참고): 대상을 가로지르는 아주 가는 빛줄. 가운데 흰 심이 가장 밝고
    # 둘레에 옅은 번짐, 양 끝으로 갈수록 가늘고 옅어진다. 색은 입자 색이 입힘 (HDR 하늘색이면 심은 흰빛으로 탄다)
    vals = []
    for j in range(h):
        t = (j + 0.5) / h
        along = math.sin(math.pi * t) ** 0.7          # 양 끝으로 가늘어짐
        for i in range(w):
            x = abs((i + 0.5) / w - 0.5)
            core_w = 0.022 * along + 0.002
            core = 1.0 - smoothstep(core_w * 0.4, core_w, x)
            halo = math.exp(-(x / (0.11 * along + 0.01)) ** 2) * 0.32
            vals.append(max(core, halo) * along ** 0.5)
    save_mask(vals, w, h, name)


def pixel_square(size=64, name="T_VFX_PixelSquare.png"):
    # 10/13 피격 뒤 흩어지는 흰 네모 (레조넌스 참고): 가장자리가 또렷한 네모에 아주 얇은 번짐
    vals = []
    half = 0.30
    for j in range(size):
        y = abs((j + 0.5) / size - 0.5)
        for i in range(size):
            x = abs((i + 0.5) / size - 0.5)
            d = max(x, y)
            vals.append(1.0 - smoothstep(half - 0.02, half + 0.01, d))
    save_mask(vals, size, size, name, glow=0.08, glow_gain=0.45)


R_TEXTURES = [laser_streak, pixel_square]


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1 and sys.argv[1] in ("d3", "f", "r"):
        for fn in {"d3": D3_TEXTURES, "f": F_TEXTURES, "r": R_TEXTURES}[sys.argv[1]]:
            fn()
        raise SystemExit
    crescent()
    light_shaft()
    shard_chip()
    thick_ring()
    # 10/7 폴리싱 — 베기는 얇고 끝이 가는 호로(색 층 + 흰 코어 층이 같은 호를 공유)
    slash_arc()
    slash_arc(name="T_VFX_SlashArcCore.png", width=0.030, power=0.9, glow=0.006, glow_gain=0.3, flat=True, inset=0.026)
    # 10/7 트래커 다듬기 — 빛 화살과 곧은 속도선
    arrow_bolt()
    speed_line()
    # 10/7 갬블러 — 카드
    card()
    # 10/8 드루이드 — 발톱
    claw()
    # 10/8 블록 E — 자기 버프
    stripe_aura()
    guard_panel()
    # 10/8 블록 D 3차 — 필살기
    for fn in D3_TEXTURES:
        fn()
    # 10/8 블록 F — 나이아 속성
    for fn in F_TEXTURES:
        fn()
    # 10/13 피격 (레조넌스 참고)
    for fn in R_TEXTURES:
        fn()
