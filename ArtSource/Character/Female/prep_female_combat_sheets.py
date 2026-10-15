# -*- coding: utf-8 -*-
# 여자 직업 4종(드루이드·갬블러·법사·트래커)의 전투 대기·공격(10/13 전달본, 4레이어)을 우리 구조로 준비한다 --- 로컬 파일만 만든다(에디터 안 씀).
#   - 남캐 전투 시트와 같은 방식: 원본 칸 크기를 그대로 두고(효과가 잘리지 않게) 8프레임을 가로로 잇는다
#   - 레이어는 몸 / 머리 / 얼굴+눈(외모 한 장) 셋. 무기·카드·마법 효과·곰 정령은 원본이 몸 레이어에 넣어 줬다
#   - 피벗은 원본이 적어 준 두 발 가운데(foot_anchor_xy)의 바로 아랫줄. 대기와 공격은 칸 크기가 달라도 이 점을 맞추면 같은 자리에 선다
#     (F1끼리 겹쳐 다른 칸이 없는지 확인한다)
#   - 얼굴이 안 보이는 프레임(트래커·드루이드 공격 가운데 네 장)은 외모 시트의 그 칸이 빈다
#   - 3/4 대기(1_threequarter_idle)는 쓰는 화면이 없어 만들지 않는다. 사망 동작은 원본에 없다
import io, json, os

import numpy as np
from PIL import Image

SRC = "C:/자료, 파일/NU/image/캐릭터/드루이드, 법사, 겜블러, 트래커 전투 모션(여)/female_class_animations_2026-10-13"
OUT = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
# (우리 이름, 묶음 폴더)
JOBS = [("Druid", "druid"), ("Gambler", "gambler"), ("Mage", "mage"), ("Tracker", "tracer")]
# (원본 폴더, 우리 이름)
ANIMS = [("2_combat_idle", "Combat_Idle"), ("3_attack", "Combat_Attack")]
# (우리 레이어 이름, 합칠 원본 레이어)
LAYERS = [("Body", ["BODY"]), ("Hair", ["HAIR"]), ("FaceEye", ["FACE", "EYES"])]
FPS = 100


def load(pack, anim, sub, i):
    return np.array(Image.open("%s/%s/%s/%s/frame-%02d.png" % (SRC, pack, anim, sub, i)).convert("RGBA"))


def merged(pack, anim, layers, i):
    out = None
    for layer in layers:
        a = load(pack, anim, "layers/%s/frames" % layer, i)
        if out is None:
            out = np.zeros_like(a)
        m = a[:, :, 3] > 0
        out[m] = a[m]
    return out      # 투명한 칸은 색까지 0 (원본이 남긴 보이지 않는 RGB를 버린다)


def placed(frame, pivot, size, origin):
    """프레임을 피벗이 origin에 오도록 size 크기 판에 놓는다 (대기·공격 F1 비교용)"""
    out = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    dx, dy = origin[0] - pivot[0], origin[1] - pivot[1]
    h, w = frame.shape[:2]
    out[dy:dy + h, dx:dx + w] = frame
    return out


meta = {"fps": FPS, "jobs": {}}
for job, pack in JOBS:
    info, first = {}, {}
    for anim, name in ANIMS:
        spec = json.load(io.open("%s/%s/%s/animation.json" % (SRC, pack, anim), encoding="utf-8"))
        cw, ch = spec["canvas"]["width"], spec["canvas"]["height"]
        ax, ay = spec["foot_anchor_xy"]
        whole = [merged(pack, anim, ["BODY", "HAIR", "FACE", "EYES"], i) for i in range(1, 9)]
        for i, im in enumerate(whole):
            # 네 레이어를 합치면 원본 완성 프레임과 같아야 한다
            want = load(pack, anim, "source_frames", i + 1)
            want[want[:, :, 3] == 0] = 0
            assert im.shape == (ch, cw, 4) and (im == want).all(), (job, name, i)
        # 발 기준점: F1에서 기준점 둘레의 맨 아랫줄이 원본이 적은 바닥 줄과 같아야 한다
        near = whole[0][:, max(0, ax - 24):ax + 25, 3] > 0
        ground = int(np.nonzero(near.any(axis=1))[0].max())
        assert ground == ay, (job, name, ground, ay)
        pivot = [ax, ay + 1]
        made, empty = [], {}
        check = [np.zeros((ch, cw, 4), dtype=np.uint8) for _ in range(8)]
        for layer_name, sources in LAYERS:
            frames = [merged(pack, anim, sources, i) for i in range(1, 9)]
            sheet = np.concatenate(frames, axis=1)
            Image.fromarray(sheet).save("%s/WCh_%s_%s_%s_Sheet.png" % (OUT, job, layer_name, name))
            made.append(layer_name)
            blank = [i for i, im in enumerate(frames) if not (im[:, :, 3] > 0).any()]
            if blank:
                empty[layer_name] = blank
            for i, im in enumerate(frames):
                m = im[:, :, 3] > 0
                assert not (check[i][:, :, 3] > 0)[m].any(), (job, name, layer_name, i)     # 레이어끼리 겹치는 칸이 없다
                check[i][m] = im[m]
        for i in range(8):
            assert (check[i] == whole[i]).all(), (job, name, i)     # 세 시트를 겹치면 원본 그대로
        durations = spec["durations"]
        assert all(d * FPS % 1000 == 0 for d in durations), (job, name, durations)
        info[name] = {"source": "%s/%s" % (pack, anim), "cell": [cw, ch], "pivot": pivot, "layers": made, "empty_frames": empty,
                      "durations_ms": durations, "frame_runs": [d * FPS // 1000 for d in durations],
                      "events": [{"name": e["name"], "frame_index": e["frame_index"], "time_ms": e["time_ms"]} for e in spec.get("events", [])]}
        first[name] = (whole[0], pivot)
        print("%-8s %-13s 칸 %dx%d 피벗 (%d,%d) 시간 %s 빈 칸 %s" % (job, name, cw, ch, pivot[0], pivot[1], durations, empty or "-"))
    # 대기 F1과 공격 F1을 피벗으로 맞춰 겹치면 같은 그림이어야 한다 (공격이 대기에서 튀지 않는다)
    size, origin = (800, 600), (400, 400)
    a = placed(first["Combat_Idle"][0], first["Combat_Idle"][1], size, origin)
    b = placed(first["Combat_Attack"][0], first["Combat_Attack"][1], size, origin)
    info["idle_attack_first_frame_diff"] = int((a != b).any(axis=2).sum())
    print("%-8s 대기 F1 과 공격 F1 의 다른 칸 %d" % (job, info["idle_attack_first_frame_diff"]))
    meta["jobs"][job] = info
json.dump(meta, io.open("%s/combat_sheets.json" % OUT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
