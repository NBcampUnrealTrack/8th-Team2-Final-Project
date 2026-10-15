# -*- coding: utf-8 -*-
# 여캐 직업 몸에 공용 여캐 파츠(머리 2·3, 얼굴 2~5)가 맞게 바운스 표를 만들고, 직업 전용 머리·얼굴 시트를 그만큼 미리 옮긴다.
#   - 파츠 표에서 번호 1은 직업 전용 줄(Hair_Female_<직업>)이 잡히지만 2·3번 머리와 2~5번 얼굴은 공용 줄이라
#     노비스 여캐 머리 자리(노비스 몸 움직임이 구워진 그림)에 그려진다 => 직업 몸 머리 자리와 프레임마다 어긋난다
#   - 남캐와 같은 방식으로 맞춘다: 직업 몸 플립북의 바운스 표(위가 +) = 노비스 머리 자리 - 직업 머리 자리
#     앞·옆은 얼굴 턱(맨 아랫줄), 뒤는 몸 목 윗줄로 잰다
#   - 직업 전용 머리·얼굴도 같은 표만큼 움직이게 되므로 시트를 프레임마다 표만큼 아래로(음수면 위로) 미리 옮겨 화면 자리는 그대로 둔다
#   - 한 번만 옮긴다: job_sheets.json 의 jobs.<직업>.<동작>.bounce 가 있으면 시트는 건드리지 않고 표만 출력한다
#   - 걷기 프레임 시간도 노비스 여캐와 같아야 공용 파츠와 박자가 안 밀린다(10/14 직업 걷기 100ms x8 = 800ms, 공용 파츠 880ms)
#     => durations_ms 를 노비스 값(여캐 노비스 플립북 = Novice/sheets.json 값)으로 적는다. 플립북 frame_run 은 에디터에서 이 값으로 다시 넣는다
#
# usage: python -I align_job_parts_to_novice.py
import io, json, os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
JOBS = ["Druid", "Tracker", "Mage", "Gambler"]
ANIMS = ["Idle_Front", "Walk_Front", "Idle_Side", "Walk_Side", "Idle_Back", "Walk_Back"]
CW = 60


def load(name):
    return np.array(Image.open("%s/%s_Sheet.png" % (HERE, name)).convert("RGBA"))


def alpha_cells(a):
    return [a[:, f * CW:(f + 1) * CW, 3] > 0 for f in range(8)]


def chin(mask):
    return int(np.nonzero(mask.any(1))[0].max())


def neck(mask):
    return int(np.nonzero(mask[:, 20:40].any(1))[0].min())


def shift_down(sheet, table):
    out = np.zeros_like(sheet)
    for f, d in enumerate(table):
        cell = sheet[:, f * CW:(f + 1) * CW]
        ys = np.nonzero((cell[:, :, 3] > 0).any(1))[0]
        assert len(ys) == 0 or (ys.min() + d >= 0 and ys.max() + d < cell.shape[0]), "칸 밖으로 잘림"
        if d >= 0:
            out[d:, f * CW:(f + 1) * CW] = cell[:cell.shape[0] - d]
        else:
            out[:d, f * CW:(f + 1) * CW] = cell[-d:]
    return out


raw = io.open(HERE + "/job_sheets.json", encoding="utf-8", newline="").read()
meta = json.loads(raw)
novice = json.load(io.open(HERE + "/../Novice/sheets.json", encoding="utf-8"))
for anim in ANIMS:
    back = anim.endswith("Back")
    n_body = alpha_cells(load("WCh_Body_" + anim))
    n_face = None if back else alpha_cells(load("WCh_FaceEye_" + anim))
    for job in JOBS:
        entry = meta["jobs"][job][anim]
        if "bounce" in entry:
            print("%-8s %-10s 이미 옮김, 표 %s" % (job, anim, entry["bounce"]))
            continue
        if back:
            j_body = alpha_cells(load("WCh_%s_Body_%s" % (job, anim)))
            table = [neck(n_body[f]) - neck(j_body[f]) for f in range(8)]
        else:
            j_face = alpha_cells(load("WCh_%s_FaceEye_%s" % (job, anim)))
            table = [chin(n_face[f]) - chin(j_face[f]) for f in range(8)]
        for layer in entry["layers"]:
            if layer == "Body":
                continue
            name = "WCh_%s_%s_%s" % (job, layer, anim)
            Image.fromarray(shift_down(load(name), table)).save("%s/%s_Sheet.png" % (HERE, name))
        entry["bounce"] = table
        entry["durations_ms"] = novice["anims"][anim]["durations_ms"]
        print("%-8s %-10s 표 %s  시간 %s" % (job, anim, table, entry["durations_ms"]))
eol = "\r\n" if "\r\n" in raw else "\n"
io.open(HERE + "/job_sheets.json", "w", encoding="utf-8", newline="").write((json.dumps(meta, ensure_ascii=False, indent=1) + "\n").replace("\n", eol))
