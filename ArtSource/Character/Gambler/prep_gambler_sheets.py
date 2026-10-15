# -*- coding: utf-8 -*-
# 겜블러(직업 1004) 애니를 우리 구조로 준비한다 --- 로컬 파일만 만든다(에디터 안 씀).
#   - 직업은 몸만 갈린다. 머리·눈·얼굴은 노비스 공용 파츠를 그대로 쓴다 (9/18 유민)
#   - 그래서 몸 레이어만 공통 60x100 캔버스로 옮긴다 (머리 중심 x=30, 발바닥 y=96 --- 노비스와 같은 규칙)
#   - 8프레임을 가로로 이어 480x100 시트로 저장
#
# 받은 폴더 이름이 실제 동작과 다르다 (발 폭을 재서 확인):
#   01_side_walk         => 옆 대기 (발이 안 벌어진다)      Side idle 폴더는 이것의 몸만 뽑은 중복본
#   side_walk_png_frames => 옆 걷기 (성큼성큼, 레이어 4장)
import io, json, os
from PIL import Image

SRC = "C:/자료, 파일/NU/image/캐릭터/겜블러/gambler_5GIF_4layers"
OUT = "C:/UnrealProjects/ProjectM/ArtSource/Character/Gambler"
CW, CH = 60, 100
ANCHOR_X, GROUND_Y = 30, 96
# 프레임 시간은 **공용(노비스) 시트에서 그대로 가져온다**. 직업은 몸만 갈리고 머리·외모는 공용 그림을 얹는데,
# 한 주기 길이가 1ms라도 다르면 머리와 몸이 매 바퀴 조금씩 밀려 서로 다른 박자로 끄덕인다
# (9/18: 옆 걷기만 몸 900ms / 머리 880ms여서 "머리가 더 많이 바운싱"으로 보였다)
NOVICE = json.load(io.open("C:/UnrealProjects/ProjectM/ArtSource/Character/Novice/sheets.json", encoding="utf-8"))

# (원본 폴더, 우리 이름) --- 4레이어로 온 것들
LAYERED = [("04_front_idle", "Idle_Front"),
           ("05_front_walk", "Walk_Front"),
           ("01_side_walk", "Idle_Side"),
           ("side_walk_png_frames", "Walk_Side"),
           ("gambler_back_walk8_4layers", "Walk_Back")]
# 머리 없이 몸만 온 것 --- 가로 자리는 같은 방향 애니의 목 위치에 맞춘다
BODYONLY = [("Back idle", "gambler_back_idle_synced-export%d.png", "Idle_Back", "Walk_Back")]
KO = {"body": "몸", "face": "얼굴", "eyes": "눈", "hair": "머리"}
# 폴더마다 레이어 폴더·파일 이름 규칙이 다르다 (폴더이름 => (레이어 폴더, 파일 이름))
NAMING = {"side_walk_png_frames": (lambda part: part, "%s_F%02d.png")}
os.makedirs(OUT, exist_ok=True)


def layer(anim, part, i):
    folder, pattern = NAMING.get(anim, (lambda p: KO[p], None))
    name = (pattern % (part, i)) if pattern else ("frame_%03d.png" % i)
    return Image.open(os.path.join(SRC, anim, folder(part), name)).convert("RGBA")


def merged(anim, parts, i):
    base = None
    for p in parts:
        im = layer(anim, p, i)
        if base is None:
            base = Image.new("RGBA", im.size, (0, 0, 0, 0))
        base.alpha_composite(im)
    return base


# 목 가로 중심 --- 몸 그림 맨 윗줄 두 줄(옷깃)의 가운데. 머리가 없는 그림의 자리를 맞출 때 쓴다
def collar_center(body, dx=0):
    bbox = body.getbbox()
    px = body.load()
    xs = [x for y in range(bbox[1], min(bbox[1] + 2, bbox[3])) for x in range(body.width) if px[x, y][3] > 0]
    return (min(xs) + max(xs)) / 2.0 + dx


def sheet_from(frames):
    sheet = Image.new("RGBA", (CW * 8, CH), (0, 0, 0, 0))
    for i, im in enumerate(frames):
        sheet.paste(im, (i * CW, 0))
    return sheet


meta = {"canvas": [CW, CH], "pivot": [ANCHOR_X, GROUND_Y], "jobId": 1004, "anims": {}}
collars = {}
for anim, name in LAYERED:
    times = NOVICE["anims"][name]["durations_ms"]
    heads, bottom = [], 0
    for i in range(1, 9):
        hb = layer(anim, "hair", i).getbbox()
        heads.append((hb[0] + hb[2]) / 2.0)
        bottom = max(bottom, merged(anim, ["body", "face", "eyes", "hair"], i).getbbox()[3])
    dx = int(round(ANCHOR_X - sum(heads) / len(heads)))
    dy = GROUND_Y - bottom
    frames = []
    for i in range(1, 9):
        cell = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
        cell.paste(layer(anim, "body", i), (dx, dy))
        frames.append(cell)
    collars[name] = sum(collar_center(f) for f in frames) / 8.0
    sheet_from(frames).save(os.path.join(OUT, "MCh_Gambler_Body_%s_Sheet.png" % name))
    meta["anims"][name] = {"source": anim, "offset": [dx, dy], "durations_ms": times}
    print("%-28s => %-11s 오프셋 (%+d,%+d)  목 중심 %.1f" % (anim, name, dx, dy, collars[name]))

for folder, pattern, name, like in BODYONLY:
    times = NOVICE["anims"][name]["durations_ms"]
    raw = [Image.open(os.path.join(SRC, folder, pattern % i)).convert("RGBA") for i in range(1, 9)]
    bottom = max(im.getbbox()[3] for im in raw)
    dy = GROUND_Y - bottom
    dx = int(round(collars[like] - sum(collar_center(im) for im in raw) / 8.0))
    frames = []
    for im in raw:
        cell = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
        cell.paste(im, (dx, dy))
        frames.append(cell)
    sheet_from(frames).save(os.path.join(OUT, "MCh_Gambler_Body_%s_Sheet.png" % name))
    meta["anims"][name] = {"source": folder, "offset": [dx, dy], "durations_ms": times}
    print("%-28s => %-11s 오프셋 (%+d,%+d)  목 중심 %.1f (%s에 맞춤)"
          % (folder, name, dx, dy, sum(collar_center(f) for f in frames) / 8.0, like))

json.dump(meta, io.open(os.path.join(OUT, "sheets.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("\n시트 %d장 + sheets.json => %s" % (len(LAYERED) + len(BODYONLY), OUT))
