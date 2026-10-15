# -*- coding: utf-8 -*-
# 효과음 만들기 --- sfx_palette.json 의 줄마다 ElevenLabs 효과음 API 로 후보를 여러 개 받아, 가장 나은 것을 골라 가공한 WAV 로 저장한다
# (로컬 파일만 만든다, 에디터 안 씀)
#   받은 후보(mp3)    : Saved/SFX/raw/<id>_<a|b|c>.mp3   (git 에 안 올라간다. 이미 받은 후보는 다시 받지 않는다 --- 크레딧 절약)
#                       줄에 alt 가 있으면 그 프롬프트로 <id>_<d|e>.mp3 도 받는다 (다른 느낌 후보)
#   가공한 후보(wav)  : Saved/SFX/cand/<id>_<a|b|c>.wav  (들어 보기 도구가 쓴다)
#   고른 결과(wav)    : Saved/SFX/wav/<id>.wav           (이것을 에디터에서 임포트한다. tick·event·phrase 는 모노, stinger 는 스테레오)
#   고른 까닭         : Saved/SFX/report.json            (후보마다 잰 값과 점수)
#   사람이 고른 것    : Saved/SFX/picks.json             ({"UI_Click": "b"} --- 있으면 자동 고르기보다 먼저다)
#
# 가공: 저역 잡음 걷기 => 가장 센 소리 덩어리 찾기 => 정한 길이(keep x FLEX)보다 길면 가장 센 구간만 남기기 => 앞·끝 페이드
#       => 음량 맞추기(가장 큰 0.1초의 세기 db, 또는 꼭대기 peak) => 꼭대기 -1dB (조금 넘는 것은 부드럽게 누른다)
# 줄에 더 적을 수 있는 것:
#   hit   : true 면 때리는 소리로 다룬다 --- 타격이 0초에 오게 자르고, 첫 순간을 세우고, 살짝 눌러 채우고, 여운을 일찍 거둔다
#   thump : 타격과 같은 순간에 까는 낮은 '툭'의 세기(dB, 몸통 꼭대기 대비. -14 가벼움 ~ -4 무거움). 원본에 낮은 쪽이 이미 실려 있으면 넣지 않는다
#   thump_shape : [시작 Hz, 끝 Hz, 가라앉는 초] --- '툭'의 모양을 바꾼다 (기본 160 => 45Hz, 0.07초). 45Hz 대는 작은 스피커에서 안 들려
#                 돌·땅처럼 몸통이 들려야 하는 소리는 끝 음을 올리고 길게 둔다 (땅 타격 10/15: 소리의 대부분이 45~60Hz 사인이라 약하게 들렸다)
#   bite  : 3kHz 위를 이만큼(dB) 올린다 (칼날·발톱·시위처럼 날카로운 재질). 음수면 깎는다 (찢어지게 밝은 소리의 톤 다운)
#   bite_hz : bite 가 먹는 아래 경계(Hz, 기본 3000) --- 둔탁한 돌처럼 '딱'(0.5~2kHz)까지 누를 때 낮춘다. bite 는 겹치는 층에는 안 먹는다
#   peak  : 음량을 db 대신 꼭대기(dBFS)에 맞춘다 (타격음)
#   delay : 앞에 붙이는 빈 시간(초) --- 모션이 시작하고 조금 뒤에 나야 하는 준비 소리
#   mix   : 다른 줄의 소리를 겹친다 [{"id": 줄, "at": 초, "gain": dB(이 소리 꼭대기 대비), "hp": 그 층의 저역을 자를 Hz, "len": 그 층을 남길 초(뒤 절반을 줄여 끝냄)}]
#           --- 한 소리에 두 동작을 주문하지 않고 따로 받아 겹친다
#   family: 재질 (FAMILY) --- 후보를 고를 때 그 재질다운 밝기·무게인지 본다
#   src   : {"file": Saved/SFX 아래 경로, "span": [시작, 끝]} --- 받지 않고 이 파일을 그 줄의 유일한 후보(a)로 쓴다 (무료 소리로 만든 겹칠 재료)
#   extra : {"f": {...}, ...} 더 붙이는 후보(F~I). 원본은 file·span(무료·합성 파일) 또는 base(받은 후보 글자). 후보마다
#           mix(줄의 mix 대신 겹칠 층), set(그 후보만 바꿀 줄 값: bite·thump 등), note(무엇인지)를 적는다.
#           무료·합성 후보는 사람이 듣고 picks.json 으로 고를 때까지 자동으로 고르지 않는다(auto: true 인 합성 후보만 자동 고르기에 든다).
#           무료 소리의 출처·라이선스는 LICENSES.md
# 고르기: 후보마다 (1) 남긴 구간이 그 덩어리를 얼마나 담았나 (2) 끝·시작이 소리 한가운데서 잘렸나 (3) 바탕 잡음
#         (4) 마른 소리(프롬프트에 dry)에 높은 음의 울림·짤랑임이 섞였나 (5) 여러 소리 줄이 한 흐름으로 이어졌나
#         (6) 때리는 소리는 빨리 올라서는가·앞에 몰려 있는가·재질다운 밝기와 무게인가·원본이 너무 작지 않은가 를 재서 점수가 가장 높은 것
# API 키는 Saved/elevenlabs_api_key.txt 한 줄(또는 환경변수 ELEVENLABS_API_KEY)에서 읽고 화면에 찍지 않는다
#
# usage: python generate_sfx.py list                 줄 수와 남은 요청 길이만 본다
#        python generate_sfx.py gen [id ...]         안 받은 후보를 받고, 고르고, 가공한다 (id 를 주면 그것만)
#        python generate_sfx.py regen <id> ...       그 소리의 후보를 전부 다시 받는다 (크레딧을 또 쓴다)
#        python generate_sfx.py refill [id ...]      거의 안 들리게 나온 후보(원본 꼭대기 -30dBFS 아래)만 다시 받는다
#        python generate_sfx.py process [id ...]     받은 후보로 다시 고르고 가공만 한다 (크레딧 안 씀)
# 게임에 넣기: (태그를 더하거나 뺐으면 python make_tags.py 후 빌드) => 에디터 콘솔에서 py "ArtSource/Audio/SFX/import_sfx.py"
import ctypes
import io
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

import numpy as np
from scipy.io import wavfile
from scipy.ndimage import minimum_filter1d
from scipy.signal import butter, resample_poly, sosfiltfilt

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
ROOT = os.path.normpath(HERE + "/../../..").replace("\\", "/")
OUT = ROOT + "/Saved/SFX"
RAW, CAND, WAV = OUT + "/raw", OUT + "/cand", OUT + "/wav"
KEY_FILE = ROOT + "/Saved/elevenlabs_api_key.txt"
API = "https://api.elevenlabs.io/v1/sound-generation?output_format=mp3_44100_128"
# 언리얼에 들어 있는 libsndfile 로 mp3 를 푼다 (따로 설치하는 것 없음)
SNDFILE = "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/libsndfile/Win64/libsndfile-1.dll"
RATE = 44100
LETTERS = "abc"          # 소리마다 받는 후보
ALT_LETTERS = "de"       # 줄에 alt(다른 느낌 프롬프트)가 있으면 더 받는 후보 --- 자동으로는 고르지 않는다
EXTRA_LETTERS = "fghi"   # 줄의 extra 후보(무료·합성 소리) --- 자동으로는 고르지 않는다
GAP = {"tick": 0.012, "event": 0.09, "phrase": 0.45}                 # 이보다 짧은 쉼은 한 소리로 본다 (초, stinger 는 전부 한 소리)
FLEX = {"tick": 1.25, "event": 1.35, "phrase": 1.6, "stinger": 1.25}   # keep 의 이 배수까지는 소리를 끝까지 둔다
LOUD_WINDOW = 0.1        # 음량을 재는 구간 (초)
SOFT_OVER = 10 ** (3.0 / 20.0)   # 꼭대기를 부드럽게 누르는 한도
# 타격음의 크기: 가장 큰 0.03초의 세기(dBFS). 줄의 peak 는 이 값에서의 차이로 읽는다 (peak -2 = 기준)
HIT_LOUDNESS = -7.0
HIT_MAX_LIMIT = 6.0     # 리미터가 눌러도 되는 최대 dB --- 더 필요하면 전체를 줄인다 (지나치게 누르면 찌그러진다)
WORKERS = 2              # 동시에 보내는 요청 수
# 재질별 기준: (무게중심 주파수 범위 Hz, 40~160Hz 가 차지하는 몫의 범위). 때리는 소리는 타격 첫 0.08초, 그 밖은 소리 전체에서 잰다
FAMILY = {"blunt": ((200, 900), (0.25, 0.85)), "heavy": ((150, 800), (0.25, 0.85)), "blade": ((1200, 3500), (0.10, 0.50)),
          "claw": ((1500, 4500), (0.10, 0.50)), "snap": ((1500, 4000), (0.10, 0.50)), "spark": ((1500, 4500), (0.05, 0.40)),
          "magic": ((600, 3000), (0.10, 0.50)), "wet": ((400, 2500), (0.10, 0.60)), "air": ((800, 5000), (0.0, 0.30))}


class SFInfo(ctypes.Structure):
    _fields_ = [("frames", ctypes.c_int64), ("samplerate", ctypes.c_int), ("channels", ctypes.c_int),
                ("format", ctypes.c_int), ("sections", ctypes.c_int), ("seekable", ctypes.c_int)]


def decode(path):
    """소리 파일 => (프레임 x 채널 float 배열, 샘플레이트)"""
    lib = ctypes.CDLL(SNDFILE)
    lib.sf_wchar_open.restype = ctypes.c_void_p
    lib.sf_wchar_open.argtypes = [ctypes.c_wchar_p, ctypes.c_int, ctypes.POINTER(SFInfo)]
    lib.sf_readf_float.restype = ctypes.c_int64
    lib.sf_readf_float.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_int64]
    lib.sf_close.argtypes = [ctypes.c_void_p]
    info = SFInfo()
    handle = lib.sf_wchar_open(path, 0x10, ctypes.byref(info))     # SFM_READ
    if not handle:
        raise RuntimeError("열 수 없음: " + path)
    want = max(int(info.frames), info.samplerate * 40)              # mp3 는 frames 가 어림값일 수 있어 넉넉히
    buf = (ctypes.c_float * (want * info.channels))()
    got = lib.sf_readf_float(handle, buf, want)
    lib.sf_close(handle)
    data = np.frombuffer(buf, dtype=np.float32, count=got * info.channels).reshape(-1, info.channels).astype(np.float64)
    return data, info.samplerate


def envelope(mono, window=0.005):
    """짧은 구간 RMS (5ms)"""
    n = max(1, int(RATE * window))
    return np.sqrt(np.convolve(mono ** 2, np.ones(n) / n, mode="same"))


def highpass(data, hz):
    return sosfiltfilt(butter(2, hz, "highpass", fs=RATE, output="sos"), data, axis=0)


def inside(value, window):
    """value 가 범위 안이면 1, 벗어난 만큼 0 으로 내려간다"""
    low, high = window
    if value < low:
        return max(0.0, value / low) if low > 0 else 1.0
    return 1.0 if value <= high else max(0.0, 1.0 - (value - high) / high)


def main_run(env, energy, peak, kind, gap):
    """가장 센 소리 덩어리의 (시작, 끝, 꼬리 끝). gap(초)보다 짧은 쉼은 한 덩어리로 본다"""
    idx = np.nonzero(env > peak * 0.04)[0]
    if kind == "stinger":
        starts, ends, best = idx[:1], idx[-1:], 0
    else:
        cuts = np.nonzero(np.diff(idx) > int(RATE * gap))[0]
        starts = np.concatenate((idx[:1], idx[cuts + 1]))
        ends = np.concatenate((idx[cuts], idx[-1:]))
        best = int(np.argmax(energy[ends + 1] - energy[starts]))
    # 꼬리: 덩어리가 끝난 뒤 여운이 가라앉는 곳까지 (다음 덩어리가 시작하기 전까지만).
    # 음악은 잔향이 길어 -60dB 까지 둔다 --- -40dB 에서 자르면 뚝 끊긴 것처럼 들린다
    wall = int(starts[best + 1]) if best + 1 < len(starts) else len(env)
    calm = np.nonzero(env[ends[best]:wall] < peak * (0.001 if kind == "stinger" else 0.005))[0]
    tail = int(ends[best]) + (int(calm[0]) if len(calm) else wall - int(ends[best]) - 1)
    return int(starts[best]), int(ends[best]), tail


def tone(mono):
    """재질을 잰다 => (무게중심 Hz, 40~160Hz 의 몫)"""
    spec = np.abs(np.fft.rfft(mono * np.hanning(len(mono)), n=max(8192, len(mono)))) ** 2
    freq = np.fft.rfftfreq(max(8192, len(mono)), 1.0 / RATE)
    total = max(float(spec.sum()), 1e-12)
    return float((spec * freq).sum() / total), float(spec[(freq >= 40) & (freq <= 160)].sum() / total)


def add_punch(out, row, low):
    """타격감을 더한다: 첫 순간을 세우고(6dB 까지), 낮은 '툭'을 타격과 같은 순간에 깔고, 살짝 눌러 채운다"""
    thump = row.get("thump")
    mono = out.mean(axis=1)
    out = out * np.clip(envelope(mono, 0.001) / np.maximum(envelope(mono, 0.030), 1e-6), 1.0, 2.5)[:, None]
    # 낮은 '툭': 160Hz 에서 45Hz 로 0.022초 만에 떨어지는 사인, 0.07초에 가라앉는다 (몸에 닿는 무게).
    # 타격과 같은 순간에 시작해야 한 소리로 들린다 --- 늦게 올라오면 '베고 나서 쿵'으로 갈라진다
    # 줄에 thump_shape [시작 Hz, 끝 Hz, 가라앉는 초] 가 있으면 그 모양으로 깔고 끝을 0 까지 내린다 (기본 모양은 0.16초에서 -20dB 로 잘린다)
    if thump is not None and low < 0.35:
        high_hz, low_hz, decay = row.get("thump_shape", (160.0, 45.0, 0.070))
        length = decay * 4.0 if "thump_shape" in row else 0.16
        if len(out) < int(RATE * (length + 0.04)):
            out = np.concatenate((out, np.zeros((int(RATE * (length + 0.04)) - len(out), out.shape[1]))))
        t = np.arange(int(RATE * length)) / RATE
        sub = np.sin(2.0 * np.pi * np.cumsum(low_hz + (high_hz - low_hz) * np.exp(-t / 0.022)) / RATE) * (1.0 - np.exp(-t / 0.0008)) * np.exp(-t / decay)
        if "thump_shape" in row:
            sub *= np.cos(np.linspace(0.0, np.pi / 2.0, len(sub))) ** 2
        out[:len(sub)] += (sub * float(np.abs(out[:int(RATE * 0.05)]).max()) * 10 ** (thump / 20.0))[:, None]
    # 누르기(+3dB): 배음이 늘어 작은 스피커에서도 존재감이 생긴다
    peak, drive = float(np.abs(out).max()), 10 ** (3.0 / 20.0)
    return np.tanh(out / peak * 0.9 * drive) / np.tanh(0.9 * drive) * peak


def limiter(out, ceiling_db=-0.5):
    """앞을 내다보는 리미터: 꼭대기를 ceiling 아래로 누르고(1.5ms 앞에서 미리), 0.04초에 걸쳐 되돌린다"""
    need = np.minimum(1.0, 10 ** (ceiling_db / 20.0) / np.maximum(np.abs(out).max(axis=1), 1e-9))
    need = minimum_filter1d(need, size=2 * int(RATE * 0.0015) + 1)
    release, gain, current = np.exp(-1.0 / (RATE * 0.04)), np.empty_like(need), 1.0
    for index, value in enumerate(need):
        current = min(value, 1.0 - (1.0 - current) * release)
        gain[index] = current
    return out * gain[:, None]


def shape(src, row, layers=(), finish=True):
    """후보 하나를 가공한다 => (소리 배열, 잰 값). src 는 (파일, 구간 또는 None), layers 는 겹칠 소리 [(배열, 초, dB)],
    finish 가 거짓이면 음량 맞추기 앞에서 멈춘다(겹칠 재료)"""
    path, span = src
    data, rate = decode(path)
    if span:
        data = data[int(span[0] * rate):int(span[1] * rate)]
    if rate != RATE:
        data = resample_poly(data, RATE, rate, axis=0)
    kind, hit = row["kind"], bool(row.get("hit"))
    raw_peak = 20 * np.log10(max(float(np.abs(data).max()), 1e-9))
    stereo = kind == "stinger" and data.shape[1] > 1
    if not stereo:
        data = data.mean(axis=1, keepdims=True)
    # 저역 잡음과 DC 를 걷는다 (짧은 소리의 툭 튀는 시작을 막는다). 때리는 소리는 30~60Hz 의 무게를 남기려고 더 낮게 자른다
    data = highpass(data, 28.0 if hit else 35.0 if stereo else 45.0)
    mono = data.mean(axis=1)
    env = envelope(mono)
    peak = float(env.max()) if len(env) else 0.0
    if peak < 1e-4:
        raise RuntimeError("소리가 비어 있음")
    energy = np.concatenate(([0.0], np.cumsum(mono ** 2)))
    # 때리는 소리는 타격 뒤의 부스러기·여운이 조금 떨어져 있어도(0.25초) 한 소리로 본다
    run_start, run_end, tail = main_run(env, energy, peak, kind, 0.25 if hit else GAP.get(kind, 0.0))
    start = max(0, run_start - int(RATE * 0.003))
    natural_end = min(len(mono), tail + int(RATE * 0.010))
    limit = int(RATE * row["keep"] * FLEX[kind])
    cut_in = lead = attack = 0.0
    if natural_end - start > limit and kind != "stinger" and not hit:
        # 정한 길이보다 길면 가장 센 구간을 남긴다 (앞쪽이 조용하고 뒤에서 소리가 나는 후보가 많다). 시작은 바로 앞의 가장 조용한 곳으로 물린다
        best = start + int(np.argmax(energy[start + limit:natural_end + 1] - energy[start:natural_end + 1 - limit]))
        if best - start > int(RATE * 0.02):
            low = max(start, best - int(min(RATE * 0.06, limit * 0.2)))
            start = low + int(np.argmin(env[low:best + 1]))
            cut_in = float(env[start] / peak)
    if hit:
        # 때리는 소리는 가장 센 순간이 0초에 오게 한다 --- 앞에 준비 소리가 붙어 있으면 화면보다 늦게 들린다
        fast = envelope(mono, 0.001)
        top = start + int(np.argmax(fast[start:natural_end]))
        low = max(start, top - int(RATE * 0.04))
        quiet = np.nonzero(fast[low:top] < fast[top] * 0.1)[0]
        onset = low + (int(quiet[-1]) if len(quiet) else 0)
        rise = np.nonzero(fast[onset:top + 1] >= fast[top] * 0.9)[0]
        attack = (int(rise[0]) if len(rise) else top - onset) * 1000.0 / RATE                                   # 올라서는 데 걸린 시간(ms)
        lead = float(energy[onset] - energy[start]) / max(float(energy[natural_end] - energy[start]), 1e-12)   # 타격 앞에 있던 소리의 몫
        start = max(0, onset - int(RATE * 0.001))
    stop = max(min(natural_end, start + limit), start + int(RATE * 0.03))
    cut_level = float(env[min(stop, len(env) - 1)] / peak) if stop < natural_end else 0.0
    run_energy = float(energy[natural_end] - energy[min(start, max(0, run_start - int(RATE * 0.003)))])
    captured = float(energy[stop] - energy[start]) / max(run_energy, 1e-12)
    main = run_energy / max(float(energy[-1]), 1e-12)
    floor = env[env <= peak * 0.04]
    noise = float(np.median(floor) / peak) if len(floor) > int(RATE * 0.03) else 0.0
    out = data[start:stop].copy()
    seg = out.mean(axis=1)
    # 재질: 때리는 소리는 타격 첫 0.08초, 그 밖은 전체. 앞쏠림은 첫 0.03초와 그 뒤 0.17초의 세기 차이(dB) --- 낮으면 '베고 나서 쿵'이다
    centroid, low_share = tone(seg[:int(RATE * 0.08)] if hit else seg)
    forward = 10 * np.log10(max(float((seg[:int(RATE * 0.03)] ** 2).sum()), 1e-12) / max(float((seg[int(RATE * 0.03):int(RATE * 0.2)] ** 2).sum()), 1e-12))
    if hit:
        out = add_punch(out, row, low_share)
        # 여운을 일찍 거둔다 (keep 의 절반부터 그만큼마다 -20dB) --- 다음 타격과 뭉개지지 않게
        t = np.arange(len(out)) / RATE - row["keep"] * 0.5
        out *= (10 ** (-np.clip(t, 0.0, None) / (row["keep"] * 0.5)))[:, None]
    if "bite" in row:
        out = out + (10 ** (row["bite"] / 20.0) - 1.0) * highpass(out, row.get("bite_hz", 3000.0))
    # 끝 페이드: 소리가 남은 채 잘렸으면 길게(최대 180ms) 내려 끊긴 티를 줄이고, 스스로 끝난 소리는 30ms(음악은 250ms)
    n = len(out)
    fade = int(min(n * 0.35, RATE * 0.180)) if cut_level > 0.06 else int(min(n * 0.25, RATE * (0.250 if kind == "stinger" else 0.030)))
    fade = max(fade, int(RATE * 0.004))
    out[-fade:] *= (np.cos(np.linspace(0.0, np.pi / 2.0, fade)) ** 2)[:, None]
    # 앞 페이드: 소리 한가운데서 시작했으면 15ms 까지, 때리는 소리는 0.5ms, 그 밖은 2ms
    head = min(n, int(min(n * 0.15, RATE * 0.015)) if cut_in > 0.06 else int(RATE * (0.0005 if hit else 0.002)))
    out[:head] *= np.linspace(0.0, 1.0, head)[:, None]
    # 높은 음의 울림(짤랑임): 2~12kHz 스펙트럼에서 봉우리가 평균보다 얼마나 솟았나 --- 종이·가죽·나무 소리는 낮고 금속·종은 높다
    seg = out.mean(axis=1)
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n=max(4096, len(seg))))
    freq = np.fft.rfftfreq(max(4096, len(seg)), 1.0 / RATE)
    band = spec[(freq >= 2000) & (freq <= 12000)]
    ring = float(np.clip((band.max() / max(band.mean(), 1e-12) - 6.0) / 24.0, 0.0, 1.0)) if len(band) else 0.0
    measured = {"sec": round(n / RATE, 3), "cut": round(cut_level, 3), "cut_in": round(cut_in, 3), "captured": round(captured, 3), "main": round(main, 3),
                "noise": round(noise, 4), "ring": round(ring, 3), "attack_ms": round(attack, 1), "lead": round(lead, 3), "centroid": round(centroid),
                "low": round(low_share, 3), "forward_db": round(forward, 1), "raw_peak_db": round(raw_peak, 1)}
    if not finish:
        return out, measured
    # 겹치기: 다른 줄의 소리를 이 소리 꼭대기 대비 gain(dB)으로 at(초)에 얹는다
    for layer, at, gain in layers:
        offset = int(RATE * at)
        if len(out) < offset + len(layer):
            out = np.concatenate((out, np.zeros((offset + len(layer) - len(out), out.shape[1]))))
        out[offset:offset + len(layer)] += layer.mean(axis=1, keepdims=True) * (float(np.abs(out).max()) * 10 ** (gain / 20.0) / max(float(np.abs(layer).max()), 1e-9))
    seg = out.mean(axis=1)
    if "peak" in row:
        # 음량: 타격음은 가장 큰 0.03초의 세기를 HIT_LOUDNESS(+줄의 차이)에 맞추고 리미터로 꼭대기를 누른다 ---
        # 꼭대기로만 맞추면 짧은 타격은 버튼 소리만큼밖에 안 커진다. 누를 양이 HIT_MAX_LIMIT 를 넘으면 그만큼 덜 키운다
        window = int(RATE * 0.03)
        power = np.concatenate(([0.0], np.cumsum(seg ** 2)))
        loudest = float((power[window:] - power[:-window]).max()) / window if len(seg) > window else float(power[-1]) / window
        gain = 10 ** ((HIT_LOUDNESS + row["peak"] + 2.0) / 20.0) / max(np.sqrt(loudest), 1e-9)
        over = 20 * np.log10(max(float(np.abs(out).max()) * gain, 1e-9)) + 0.5
        gain *= 10 ** (-max(over - HIT_MAX_LIMIT, 0.0) / 20.0)
        out = limiter(out * gain)
    else:
        # 음량: 가장 큰 0.1초의 세기를 줄마다 정한 값(db)에 맞춘다 --- 길이가 다른 소리끼리도 들리는 크기가 비슷해진다
        window = int(RATE * LOUD_WINDOW)
        power = np.concatenate(([0.0], np.cumsum(seg ** 2)))
        loudest = float((power[window:] - power[:-window]).max()) if len(seg) > window else float(power[-1])
        out *= (10 ** (row["db"] / 20.0)) / max(np.sqrt(loudest / window), 1e-9)
    # 꼭대기 -1dB: 넘는 만큼(3dB 까지)은 부드럽게 눌러 세기를 지키고, 그보다 더 넘으면 전체를 줄인다
    limit_level = 10 ** (-1.0 / 20.0)
    over = float(np.abs(out).max()) / limit_level
    if over > SOFT_OVER:
        out *= SOFT_OVER / over
    if over > 1.0:
        knee = limit_level * 0.6
        loud = np.abs(out) > knee
        out[loud] = np.sign(out[loud]) * (knee + (limit_level - knee) * np.tanh((np.abs(out[loud]) - knee) / (limit_level - knee)))
    if "delay" in row:
        out = np.concatenate((np.zeros((int(RATE * row["delay"]), out.shape[1])), out))
    measured.update({"sec": round(len(out) / RATE, 3), "over_db": round(20 * np.log10(max(over, 1e-9)), 1), "peak_db": round(20 * np.log10(float(np.abs(out).max())), 1)})
    return out, measured


def letters(row):
    if "src" in row:
        return "a"
    return LETTERS + (ALT_LETTERS if "alt" in row else "") + "".join(sorted(row.get("extra", {})))


def fetched(row):
    """ElevenLabs 로 받는 후보 글자"""
    return "" if "src" in row else LETTERS + (ALT_LETTERS if "alt" in row else "")


def source(row, letter):
    """후보의 원본 => (파일, 구간 또는 None)"""
    spec = row["src"] if "src" in row else row.get("extra", {}).get(letter)
    if spec is None or "base" in spec:
        return "%s/%s_%s.mp3" % (RAW, row["id"], spec["base"] if spec else letter), None
    return OUT + "/" + spec["file"], spec.get("span")


def settings(row, letter):
    """후보에 쓰는 줄 값 (extra 의 set 이 덮는다)"""
    spec = row.get("extra", {}).get(letter, {})
    return dict(row, **spec.get("set", {})) if "set" in spec else row


def prompt_of(row, letter):
    if letter in ALT_LETTERS:
        return row["alt"]
    spec = row.get("extra", {}).get(letter)
    return row.get("prompt", "") if spec is None or "base" in spec else ""


def score(row, m, prompt):
    """높을수록 좋다"""
    value = m["captured"] - 1.5 * m["cut"] - 0.5 * m["cut_in"] - 3.0 * m["noise"]
    if "dry" in prompt:
        value -= 0.6 * m["ring"]
    # 원본이 너무 작게 나온 후보는 크게 키우는 동안 잡음도 같이 올라온다 (-18dBFS 아래부터 감점, -30dBFS 아래는 받기에 실패한 것이라 버린다).
    # 주문에 soft·quiet 를 넣으면 원본이 작게 나온다 --- 부드러움은 재질로 적고 크기는 db 로 정할 것
    value -= 0.5 * min(max((-18.0 - m["raw_peak_db"]) / 8.0, 0.0), 1.0) + (5.0 if m["raw_peak_db"] < -30.0 else 0.0)
    family = FAMILY.get(row.get("family"))
    if family:
        value += 0.25 * inside(m["centroid"], family[0])
    if row.get("hit"):
        # 때리는 소리: 빨리 올라서고(12ms 안), 앞에 몰려 있고, 타격 앞에 딴 소리가 없고, 재질다운 무게가 있는 후보가 낫다
        # 앞쏠림은 0~12dB 가 알맞다: 낮으면 '베고 나서 쿵'으로 갈라지고, 너무 높으면 몸통 없이 '딱' 소리뿐이다
        value += 0.3 * (1.0 - min(m["attack_ms"] / 12.0, 1.0)) - 0.5 * m["lead"]
        value += 0.3 * (min(max((m["forward_db"] + 6.0) / 6.0, 0.0), 1.0) if m["forward_db"] < 12.0 else max(0.0, 1.0 - (m["forward_db"] - 12.0) / 12.0))
        if family and "thump" not in row:
            value += 0.25 * inside(m["low"], family[1])
    # 여러 소리로 이루어진 줄은 한 흐름으로 이어진 후보가 낫다 (사이가 길게 뜨면 뒤쪽이 버려진다)
    if row["kind"] in ("phrase", "stinger"):
        value -= 0.3 * (1.0 - m["main"])
        # 기준 길이에 비해 너무 짧으면 뒤쪽 동작이 빠진 것이다 (물약: 마개만 있고 삼키는 소리가 없음 등)
        value -= 0.5 * max(0.0, 0.6 - m["sec"] / row["keep"])
    return round(float(value), 3)


def write(path, data):
    wavfile.write(path, RATE, np.round(np.clip(data, -1.0, 1.0) * 32767.0).astype(np.int16).squeeze())


def api_key():
    key = os.environ.get("ELEVENLABS_API_KEY", "").strip()
    if not key and os.path.exists(KEY_FILE):
        key = io.open(KEY_FILE, encoding="utf-8-sig").read().strip()
    if not key:
        print("API 키가 없음 --- Saved/elevenlabs_api_key.txt 에 한 줄로 넣어 주세요")
        sys.exit(1)
    return key


def fetch(row, letter, key):
    body = json.dumps({"text": prompt_of(row, letter), "duration_seconds": row["sec"], "prompt_influence": 0.4 if row["kind"] == "stinger" else 0.6})
    out = "%s/%s_%s.mp3" % (RAW, row["id"], letter)
    request = "%s/_request_%s_%s.json" % (RAW, row["id"], letter)
    io.open(request, "w", encoding="utf-8").write(body)
    # 키는 표준 입력으로 넘기는 설정에 담는다 (명령줄·화면에 남기지 않는다)
    config = 'header = "xi-api-key: %s"\nheader = "Content-Type: application/json"\n' % key
    try:
        for attempt in range(4):
            r = subprocess.run(["curl", "-s", "-m", "120", "-X", "POST", API, "-K", "-", "--data-binary", "@" + request, "-o", out, "-w", "%{http_code}"],
                               input=config.encode("utf-8"), capture_output=True)
            code = r.stdout.decode("utf-8", "replace").strip()
            if code == "200":
                return
            detail = io.open(out, encoding="utf-8", errors="replace").read()[:300] if os.path.exists(out) else ""
            if os.path.exists(out):
                os.remove(out)
            if code != "429" and not code.startswith("5"):
                raise RuntimeError("HTTP %s %s" % (code, detail))
            time.sleep(4 * (attempt + 1))     # 요청이 몰렸으면 잠깐 쉬고 다시
        raise RuntimeError("HTTP %s (다시 시도해도 실패)" % code)
    finally:
        if os.path.exists(request):
            os.remove(request)


def layers_of(parts, rows, report):
    """겹칠 재료: 그 줄에서 이미 고른 후보를 음량 맞추기 전 상태로 가져온다 (그래서 재료 줄이 목록에서 먼저 와야 한다)"""
    layers = []
    for part in parts:
        layer_row = rows[part["id"]]
        chosen = report[part["id"]]["chosen"]
        layer, _ = shape(source(layer_row, chosen), settings(layer_row, chosen), finish=False)
        if "len" in part:
            # 층을 len 초까지만 남기고 뒤쪽 절반을 서서히 줄인다 --- 길게 깔린 땅울림이 타격의 잦아듦을 덮지 않게
            layer = layer[:int(RATE * part["len"])].copy()
            fade = len(layer) // 2
            layer[-fade:] *= (np.cos(np.linspace(0.0, np.pi / 2.0, fade)) ** 2)[:, None]
        layers.append((highpass(layer, part["hp"]) if "hp" in part else layer, part["at"], part["gain"]))
    return layers


def choose(row, picks, rows, report):
    """받은 후보를 전부 가공하고 하나를 고른다 => report 한 줄"""
    layers = layers_of(row.get("mix", []), rows, report)
    extra = row.get("extra", {})
    found = {}
    for letter in letters(row):
        src = source(row, letter)
        if not os.path.exists(src[0]):
            continue
        mixed = layers_of(extra[letter]["mix"], rows, report) if "mix" in extra.get(letter, {}) else layers
        try:
            data, measured = shape(src, settings(row, letter), mixed)
        except Exception as e:
            found[letter] = {"error": str(e)[:120]}
            continue
        measured["score"] = score(row, measured, prompt_of(row, letter))
        write("%s/%s_%s.wav" % (CAND, row["id"], letter), data)
        found[letter] = measured
    good = {k: v for k, v in found.items() if "score" in v}
    if not good:
        return None
    # 자동 고르기는 받은 후보(A~C)와 auto 합성 후보에서, 그것이 하나도 없을 때만 다른 느낌(D·E)에서 --- 무료 후보는 사람이 골랐을 때만 쓴다
    main = {k: v for k, v in good.items() if k in LETTERS or extra.get(k, {}).get("auto")} or {k: v for k, v in good.items() if k in ALT_LETTERS}
    auto = max(main, key=lambda k: main[k]["score"]) if main else None
    chosen = picks[row["id"]] if picks.get(row["id"]) in good else auto
    if chosen is None:
        return None
    mixed = layers_of(extra[chosen]["mix"], rows, report) if "mix" in extra.get(chosen, {}) else layers
    data, _ = shape(source(row, chosen), settings(row, chosen), mixed)
    write("%s/%s.wav" % (WAV, row["id"]), data)
    return {"chosen": chosen, "auto": auto, "candidates": found}


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "list"
    rows = json.load(io.open(HERE + "/sfx_palette.json", encoding="utf-8"))["sounds"]
    by_id = {r["id"]: r for r in rows}
    assert len(by_id) == len(rows), "id 가 겹침"
    picked = sys.argv[2:]
    for name in picked:
        assert name in by_id, "없는 id: " + name
    for index, row in enumerate(rows):
        before = [r["id"] for r in rows[:index]]
        for part in row.get("mix", []) + [p for spec in row.get("extra", {}).values() for p in spec.get("mix", [])]:
            assert part["id"] in before, "겹칠 재료 줄은 쓰는 줄보다 앞에 있어야 함: %s <= %s" % (row["id"], part["id"])
        for letter, spec in row.get("extra", {}).items():
            assert letter in EXTRA_LETTERS, "extra 후보 글자는 %s 중에서: %s" % (EXTRA_LETTERS, row["id"])
        for spec in ([row["src"]] if "src" in row else []) + list(row.get("extra", {}).values()):
            assert ("file" in spec) != ("base" in spec), "원본은 file 과 base 중 하나만: %s" % row["id"]
            if "file" in spec:
                assert os.path.exists(OUT + "/" + spec["file"]), "없는 파일: " + spec["file"]
            else:
                assert spec["base"] in fetched(row), "base 는 받는 후보 글자(%s)여야 함: %s" % (fetched(row), row["id"])
            span = spec.get("span")
            assert span is None or (len(span) == 2 and 0.0 <= span[0] < span[1]), "span 은 [시작, 끝] (시작 < 끝): %s" % row["id"]
    for folder in (RAW, CAND, WAV):
        os.makedirs(folder, exist_ok=True)
    todo = [r for r in rows if not picked or r["id"] in picked]
    report = json.load(io.open(OUT + "/report.json", encoding="utf-8")) if os.path.exists(OUT + "/report.json") else {}
    # 겹칠 재료는 기록에 이미 있거나 이번에 먼저 가공돼야 한다 (일부만 process 할 때)
    for index, row in enumerate(todo):
        ready = set(report) | {r["id"] for r in todo[:index]}
        for part in row.get("mix", []) + [q for spec in row.get("extra", {}).values() for q in spec.get("mix", [])]:
            assert part["id"] in ready, "겹칠 재료가 아직 가공 안 됨: %s => 먼저 process %s" % (row["id"], part["id"])
    need = [(r, letter) for r in todo for letter in fetched(r) if mode == "regen" or not os.path.exists("%s/%s_%s.mp3" % (RAW, r["id"], letter))
            or (mode == "refill" and report.get(r["id"], {}).get("candidates", {}).get(letter, {}).get("raw_peak_db", 0.0) < -30.0)]
    if mode == "list":
        print("소리 %d개 (후보 %d개씩, alt 줄은 %d개 더, 무료·합성 후보 %d개) --- 아직 안 받은 후보 %d개, 요청 길이 %.1f초" % (
            len(rows), len(LETTERS), len(ALT_LETTERS), sum(len(r.get("extra", {})) for r in rows), len(need), sum(r["sec"] for r, _ in need)))
        return
    failed = []
    if mode in ("gen", "regen", "refill") and need:
        key = api_key()

        def job(item):
            try:
                fetch(item[0], item[1], key)
                return None
            except Exception as e:
                return "%s_%s: %s" % (item[0]["id"], item[1], str(e)[:200])

        # 키·요금제 문제면 전부 실패하므로 하나 먼저 보내 본다
        first = job(need[0])
        if first and ("HTTP 401" in first or "HTTP 402" in first or "HTTP 403" in first):
            print("키·요금제 문제로 보여 멈춤:", first)
            return
        failed = [first] if first else []
        with ThreadPoolExecutor(max_workers=WORKERS) as pool:
            failed += [f for f in pool.map(job, need[1:]) if f]
    picks = json.load(io.open(OUT + "/picks.json", encoding="utf-8")) if os.path.exists(OUT + "/picks.json") else {}
    for row in todo:
        line = choose(row, picks, by_id, report)
        if line is None:
            print("%-26s 후보 없음" % row["id"])
            continue
        report[row["id"]] = line
        m = line["candidates"][line["chosen"]]
        print("%-26s %s (자동 %s)  %.2f초  잘림 %.2f  담음 %.2f  오름 %.0fms  밝기 %dHz  낮은쪽 %.2f  점수 %s" % (
            row["id"], line["chosen"], line["auto"], m["sec"], m["cut"], m["captured"], m["attack_ms"], m["centroid"], m["low"],
            {k: v.get("score") for k, v in line["candidates"].items()}))
    # 목록에서 빠진 줄은 기록에서도 걷는다
    report = {k: v for k, v in report.items() if k in by_id}
    json.dump(report, io.open(OUT + "/report.json", "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("받기 실패 %d" % len(failed))
    for f in failed[:10]:
        print("  ", f)


if __name__ == "__main__":
    main()
