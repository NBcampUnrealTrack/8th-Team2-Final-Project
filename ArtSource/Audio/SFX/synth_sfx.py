# -*- coding: utf-8 -*-
# 받아서는 안 나오는 소리를 계산으로 만든다 => Saved/SFX/synth/<이름>.wav (sfx_palette.json 의 extra 후보가 쓴다)
# ElevenLabs 에 활 시위 소리를 주문하면 거의 무음이 온다(6번 중 4번) --- 그래서 활은 소리가 나는 원리대로 만든다
#   활 쏘기: (1) 놓는 순간 시위가 활대에 걸리며 나는 '탁'(짧은 잡음 + 나무 울림)
#            (2) 시위의 떨림 --- 가운데(오늬)에서 놓아 홀수 배음이 강하다, 놓는 순간 장력이 흔들려 음높이가 살짝 내려앉는다,
#                높은 배음일수록 빨리 사라진다, 처음 잠깐은 팔보호대에 스쳐 비대칭으로 떨린다(쨍한 '팅')
#            (3) 활대의 낮은 울림
#   활 당기기: 시위·활대가 버티다 미끄러지기를 되풀이하는 '끼익' --- 빨라지는 딸깍임을 나무 울림에 통과시킨다
# 난수 씨앗이 고정이라 다시 돌려도 같은 소리가 나온다
# usage: python synth_sfx.py
import os

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
ROOT = os.path.normpath(HERE + "/../../..").replace("\\", "/")
OUT = ROOT + "/Saved/SFX/synth"
RATE = 44100


def band(data, low, high, order=2):
    return sosfilt(butter(order, [low, high], "bandpass", fs=RATE, output="sos"), data)


def modes(excite, table):
    """나무 울림: 짧게 사라지는 공진 [(Hz, 사라지는 시간 초, 세기)] 에 excite 를 통과시킨다"""
    t = np.arange(int(RATE * 0.25)) / RATE
    kernel = sum(gain * np.sin(2 * np.pi * hz * t) * np.exp(-t / tau) for hz, tau, gain in table)
    return np.convolve(excite, kernel)[:len(excite)]


def bow_twang(f0, pluck, tau1, buzz, snap_tone, seed):
    rng = np.random.default_rng(seed)
    n = int(RATE * 0.7)
    t = np.arange(n) / RATE
    # (2) 시위 떨림
    glide = 1.0 + 0.07 * np.exp(-t / 0.012)
    phase = 2 * np.pi * np.cumsum(f0 * glide) / RATE
    string = np.zeros(n)
    for k in range(1, 60):
        stretch = 1.0 + 0.0003 * k * k
        if k * f0 * stretch > 11000:
            break
        amp = abs(np.sin(np.pi * k * pluck)) / k ** 0.75
        string += amp * np.sin(k * stretch * phase + rng.uniform(0, 2 * np.pi)) * np.exp(-t / (tau1 / (1.0 + 0.45 * (k - 1))))
    string /= np.abs(string).max()
    # 여러 가닥을 꼰 시위라 소리가 맑지 않다: 떨림에 거친 결(200Hz 아래로 흔들리는 잡음)을 섞는다
    string *= 1.0 + 0.35 * band(rng.standard_normal(n), 20.0, 200.0) / 0.1
    string /= np.abs(string).max()
    # 팔보호대에 스치는 떨림: 처음 0.06초만 한쪽으로 찌그러뜨려 쨍한 배음을 더한다
    rub = (np.tanh(4.0 * (string + 0.3)) - np.tanh(1.2)) * np.exp(-t / 0.03)
    string = string + buzz * rub
    string *= 1.0 - np.exp(-t / 0.0006)
    # (1) 놓는 순간의 '탁': 8ms 잡음을 나무 울림에 통과
    click = rng.standard_normal(n) * np.exp(-t / 0.003)
    snap = band(click, 1200.0, 7000.0) * 0.6 + modes(click, [(snap_tone, 0.010, 1.0), (snap_tone * 2.1, 0.006, 0.6), (snap_tone * 3.7, 0.004, 0.4)]) * 0.12
    # (3) 활대의 낮은 울림
    limb = np.sin(2 * np.pi * np.cumsum(55.0 + 40.0 * np.exp(-t / 0.02)) / RATE) * np.exp(-t / 0.05) * (1.0 - np.exp(-t / 0.001))
    out = string / np.abs(string).max() * 0.75 + snap / np.abs(snap).max() * 1.3 + limb * 0.45
    out *= np.minimum(1.0, np.exp(-(t - 0.45) / 0.06))     # 끝을 거둔다
    return out / np.abs(out).max() * 0.89


def bow_draw(rate_from, rate_to, length, wood, seed):
    rng = np.random.default_rng(seed)
    n = int(RATE * length)
    t = np.arange(n) / RATE
    # 버티다 미끄러지기: 딸깍임 간격이 점점 짧아지고(rate_from => rate_to Hz), 조금씩 흔들린다
    clicks = np.zeros(n)
    position = 0.0
    while True:
        frac = position / n
        rate = rate_from + (rate_to - rate_from) * frac ** 0.7
        position += RATE / rate * rng.uniform(0.8, 1.2)
        if position >= n - 1:
            break
        clicks[int(position)] = rng.uniform(0.5, 1.0) * (0.3 + 0.7 * np.sin(np.pi * min(frac * 1.3, 1.0)) ** 0.6)
    creak = modes(clicks, wood)
    # 장갑·옷이 스치는 바람결
    rustle = band(rng.standard_normal(n), 1500.0, 6000.0) * (np.sin(np.pi * t / length) ** 2) * 0.25
    out = creak / np.abs(creak).max() + rustle
    fade = int(RATE * 0.03)
    out[:fade] *= np.linspace(0, 1, fade)
    out[-fade:] *= np.linspace(1, 0, fade)
    return out / np.abs(out).max() * 0.89


SOUNDS = {
    # 활 쏘기: 낮고 굵은 시위 / 조금 높고 짧은 시위 / 쨍함이 큰 시위
    "bow_twang_1": lambda: bow_twang(f0=92.0, pluck=0.47, tau1=0.11, buzz=0.4, snap_tone=520.0, seed=11),
    "bow_twang_2": lambda: bow_twang(f0=118.0, pluck=0.43, tau1=0.08, buzz=0.3, snap_tone=680.0, seed=12),
    "bow_twang_3": lambda: bow_twang(f0=105.0, pluck=0.40, tau1=0.10, buzz=0.7, snap_tone=600.0, seed=13),
    # 활 당기기: 느린 끼익 / 빠른 끼익
    "bow_draw_1": lambda: bow_draw(70.0, 180.0, 0.42, [(480.0, 0.012, 1.0), (1150.0, 0.008, 0.7), (2400.0, 0.004, 0.35)], seed=21),
    "bow_draw_2": lambda: bow_draw(110.0, 260.0, 0.34, [(620.0, 0.010, 1.0), (1500.0, 0.006, 0.6), (3100.0, 0.003, 0.3)], seed=22),
}

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, make in SOUNDS.items():
        wavfile.write("%s/%s.wav" % (OUT, name), RATE, np.round(make() * 32767.0).astype(np.int16))
    print("합성 %d개 => %s" % (len(SOUNDS), OUT))
