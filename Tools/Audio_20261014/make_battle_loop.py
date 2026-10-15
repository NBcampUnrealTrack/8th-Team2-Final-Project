# -*- coding: utf-8 -*-
"""Suno 전투곡을 끊김 없이 반복되는 루프 파일로 만든다.

1. 박자·마디(4박) 시작점을 찾는다.
2. 마디마다 음색·화성(크로마+MFCC, 박 단위)을 뽑고, '마디 j 이후 K마디'가 '마디 i 이후 K마디'와 가장 닮은 (i, j) 를 고른다.
   그러면 j 에서 i 로 되돌아가도 곡이 원래 흘러가던 것과 같은 음악이 이어진다. 인트로(앞)와 아웃트로(뒤)는 피한다.
3. 원본 해상도에서 j 를 ±25ms 안에서 파형 상관으로 미세 조정하고, 이음매는 i 직전 소리와 40ms 동안 겹쳐 잇는다.
4. 음량은 전투 중 효과음이 묻히지 않게 목표 LUFS 로 낮춘다.

출력: <out>.wav(루프), <out>_seam_check.wav(이음매 앞뒤 8초씩 두 번 반복, 귀로 확인용), <out>.json(근거)
사용: python -I make_battle_loop.py <원본> <출력 경로(확장자 없이)> [목표 LUFS=-20]
"""
import json
import subprocess
import sys

import librosa
import numpy as np
import soundfile as sf

SRC, OUT = sys.argv[1], sys.argv[2]
TARGET_LUFS = float(sys.argv[3]) if len(sys.argv) > 3 else -20.0
FFMPEG = 'ffmpeg'

# ---- 분석(모노 22.05k)
y, sr = librosa.load(SRC, sr=22050, mono=True)
hop = 512
onset = librosa.onset.onset_strength(y=y, sr=sr, hop_length=hop)
tempo, beat_frames = librosa.beat.beat_track(onset_envelope=onset, sr=sr, hop_length=hop, start_bpm=150)
beat_t = librosa.frames_to_time(beat_frames, sr=sr, hop_length=hop)

# 마디 첫 박: 저음(킥) 에너지가 가장 큰 박 위치(4로 나눈 나머지)
S = np.abs(librosa.stft(y, hop_length=hop))
freqs = librosa.fft_frequencies(sr=sr)
low = S[freqs < 150].sum(0)
phase_score = [low[beat_frames[p::4]].mean() + onset[beat_frames[p::4]].mean() * 0 for p in range(4)]
phase = int(np.argmax(phase_score))
bar_beats = list(range(phase, len(beat_t) - 4, 4))
bar_t = beat_t[bar_beats]

# 박 단위 특징 → 마디는 4박 특징을 이어 붙인다(박자 안의 모양까지 비교)
chroma = librosa.feature.chroma_cqt(y=y, sr=sr, hop_length=hop)
mfcc = librosa.feature.mfcc(y=y, sr=sr, n_mfcc=13, hop_length=hop)
feat = np.vstack([chroma, librosa.util.normalize(mfcc, axis=1) * 0.6])
beat_feat = librosa.util.sync(feat, beat_frames, aggregate=np.mean)   # (D, beats+1)
beat_feat = beat_feat / (np.linalg.norm(beat_feat, axis=0, keepdims=True) + 1e-9)


def window(bar_index, bars):
    b0 = bar_beats[bar_index]
    cols = beat_feat[:, b0:b0 + 4 * bars]
    return cols


K = 4
dur = len(y) / sr
cands = []
for i in range(len(bar_t)):
    if bar_t[i] < 3.0:                       # 인트로 첫 마디들은 피한다
        continue
    for j in range(i + 1, len(bar_t)):
        loop_len = bar_t[j] - bar_t[i]
        if loop_len < 60.0 or bar_t[j] > dur - 6.0:   # 너무 짧은 루프·아웃트로 피함
            continue
        if bar_beats[j] + 4 * K >= beat_feat.shape[1] or bar_beats[i] + 4 * K >= beat_feat.shape[1]:
            continue
        a, b = window(i, K), window(j, K)
        ahead = float((a * b).sum(0).mean())                 # 이음매 뒤로 이어질 음악이 같은가
        pa = beat_feat[:, bar_beats[i] - 4:bar_beats[i]] if bar_beats[i] >= 4 else None
        pb = beat_feat[:, bar_beats[j] - 4:bar_beats[j]]
        behind = float((pa * pb).sum(0).mean()) if pa is not None else ahead
        score = ahead * 0.7 + behind * 0.3 + 0.0006 * loop_len   # 길수록 반복감이 덜하다
        cands.append((score, i, j, ahead, behind))
cands.sort(reverse=True)
best = cands[0]
_, bi, bj, ahead, behind = best
t_i, t_j = float(bar_t[bi]), float(bar_t[bj])

# ---- 원본 해상도에서 미세 조정
audio, fs = sf.read(SRC.replace('.mp3', '.wav')) if SRC.endswith('.wav') else (None, None)
if audio is None:
    tmp = OUT + '_decoded.wav'
    subprocess.run([FFMPEG, '-y', '-v', 'error', '-i', SRC, '-ar', '48000', '-ac', '2', '-c:a', 'pcm_f32le', tmp], check=True)
    audio, fs = sf.read(tmp, dtype='float32')
mono = audio.mean(1)
si, sj = int(round(t_i * fs)), int(round(t_j * fs))
win = int(0.6 * fs)
search = int(0.025 * fs)
ref = mono[si:si + win]
best_off, best_corr = 0, -1e9
for off in range(-search, search + 1, 4):
    seg = mono[sj + off:sj + off + win]
    c = float(np.dot(ref, seg) / (np.linalg.norm(ref) * np.linalg.norm(seg) + 1e-9))
    if c > best_corr:
        best_corr, best_off = c, off
sj += best_off
# 샘플 단위 마무리: ±4 샘플 안에서 한 번 더
fine_off, fine_corr = 0, -1e9
for off in range(-4, 5):
    seg = mono[sj + off:sj + off + win]
    c = float(np.dot(ref, seg) / (np.linalg.norm(ref) * np.linalg.norm(seg) + 1e-9))
    if c > fine_corr:
        fine_corr, fine_off = c, off
sj += fine_off

loop = audio[si:sj].copy()
# 이음매: 루프 끝 40ms 를 'i 직전 소리'와 같은 위상으로 겹쳐, 끝 → 처음이 원곡처럼 이어지게
n = int(0.04 * fs)
fade = np.linspace(0.0, 1.0, n, dtype=np.float32)[:, None]
eq_out, eq_in = np.cos(fade * np.pi / 2), np.sin(fade * np.pi / 2)
loop[-n:] = loop[-n:] * eq_out + audio[si - n:si] * eq_in

# ---- 음량: 측정 → 목표 LUFS 로 이득, 피크 -1 dBFS 이하
tmp_loop = OUT + '_prenorm.wav'
sf.write(tmp_loop, loop, fs, subtype='FLOAT')
meas = subprocess.run([FFMPEG, '-v', 'info', '-i', tmp_loop, '-af', 'ebur128=framelog=quiet', '-f', 'null', '-'], capture_output=True, text=True).stderr
lufs = float([l for l in meas.splitlines() if l.strip().startswith('I:')][-1].split()[1])
gain = 10 ** ((TARGET_LUFS - lufs) / 20)
loop *= gain
peak = float(np.abs(loop).max())
if peak > 10 ** (-1 / 20):
    loop *= 10 ** (-1 / 20) / peak
sf.write(OUT + '.wav', loop, fs, subtype='PCM_16')

# 이음매 확인용: 루프 끝 8초 + 처음 8초를 두 번
tail, head = loop[-8 * fs:], loop[:8 * fs]
sf.write(OUT + '_seam_check.wav', np.concatenate([tail, head, tail, head]), fs, subtype='PCM_16')

# 이음매 품질 지표: 이음매 앞뒤 스펙트럼 변화량을 곡 전체 박 경계 변화량과 비교
def frame_spec(x, at):
    w = x[at - 2048:at + 2048].mean(1) if x.ndim == 2 else x[at - 2048:at + 2048]
    return np.abs(np.fft.rfft(w * np.hanning(len(w))))
seam = np.concatenate([loop[-fs:], loop[:fs]])
flux_seam = float(np.linalg.norm(frame_spec(seam, fs + 2048) - frame_spec(seam, fs - 2048)))
flux_ref = [float(np.linalg.norm(frame_spec(loop, int(t * fs) + 2048) - frame_spec(loop, int(t * fs) - 2048)))
            for t in np.arange(2.0, len(loop) / fs - 2.0, 60 / float(np.atleast_1d(tempo)[0]))]
report = {
    'source': SRC, 'tempo_bpm': float(np.atleast_1d(tempo)[0]), 'downbeat_phase': phase,
    'loop_start_s': si / fs, 'loop_end_s': sj / fs, 'loop_length_s': (sj - si) / fs,
    'bars': int(bj - bi), 'similarity_ahead': ahead, 'similarity_behind': behind,
    'waveform_corr_at_seam': best_corr, 'seam_flux': flux_seam,
    'beat_flux_median': float(np.median(flux_ref)), 'beat_flux_p90': float(np.percentile(flux_ref, 90)),
    'measured_lufs_before': lufs, 'target_lufs': TARGET_LUFS, 'gain_db': 20 * np.log10(gain), 'peak_dbfs_after': 20 * np.log10(float(np.abs(loop).max()) + 1e-9),
    'top_candidates': [{'score': c[0], 'start': float(bar_t[c[1]]), 'end': float(bar_t[c[2]]), 'ahead': c[3], 'behind': c[4]} for c in cands[:5]],
}
json.dump(report, open(OUT + '.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(json.dumps({k: report[k] for k in report if k != 'top_candidates'}, ensure_ascii=False, indent=1))
print('top', report['top_candidates'])
