"""10/14 물 속성 임팩트 다듬기 · 노비스 주먹 기본 공격용 마스크 텍스처 (R=G=B=A=밝기, 가산 머티리얼 M_NU_SpriteFront 용).

규칙은 make_vfx_textures_v3.py 와 같다 (배경 투명, 방향이 있는 모양은 머리가 텍스처 위쪽).

만드는 것:
- T_VFX_WaterStreak 128x128  날아가는 물줄기(속도 정렬용). 작은 머리가 위, 꼬리가 길고 가늘다 — 물방울보다 길어 '튀어 나가는 물' 로 읽힘
- T_VFX_WaterBlob   128x128  울퉁불퉁한 물 덩어리. 가장자리가 밝은 수면 테두리, 안은 옅게 — 왕관 대신 여러 개가 솟았다 떨어지며 물보라가 됨
- T_VFX_FoamRing    512x512  바닥에 번지는 물결 고리. 반지름이 낮은 주파수로 흔들리고 거품 마디가 끊겨, 반듯한 원이 커지는 것처럼 안 보이게.
                             안쪽은 옅은 물막이 가운데로 빠짐
- T_VFX_LensRim     512x512  주먹 앞 렌즈 막의 테두리. 가는 흰 코어 + 바깥으로 빠지는 글로우, 둘레를 따라 밝기가 조금씩 다름
- T_VFX_LensSwirl   512x512  렌즈 막 안의 소용돌이 기류. 테두리 쪽으로 갈수록 진한 나선 가닥들 + 아주 옅은 막
- T_VFX_LensWave    512x512  주먹 파동 렌즈(10/14 2차). 밝기가 가운데 0.15 -> 테두리 1.0 으로 오른다 — 머티리얼 침식(NUA_Erode)이
                             어두운 픽셀부터 지우므로 가운데가 먼저 사라지고 바깥으로 뻗는 줄기 -> 테두리 순으로 남는다
- T_VFX_WaterPetal  256x512  물보라 꽃잎 한 장(피벗 아래 가운데). 뿌리에서 오른쪽으로 휘며 가늘어지는 혀 모양 물막, 끝에 물방울.
                             가장자리 선이 밝고 막은 옅어 침식되면 막부터 찢어지고 테두리 물줄기가 남는다

사용법:  python ArtSource/VFX/Job/V3/make_vfx_textures_water_fist.py
출력:    같은 폴더의 T_VFX_*.png
"""

import math
import random

from PIL import Image, ImageDraw, ImageFilter

from make_vfx_textures_v3 import OUT_DIR, _lighter, _scale, _to_mask_img, save_mask, smoothstep


def water_streak(size=128, name="T_VFX_WaterStreak.png"):
    # 머리(반지름 0.045, y 0.08)에서 아래로 0.98 까지 길게 가늘어지는 물줄기. 머리가 크면 올챙이처럼 읽혀서(10/7 트래커 교훈)
    # 머리는 꼬리보다 조금만 굵게 하고, 꼬리 가운데 줄을 밝게 해 물줄기의 윤기를 냄
    vals = []
    hx, hy, hr = 0.5, 0.08, 0.045
    for j in range(size):
        y = (j + 0.5) / size
        for i in range(size):
            x = (i + 0.5) / size
            dh = math.hypot(x - hx, y - hy) / hr
            v = 0.0
            if dh < 1.15:
                v = 1.0 - smoothstep(0.7, 1.15, dh)
            if y > hy:
                t = (y - hy) / (0.98 - hy)
                if t <= 1.0:
                    hw = hr * 0.9 * (1.0 - t) ** 0.9
                    if hw > 1e-4:
                        e = 1.0 - smoothstep(hw * 0.6, hw * 1.15, abs(x - hx))
                        v = max(v, e * (0.9 - 0.7 * t))
            vals.append(min(1.0, v))
    save_mask(vals, size, size, name, glow=0.015, glow_gain=0.4)


def water_blob(size=128, name="T_VFX_WaterBlob.png", seed=4):
    # 반지름을 몇 개의 사인으로 흔든 덩어리. 테두리 띠(밝음) + 안쪽 옅은 면 + 왼쪽 위 반사광
    rnd = random.Random(seed)
    waves = [(rnd.randint(2, 6), rnd.uniform(0.02, 0.05), rnd.uniform(0, 6.28)) for _ in range(4)]
    vals = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            a = math.atan2(y, x)
            R = 0.36 + sum(k * math.sin(f * a + p) for f, k, p in waves) * 0.5
            d = r / R
            fill = (1.0 - smoothstep(0.9, 1.02, d)) * (0.22 + 0.35 * d * d)
            rim = math.exp(-((d - 0.93) / 0.07) ** 2) * 0.75
            spec = 0.6 * math.exp(-(((x + 0.12) ** 2 + (y + 0.13) ** 2) / 0.003))
            vals.append(min(1.0, max(fill, rim) + spec * (1.0 - smoothstep(0.8, 1.0, d))))
    save_mask(vals, size, size, name, glow=0.03, glow_gain=0.3)


def foam_ring(size=512, name="T_VFX_FoamRing.png", seed=11):
    # 반지름 0.40 의 물결 고리. 바깥 가장자리(진행 쪽)가 날카롭고 밝고 안쪽으로 부드럽게 빠짐.
    # 거품 마디: 둘레 방향 밝기를 여러 사인 곱으로 흔들어 끊긴 듯한 마디를 만들고, 반지름도 흔들어 반듯한 원을 피함
    rnd = random.Random(seed)
    rw = [(rnd.randint(3, 9), rnd.uniform(0.002, 0.006), rnd.uniform(0, 6.28)) for _ in range(4)]
    bw = [(rnd.randint(5, 17), rnd.uniform(0, 6.28)) for _ in range(4)]
    vals = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            a = math.atan2(y, x)
            R = 0.40 + sum(k * math.sin(f * a + p) for f, k, p in rw)
            d = r - R
            edge = math.exp(-(d / 0.013) ** 2) if d > 0 else math.exp(-(d / 0.045) ** 2)
            b = 1.0
            for f, p in bw:
                b *= 0.75 + 0.25 * math.sin(f * a + p)
            b = 0.5 + 0.5 * smoothstep(0.2, 0.7, b)
            sheet = 0.16 * smoothstep(0.08, R, r) * (1.0 - smoothstep(R - 0.01, R + 0.005, r))
            vals.append(min(1.0, edge * b + sheet))
    save_mask(vals, size, size, name, glow=0.008, glow_gain=0.45)


def lens_rim(size=512, name="T_VFX_LensRim.png", seed=3):
    # 반지름 0.44 의 가는 테두리(코어 반폭 0.006) + 바깥 글로우. 둘레 밝기는 0.75~1 로 천천히 흔들림
    rnd = random.Random(seed)
    bw = [(rnd.randint(2, 5), rnd.uniform(0, 6.28)) for _ in range(3)]
    vals = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            a = math.atan2(y, x)
            d = r - 0.44
            core = math.exp(-(d / 0.006) ** 2)
            halo = 0.45 * math.exp(-(d / 0.030) ** 2)
            inner = 0.10 * smoothstep(0.20, 0.44, r) * (1.0 - smoothstep(0.43, 0.45, r))
            b = 1.0
            for f, p in bw:
                b *= 0.92 + 0.08 * math.sin(f * a + p)
            vals.append(min(1.0, (core + halo) * b + inner))
    save_mask(vals, size, size, name)


def lens_swirl(size=512, name="T_VFX_LensSwirl.png", seed=8):
    # 나선 가닥: 반지름 0.12~0.42 에서 시작해 각도를 따라 바깥으로 조금씩 감기는 호. 가닥마다 길이 · 굵기 · 밝기가 다름.
    # 머리(진행 끝)가 밝고 꼬리가 옅다. 마지막에 아주 옅은 막(0.08, 테두리 쪽 진함)을 깐다
    rnd = random.Random(seed)
    s = 3
    S = size * s
    img = Image.new("L", (S, S), 0)
    dr = ImageDraw.Draw(img)
    for k in range(46):
        r0 = rnd.uniform(0.10, 0.40)
        a0 = rnd.uniform(0, 2 * math.pi)
        span = rnd.uniform(0.6, 1.8)
        grow = rnd.uniform(0.0, 0.06)
        w = rnd.uniform(0.006, 0.016) * (0.6 + r0)
        peak = rnd.uniform(170, 255) * (0.6 + 0.4 * r0 / 0.40)
        steps = 40
        prev = None
        for i in range(steps + 1):
            t = i / steps
            a = a0 + span * t
            r = min(0.43, r0 + grow * t)
            p = (0.5 + math.cos(a) * r) * S, (0.5 + math.sin(a) * r) * S
            if prev:
                v = int(peak * (0.15 + 0.85 * t ** 1.5))
                dr.line([prev, p], fill=v, width=max(1, int(w * S * (0.4 + 0.6 * t))))
            prev = p
    img = img.filter(ImageFilter.GaussianBlur(1.0 * s)).resize((size, size), Image.LANCZOS)
    img = _lighter(img, _scale(img.filter(ImageFilter.GaussianBlur(5)), 0.6))
    film = Image.new("L", (size, size))
    fv = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            fv.append(int(255 * 0.10 * smoothstep(0.05, 0.43, r) * (1.0 - smoothstep(0.42, 0.45, r))))
    film.putdata(fv)
    _to_mask_img(_lighter(img, film), name)


def lens_wave(size=512, name="T_VFX_LensWave.png", seed=21):
    # 테두리(반지름 0.44, 밝기 1) + 가운데에서 바깥으로 뻗는 줄기(살짝 휨, 밝기가 바깥으로 갈수록 셈) + 옅은 막.
    # 밝기 = 침식 순서: 막(가운데 0.12) -> 줄기 안쪽 -> 줄기 바깥쪽 -> 테두리
    rnd = random.Random(seed)
    s = 3
    S = size * s
    img = Image.new("L", (S, S), 0)
    dr = ImageDraw.Draw(img)
    for k in range(70):
        a0 = rnd.uniform(0, 2 * math.pi)
        r0 = rnd.uniform(0.04, 0.20)
        r1 = rnd.uniform(0.30, 0.425)
        bend = rnd.uniform(0.15, 0.35)
        w = rnd.uniform(0.004, 0.009)
        steps = 30
        prev = None
        for i in range(steps + 1):
            t = i / steps
            r = r0 + (r1 - r0) * t
            a = a0 + bend * (r / 0.44) ** 2
            p = (0.5 + math.cos(a) * r) * S, (0.5 + math.sin(a) * r) * S
            if prev:
                v = int(255 * min(0.82, 0.18 + 0.62 * (r / 0.44) ** 1.3))
                dr.line([prev, p], fill=v, width=max(1, int(w * S * (0.5 + 0.5 * t))))
            prev = p
    img = img.filter(ImageFilter.GaussianBlur(0.8 * s)).resize((size, size), Image.LANCZOS)
    base = Image.new("L", (size, size))
    bv = []
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            d = r - 0.44
            rim = math.exp(-(d / 0.007) ** 2) + 0.45 * math.exp(-(d / 0.025) ** 2)
            film = (0.12 + 0.28 * (r / 0.44) ** 2) * (1.0 - smoothstep(0.43, 0.45, r))
            bv.append(int(255 * min(1.0, max(rim, film))))
    base.putdata(bv)
    _to_mask_img(_lighter(base, img), name)


def water_petal(w=256, h=512, name="T_VFX_WaterPetal.png", seed=6):
    # 뿌리(아래 가운데, 폭 0.5)에서 위로 솟으며 오른쪽으로 살짝 휘고 가늘어지는 물막. 왼쪽 · 오른쪽 가장자리 선이 밝고(1),
    # 막(0.3)에는 세로 물결 줄무늬. 끝(위)에 떨어져 나가는 물방울 두 개
    rnd = random.Random(seed)
    s = 3
    W, H = w * s, h * s
    film = Image.new("L", (W, H), 0)
    edge = Image.new("L", (W, H), 0)
    df, de = ImageDraw.Draw(film), ImageDraw.Draw(edge)
    left, right, mid = [], [], []
    steps = 40
    for i in range(steps + 1):
        t = i / steps
        y = 0.97 - 0.84 * t
        cx = 0.40 + 0.30 * t * t
        hw = 0.17 * (1.0 - t) ** 0.6 * (1.0 + 0.3 * math.sin(math.pi * t)) + 0.01
        wob = 0.008 * math.sin(17 * t + 1.3)
        left.append(((cx - hw + wob) * W, y * H))
        right.append(((cx + hw - wob * 0.6) * W, y * H))
        mid.append((cx * W, y * H))
    df.polygon(left + list(reversed(right)), fill=95)
    for k in range(7):
        u = (k + 1) / 8
        pts = [(l[0] + (r[0] - l[0]) * u, l[1]) for l, r in zip(left, right)]
        de.line(pts[:int(steps * rnd.uniform(0.5, 0.95))], fill=int(255 * 0.5), width=max(1, int(0.010 * W)))
    de.line(left, fill=255, width=max(2, int(0.016 * W)))
    de.line(right, fill=235, width=max(2, int(0.012 * W)))
    tip = mid[-1]
    for k, (dx, dy, r) in enumerate(((0.06, -0.05, 0.035), (0.11, -0.10, 0.022))):
        x, y = tip[0] + dx * W, tip[1] + dy * H
        de.ellipse([x - r * W, y - r * W, x + r * W, y + r * W], fill=245)
    film = film.filter(ImageFilter.GaussianBlur(2 * s))
    out = _lighter(film, edge.filter(ImageFilter.GaussianBlur(0.8 * s))).resize((w, h), Image.LANCZOS)
    out = _lighter(out, _scale(out.filter(ImageFilter.GaussianBlur(3)), 0.45))
    _to_mask_img(out, name)


if __name__ == "__main__":
    water_streak()
    water_blob()
    foam_ring()
    lens_rim()
    lens_swirl()
    lens_wave()
    water_petal()
    print("출력:", OUT_DIR)
