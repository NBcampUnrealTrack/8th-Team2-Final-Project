# -*- coding: utf-8 -*-
"""GPT 가 만든 프롤로그 그림을 게임용 도트 그림책 텍스처로 맞춘다.

입력: ArtSource/Prologue_20261014/gen/<이름>.png  (GPT 원본, 가로 그림이면 크기 무관)
출력: ArtSource/Prologue_20261014/final/<텍스처 이름>.png  (704x396, 도트 격자 고정)

- 가운데 16:9 로 자른 뒤 704x396 로 줄인다. 게임은 이 그림을 화면을 덮는 정수 배율(1080p 3배, 1440p 4배, 4K 6배)로
  그리고 남는 여백(1080p 기준 좌우 32칸·상하 18칸) 안에서 한 칸 단위로 천천히 움직인다.
- 줄일 때 평균(BOX). GPT 의 '도트 풍' 그림은 칸 크기가 고르지 않아, 이렇게 한 번 격자에 다시 앉혀야
  게임 화면의 도트와 같은 밀도로 보인다. 색 수 제한(팔레트)은 하지 않는다 — 48~96색 미디언컷·옥트리로 묶어 보니
  정령 빛의 분홍·청록이 베이지로 뭉개져서 뺐다(COLORS 를 0 보다 크게 주면 다시 켤 수 있다).
- 원본은 지우지 않는다. 결과를 본 뒤 build_prologue.py 를 다시 돌리면 임시 카드 대신 이 그림이 들어간다.

사용: python -I process_art.py <원본.png> <텍스처 이름>   예) p1_cradle_v1.png T_Prologue_01_Cradle
"""
import os
import sys

from PIL import Image, ImageEnhance

W, H = 704, 396
COLORS = 0
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FINAL = os.path.join(ROOT, 'ArtSource', 'Prologue_20261014', 'final')


def process(src, name):
    im = Image.open(src).convert('RGB')
    w, h = im.size
    target = W / H
    if w / h > target:            # 너무 넓으면 좌우를 자른다
        cw = int(round(h * target))
        im = im.crop(((w - cw) // 2, 0, (w - cw) // 2 + cw, h))
    else:                         # 너무 높으면 위아래를 자른다(가운데 16:9 띠)
        ch = int(round(w / target))
        im = im.crop((0, (h - ch) // 2, w, (h - ch) // 2 + ch))
    small = im.resize((W, H), Image.BOX)
    small = ImageEnhance.Contrast(small).enhance(1.06)
    quant = small.quantize(colors=COLORS, method=Image.Quantize.MAXCOVERAGE, dither=Image.Dither.NONE).convert('RGB') if COLORS > 0 else small
    os.makedirs(FINAL, exist_ok=True)
    out = os.path.join(FINAL, name + '.png')
    quant.save(out)
    # 검토용: 3배 최근접 확대(1080p 화면에서 보이는 모습)
    quant.resize((W * 3, H * 3), Image.NEAREST).save(os.path.join(FINAL, name + '_preview3x.png'))
    print(out, quant.size)


if __name__ == '__main__':
    process(sys.argv[1], sys.argv[2])
