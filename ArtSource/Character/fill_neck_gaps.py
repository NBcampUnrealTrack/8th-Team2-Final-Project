# 남자 직업 몸 시트의 목 메우기 --- 머리와 몸 사이로 배경이 비치지 않게 한다
#
# 직업 묶음 원본은 목·뒷머리 끝이 머리 쪽 층(머리카락·얼굴)에 그려져 있다. 게임은 직업 몸 위에 공용 머리(노비스 머리 + 얼굴)를
# 바운스 표 높이로 얹는데, 공용 머리에는 그 칸이 없어서 원본에서는 붙어 있는 열이 게임에서는 1~3줄 비어 보인다.
# 그 열의 빈 칸을 원본 머리 층의 색 그대로 몸 시트에 옮겨 넣는다 (몸은 머리 뒤에 그려지므로 머리에 가려지는 칸은 보이지 않는다).
#
# 기준: 게임 합성에서 머리 맨 아래와 몸 사이에 투명 칸이 낀 열 가운데
#   (1) 원본 합성에서는 머리가 몸과 바로 닿는 열 => 원본이 칠한 칸을 전부 메운다
#   (2) 원본에도 틈이 있지만 게임보다 좁은 열 (게임 틈 3줄 이하) => 원본이 칠한 칸만 메워 원본만큼 좁힌다
# 원본에도 똑같이 비어 있는 칸(턱 앞, 뒷목 오목한 자리)은 그림이 그런 것이라 건드리지 않는다. 메운 칸 말고는 한 칸도 바꾸지 않는다.
#
# 바운스 표(Config/DefaultGame.ini [NUFrameBounceSettings] Tracks, 뒷모습은 BackViewPartTuneZ 포함)를 읽어 머리 높이를 정하므로
# 표를 고친 뒤에는 다시 돌린다. 이미 메운 시트에 다시 돌려도 된다 (메운 칸은 몸이 되어 더 메울 것이 없다).
# 돌린 뒤 에디터에서 바뀐 시트를 다시 임포트하고, 몸 스프라이트의 그리는 상자를 다시 굽는다 (피벗을 한 번 움직였다 되돌리면 된다).
#
# usage: python fill_neck_gaps.py [--check]     (--check 는 시트를 쓰지 않고 메울 칸만 센다)
import glob
import io
import json
import os
import re
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/") + "/"
RAW = "C:/자료, 파일/NU/image/캐릭터/"
INI = HERE + "../../Config/DefaultGame.ini"
PACKS = {"Gambler": "겜블러(남)/gambler_5GIF_4layers", "Tracker": "트래커(남)/ranger_5GIF_4layers",
         "Druid": "드루이드(남)/sage_druid_7GIF_4layers", "NaiaKnight": "나이아나이트(남)/knight_animations_4layers"}
ANIMS = ["Idle_Front", "Walk_Front", "Idle_Side", "Walk_Side", "Idle_Back", "Walk_Back"]
LAYERS = {"body": ("몸", "body", "body_frames"), "hair": ("머리", "hair", "hair_frames"), "face": ("얼굴", "face", "face_frames"), "eyes": ("눈", "eyes", "eyes_frames")}
CW, CH = 60, 100
NEAR = 3      # (2)를 적용하는 가장 큰 틈 --- 이보다 벌어진 열은 목이 아니라 머리카락 끝과 어깨 사이


def natural(path):
    return [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", os.path.basename(path))]


def load(path):
    return np.array(Image.open(path).convert("RGBA"))


def cells(sheet):
    """시트를 칸 여덟으로 --- 복사가 아니라 시트를 그대로 가리킨다 (칸에 쓰면 시트가 바뀐다)"""
    return [sheet[:, f * CW:(f + 1) * CW] for f in range(8)]


def motions(job):
    """묶음 안에서 머리 층까지 나뉜 동작마다 {층: [8장]}. 16장짜리는 짝수 장만 뽑은 것도 본다 (트래커 02_side_walk = 옆 대기)"""
    out = {}
    for folder in sorted(glob.glob(glob.escape(RAW + PACKS[job]) + "/*")):
        if not os.path.isdir(folder):
            continue
        found = {}
        for base in (folder, folder + "/frames"):
            for layer, names in LAYERS.items():
                for name in names:
                    files = sorted(glob.glob(glob.escape(base + "/" + name) + "/*.png"), key=natural)
                    if len(files) >= 8 and layer not in found:
                        found[layer] = files
        if "body" in found and "hair" in found:
            name = os.path.basename(folder)
            out[name] = {l: f[:8] for l, f in found.items()}
            if all(len(f) >= 16 for f in found.values()):
                out[name + "#even"] = {l: f[0:16:2] for l, f in found.items()}
    return out


def place(a, t):
    out = np.zeros((CH, CW, 4), dtype=np.uint8)
    h, w = a.shape[:2]
    sx, sy, dx, dy = max(0, -t[0]), max(0, -t[1]), max(0, t[0]), max(0, t[1])
    ww, hh = min(w - sx, CW - dx), min(h - sy, CH - dy)
    if ww > 0 and hh > 0:
        out[dy:dy + hh, dx:dx + ww] = a[sy:sy + hh, sx:sx + ww]
    return out


def bbox(mask):
    ys, xs = np.nonzero(mask)
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1


def find_reference(body, cache):
    """몸 칸과 윤곽이 같은 원본 동작 => (이름, 프레임별 옮김, 층 파일들). 이미 메운 칸이 있어도 찾도록 원본 몸이 칸 안에 다 들어 있는지로 보고,
    그런 것이 여럿이면 색까지 가장 같은 것을 고른다 (트래커 옆 대기는 색을 다듬은 통짜 그림에서 와서 윤곽만 같다)"""
    best = None
    for name, layers in cache.items():
        shifts, same = [], 0.0
        for f in range(8):
            raw = load(layers["body"][f])
            mask, cell = raw[:, :, 3] > 0, body[f][:, :, 3] > 0
            if not mask.any() or not cell.any():
                break
            rb, cb = bbox(mask), bbox(cell)
            t = (int(cb[0] - rb[0]), int(cb[3] - rb[3]))     # 왼쪽 끝과 발끝을 맞춘다
            placed = place(raw, t)
            pm = placed[:, :, 3] > 0
            if pm.sum() != mask.sum() or (pm & ~cell).any() or (cell & ~pm).sum() > 30:
                break
            same += (placed[pm] == body[f][pm]).all(axis=1).mean()
            shifts.append(t)
        if len(shifts) == 8 and (best is None or same > best[0]):
            best = (same, name, shifts, layers)
    return None if best is None else best[1:]


def tables():
    out, tune = {}, {}
    for line in io.open(INI, encoding="utf-8"):
        m = re.match(r'\+Tracks=\(Flipbook="[^"]*/([^/."]+)\.[^"]*",Offsets=\(([^)]*)\)\)', line)
        if m:
            out[m.group(1)] = [int(round(float(v))) for v in m.group(2).split(",")]
        m = re.search(r'FieldSkin="[^"]*SK_Body_Male_(\w+)\.', line)
        if m:
            t = re.search(r"BackViewPartTuneZ=([-\d.]+)", line)
            tune[m.group(1)] = int(round(float(t.group(1)))) if t else 0
    return out, tune


def column_gap(head, body):
    """열 x => 머리 맨 아래와 그 아래 첫 몸 칸 사이 투명 줄 수 (머리가 몸과 겹치면 0, 아래에 몸이 없으면 빠진다)"""
    out = {}
    for x in np.nonzero(head.any(axis=0))[0]:
        bottom = np.nonzero(head[:, x])[0].max()
        if body[bottom, x]:
            out[int(x)] = (0, int(bottom))
            continue
        below = np.nonzero(body[bottom + 1:, x])[0]
        if len(below):
            out[int(x)] = (int(below[0]), int(bottom))
    return out


def main():
    check = "--check" in sys.argv
    table, tune = tables()
    report = {}
    for job in PACKS:
        cache = motions(job)
        for anim in ANIMS:
            key = "MCh_%s_Body_%s" % (job, anim)
            path = "%s%s/%s_Sheet.png" % (HERE, job, key)
            sheet = load(path)
            body = cells(sheet)
            ref = find_reference(body, cache)
            if ref is None:
                print("%-34s 머리 층이 있는 원본 없음 --- 건너뜀" % key)
                continue
            name, shifts, layers = ref
            hair = cells(load("%sNovice/MCh_Hair1_%s_Sheet.png" % (HERE, anim)))
            face = cells(load("%sNovice/MCh_FaceEye_%s_Sheet.png" % (HERE, anim)))
            ups = [v + (tune.get(job, 0) if anim.endswith("Back") else 0) for v in table[key]]
            closed, narrowed = [], []
            for f in range(8):
                game = np.roll((hair[f][:, :, 3] > 0) | (face[f][:, :, 3] > 0), -ups[f], axis=0)
                art = np.zeros((CH, CW, 4), dtype=np.uint8)
                for l in ("face", "eyes", "hair"):     # 원본 합성 순서 --- 얼굴 위에 눈, 그 위에 머리카락
                    if l in layers:
                        p = place(load(layers[l][f]), shifts[f])
                        m = p[:, :, 3] > 0
                        art[m] = p[m]
                bm = body[f][:, :, 3] > 0
                want = column_gap(art[:, :, 3] > 0, bm)
                n = [0, 0]
                for x, (gap, bottom) in column_gap(game, bm).items():
                    if gap == 0 or x not in want or not (want[x][0] == 0 or gap <= NEAR):
                        continue
                    for y in range(bottom + 1, bottom + 1 + gap):
                        if art[y, x, 3] > 0:
                            body[f][y, x] = art[y, x]
                            body[f][y, x, 3] = 255
                            n[want[x][0] != 0] += 1
                closed.append(n[0])
                narrowed.append(n[1])
            report[key] = {"source": name, "offsets": ups, "closed": closed, "narrowed": narrowed}
            print("%-34s 원본 %-26s 머리 높이 %-30s 닫은 칸 %-3d 좁힌 칸 %d" % (key, name, ups, sum(closed), sum(narrowed)))
            if sum(closed) + sum(narrowed) and not check:
                Image.fromarray(sheet).save(path)
    if not check:
        json.dump(report, open(HERE + "neck_fill.json", "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("닫은 칸:", sum(sum(r["closed"]) for r in report.values()), "/ 좁힌 칸:", sum(sum(r["narrowed"]) for r in report.values()))


if __name__ == "__main__":
    main()
