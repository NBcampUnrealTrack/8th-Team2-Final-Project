"""직업 스킬 VFX 용 마스크 텍스처를 만든다 (가산 머티리얼 + User.EffectColor 틴트용).

각 PNG 는 RGBA 이고 R=G=B=A=밝기다 - 머티리얼이 어느 채널을 읽어도 같은 모양이 나온다.
흰색이 가장 밝고 가장자리는 부드러운 글로우로 풀린다. 색은 EffectColor 가 입힌다.

방향 규약: 방향이 있는 모양(화살 · 베기)은 **머리가 텍스처 위쪽**이다.

사용법:  python ArtSource/VFX/Job/make_vfx_textures.py
출력:    같은 폴더의 T_VFX_*.png
"""

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

OUT_DIR = Path(__file__).resolve().parent
SUPER = 4  # 그린 뒤 줄여서 가장자리를 매끈하게


class Canvas:
    def __init__(self, size):
        self.size = size
        self.big = size * SUPER
        self.img = Image.new("L", (self.big, self.big), 0)
        self.draw = ImageDraw.Draw(self.img)

    def p(self, x, y):
        return (x * self.big, y * self.big)

    def poly(self, points, value=255):
        self.draw.polygon([self.p(x, y) for x, y in points], fill=value)

    def line(self, a, b, width, value=255):
        self.draw.line([self.p(*a), self.p(*b)], fill=value, width=max(1, int(width * self.big)))

    def ellipse(self, cx, cy, rx, ry, value=255, outline_width=None):
        box = [(cx - rx) * self.big, (cy - ry) * self.big, (cx + rx) * self.big, (cy + ry) * self.big]
        if outline_width is None:
            self.draw.ellipse(box, fill=value)
        else:
            self.draw.ellipse(box, outline=value, width=max(1, int(outline_width * self.big)))

    def round_rect(self, x0, y0, x1, y1, radius, value=255, outline_width=None):
        box = [x0 * self.big, y0 * self.big, x1 * self.big, y1 * self.big]
        if outline_width is None:
            self.draw.rounded_rectangle(box, radius=radius * self.big, fill=value)
        else:
            self.draw.rounded_rectangle(box, radius=radius * self.big, outline=value,
                                        width=max(1, int(outline_width * self.big)))

    def finish(self, glow=0.035, glow_gain=0.6):
        small = self.img.resize((self.size, self.size), Image.LANCZOS)
        blurred = small.filter(ImageFilter.GaussianBlur(self.size * glow))
        # 본 모양은 그대로 두고 그 둘레에 부드러운 글로우를 얹음
        glow_layer = blurred.point(lambda v: int(v * glow_gain))
        # 본 모양과 글로우 중 밝은 쪽
        out = Image.new("L", (self.size, self.size))
        out.putdata([max(a, b) for a, b in zip(small.getdata(), glow_layer.getdata())])
        return Image.merge("RGBA", (out, out, out, out))

    def save(self, name, **kwargs):
        image = self.finish(**kwargs)
        image.save(OUT_DIR / name)
        print("저장", name, image.size)


def arrow():
    c = Canvas(128)
    c.line((0.5, 0.24), (0.5, 0.86), 0.035)               # 몸통
    c.poly([(0.5, 0.05), (0.66, 0.31), (0.34, 0.31)])     # 촉
    c.poly([(0.5, 0.70), (0.64, 0.92), (0.5, 0.84)], 220)  # 깃 오른쪽
    c.poly([(0.5, 0.70), (0.36, 0.92), (0.5, 0.84)], 220)  # 깃 왼쪽
    c.save("T_VFX_Arrow.png")


def card():
    c = Canvas(128)
    c.round_rect(0.29, 0.10, 0.71, 0.90, 0.05, 70)                      # 카드 면(옅게)
    c.round_rect(0.29, 0.10, 0.71, 0.90, 0.05, 255, outline_width=0.028)  # 바깥 테두리
    c.round_rect(0.34, 0.16, 0.66, 0.84, 0.03, 200, outline_width=0.014)  # 안쪽 테두리
    c.poly([(0.5, 0.38), (0.58, 0.5), (0.5, 0.62), (0.42, 0.5)], 255)    # 중앙 마름모
    for x, y in ((0.40, 0.23), (0.60, 0.23), (0.40, 0.77), (0.60, 0.77)):
        c.ellipse(x, y, 0.014, 0.014, 230)
    c.save("T_VFX_Card.png", glow=0.03)


def rune_circle():
    c = Canvas(256)
    cx = cy = 0.5
    c.ellipse(cx, cy, 0.46, 0.46, 255, outline_width=0.012)
    c.ellipse(cx, cy, 0.425, 0.425, 200, outline_width=0.006)
    c.ellipse(cx, cy, 0.30, 0.30, 230, outline_width=0.010)
    for i in range(48):  # 바깥 눈금
        a = i / 48 * math.tau
        r0, r1 = (0.425, 0.455) if i % 4 else (0.42, 0.46)
        c.line((cx + math.cos(a) * r0, cy + math.sin(a) * r0),
               (cx + math.cos(a) * r1, cy + math.sin(a) * r1), 0.005, 220)
    star = [(cx + math.cos(i / 8 * math.tau - math.pi / 2) * 0.30,
             cy + math.sin(i / 8 * math.tau - math.pi / 2) * 0.30) for i in range(8)]
    for i in range(8):  # 팔각 별 {8/3}
        c.line(star[i], star[(i + 3) % 8], 0.007, 235)
    rng = random.Random(7)
    for i in range(8):  # 룬 8개 - 짧은 획 몇 개
        a = i / 8 * math.tau - math.pi / 2 + math.pi / 8
        gx, gy = cx + math.cos(a) * 0.375, cy + math.sin(a) * 0.375
        for _ in range(3):
            ang = a + rng.uniform(-1.2, 1.2)
            ln = rng.uniform(0.018, 0.034)
            ox, oy = rng.uniform(-0.012, 0.012), rng.uniform(-0.012, 0.012)
            c.line((gx + ox, gy + oy), (gx + ox + math.cos(ang) * ln, gy + oy + math.sin(ang) * ln), 0.006, 255)
    c.ellipse(cx, cy, 0.045, 0.045, 255)
    c.save("T_VFX_RuneCircle.png", glow=0.02)


def beast_silhouette():
    c = Canvas(256)
    body, rim = 150, 255
    c.ellipse(0.5, 0.97, 0.43, 0.24, body)                 # 어깨
    c.poly([(0.20, 0.42), (0.27, 0.22), (0.38, 0.38)], body)   # 왼쪽 귀(뾰족)
    c.poly([(0.80, 0.42), (0.73, 0.22), (0.62, 0.38)], body)   # 오른쪽 귀
    c.ellipse(0.5, 0.56, 0.28, 0.24, body)                 # 머리
    c.ellipse(0.5, 0.69, 0.13, 0.09, 190)                  # 주둥이
    c.ellipse(0.5, 0.655, 0.03, 0.02, 255)                 # 코
    c.poly([(0.36, 0.48), (0.47, 0.54), (0.36, 0.57)], rim)    # 왼눈(사납게 기운)
    c.poly([(0.64, 0.48), (0.53, 0.54), (0.64, 0.57)], rim)    # 오른눈
    c.line((0.34, 0.44), (0.47, 0.50), 0.018, 70)              # 찌푸린 눈썹
    c.line((0.66, 0.44), (0.53, 0.50), 0.018, 70)
    c.poly([(0.44, 0.735), (0.465, 0.80), (0.485, 0.735)], rim)  # 송곳니
    c.poly([(0.515, 0.735), (0.535, 0.80), (0.56, 0.735)], rim)
    c.save("T_VFX_BeastSilhouette.png", glow=0.03, glow_gain=0.85)


def crack():
    c = Canvas(256)
    rng = random.Random(21)
    cx = cy = 0.5
    branches = 11
    for i in range(branches):
        a = i / branches * math.tau + rng.uniform(-0.2, 0.2)
        length = rng.uniform(0.28, 0.47)
        x, y = cx, cy
        width = 0.016
        steps = 7
        for s in range(steps):
            a += rng.uniform(-0.45, 0.45)
            nx = x + math.cos(a) * length / steps
            ny = y + math.sin(a) * length / steps
            c.line((x, y), (nx, ny), width, 255)
            if s == 3 and rng.random() < 0.7:  # 곁가지
                ba = a + rng.choice((-1, 1)) * rng.uniform(0.5, 0.9)
                c.line((nx, ny), (nx + math.cos(ba) * 0.08, ny + math.sin(ba) * 0.08), width * 0.6, 200)
            x, y = nx, ny
            width *= 0.78
    c.ellipse(cx, cy, 0.04, 0.04, 255)
    c.save("T_VFX_Crack.png", glow=0.02)


def cut_line():
    c = Canvas(256)
    # 위에서 아래로 그어진 베기 자국 - 가운데가 두껍고 양끝이 뾰족함, 살짝 기움
    top, bottom = (0.58, 0.04), (0.42, 0.96)
    mid_w = 0.045
    points_l, points_r = [], []
    n = 24
    for i in range(n + 1):
        t = i / n
        x = top[0] + (bottom[0] - top[0]) * t
        y = top[1] + (bottom[1] - top[1]) * t
        w = mid_w * math.sin(t * math.pi) ** 0.8
        points_l.append((x - w, y))
        points_r.append((x + w, y))
    c.poly(points_l + points_r[::-1])
    c.save("T_VFX_CutLine.png", glow=0.03)


if __name__ == "__main__":
    arrow()
    card()
    rune_circle()
    beast_silhouette()
    crack()
    cut_line()
