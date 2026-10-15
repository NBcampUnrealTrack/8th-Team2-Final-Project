# -*- coding: utf-8 -*-
# 남캐 법사(엘리멘탈리스트, 직업 1003) 필드 애니와 새 머리 4번(긴 꽁지머리)을 우리 구조로 준비한다 --- 로컬 파일만 만든다(에디터 안 씀).
#   - 직업은 몸만 갈린다. 얼굴은 노비스 공용 파츠를 얹고, 머리는 고른 머리 파츠를 얹는다 (9/18 유민)
#   - 받은 묶음(64x104, 4레이어)은 노비스 바탕 위에 그린 그림이라 앞·옆 대기·뒤는 발·얼굴 위치가 노비스와 프레임마다 같다.
#     옆 걷기만 걸음이 달라 얼굴이 노비스보다 3~5줄 낮다 => 그 차이를 바운스 표(위가 +)로 적는다
#   - 몸: 발바닥을 y=96(피벗) 바로 위 줄에 맞추고, 가로는 얼굴(앞·옆: 파란 눈 무게중심)이 노비스 얼굴과 겹치게,
#     얼굴이 안 보이는 뒷모습은 얼굴층에 남은 두 귀가 노비스 귀와 겹치게 놓는다
#   - 머리 4번: 받은 머리 층을 몸과 같은 자리에 놓되, 프레임마다 마법사 얼굴과 노비스 얼굴의 높이 차이만큼 옮겨
#     노비스 얼굴 기준으로 둔다 --- 그래야 어느 직업 몸 위에서도 얼굴(공용 파츠)과 같이 움직인다
#   - 프레임 시간은 노비스 시트에서 그대로 가져온다 (머리와 몸의 한 주기가 1ms라도 다르면 박자가 밀린다, 9/18)
#
# 결과: Mage/MCh_Mage_Body_<동작>_Sheet.png, Novice/MCh_Hair4_<동작>_Sheet.png, Mage/sheets.json(오프셋·시간·바운스 표)
# usage: python -I prep_mage_sheets.py
import io, json, os
import numpy as np
from PIL import Image
from scipy import ndimage

SRC = "C:/자료, 파일/NU/image/캐릭터/엘리멘탈리스트(남)/wizard_walk_idle_v4_layers4"
HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
NOVICE_DIR = HERE + "/../Novice"
CW, CH, GROUND_Y = 60, 100, 96
BACK_HAIR_DROP = 2
NOVICE = json.load(io.open(NOVICE_DIR + "/sheets.json", encoding="utf-8"))
# (우리 이름, 묶음 폴더)
ANIMS = [("Idle_Front", "front_idle"), ("Walk_Front", "front_walk"), ("Idle_Side", "side_idle"),
         ("Walk_Side", "side_walk"), ("Idle_Back", "back_idle"), ("Walk_Back", "back_walk")]


def layer(anim, part, i):
    return np.asarray(Image.open("%s/%s/%s_frames/F%02d.png" % (SRC, anim, part, i)).convert("RGBA"))


def novice_cells(prefix, name):
    a = np.asarray(Image.open("%s/%s_%s_Sheet.png" % (NOVICE_DIR, prefix, name)).convert("RGBA"))
    return [a[:, f * CW:(f + 1) * CW] for f in range(8)]


def eye_center(rgba):
    """파란 눈동자 칸의 무게중심 (얼굴이 안 보이면 None)"""
    r, g, b, a = [rgba[:, :, k].astype(int) for k in range(4)]
    m = (a > 0) & (b > r + 40) & (b > 120)
    ys, xs = np.nonzero(m)
    return (float(xs.mean()), float(ys.mean())) if len(xs) else None


def ear_center(rgba):
    """뒷모습 얼굴층(두 귀만 남음)의 귀 자리 --- 왼귀·오른귀 무게중심의 가운데"""
    ys, xs = np.nonzero(rgba[:, :, 3] > 0)
    mid = (xs.min() + xs.max()) / 2.0
    left, right = xs < mid, xs >= mid
    return ((xs[left].mean() + xs[right].mean()) / 2.0, (ys[left].mean() + ys[right].mean()) / 2.0)


def enclosed_mask(solid):
    """불투명 칸에 둘러싸여 칸 가장자리와 이어지지 않는 빈칸"""
    label, count = ndimage.label(~solid)
    outside = set(np.unique(label[0])) | set(np.unique(label[-1])) | set(np.unique(label[:, 0])) | set(np.unique(label[:, -1]))
    return np.isin(label, [k for k in range(1, count + 1) if k not in outside])


def place(img, ox, oy):
    """img 를 (ox, oy) 만큼 옮겨 60x100 칸에 놓는다 (칸 밖은 잘리고, 잘린 불투명 칸 수를 같이 돌려준다)"""
    out = np.zeros((CH, CW, 4), dtype=np.uint8)
    h, w = img.shape[:2]
    sx, sy, dx, dy = max(0, -ox), max(0, -oy), max(0, ox), max(0, oy)
    ww, hh = min(w - sx, CW - dx), min(h - sy, CH - dy)
    out[dy:dy + hh, dx:dx + ww] = img[sy:sy + hh, sx:sx + ww]
    clipped = int((img[:, :, 3] > 0).sum() - (out[:, :, 3] > 0).sum())
    return out, clipped


def merged_face(anim, i):
    face = layer(anim, "face", i).copy()
    eyes = layer(anim, "eyes", i)
    m = eyes[:, :, 3] > 0
    face[m] = eyes[m]
    return face


def sheet(frames):
    out = np.zeros((CH, CW * 8, 4), dtype=np.uint8)
    for f, cell in enumerate(frames):
        out[:, f * CW:(f + 1) * CW] = cell
    return Image.fromarray(out)


meta = {"canvas": [CW, CH], "pivot": [CW // 2, GROUND_Y], "jobId": 1003, "anims": {}}
for name, anim in ANIMS:
    bodies = [layer(anim, "body", i) for i in range(1, 9)]
    hairs = [layer(anim, "hair", i) for i in range(1, 9)]
    h, w = bodies[0].shape[:2]
    # 세로: 네 층을 합친 맨 아랫줄(발바닥)이 피벗 바로 위 줄에 오게
    bottom = max(int(np.nonzero((layer(anim, p, i)[:, :, 3] > 0).any(axis=1))[0].max())
                 for p in ("body", "face", "eyes", "hair") for i in range(1, 9) if (layer(anim, p, i)[:, :, 3] > 0).any())
    oy = (GROUND_Y - 1) - bottom
    base_ox = (CW - w) // 2
    nface = novice_cells("MCh_FaceEye", name)
    eyes = [(eye_center(merged_face(anim, i + 1)), eye_center(nface[i])) for i in range(8)]
    if all(a and b for a, b in eyes):
        # 앞·옆: 눈 무게중심이 노비스 눈과 겹치게
        ox = base_ox - int(round(sum(a[0] + base_ox - b[0] for a, b in eyes) / 8.0))
        head_dy = [int(round(a[1] + oy - b[1])) for a, b in eyes]
        drop = 0
        how = "눈"
    else:
        # 뒤: 얼굴층에 남은 두 귀가 노비스 귀와 겹치게 --- 발로 맞추면 머리가 대기에서 1칸 왼쪽, 대기·걷기 모두 1칸 위에 놓였다(10/14 유민 지적)
        # 귀 자리 = 왼귀·오른귀 무게중심의 가운데. 가로는 동작 평균 한 값, 세로 차이는 프레임마다 바운스 표로
        # 귀로 맞춘 자리는 머리 4번 아랫선이 짧아 뒷목이 보인다 => 머리 4번 시트만 BACK_HAIR_DROP 칸 더 내린다(유민 10/14).
        # 표에는 넣지 않는다 --- 표는 몸의 모든 파츠를 같이 움직여 법사 몸에서 다른 머리가 뜬다
        ears = [(ear_center(merged_face(anim, i + 1)), ear_center(nface[i])) for i in range(8)]
        ox = base_ox - int(round(sum(a[0] + base_ox - b[0] for a, b in ears) / 8.0))
        head_dy = [int(round(a[1] + oy - b[1])) for a, b in ears]
        drop = BACK_HAIR_DROP
        how = "귀"
    body_cells, hair_cells, clip_body, clip_hair, filled, moved, jaw_filled = [], [], 0, 0, 0, 0, 0
    nbody, nhair1 = novice_cells("MCh_Body", name), novice_cells("MCh_Hair1", name)
    for f in range(8):
        cell, c = place(bodies[f], ox, oy)
        clip_body += c
        # 머리는 노비스 얼굴 기준 자리로 --- 마법사 얼굴이 노비스보다 head_dy 줄 낮으면 그만큼 올려 둔다
        hair_cell, c = place(hairs[f], ox, oy - head_dy[f] + drop)
        hair_cells.append(hair_cell)
        clip_hair += c
        # 리본 끈: 원본이 몸 층에 그린 머리 높이의 작은 떨어진 조각(옆모습 꽁지 리본 끝)은 머리 4번 시트로 옮긴다 ---
        # 몸에 두면 다른 머리에서 뒤통수 옆에 점으로 뜬다. 머리 4번은 head_dy 만큼 올려 둔 자리라 그만큼 빼서 놓는다
        # 목 메우기보다 먼저 한다 --- 메운 칸도 떨어진 작은 조각이라 같이 옮겨져 다른 몸에서 턱 밑·뒷목에 줄로 떴다(10/14 유민 지적)
        label, count = ndimage.label(cell[:, :, 3] > 0, structure=np.ones((3, 3)))
        for k in range(1, count + 1):
            piece = label == k
            ys, xs = np.nonzero(piece)
            if len(ys) < 15 and ys.max() < 40 + head_dy[f]:
                ty = ys - head_dy[f] + drop
                ok = (ty >= 0) & (ty < CH)
                hair_cell[ty[ok], xs[ok]] = cell[ys[ok], xs[ok]]
                cell[piece] = 0
                moved += int(ok.sum())
        # 목·턱 메우기: 마법사 얼굴 층은 턱 밑과 목까지 그려져 있는데 공용 얼굴은 얼굴만이라, 게임 합성(몸 + 공용 얼굴 + 머리 4번)에서
        # 그 자리가 비어 머리카락 아래로 막힌 구멍이 난다 => 원본 얼굴 층의 그 칸 색을 몸 시트에 옮긴다 (몸은 머리 뒤라 파츠가 덮는 칸은 안 보인다)
        # 메우는 칸은 머리 4번 합성에서 막힌 빈칸이 되는 곳만 --- 그 밖의 칸(뒷목 외곽선 등)까지 옮기면 짧은 머리에서 점으로 뜬다
        wface, _ = place(merged_face(anim, f + 1), ox, oy)
        game = (cell[:, :, 3] > 0) | np.roll((nface[f][:, :, 3] > 0) | (hair_cell[:, :, 3] > 0), head_dy[f], axis=0)
        fill = (wface[:, :, 3] > 0) & enclosed_mask(game)
        cell[fill] = wface[fill]
        filled += int(fill.sum())
        # 턱 밑·뒷목: 노비스 얼굴층은 옆모습에서 턱 아래와 목을 그리지 않고 남캐 머리 파츠가 그 칸을 갖는다(머리 1·3번).
        # 마법사 머리는 자기 얼굴층이 목까지 그려 그 칸이 없어 다른 몸에서 목이 머리와 떨어져 보였다(10/14 유민 지적)
        # => 노비스 합성(몸 + 얼굴 + 머리 1번)에서 머리 1번만 덮는 칸을 머리 1번 색 그대로 머리 4번에 넣는다
        if "Side" in name:
            h1 = nhair1[f]
            jaw = (h1[:, :, 3] > 0) & (hair_cell[:, :, 3] == 0) & (nbody[f][:, :, 3] == 0) & (nface[f][:, :, 3] == 0)
            jaw[:20] = False
            jaw[45:] = False
            hair_cell[jaw] = h1[jaw]
            jaw_filled += int(jaw.sum())
            # 그래도 턱선에 남는 한 칸짜리 막힌 빈칸(머리 1번 합성엔 없는 것)은 둘레 8칸에서 가장 많은 색으로
            comp = np.zeros_like(hair_cell)
            for l in (nbody[f], nface[f], hair_cell):
                comp[l[:, :, 3] > 0] = l[l[:, :, 3] > 0]
            ref = (nbody[f][:, :, 3] > 0) | (nface[f][:, :, 3] > 0) | (h1[:, :, 3] > 0)
            pin = enclosed_mask(comp[:, :, 3] > 0) & ~enclosed_mask(ref)
            pin[:20] = False
            pin[45:] = False
            for y, x in zip(*np.nonzero(pin)):
                around = [tuple(comp[yy, xx]) for yy in range(y - 1, y + 2) for xx in range(x - 1, x + 2)
                          if 0 <= yy < CH and 0 <= xx < CW and comp[yy, xx, 3] > 0]
                hair_cell[y, x] = max(set(around), key=around.count)
            jaw_filled += int(pin.sum())
        body_cells.append(cell)
    sheet(body_cells).save("%s/MCh_Mage_Body_%s_Sheet.png" % (HERE, name))
    sheet(hair_cells).save("%s/MCh_Hair4_%s_Sheet.png" % (NOVICE_DIR, name))
    bounce = [-d for d in head_dy]       # 표는 위가 + --- 공용 머리를 마법사 얼굴 자리로 내린다
    meta["anims"][name] = {"source": anim, "offset": [ox, oy], "align": how, "durations_ms": NOVICE["anims"][name]["durations_ms"],
                           "bounce": bounce, "clipped_px": {"body": clip_body, "hair4": clip_hair}, "neck_filled_px": filled, "ribbon_moved_px": moved, "jaw_filled_px": jaw_filled}
    print("%-11s 오프셋 (%+d,%+d) %s 기준  바운스 %s  잘린 칸 몸 %d 머리 %d  목 메운 칸 %d  리본 끈 옮긴 칸 %d  턱 밑 메운 칸 %d" % (name, ox, oy, how, bounce, clip_body, clip_hair, filled, moved, jaw_filled))

json.dump(meta, io.open(HERE + "/sheets.json", "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("몸 시트 6장 => %s, 머리 4번 시트 6장 => %s" % (HERE, os.path.normpath(NOVICE_DIR)))
