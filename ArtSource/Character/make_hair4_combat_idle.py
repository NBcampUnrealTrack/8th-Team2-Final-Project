# -*- coding: utf-8 -*-
# 머리 4번(긴 꽁지머리) 전투대기 시트를 직업마다 만든다 --- 로컬 파일만 만든다(에디터 안 씀).
#   경환 님(10/14): "공격 대기모션은 마법사에 머리긴거 공격 대기모션 머리카락 사용하시면 됩니다"
#   => 마법사 전투대기 머리(Mage/MCh_Mage_Hair_Combat_Idle)를 각 직업 전투대기 칸에 프레임마다 옮겨 놓는다.
#   - 자리: 프레임마다 마법사 머리가 그 직업의 원래 전투대기 머리와 가장 많이 겹치는 정수 이동
#     (얼굴 무게중심 차이 둘레 +-8칸에서 찾는다). 직업마다 머리 흔들림 박자가 달라서 프레임마다 따로 잰다
#   - 빈칸 메우기: 직업 얼굴은 마법사 얼굴과 모양이 조금 달라 마법사 머리를 얹으면 뺨·귀 자리에 막힌 빈칸이 생긴다
#     => 원래 합성에는 없던 막힌 빈칸만 같은 이동의 마법사 얼굴 색으로 채워 머리 4번 시트에 넣는다(머리는 얼굴 위 층이라 빈칸만 덮는다)
#   - 칸 크기·피벗·프레임 시간은 그 직업 전투대기 몸 시트 그대로(combat_meta.json)
#
# 결과: <직업>/MCh_<직업>_Hair4_Combat_Idle_Sheet.png, Novice/MCh_Hair4_Combat_Idle_Sheet.png (마법사는 자기 머리가 긴 머리라 안 만듦)
# usage: python -I make_hair4_combat_idle.py
import os
import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
# 직업: (시트 경로 틀, 원래 머리 층 이름, 결과 이름)
JOBS = {"Novice": ("Novice/MCh_%s_Combat_Idle_Sheet.png", "Hair1", "Novice/MCh_Hair4_Combat_Idle_Sheet.png"),
        "NaiaKnight": ("NaiaKnight/MCh_NaiaKnight_%s_Combat_Idle_Sheet.png", "Hair", "NaiaKnight/MCh_NaiaKnight_Hair4_Combat_Idle_Sheet.png"),
        "Gambler": ("Gambler/MCh_Gambler_%s_Combat_Idle_Sheet.png", "Hair", "Gambler/MCh_Gambler_Hair4_Combat_Idle_Sheet.png"),
        "Tracker": ("Tracker/MCh_Tracker_%s_Combat_Idle_Sheet.png", "Hair", "Tracker/MCh_Tracker_Hair4_Combat_Idle_Sheet.png"),
        "Druid": ("Druid/MCh_Druid_%s_Combat_Idle_Sheet.png", "Hair", "Druid/MCh_Druid_Hair4_Combat_Idle_Sheet.png")}
SEARCH = 8


def cells(name):
    a = np.asarray(Image.open("%s/%s" % (HERE, name)).convert("RGBA"))
    w = a.shape[1] // 8
    return [a[:, k * w:(k + 1) * w] for k in range(8)]


def place(img, dx, dy, shape):
    """img 를 (dx, dy) 옮겨 shape 칸에 놓는다 (칸 밖은 잘린다)"""
    out = np.zeros(shape + (4,), np.uint8)
    h, w = img.shape[:2]
    sx, sy, tx, ty = max(0, -dx), max(0, -dy), max(0, dx), max(0, dy)
    ww, hh = min(w - sx, shape[1] - tx), min(h - sy, shape[0] - ty)
    out[ty:ty + hh, tx:tx + ww] = img[sy:sy + hh, sx:sx + ww]
    return out


def solid(*layers):
    m = np.zeros(layers[0].shape[:2], bool)
    for l in layers:
        m |= l[:, :, 3] > 0
    return m


def enclosed(mask):
    """불투명 칸에 둘러싸여 칸 가장자리와 이어지지 않는 빈칸"""
    label, count = ndimage.label(~mask)
    edge = set(np.unique(label[0])) | set(np.unique(label[-1])) | set(np.unique(label[:, 0])) | set(np.unique(label[:, -1]))
    return np.isin(label, [k for k in range(1, count + 1) if k not in edge])


def centroid(mask):
    ys, xs = np.nonzero(mask)
    return xs.mean(), ys.mean()


mage_hair = cells("Mage/MCh_Mage_Hair_Combat_Idle_Sheet.png")
mage_face = cells("Mage/MCh_Mage_FaceEye_Combat_Idle_Sheet.png")
for job, (pattern, hair_name, out_name) in JOBS.items():
    body, face, hair = cells(pattern % "Body"), cells(pattern % "FaceEye"), cells(pattern % hair_name)
    shape = body[0].shape[:2]
    out_cells, shifts, filled, clipped = [], [], [], 0
    for f in range(8):
        (jx, jy), (mx, my) = centroid(face[f][:, :, 3] > 0), centroid(mage_face[f][:, :, 3] > 0)
        cx, cy = int(round(jx - mx)), int(round(jy - my))
        target = hair[f][:, :, 3] > 0
        best = None
        for dy in range(cy - SEARCH, cy + SEARCH + 1):
            for dx in range(cx - SEARCH, cx + SEARCH + 1):
                m = place(mage_hair[f], dx, dy, shape)[:, :, 3] > 0
                iou = (m & target).sum() / max((m | target).sum(), 1)
                if best is None or iou > best[0]:
                    best = (iou, dx, dy)
        _, dx, dy = best
        cell = place(mage_hair[f], dx, dy, shape)
        clipped += int((mage_hair[f][:, :, 3] > 0).sum() - (cell[:, :, 3] > 0).sum())
        # 원래 합성에 없던 막힌 빈칸만 마법사 얼굴 색으로
        fill = enclosed(solid(body[f], face[f], cell)) & ~enclosed(solid(body[f], face[f], hair[f]))
        mface = place(mage_face[f], dx, dy, shape)
        use = fill & (mface[:, :, 3] > 0)
        cell[use] = mface[use]
        assert not (fill & ~use).any(), "%s F%d: 마법사 얼굴로 못 메운 빈칸 %d" % (job, f, int((fill & ~use).sum()))
        out_cells.append(cell)
        shifts.append((dx, dy))
        filled.append(int(use.sum()))
    Image.fromarray(np.concatenate(out_cells, axis=1)).save("%s/%s" % (HERE, out_name))
    print("%-10s 칸 %dx%d 이동 %s 메운 칸 %s 잘린 칸 %d => %s" % (job, shape[1], shape[0], shifts, filled, clipped, out_name))
