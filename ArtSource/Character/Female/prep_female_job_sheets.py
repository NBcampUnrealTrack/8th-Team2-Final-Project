# -*- coding: utf-8 -*-
# 여자 직업 4종(드루이드·갬블러·법사·트래커)의 4레이어 묶음(얼굴·눈·머리·몸, 10/8 전달본)을 우리 구조로 준비한다 --- 로컬 파일만 만든다(에디터 안 씀).
#   - 동작마다 낱장 8프레임을 공통 60x100 칸으로 옮겨 가로로 이은 480x100 시트로 저장 (남캐·여자 기본 몸과 같은 규격: 발바닥 y=96, 몸 중심 x=30)
#   - 레이어는 몸 / 머리 / 얼굴+눈(외모 한 장) 셋으로 낸다. 한 동작의 세 시트는 같은 오프셋을 쓴다 --- 겹치면 원본 그대로다
#   - 가로 기준은 방향마다 대기 몸의 두 발 가운데. 같은 방향의 걷기는 대기와 같은 오프셋을 쓴다 (원본 캔버스에서 이미 맞춰 그려져 있다)
#   - 머리·얼굴은 직업끼리 디자인은 같지만 픽셀이 다르다 (몸에 가려진 자리가 직업마다 다르고, 대기 때 까딱이는 박자도 다르다) --- 직업마다 따로 낸다
#   - 뒷모습은 얼굴·눈 레이어가 비어 있어 외모 시트를 만들지 않는다
#   - 법사 몸 시트(WCh_Mage_Body_*)는 10/2의 통짜 그림을 이 묶음의 몸 레이어로 덮는다
import io, json, os
from PIL import Image

SRC = "C:/자료, 파일/NU/image/캐릭터"
OUT = "C:/UnrealProjects/ProjectM/ArtSource/Character/Female"
CW, CH = 60, 100
ANCHOR_X, GROUND_Y = 30, 96
# (우리 이름, 묶음 폴더)
JOBS = [("Druid", "드루이드(여)/Druid_Four_Layers"), ("Gambler", "겜블러(여)/Gambler_Four_Layers"),
        ("Mage", "법사(여)/Mage_Four_Layers"), ("Tracker", "트래커(여)/Tracer_Four_Layers")]
# (원본 폴더, 우리 이름, 가로 기준을 재는 대기 동작)
ANIMS = [("front_idle", "Idle_Front", "front_idle"), ("front_walk", "Walk_Front", "front_idle"),
         ("side_idle", "Idle_Side", "side_idle"), ("side_walk", "Walk_Side", "side_idle"),
         ("rear_idle", "Idle_Back", "rear_idle"), ("rear_walk", "Walk_Back", "rear_idle")]
# (우리 레이어 이름, 합칠 원본 레이어)
LAYERS = [("Body", ["BODY"]), ("Hair", ["HAIR"]), ("FaceEye", ["FACE", "EYES"])]


def load(pack, anim, layer, i):
    return Image.open(os.path.join(SRC, pack, anim, "layers", layer, "frames", "frame-%02d.png" % i)).convert("RGBA")


def clean(im):
    # 투명한 칸의 색을 지운다 (원본은 보이지 않는 RGB를 몸 레이어에 남겨 뒀다 --- 필터링·압축에서 번지지 않게)
    im.putdata([p if p[3] else (0, 0, 0, 0) for p in im.getdata()])
    return im


def merged(pack, anim, layers, i):
    out = Image.new("RGBA", load(pack, anim, layers[0], i).size, (0, 0, 0, 0))
    for layer in layers:
        out.alpha_composite(load(pack, anim, layer, i))
    return clean(out)


def feet_center(im):
    # 맨 아래 4줄(발바닥)에서 불투명한 칸의 가로 가운데
    box = im.getbbox()
    alpha = im.split()[3]
    xs = [x for y in range(box[3] - 4, box[3]) for x in range(im.width) if alpha.getpixel((x, y)) > 0]
    return (min(xs) + max(xs) + 1) / 2.0


meta = {"canvas": [CW, CH], "pivot": [ANCHOR_X, GROUND_Y], "jobs": {}}
for job, pack in JOBS:
    info = {}
    for anim, name, ref in ANIMS:
        spec = json.load(io.open(os.path.join(SRC, pack, anim, "animation.json"), encoding="utf-8"))
        centers = [feet_center(merged(pack, ref, ["BODY"], i)) for i in range(1, 9)]
        dx = int(round(ANCHOR_X - sum(centers) / len(centers)))
        whole = [merged(pack, anim, ["BODY", "HAIR", "FACE", "EYES"], i) for i in range(1, 9)]
        dy = GROUND_Y - max(im.getbbox()[3] for im in whole)
        made = []
        check = [Image.new("RGBA", (CW, CH), (0, 0, 0, 0)) for _ in range(8)]
        for layer_name, sources in LAYERS:
            frames = [merged(pack, anim, sources, i) for i in range(1, 9)]
            if not any(im.getbbox() for im in frames):
                continue  # 뒷모습의 얼굴·눈
            sheet = Image.new("RGBA", (CW * 8, CH), (0, 0, 0, 0))
            for i, im in enumerate(frames):
                cell = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
                cell.paste(im, (dx, dy))
                # 칸 밖으로 잘린 불투명 칸이 없어야 한다
                assert sum(1 for p in cell.getdata() if p[3]) == sum(1 for p in im.getdata() if p[3]), (job, name, layer_name, i)
                sheet.paste(cell, (i * CW, 0))
                check[i].alpha_composite(cell)
            sheet.save(os.path.join(OUT, "WCh_%s_%s_%s_Sheet.png" % (job, layer_name, name)))
            made.append(layer_name)
        # 세 시트를 겹치면 원본 프레임과 같아야 한다
        for i, im in enumerate(whole):
            want = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
            want.paste(im, (dx, dy))
            assert want.tobytes() == check[i].tobytes(), (job, name, i)
        info[name] = {"source": anim, "offset": [dx, dy], "layers": made, "durations_ms": spec["durations_ms"]}
        print("%-8s %-11s 캔버스 %dx%d 오프셋 (%+d,%+d) 레이어 %s" % (job, name, whole[0].width, whole[0].height, dx, dy, "·".join(made)))
    meta["jobs"][job] = info
json.dump(meta, io.open(os.path.join(OUT, "job_sheets.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
