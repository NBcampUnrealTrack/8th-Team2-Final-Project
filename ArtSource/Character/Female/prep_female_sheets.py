# -*- coding: utf-8 -*-
# 경환 님 여자 기본 몸(노비스, 레이어 분리 없는 통짜 그림)을 우리 구조로 준비한다 --- 로컬 파일만 만든다(에디터 안 씀).
#   - 57x94 낱장 8프레임을 공통 60x100 칸으로 옮겨 가로로 이은 480x100 시트로 저장 (남캐와 같은 규격: 발바닥 y=96, 몸 중심 x=30)
#   - 가로 기준은 방향마다 대기 그림의 두 발 가운데. 같은 방향의 걷기는 대기와 같은 오프셋을 쓴다 (원본 캔버스에서 이미 맞춰 그려져 있다)
import io, json, os
from PIL import Image

SRC = "C:/자료, 파일/NU/image/캐릭터"
OUT = "C:/UnrealProjects/ProjectM/ArtSource/Character/Female"
CW, CH = 60, 100
ANCHOR_X, GROUND_Y = 30, 96
# 법사는 10/10부터 4레이어 묶음을 쓴다 (prep_female_job_sheets.py 가 WCh_Mage_Body_* 를 몸 레이어로 다시 만든다) --- 여기서는 노비스만
SETS = [("노비스_여자/노비스 여자", "Female_Novice", "WCh_Body")]
# (원본 이름, 우리 이름, 가로 기준을 재는 대기 그림)
ANIMS = [("idle_front", "Idle_Front", "idle_front"), ("run_front", "Walk_Front", "idle_front"),
         ("idle_right", "Idle_Side", "idle_right"), ("run_rightf", "Walk_Side", "idle_right"),
         ("idle_back", "Idle_Back", "idle_back"), ("run_back", "Walk_Back", "idle_back")]


def load(folder, prefix, anim, i):
    return Image.open(os.path.join(SRC, folder, "%s_%s%d.png" % (prefix, anim, i))).convert("RGBA")


def feet_center(im):
    # 맨 아래 4줄(발바닥)에서 불투명한 칸의 가로 가운데
    box = im.getbbox()
    alpha = im.split()[3]
    xs = [x for y in range(box[3] - 4, box[3]) for x in range(im.width) if alpha.getpixel((x, y)) > 0]
    return (min(xs) + max(xs) + 1) / 2.0


meta = {"canvas": [CW, CH], "pivot": [ANCHOR_X, GROUND_Y], "sets": {}}
for folder, prefix, out_prefix in SETS:
    info = {}
    for anim, name, ref in ANIMS:
        if not os.path.exists(os.path.join(SRC, folder, "%s_%s1.png" % (prefix, anim))):
            print("%-16s %-11s 원본 없음 --- 건너뜀" % (prefix, name))
            continue
        centers = [feet_center(load(folder, prefix, ref, i)) for i in range(1, 9)]
        dx = int(round(ANCHOR_X - sum(centers) / len(centers)))
        bottom = max(load(folder, prefix, anim, i).getbbox()[3] for i in range(1, 9))
        dy = GROUND_Y - bottom
        sheet = Image.new("RGBA", (CW * 8, CH), (0, 0, 0, 0))
        for i in range(1, 9):
            sheet.paste(load(folder, prefix, anim, i), ((i - 1) * CW + dx, dy))
        sheet.save(os.path.join(OUT, "%s_%s_Sheet.png" % (out_prefix, name)))
        info[name] = {"source": anim, "offset": [dx, dy]}
        print("%-16s %-11s 오프셋 (%+d,%+d)" % (prefix, name, dx, dy))
    meta["sets"][out_prefix] = info
json.dump(meta, io.open(os.path.join(OUT, "sheets.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
