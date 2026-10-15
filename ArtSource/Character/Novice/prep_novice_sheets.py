# -*- coding: utf-8 -*-
# 경환 님 노비스 애니(레이어 분리본)를 우리 구조로 준비한다 --- 로컬 파일만 만든다(에디터 안 씀).
#   - 애니마다 4레이어를 같은 오프셋으로 공통 60x100 캔버스에 옮긴다 (머리 중심 x=30, 발바닥 y=96)
#   - face + eyes를 합쳐 외모(FaceEye) 한 장으로 만든다 (우리 파츠 구조가 얼굴+눈 한 칸)
#   - 8프레임을 가로로 이어 480x100 시트로 저장 (레이어 x 애니 = 18장)
#   - 프레임 시간은 animation.json 그대로 sheets.json에 남긴다 (플립북 fps 100 + 프레임별 길이)
import io, json, os
from PIL import Image

SRC = "C:/자료, 파일/NU/image/캐릭터/character_animation_layers (1)/character_animation_layers"
# 나중에 따로 받은 수정본 --- 폴더 구조가 다르다(레이어 폴더가 한글 이름, 프레임이 frame_001.png, 시간은 metadata.json)
FIXED = {"side_walk": {"dir": "C:/자료, 파일/NU/image/캐릭터/walk_arm_fixed_8f_4layers/walk_arm_fixed_8f_4layers",
                       "layer_dir": {"body": "몸", "face": "얼굴", "eyes": "눈", "hair": "머리"},
                       # 새 원본은 100ms 균일이지만, 플립북을 이미 110ms로 만들어 둬서 걸음 속도는 그대로 둔다
                       "durations": [110] * 8}}
OUT = "C:/UnrealProjects/ProjectM/ArtSource/Character/Novice"
CW, CH = 60, 100          # 공통 캔버스 (지금 남캐와 같은 크기)
ANCHOR_X, GROUND_Y = 30, 96
ANIMS = [("front_idle", "Idle_Front"), ("front_walk", "Walk_Front"),
         ("side_idle_a", "Idle_Side"), ("side_walk", "Walk_Side"),
         ("back_idle", "Idle_Back"), ("back_walk", "Walk_Back")]
LAYERS = [("Body", ["body"]), ("FaceEye", ["face", "eyes"]), ("Hair1", ["hair"])]
# 프레임마다 가장 낮은 픽셀을 발바닥선에 붙일 애니. 한 프레임의 레이어 전부를 같은 만큼 내린다 (레이어끼리는 안 어긋난다).
# 9/17 옛 옆 걷기는 두 번째 걸음(5~8프레임)이 1~2px 떠서 절뚝여 여기에 Walk_Side를 넣었지만,
# 9/18 팔 수정본은 3번 프레임만 1px 뜨고 그건 걸을 때 몸이 떠오르는 자연스러운 튐이다.
# 그걸 눌러 붙이면 3 => 4 프레임에서 머리가 한 번에 2px 튀어 오른다 (유민 "옆으로 걸을 때 머리 움직임이 부자연스러워").
# 1프레임 1px 발 뜸이 2px 머리 스냅보다 훨씬 덜 보여서 보정을 뺐다
GROUND_LOCK = set()
os.makedirs(OUT, exist_ok=True)


def load(anim, layer, i):
    fix = FIXED.get(anim)
    if fix is not None:
        return Image.open(os.path.join(fix["dir"], fix["layer_dir"][layer], "frame_%03d.png" % i)).convert("RGBA")
    return Image.open(os.path.join(SRC, anim, layer + "_frames", "F%02d.png" % i)).convert("RGBA")


def merged(anim, layers, i):
    base = None
    for l in layers:
        im = load(anim, l, i)
        if base is None:
            base = Image.new("RGBA", im.size, (0, 0, 0, 0))
        base.alpha_composite(im)
    return base


meta = {"canvas": [CW, CH], "pivot": [ANCHOR_X, GROUND_Y], "anims": {}}
for anim, name in ANIMS:
    # 기준점: 머리 가로 중심 평균(걸을 때도 좌우로 거의 안 흔들린다) + 전 프레임 중 가장 낮은 픽셀
    heads, bottom = [], 0
    for i in range(1, 9):
        hb = load(anim, "hair", i).getbbox()
        heads.append((hb[0] + hb[2]) / 2.0)
        bottom = max(bottom, merged(anim, ["body", "face", "eyes", "hair"], i).getbbox()[3])
    dx = int(round(ANCHOR_X - sum(heads) / len(heads)))
    dy = GROUND_Y - bottom
    frame_dy = [0] * 8
    if name in GROUND_LOCK:
        frame_dy = [bottom - merged(anim, ["body", "face", "eyes", "hair"], i).getbbox()[3] for i in range(1, 9)]
    if anim in FIXED:
        times = FIXED[anim]["durations"]
    else:
        times = json.load(io.open(os.path.join(SRC, anim, "animation.json"), encoding="utf-8")).get("durations", [])
    meta["anims"][name] = {"source": anim, "offset": [dx, dy], "frame_ground_lock": frame_dy, "durations_ms": times}
    print("%-12s => %-11s 오프셋 (%+d,%+d)  프레임별 추가 내림 %s  시간 %s" % (anim, name, dx, dy, frame_dy, times))

    for layer_name, parts in LAYERS:
        sheet = Image.new("RGBA", (CW * 8, CH), (0, 0, 0, 0))
        for i in range(1, 9):
            src = merged(anim, parts, i)
            tmp = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
            tmp.paste(src, (dx, dy + frame_dy[i - 1]))
            sheet.paste(tmp, ((i - 1) * CW, 0))
        out = os.path.join(OUT, "MCh_%s_%s_Sheet.png" % (layer_name, name))
        sheet.save(out)
json.dump(meta, io.open(os.path.join(OUT, "sheets.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("\n시트 %d장 + sheets.json => %s" % (len(ANIMS) * len(LAYERS), OUT))
