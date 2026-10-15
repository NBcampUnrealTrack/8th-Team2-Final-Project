# -*- coding: utf-8 -*-
"""반복 주기를 알고 있는 곡에서 박에 맞춘 루프 파일을 만든다(make_battle_loop.py 분석 뒤 정밀 단계).

- 루프 길이: 시작점 근처 16초 온셋 패턴이 가장 닮는 지연(L, 초)을 L0 ±0.3초 안에서 5.8ms 단위로 찾는다(박 단위로 이어짐).
- 시작점: 원하는 시각 근처의 가장 센 박(킥) 위치. 끝점 = 시작점 + L 을 저음 파형 상관으로 ±10ms 안에서 다시 맞춘다.
- 이음매: 루프 끝 한 박을 '시작점 직전 한 박'과 등전력으로 겹친다 → 끝에서 처음으로 넘어갈 때 원곡의 같은 박 흐름이 이어진다.
- 음량: 목표 LUFS, 피크 -1 dBFS 이하.

사용: python -I render_loop.py <원본> <출력(확장자 없이)> <시작 근처 초> <L0 초> [목표 LUFS=-19] [BPM 힌트=150]
"""
import json
import subprocess
import sys

import librosa
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt

SRC, OUT = sys.argv[1], sys.argv[2]
START_HINT, L0 = float(sys.argv[3]), float(sys.argv[4])
TARGET = float(sys.argv[5]) if len(sys.argv) > 5 else -19.0
BPM_HINT = float(sys.argv[6]) if len(sys.argv) > 6 else 150.0   # 박 주기를 찾을 범위의 가운데(±10%)

tmp = OUT + '_decoded.wav'
subprocess.run(['ffmpeg', '-y', '-v', 'error', '-i', SRC, '-ar', '48000', '-ac', '2', '-c:a', 'pcm_f32le', tmp], check=True)
audio, fs = sf.read(tmp, dtype='float32')
mono = audio.mean(1)

# 온셋 포락선(분석용 22.05k, hop 128)
y = librosa.resample(mono, orig_sr=fs, target_sr=22050)
sr, hop = 22050, 128
fps = sr / hop
env = librosa.onset.onset_strength(y=y, sr=sr, hop_length=hop)
ac = librosa.autocorrelate(env - env.mean(), max_size=int(3.5 * fps))
lo4, hi4 = int(4 * 60.0 / (BPM_HINT * 1.10) * fps), int(4 * 60.0 / (BPM_HINT * 0.90) * fps)
p4 = lo4 + int(np.argmax(ac[lo4:hi4]))
a, b, c = ac[p4 - 1], ac[p4], ac[p4 + 1]
T = (p4 + 0.5 * (a - c) / (a - 2 * b + c)) / fps / 4

# 시작점: 힌트 ±1박 안에서 저음 에너지가 가장 센 박
sos = butter(4, 160, 'low', fs=fs, output='sos')
bass = sosfiltfilt(sos, mono)
bass_env = np.abs(bass)
search = np.arange(int((START_HINT - T) * fs), int((START_HINT + T) * fs), int(0.002 * fs))
win_k = int(0.06 * fs)
si = int(max(search, key=lambda s: bass_env[s:s + win_k].sum() - bass_env[s - win_k:s].sum()))
import os
EXACT = os.environ.get('LOOP_EXACT') == '1'   # 시작·길이를 이미 정밀하게 찾았으면 그대로 쓴다(앞뒤 정렬 탐색 결과)
if EXACT:
    si = int(round(START_HINT * fs))
t_i = si / fs


def local_corr(start, L, win=16.0):
    s0, n, s1 = int(start * fps), int(win * fps), int((start + L) * fps)
    x, z = env[s0:s0 + n], env[s1:s1 + n]
    x = (x - x.mean()) / (x.std() + 1e-9)
    z = (z - z.mean()) / (z.std() + 1e-9)
    return float(np.dot(x, z) / n)


Ls = np.arange(L0 - 0.3, L0 + 0.3, 1 / fps)
L = L0 if EXACT else float(max(Ls, key=lambda l: local_corr(t_i, l)))
sj = si + int(round(L * fs))
# 끝점 미세 정렬: 두 점 직전 3초의 온셋 포락선(64샘플 간격)을 ±80ms 안에서 맞춘다.
# (Suno 곡은 같은 구간도 파형이 똑같지 않아 파형 상관은 쓸 수 없다 — 박이 들어오는 시각을 맞춘다)
def fine_env(x):
    yy = librosa.resample(x, orig_sr=fs, target_sr=22050)
    return librosa.onset.onset_strength(y=yy, sr=22050, hop_length=64)
ffps = 22050 / 64
W = int(3.0 * fs)
ea = fine_env(mono[si - W:si + int(0.5 * fs)])
eb = fine_env(mono[sj - W:sj + int(0.5 * fs)])
m = min(len(ea), len(eb))
ea = (ea[:m] - ea[:m].mean()) / ea[:m].std()
eb = (eb[:m] - eb[:m].mean()) / eb[:m].std()
best_c, best_lag = -1e9, 0
for lag in range(-int(0.08 * ffps), int(0.08 * ffps) + 1):
    cc = np.dot(ea[lag:], eb[:m - lag]) / (m - lag) if lag >= 0 else np.dot(ea[:m + lag], eb[-lag:]) / (m + lag)
    if cc > best_c:
        best_c, best_lag = float(cc), lag
if not EXACT:
    sj -= int(round(best_lag / ffps * fs))   # 음수 지연 = 끝 구간이 늦다 → 끝점을 그만큼 뒤로
else:
    best_lag = 0
best = best_lag / ffps
loop = audio[si:sj].copy()
n = int(round(T * fs))     # 한 박 길이로 겹친다
x = np.linspace(0.0, 1.0, n, dtype=np.float32)[:, None]
loop[-n:] = loop[-n:] * np.cos(x * np.pi / 2) + audio[si - n:si] * np.sin(x * np.pi / 2)

# 음량
pre = OUT + '_prenorm.wav'
sf.write(pre, loop, fs, subtype='FLOAT')
meas = subprocess.run(['ffmpeg', '-v', 'info', '-i', pre, '-af', 'ebur128=framelog=quiet', '-f', 'null', '-'], capture_output=True, text=True).stderr
lufs = float([l for l in meas.splitlines() if l.strip().startswith('I:')][-1].split()[1])
gain = 10 ** ((TARGET - lufs) / 20)
loop *= gain
peak = float(np.abs(loop).max())
if peak > 10 ** (-1 / 20):
    loop *= 10 ** (-1 / 20) / peak
sf.write(OUT + '.wav', loop, fs, subtype='PCM_16')

# 확인용: (루프 끝 8초 → 처음 8초) 두 번. 여기서 튀는 곳이 없으면 게임에서도 없다.
tail, head = loop[-8 * fs:], loop[:8 * fs]
sf.write(OUT + '_seam_check.wav', np.concatenate([tail, head, tail, head]), fs, subtype='PCM_16')

# 이음매 지표: 이음매 앞뒤 스펙트럼 변화 vs 곡 안의 박 경계 변화
def spec(xx, at):
    w = xx[at - 2048:at + 2048].mean(1)
    return np.abs(np.fft.rfft(w * np.hanning(len(w))))
seam = np.concatenate([loop[-fs:], loop[:fs]])
flux_seam = float(np.linalg.norm(spec(seam, fs + 2048) - spec(seam, fs - 2048)))
grid = np.arange(2.0, len(loop) / fs - 2.0, T)
flux = [float(np.linalg.norm(spec(loop, int(t * fs) + 2048) - spec(loop, int(t * fs) - 2048))) for t in grid]
rep = {'source': SRC, 'beat_s': T, 'bpm': 60 / T, 'loop_start_s': si / fs, 'loop_end_s': sj / fs, 'loop_length_s': (sj - si) / fs,
       'loop_length_beats': (sj - si) / fs / T, 'onset_corr_at_L': local_corr(t_i, L), 'onset_align_corr': best_c, 'end_shift_ms': -best * 1000,
       'crossfade_s': n / fs, 'seam_flux': flux_seam, 'beat_flux_median': float(np.median(flux)), 'beat_flux_p90': float(np.percentile(flux, 90)),
       'lufs_before': lufs, 'target_lufs': TARGET, 'gain_db': 20 * np.log10(gain), 'peak_dbfs': 20 * np.log10(float(np.abs(loop).max()) + 1e-9)}
rep = {k: (float(v) if isinstance(v, (np.floating, float)) else v) for k, v in rep.items()}
json.dump(rep, open(OUT + '.json', 'w', encoding='utf-8'), indent=1)
print(json.dumps(rep, indent=1))
