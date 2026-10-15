"""10/14 나이아 속성 임팩트 2차 다듬기용 마스크 텍스처 (R=G=B=A=밝기, 가산 머티리얼 M_NU_SpriteFront 용).

규칙은 make_vfx_textures_v3.py 와 같다 (배경 투명).

만드는 것:
- T_VFX_LeafBloom  512x512  풀 속성 임팩트의 잎 꽃잎 폭발. 가운데에서 바깥으로 뻗는 잎 7장(끝이 뾰족한 잎 모양, 바깥 가장자리와
                            잎맥이 밝고 잎 면은 옅음). 밝기가 낮은 잎 면부터 침식(NUA_Erode)으로 사라져 잎 테두리 · 잎맥이 남는다.
                            방사형 빛살(RadialSpikes)처럼 '번쩍이는 별' 이 아니라 '잎이 터져 나온 꽃' 으로 읽히게

사용법:  python ArtSource/VFX/Job/V3/make_vfx_textures_element2.py
출력:    같은 폴더의 T_VFX_*.png
"""

import math

from make_vfx_textures_v3 import OUT_DIR, save_mask, smoothstep


def leaf_bloom(size=512, name="T_VFX_LeafBloom.png", petals=7):
    # 잎 하나 = 잎 축 방향 거리 t(0 가운데 ~ 1 끝)에서 폭 w(t) = 0.11 * sin(pi * t^0.8) 인 뾰족한 잎.
    # 잎마다 축이 살짝 휘고(바람개비처럼), 길이가 조금씩 다름
    vals = []
    axes = []
    for k in range(petals):
        a = 2 * math.pi * k / petals + 0.2 * math.sin(k * 2.3)
        length = 0.40 + 0.04 * math.sin(k * 1.7)
        axes.append((a, length))
    for j in range(size):
        y = (j + 0.5) / size - 0.5
        for i in range(size):
            x = (i + 0.5) / size - 0.5
            r = math.hypot(x, y)
            ang = math.atan2(y, x)
            v = 0.0
            for a, length in axes:
                bend = 0.35 * (r / length) ** 2
                d = math.atan2(math.sin(ang - a - bend), math.cos(ang - a - bend))
                t = r / length
                if t <= 0.0 or t >= 1.0:
                    continue
                w = 0.11 * math.sin(math.pi * t ** 0.8)
                side = abs(d) * r
                if side > w * 1.15:
                    continue
                e = side / max(w, 1e-4)
                body = (1.0 - smoothstep(0.85, 1.15, e)) * (0.18 + 0.22 * t)
                rim = math.exp(-((e - 0.9) / 0.12) ** 2) * (0.6 + 0.4 * (1.0 - t))
                rib = math.exp(-(e / 0.08) ** 2) * 0.55 * (1.0 - t)
                v = max(v, body, rim, rib)
            core = math.exp(-(r / 0.05) ** 2)
            vals.append(min(1.0, max(v, core)))
    save_mask(vals, size, size, name, glow=0.01, glow_gain=0.4)


if __name__ == "__main__":
    leaf_bloom()
    print("출력:", OUT_DIR)
