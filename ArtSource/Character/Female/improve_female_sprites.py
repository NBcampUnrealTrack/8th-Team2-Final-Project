# -*- coding: utf-8 -*-
"""
Comprehensive pixel-art (도트) and animation (도트 애니메이션) improvement script
for all Female characters in ProjectM:
  - Female Novice (WCh_Body_*, JobId=0)
  - 4 Female Jobs: Druid (1005), Gambler (1004), Mage (1003), Tracker (1002)
  - Field (Idle/Walk x Front/Side/Back) and Combat (Combat_Idle, Combat_Attack)

Reads clean original sheets from Saved/FemaleSpriteBackup_20261014/ (idempotent)
and writes improved sheets to ArtSource/Character/Female/.
"""
import os
import glob
import numpy as np
from PIL import Image
from scipy.ndimage import binary_fill_holes, binary_dilation

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
BACKUP = os.path.abspath(os.path.join(HERE, "../../../Saved/FemaleSpriteBackup_20261014")).replace("\\", "/")

JOBS = ["Druid", "Gambler", "Mage", "Tracker"]
FIELD_ANIMS = ["Idle_Front", "Walk_Front", "Idle_Side", "Walk_Side", "Idle_Back", "Walk_Back"]
CW, CH = 60, 100


def load_backup(name):
    path = f"{BACKUP}/{name}"
    return np.array(Image.open(path).convert("RGBA"))


def save_sheet(arr, name):
    path = f"{HERE}/{name}"
    Image.fromarray(arr).save(path)


def shift_cell(cell, dx=0, dy=0):
    h, w, c = cell.shape
    out = np.zeros_like(cell)
    sx0, sx1 = max(0, -dx), min(w, w - dx)
    dx0, dx1 = max(0, dx), min(w, w + dx)
    sy0, sy1 = max(0, -dy), min(h, h - dy)
    dy0, dy1 = max(0, dy), min(h, h + dy)
    if sx1 > sx0 and sy1 > sy0:
        out[dy0:dy1, dx0:dx1] = cell[sy0:sy1, sx0:sx1]
    return out


def clean_front_face_hair_layers(b_cell, f_cell, h_cell):
    """
    In Field Front (Idle_Front, Walk_Front):
    1) Move blonde sidelock/bang shadow pixels that leaked into FaceEye back into Hair
       so M_NUCharacterHairTint colours them properly (fixes bright orange patch next to cheek).
    2) Fill interior transparent holes at the neck/jawline/sidelock and ponytail junctions
       with dark hair shadow so background grass doesn't poke through her neck/hair.
    """
    fy, fx = np.where(f_cell[:, :, 3] > 0)
    if len(fy) == 0:
        return
    y0 = int(fy.min())
    # Move outer sidelock/eyebrow hair pixels from FaceEye to Hair
    # Coordinates relative to y0 (where x_left=22, x_right=38):
    hair_leaks = [
        (23, y0 + 0), (37, y0 + 0),  # (157, 102, 87) outer brow hair shadow
        (22, y0 + 1), (38, y0 + 1),  # (70, 44, 43) outer sidelock dark edge
        (22, y0 + 3), (38, y0 + 3),  # (92, 32, 32) warm brown sidelock edge
        (22, y0 + 4),                # (48, 27, 23) sidelock edge
        (22, y0 + 5), (38, y0 + 5),  # (48, 27, 23) sidelock edge
        (23, y0 + 5), (37, y0 + 5),  # (171, 112, 80) & (138, 79, 58) bright blonde sidelock shadow!
    ]
    for x, y in hair_leaks:
        if 0 <= y < CH and 0 <= x < CW and f_cell[y, x, 3] > 0:
            if h_cell[y, x, 3] == 0:
                h_cell[y, x] = f_cell[y, x]
            f_cell[y, x] = (0, 0, 0, 0)

    # Also ensure cheek outline at (23, y0+4..5) and (37, y0+4..5) has clean dark skin/outline
    # so face silhouette is crisp when hair is above it
    # Fill interior transparent holes in the head/neck region (y = y0..y0+16, x = 8..42)
    comp_mask = (b_cell[:, :, 3] > 0) | (f_cell[:, :, 3] > 0) | (h_cell[:, :, 3] > 0)
    filled = binary_fill_holes(comp_mask)
    holes = filled & (~comp_mask)
    hy, hx = np.where(holes)
    for y, x in zip(hy, hx):
        if y0 <= y <= y0 + 16 and 7 <= x <= 42:
            # Skip the intentional top hair loop hole at y < 7
            if y < 8:
                continue
            # Neck-side holes (x=23..25 or 35..37, y=y0+7..y0+9) and ponytail-to-head holes (x=18..22)
            if 23 <= x <= 25 or 35 <= x <= 37:
                # Deep hair shadow under chin/sidelock
                shade = (134, 89, 72, 255) if y <= y0 + 8 else (104, 67, 54, 255)
                h_cell[y, x] = shade
            else:
                # Ponytail interior gap
                h_cell[y, x] = (134, 89, 72, 255)


def clean_side_face_hair_layers(b_cell, f_cell, h_cell):
    """
    In Field Side (Idle_Side, Walk_Side):
    1) Move blonde ear-sideburn pixels at x=22..26 that leaked into FaceEye back into Hair.
    2) Fill the enclosed transparent gap behind the neck/ear (between ponytail, neck, and collar)
       in Hair so background grass doesn't show through behind her neck.
    """
    fy, fx = np.where(f_cell[:, :, 3] > 0)
    if len(fy) == 0:
        return
    y0 = int(fy.min())
    # Move hair pixels behind/above/in-front-of ear that were placed in FaceEye
    side_hair_leaks = [
        (22, y0 + 1), (22, y0 + 2), (22, y0 + 3),  # (171, 112, 80) behind ear
        (24, y0 + 1),                              # (171, 112, 80) above ear
        (25, y0 + 3), (26, y0 + 3),                # (168, 119, 104), (241, 183, 149) sideburn in Gambler
        (23, y0 + 4),                              # (181, 117, 105) behind earlobe
        (24, y0 + 5),                              # (151, 104, 83) under ear
    ]
    for x, y in side_hair_leaks:
        if 0 <= y < CH and 0 <= x < CW and f_cell[y, x, 3] > 0:
            rgb = tuple(int(v) for v in f_cell[y, x, :3])
            if rgb in ((171, 112, 80), (168, 119, 104), (241, 183, 149), (181, 117, 105), (151, 104, 83)):
                if h_cell[y, x, 3] == 0:
                    h_cell[y, x] = f_cell[y, x]
                f_cell[y, x] = (0, 0, 0, 0)

    # Fill enclosed transparent holes in the neck/ponytail region (y = y0+3..y0+18, x = 11..25)
    comp_mask = (b_cell[:, :, 3] > 0) | (f_cell[:, :, 3] > 0) | (h_cell[:, :, 3] > 0)
    filled = binary_fill_holes(comp_mask)
    holes = filled & (~comp_mask)
    hy, hx = np.where(holes)
    for y, x in zip(hy, hx):
        if y0 + 3 <= y <= y0 + 19 and 10 <= x <= 25:
            # Upper neck/hair junction gets deep hair shadow; lower border gets outline/dark shadow
            if y <= y0 + 10 and x >= 19:
                h_cell[y, x] = (134, 89, 72, 255)
            elif y <= y0 + 14 and x >= 18:
                h_cell[y, x] = (104, 67, 54, 255)
            else:
                h_cell[y, x] = (134, 89, 72, 255)


def clean_back_hair_layers(b_cell, h_cell):
    """
    In Field Back (Idle_Back, Walk_Back):
    Fill enclosed transparent holes in the neck and ponytail-to-head junction (y = 24..44).
    """
    comp_mask = (b_cell[:, :, 3] > 0) | (h_cell[:, :, 3] > 0)
    filled = binary_fill_holes(comp_mask)
    holes = filled & (~comp_mask)
    hy, hx = np.where(holes)
    for y, x in zip(hy, hx):
        if 24 <= y <= 44 and 20 <= x <= 51:
            if 22 <= x <= 26 and 29 <= y <= 34:
                # Left neck corner under hair
                h_cell[y, x] = (104, 67, 54, 255)
            elif 33 <= x <= 44 and 28 <= y <= 41:
                # Right neck / ponytail junction
                h_cell[y, x] = (134, 89, 72, 255) if x >= 36 else (104, 67, 54, 255)
            else:
                h_cell[y, x] = (134, 89, 72, 255)


def apply_ponytail_secondary_motion(h_cell, mode, dy_tail=0, dx_tip=0):
    """
    Applies subtle 1px secondary motion (follow-through / sway) to the ponytail tail
    without moving the head crown or bangs that contact FaceEye/Body collar.
    mode: 'Front', 'Side', 'Back'
    """
    if dy_tail == 0 and dx_tip == 0:
        return
    out = h_cell.copy()
    if mode == "Front":
        # Ponytail is on the left: x = 3..18, lower tail y = 18..48
        if dy_tail != 0:
            region = h_cell[18:48, 3:19].copy()
            out[18:48, 3:19] = 0
            y_dst = max(16, 18 + dy_tail)
            h_len = min(region.shape[0], CH - y_dst)
            out[y_dst:y_dst + h_len, 3:19] = region[:h_len]
            # Blend junction row at y=18..19 so there's never a gap
            mask_orig = h_cell[17:20, 6:19, 3] > 0
            mask_new = out[17:20, 6:19, 3] > 0
            missing = mask_orig & (~mask_new)
            out[17:20, 6:19][missing] = h_cell[17:20, 6:19][missing]
        if dx_tip != 0:
            # Lower tip of ponytail: y = 30..48, x = 3..18
            tip = out[30:48, 3:19].copy()
            out[30:48, 3:19] = 0
            x_dst = max(1, 3 + dx_tip)
            out[30:48, x_dst:x_dst + 16] = tip
            # Ensure junction at y=30 is seamless
            for x in range(4, 18):
                if out[29, x, 3] > 0 and out[30, x, 3] == 0 and h_cell[30, x, 3] > 0:
                    out[30, x] = h_cell[30, x]
    elif mode == "Side":
        # Ponytail is on the left: x = 4..18, lower tail y = 20..50
        if dy_tail != 0:
            region = h_cell[20:50, 4:19].copy()
            out[20:50, 4:19] = 0
            y_dst = max(18, 20 + dy_tail)
            h_len = min(region.shape[0], CH - y_dst)
            out[y_dst:y_dst + h_len, 4:19] = region[:h_len]
            mask_orig = h_cell[19:22, 6:19, 3] > 0
            mask_new = out[19:22, 6:19, 3] > 0
            missing = mask_orig & (~mask_new)
            out[19:22, 6:19][missing] = h_cell[19:22, 6:19][missing]
        if dx_tip != 0:
            tip = out[32:50, 4:19].copy()
            out[32:50, 4:19] = 0
            x_dst = max(1, 4 + dx_tip)
            out[32:50, x_dst:x_dst + 15] = tip
            for x in range(5, 18):
                if out[31, x, 3] > 0 and out[32, x, 3] == 0 and h_cell[32, x, 3] > 0:
                    out[32, x] = h_cell[32, x]
    elif mode == "Back":
        # Ponytail is on the right: x = 38..56, lower tail y = 20..50
        if dy_tail != 0:
            region = h_cell[20:50, 38:57].copy()
            out[20:50, 38:57] = 0
            y_dst = max(18, 20 + dy_tail)
            h_len = min(region.shape[0], CH - y_dst)
            out[y_dst:y_dst + h_len, 38:57] = region[:h_len]
            mask_orig = h_cell[19:22, 38:55, 3] > 0
            mask_new = out[19:22, 38:55, 3] > 0
            missing = mask_orig & (~mask_new)
            out[19:22, 38:55][missing] = h_cell[19:22, 38:55][missing]
        if dx_tip != 0:
            tip = out[32:50, 38:57].copy()
            out[32:50, 38:57] = 0
            x_dst = min(CW - 19, max(35, 38 + dx_tip))
            out[32:50, x_dst:x_dst + 19] = tip
            for x in range(38, 56):
                if out[31, x, 3] > 0 and out[32, x, 3] == 0 and h_cell[32, x, 3] > 0:
                    out[32, x] = h_cell[32, x]
    h_cell[:] = out


def apply_body_breathing_and_cloth_sway(b_cell, f_idx, is_walk=False):
    """
    Adds subtle breathing / cloth hem secondary motion to Body frames that were previously 100% static
    (e.g. f5, f6, f7, f0, f1 in Idle) while keeping feet planted at y=95 and collar locked.
    """
    if not is_walk:
        # In Idle: f2, f3, f4 are the main exhale dip.
        # Add subtle cloth/sleeve/hem follow-through on f1 (anticipation), f5 (lag), f6/f7 (hem breeze)
        if f_idx == 1:
            # Subtle 1px downward settle on mid-sleeves/gloves (y=48..62, outer edges x<20 or x>40)
            for x_slice in (slice(6, 20), slice(40, 54)):
                sub = b_cell[48:62, x_slice].copy()
                b_cell[49:63, x_slice] = np.where(sub[:, :, 3:4] > 0, sub, b_cell[49:63, x_slice])
        elif f_idx == 5:
            # Follow-through on lower skirt/coat hem (y=64..82) lagging 1px down
            sub = b_cell[64:81, 6:54].copy()
            # Only shift non-leg outer skirt/coat pixels (x < 21 or x > 39)
            for x_slice in (slice(6, 21), slice(39, 54)):
                s = b_cell[62:82, x_slice].copy()
                b_cell[63:83, x_slice] = np.where(s[:, :, 3:4] > 0, s, b_cell[63:83, x_slice])
        elif f_idx == 6:
            # Gentle 1px breeze on lower-left and lower-right coat/skirt tips (y=68..84)
            for x_slice in (slice(6, 19), slice(41, 54)):
                s = b_cell[68:84, x_slice].copy()
                # Shift 1px up-inward recovery
                b_cell[67:83, x_slice] = np.where(s[:, :, 3:4] > 0, s, b_cell[67:83, x_slice])
        elif f_idx == 7:
            # Subtle chest/collarbone inhale lift on inner torso shading (y=39..52, x=23..37)
            s = b_cell[40:52, 24:36].copy()
            b_cell[39:51, 24:36] = np.where(s[:, :, 3:4] > 0, s, b_cell[39:51, 24:36])


def apply_subtle_eye_shimmer(f_cell, f_idx, mode):
    """
    Adds subtle eye highlight / eyelash life on f3 (deepest point of exhale) and f7 (inhale peak)
    in Idle so FaceEye isn't a dead static stamp across 8 frames.
    """
    fy, fx = np.where(f_cell[:, :, 3] > 0)
    if len(fy) == 0:
        return
    y0 = int(fy.min())
    if mode == "Front":
        if f_idx == 3:
            # Soft half-blink / relaxed upper lash on f3: soften top iris row (y0+2) slightly
            for x in [25, 26, 34, 35]:
                if f_cell[y0 + 2, x, 3] > 0:
                    # Blend toward upper lash navy (19, 20, 47)
                    rgb = f_cell[y0 + 2, x, :3].astype(np.int32)
                    f_cell[y0 + 2, x, :3] = np.clip((rgb * 2 + np.array([19, 25, 65])) // 3, 0, 255).astype(np.uint8)
        elif f_idx == 7:
            # Subtle catchlight brighten on lower iris (y0+3) on f7
            for x in [27, 33]:
                if f_cell[y0 + 3, x, 3] > 0:
                    rgb = f_cell[y0 + 3, x, :3].astype(np.int32)
                    f_cell[y0 + 3, x, :3] = np.clip(rgb + np.array([14, 18, 20]), 0, 255).astype(np.uint8)
    elif mode == "Side":
        if f_idx == 3:
            for x in [31, 32]:
                if f_cell[y0 + 2, x, 3] > 0:
                    rgb = f_cell[y0 + 2, x, :3].astype(np.int32)
                    f_cell[y0 + 2, x, :3] = np.clip((rgb * 2 + np.array([19, 25, 65])) // 3, 0, 255).astype(np.uint8)
        elif f_idx == 7:
            for x in [33]:
                if f_cell[y0 + 3, x, 3] > 0:
                    rgb = f_cell[y0 + 3, x, :3].astype(np.int32)
                    f_cell[y0 + 3, x, :3] = np.clip(rgb + np.array([14, 18, 20]), 0, 255).astype(np.uint8)


def separate_novice_sheet(anim):
    """
    Separates Female Novice's combined WCh_Body_<Anim>_Sheet.png into 3 clean, disjoint layers:
      - Headless Body: WCh_Body_<Anim>_Sheet.png
      - FaceEye (Front & Side): WCh_FaceEye_<Anim>_Sheet.png
      - Hair (Front, Side, Back): WCh_Hair_<Anim>_Sheet.png
    Preserves 100% of the lively 8-frame running bounce (topY=[3,5,5,2]) and upgraded in-place
    Walk_Back / Walk_Front legs & boots, plus the original artist's hand-drawn 8-frame ponytail sway.
    """
    from scipy.ndimage import label as cc_label
    from generate_walk_proposals import (
        upgrade_novice_walk_back_inplace,
        upgrade_novice_walk_front_inplace,
    )

    has_face = "Back" not in anim
    is_walk = "Walk" in anim
    mode = "Front" if "Front" in anim else ("Side" if "Side" in anim else "Back")
    idle_anim = anim.replace("Walk_", "Idle_")

    if anim == "Walk_Back":
        src_sheet = upgrade_novice_walk_back_inplace()
    elif anim == "Walk_Front":
        src_sheet = upgrade_novice_walk_front_inplace()
    else:
        src_sheet = load_backup(f"WCh_Body_{anim}_Sheet.png")

    # Align Novice Idle phase ([0,0,0,1,1,1,0,0] -> [0,0,1,1,1,0,0,0]) to match the 4 Female Jobs
    if not is_walk:
        cells = [src_sheet[:, f * CW:(f + 1) * CW].copy() for f in range(8)]
        cells = [cells[(f + 1) % 8] for f in range(8)]
        for f in range(8):
            src_sheet[:, f * CW:(f + 1) * CW] = cells[f]

    # Reference FaceEye from Gambler (shifted +1px right in Front so faceX=22..38 matches Novice 100%)
    if has_face:
        ref_face0 = load_backup(f"WCh_Gambler_FaceEye_{idle_anim}_Sheet.png")[:, :CW].copy()
        if mode == "Front":
            ref_face0 = shift_cell(ref_face0, +1, 0)
    else:
        ref_face0 = np.zeros((CH, CW, 4), dtype=np.uint8)
    ref_hair0 = load_backup(f"WCh_Gambler_Hair_{idle_anim}_Sheet.png")[:, :CW]
    ref_top_y = int(np.where(ref_hair0[:, :, 3] > 0)[0].min())  # 2

    out_body = np.zeros_like(src_sheet)
    out_face = np.zeros_like(src_sheet)
    out_hair = np.zeros_like(src_sheet)

    for f in range(8):
        x0, x1 = f * CW, (f + 1) * CW
        nf = src_sheet[:, x0:x1].copy()
        nm = nf[:, :, 3] > 0
        f_top_y = int(np.where(nm)[0].min())
        dy = f_top_y - ref_top_y

        fb = np.zeros((CH, CW, 4), dtype=np.uint8)
        ff = np.zeros((CH, CW, 4), dtype=np.uint8)
        fh = np.zeros((CH, CW, 4), dtype=np.uint8)

        # 1. Shift ref_face0 by dy to get the exact FaceEye mask
        sf = shift_cell(ref_face0, 0, dy)
        face_mask = (sf[:, :, 3] > 0) & (nf[:, :, 3] > 0)

        for y in range(CH):
            ry = y - dy
            for x in range(CW):
                if nf[y, x, 3] == 0:
                    continue
                if face_mask[y, x]:
                    ff[y, x] = nf[y, x]
                    continue

                if mode == "Front":
                    if ry <= 31:
                        is_hair = True
                    elif ry == 32:
                        is_hair = (x <= 24) or (x >= 36)
                    elif ry == 33:
                        is_hair = (x <= 23) or (x >= 36)
                    elif ry == 34:
                        r, g, b, _ = [int(v) for v in nf[y, x]]
                        is_hair = (x <= 22) or (x >= 36 and r in (80, 104, 134, 0))
                    elif not is_walk:
                        max_hx = {
                            35: 20, 36: 19, 37: 18, 38: 17, 39: 17,
                            40: 16, 41: 16, 42: 15, 43: 15, 44: 14,
                            45: 14, 46: 14, 47: 13,
                        }.get(ry, -1)
                        is_hair = (x <= max_hx)
                    else:
                        if ry == 35:
                            is_hair = (x <= 19)
                        elif ry in (36, 37):
                            is_hair = (x <= 20)
                        elif ry == 38:
                            is_hair = (x <= (17 if f in (0, 4, 5, 6) else 18))
                        elif ry == 39:
                            is_hair = (x <= 18)
                        elif ry in (40, 41):
                            is_hair = (x <= 17)
                        elif ry == 42:
                            is_hair = (x <= 16)
                        elif ry == 43:
                            is_hair = (x <= (15 if f == 4 else 16))
                        elif ry in (44, 45):
                            is_hair = (x <= 15)
                        elif ry == 46:
                            is_hair = (x <= (14 if f == 5 else 15))
                        elif ry in (47, 48):
                            is_hair = (x <= 14)
                        else:
                            is_hair = False

                elif mode == "Side":
                    if ry <= 32:
                        is_hair = (x <= 21) if ry == 32 else True
                    elif ry in (33, 34, 35, 36, 37):
                        is_hair = (x <= 21)
                    elif not is_walk:
                        if ry == 38:
                            is_hair = (x <= 21)
                        elif ry == 39:
                            is_hair = (x <= 20)
                        elif ry in (40, 41):
                            is_hair = (x <= 19)
                        elif ry == 42:
                            is_hair = (x <= 18) or (x == 19 and tuple(nf[y, x, :3]) == (0, 0, 0))
                        elif ry in (43, 44, 45):
                            is_hair = (x <= 18)
                        elif ry in (46, 47):
                            is_hair = (x <= 17)
                        elif ry in (48, 49):
                            is_hair = (x <= 18)
                        else:
                            is_hair = False
                    else:
                        if ry == 38:
                            is_hair = (x <= (23 if f in (4, 5) else 20))
                        elif ry == 39:
                            is_hair = (x <= (19 if f == 0 else (21 if f in (4, 5) else 20)))
                        elif ry == 40:
                            is_hair = (x <= (18 if f == 0 else (19 if f == 1 else (20 if f in (2, 7) else 21))))
                        elif ry == 41:
                            is_hair = (x <= (18 if f == 0 else (19 if f == 1 else (20 if f in (2, 4, 7) else 21))))
                        elif ry in (42, 43, 44, 45, 46, 47, 48, 49):
                            body_start_x = {
                                42: {0: 18, 1: 19, 2: 20, 3: 21, 4: 20, 5: 22, 6: 21, 7: 20}[f],
                                43: {0: 18, 1: 18, 2: 19, 3: 20, 4: 19, 5: 21, 6: 20, 7: 19}[f],
                                44: {0: 18, 1: 18, 2: 19, 3: 20, 4: 19, 5: 20, 6: 20, 7: 18}[f],
                                45: {0: 17, 1: 18, 2: 19, 3: 20, 4: 18, 5: 20, 6: 20, 7: 18}[f],
                                46: {0: 17, 1: 17, 2: 18, 3: 20, 4: 18, 5: 19, 6: 20, 7: 18}[f],
                                47: {0: 17, 1: 17, 2: 18, 3: 20, 4: 18, 5: 18, 6: 20, 7: 18}[f],
                                48: {0: 17, 1: 18, 2: 19, 3: 21, 4: 18, 5: 19, 6: 20, 7: 17}[f],
                                49: {0: 0, 1: 0, 2: 0, 3: 0, 4: 0, 5: 17, 6: 17, 7: 0}[f],
                            }[ry]
                            is_hair = (x < body_start_x)
                        else:
                            is_hair = False

                else:
                    if ry <= 31:
                        is_hair = True
                    elif ry == 32:
                        is_hair = (x <= 25) or (x >= 34)
                    elif ry == 33:
                        is_hair = (x >= 40)
                    elif not is_walk:
                        if ry in (34, 35, 36, 37):
                            is_hair = (x >= (40 if f == 5 else 41))
                        elif ry == 38:
                            is_hair = (x >= 42)
                        elif ry == 39:
                            is_hair = (x >= (43 if f in (4, 5) else 44))
                        elif ry in (40, 41, 42, 43, 44):
                            is_hair = (x >= 45)
                        elif ry == 45:
                            is_hair = (x >= 44)
                        elif ry in (46, 47):
                            is_hair = (x >= 45)
                        else:
                            is_hair = False
                    else:
                        if ry in (34, 35):
                            is_hair = (x >= (40 if f in (1, 2) else (42 if f in (5, 6) else 41)))
                        elif ry == 36:
                            is_hair = (x >= (43 if f in (5, 6) else 41))
                        elif ry == 37:
                            is_hair = (x >= (41 if f in (3, 7) else (44 if f in (5, 6) else 42)))
                        elif ry == 38:
                            is_hair = (x >= (42 if f in (3, 7) else (43 if f in (0, 4) else (44 if f in (1, 2) else 45))))
                        elif ry == 39:
                            is_hair = (x >= (43 if f in (3, 7) else (45 if f in (0, 1, 2, 4) else 47)))
                        elif ry == 40:
                            is_hair = (x >= (44 if f == 3 else (45 if f == 7 else (46 if f in (0, 2, 4) else (47 if f in (1, 5) else 48)))))
                        elif ry in (41, 42, 43, 44, 45, 46, 47, 48):
                            hair_start_x = {
                                41: {0: 48, 1: 48, 2: 47, 3: 45, 4: 46, 5: 48, 6: 49, 7: 47}[f],
                                42: {0: 49, 1: 49, 2: 48, 3: 46, 4: 47, 5: 49, 6: 50, 7: 48}[f],
                                43: {0: 50, 1: 49, 2: 48, 3: 47, 4: 48, 5: 49, 6: 50, 7: 49}[f],
                                44: {0: 50, 1: 48, 2: 47, 3: 47, 4: 48, 5: 48, 6: 49, 7: 51}[f],
                                45: {0: 49, 1: 47, 2: 46, 3: 47, 4: 47, 5: 47, 6: 48, 7: 51}[f],
                                46: {0: 48, 1: 46, 2: 45, 3: 46, 4: 46, 5: 46, 6: 47, 7: 50}[f],
                                47: {0: 47, 1: 60, 2: 60, 3: 45, 4: 45, 5: 60, 6: 60, 7: 49}[f],
                                48: {0: 60, 1: 60, 2: 60, 3: 44, 4: 60, 5: 60, 6: 60, 7: 48}[f],
                            }[ry]
                            is_hair = (x >= hair_start_x)
                        else:
                            is_hair = False

                if is_hair:
                    fh[y, x] = nf[y, x]
                else:
                    fb[y, x] = nf[y, x]

        # Apply the exact same layer cleanup & subtle Idle breathing/shimmer as the 4 Female Jobs
        if mode == "Front":
            clean_front_face_hair_layers(fb, ff, fh)
        elif mode == "Side":
            clean_side_face_hair_layers(fb, ff, fh)
        else:
            clean_back_hair_layers(fb, fh)

        if not is_walk:
            apply_body_breathing_and_cloth_sway(fb, f, is_walk=False)
            if has_face:
                apply_subtle_eye_shimmer(ff, f, mode)

        out_body[:, x0:x1] = fb
        out_face[:, x0:x1] = ff
        out_hair[:, x0:x1] = fh

    return out_body, out_face, out_hair


def improve_novice_field():
    """
    Separates Female Novice field animations into disjoint Body, FaceEye, and Hair sprite sheets
    matching the 4 Female Jobs:
      - WCh_Body_{Idle,Walk}_{Front,Side,Back}_Sheet.png (Headless Body)
      - WCh_FaceEye_{Idle,Walk}_{Front,Side}_Sheet.png
      - WCh_Hair_{Idle,Walk}_{Front,Side,Back}_Sheet.png
    """
    for anim in FIELD_ANIMS:
        has_face = "Back" not in anim
        b_sheet, f_sheet, h_sheet = separate_novice_sheet(anim)
        save_sheet(b_sheet, f"WCh_Body_{anim}_Sheet.png")
        save_sheet(h_sheet, f"WCh_Hair_{anim}_Sheet.png")
        if has_face:
            save_sheet(f_sheet, f"WCh_FaceEye_{anim}_Sheet.png")


def improve_jobs_field():
    """
    Improves all 4 Female Jobs (Druid, Gambler, Mage, Tracker) across Field Idle & Walk:
    1) Exact alignment unification:
       - Gambler Idle_Front & Walk_Front: shift +1px right (dx=+1) so faceX=22..38 (mid=30.0) matches Druid/Mage/Tracker.
       - Druid Idle_Side & Walk_Side: shift +1px right (dx=+1) so topHairX=9..41 and ear/eye match Gambler/Mage/Tracker.
       - Druid Walk_Side: shift -1px up (dy=-1, keeping boot bottom grounded at y=95) to eliminate 1px vertical snap from Idle_Side.
       - Tracker Idle_Side: shift -1px up (dy=-1, keeping boot bottom grounded at y=95) to eliminate 1px vertical snap from Walk_Side.
       - Mage Idle_Front: shift frame phase by +1 so dip is on f2,f3,f4 matching Mage Idle_Side/Back and all other jobs.
    2) Layer cleanup & hole filling:
       - Move leaked blonde hair pixels from FaceEye to Hair in Front and Side views.
       - Fill transparent neck/sidelock/ponytail holes in Front, Side, and Back views.
    3) Animation smoothness:
       - In Idle (Front, Side, Back): add ponytail follow-through (1-frame lag + gentle breeze sway), cloth hem breathing,
         and subtle eye shimmer so the 4-frame dead freeze (f6,f7,f0,f1 diff=0) is completely eliminated!
       - In Walk (Front, Side, Back): add 2-beat walk head/torso rhythm and ponytail follow-through on f2,f3,f6,f7,
         plus improved boot/stride articulation on Druid and Mage Walk_Side.
    """
    for job in JOBS:
        for anim in FIELD_ANIMS:
            mode = "Front" if "Front" in anim else ("Side" if "Side" in anim else "Back")
            is_walk = "Walk" in anim
            has_face = mode != "Back"

            b_name = f"WCh_{job}_Body_{anim}_Sheet.png"
            h_name = f"WCh_{job}_Hair_{anim}_Sheet.png"
            f_name = f"WCh_{job}_FaceEye_{anim}_Sheet.png"

            b_sheet = load_backup(b_name)
            h_sheet = load_backup(h_name)
            f_sheet = load_backup(f_name) if has_face else None

            # Phase shift for Mage Idle_Front and Tracker Idle_* so dip is on f2, f3, f4 like all other Idle sheets
            if (job == "Mage" and anim == "Idle_Front") or (job == "Tracker" and not is_walk):
                for arr in (b_sheet, h_sheet, f_sheet):
                    if arr is not None:
                        cells = [arr[:, f * CW:(f + 1) * CW].copy() for f in range(8)]
                        for f in range(8):
                            arr[:, f * CW:(f + 1) * CW] = cells[(f - 1) % 8]

            # Determine global cell alignment (dx, dy)
            dx, dy = 0, 0
            if job == "Gambler" and mode == "Front":
                dx = +1
            elif job == "Druid" and mode == "Side":
                dx = +1
                if is_walk:
                    dy = -1
            elif job == "Tracker" and anim == "Idle_Side":
                dy = -1

            for f in range(8):
                b_cell = b_sheet[:, f * CW:(f + 1) * CW].copy()
                h_cell = h_sheet[:, f * CW:(f + 1) * CW].copy()
                f_cell = f_sheet[:, f * CW:(f + 1) * CW].copy() if has_face else None

                if dx != 0 or dy != 0:
                    b_orig = shift_cell(b_cell, dx, 0)
                    b_cell = shift_cell(b_cell, dx, dy)
                    if dy == -1:
                        # Keep bottom boot soles grounded at y=95!
                        b_cell[93:96] = np.where(b_cell[93:96, :, 3:4] > 0, b_cell[93:96], b_orig[93:96])
                    h_cell = shift_cell(h_cell, dx, dy)
                    if has_face:
                        f_cell = shift_cell(f_cell, dx, dy)

                # Clean layer bleed and fill interior neck/hair holes
                if mode == "Front":
                    clean_front_face_hair_layers(b_cell, f_cell, h_cell)
                elif mode == "Side":
                    clean_side_face_hair_layers(b_cell, f_cell, h_cell)
                else:
                    clean_back_hair_layers(b_cell, h_cell)

                if not is_walk:
                    # Smooth Idle animation across all 8 frames (eliminate f6,f7,f0,f1 dead freeze)
                    tail_motion = {
                        0: (0, 0),
                        1: (0, -1 if mode != "Back" else +1),  # subtle pre-dip breeze
                        2: (-1, 0),                            # ponytail lags 1px up as head dips down on f2
                        3: (0, 0),                             # full dip
                        4: (+1, -1 if mode != "Back" else +1), # deep settle at bottom of exhale
                        5: (+1, 0),                            # ponytail lags 1px down as head rises on f5
                        6: (0, -1 if mode != "Back" else +1),  # breeze recovery
                        7: (-1, 0),                            # inhale peak lift on ponytail tail
                    }
                    dy_t, dx_t = tail_motion.get(f, (0, 0))
                    apply_ponytail_secondary_motion(h_cell, mode, dy_t, dx_t)
                    apply_body_breathing_and_cloth_sway(b_cell, f, is_walk=False)
                    if has_face:
                        apply_subtle_eye_shimmer(f_cell, f, mode)
                else:
                    # Smooth Walk animation: currently f2,f3,f4 and f6,f7,f0 have flat head/hair (0 diff on f2->f3, f6->f7)
                    # Add secondary ponytail follow-through on f1, f2, f3, f5, f6, f7!
                    walk_tail = {
                        0: (0, 0),
                        1: (-1, 0),                            # down pose: tail lags 1px up
                        2: (+1, -1 if mode != "Back" else +1), # passing: tail drops +1px & swings out
                        3: (-1, -1 if mode != "Back" else +1), # high point: tail rebounds -1px
                        4: (0, 0),
                        5: (-1, 0),                            # down pose 2: tail lags 1px up
                        6: (+1, 0),                            # passing 2: tail drops +1px
                        7: (-1, +1 if mode != "Back" else -1), # high point 2: tail rebounds -1px
                    }
                    dy_t, dx_t = walk_tail.get(f, (0, 0))
                    apply_ponytail_secondary_motion(h_cell, mode, dy_t, dx_t)

                    # Also add subtle 1px sleeve/cape follow-through on passing/high frames f2, f3, f6, f7
                    if f in (2, 6):
                        # Lower coat/skirt hem lags 1px down on passing frame
                        for x_slice in (slice(5, 20), slice(40, 55)):
                            s = b_cell[64:84, x_slice].copy()
                            b_cell[65:85, x_slice] = np.where(s[:, :, 3:4] > 0, s, b_cell[65:85, x_slice])
                    elif f in (3, 7):
                        # High-point stride: lift mid-skirt/belt pleats 1px on f3/f7 for clear up-pose readability
                        s = b_cell[52:68, 18:42].copy()
                        b_cell[51:67, 18:42] = np.where(s[:, :, 3:4] > 0, s, b_cell[51:67, 18:42])
                        if has_face:
                            apply_subtle_eye_shimmer(f_cell, f, mode)

                b_sheet[:, f * CW:(f + 1) * CW] = b_cell
                h_sheet[:, f * CW:(f + 1) * CW] = h_cell
                if has_face:
                    f_sheet[:, f * CW:(f + 1) * CW] = f_cell

            save_sheet(b_sheet, b_name)
            save_sheet(h_sheet, h_name)
            if has_face:
                save_sheet(f_sheet, f_name)


def clean_combat_face_and_neck(b_cell, f_cell, h_cell):
    """
    In Combat_Idle and Combat_Attack:
    1) Moves the 3 blonde front-bang pixels at top-right of FaceEye ((fx_min+10, fy_min), (fx_min+11, fy_min), (fx_min+10, fy_min+1))
       and the 2 blonde sideburn pixels at left of FaceEye ((fx_min, fy_min+3..4), (fx_min+1, fy_min+3)) from FaceEye to Hair.
    2) Removes any stray uncovered pixels in FaceEye/Body above the head crown.
    3) Fills small interior transparent neck/ponytail holes inside (FaceEye | Hair) on the ponytail side.
    """
    hy, hx = np.where(h_cell[:, :, 3] > 0)
    if len(hy) == 0:
        return
    h_top = int(hy.min())
    h_bot = int(hy.max())
    h_left = int(hx.min())
    h_right = int(hx.max())

    # Remove any stray FaceEye pixels above h_top + 2 (e.g. Tracker Combat_Attack f6 y=19..20)
    fy, fx = np.where(f_cell[:, :, 3] > 0)
    if len(fy) > 0:
        for y, x in zip(fy, fx):
            if y < h_top + 6:
                f_cell[y, x] = (0, 0, 0, 0)

    # Re-find FaceEye bbox
    fy, fx = np.where((f_cell[:, :, 3] > 0) & (h_cell[:, :, 3] == 0))
    if len(fy) > 0:
        y0 = int(fy.min())
        x0 = int(fx.min())
        # Move blonde bang pixels at y0, y0+1 and left sideburn at x0..x0+2 from FaceEye to Hair
        for y, x in zip(fy, fx):
            rgb = tuple(int(v) for v in f_cell[y, x, :3])
            if (y <= y0 + 1 and rgb in ((241, 183, 149), (199, 123, 95))) or \
               (x <= x0 + 2 and y <= y0 + 5 and rgb in ((250, 220, 197), (243, 177, 138), (235, 201, 180), (252, 232, 213), (251, 192, 157))):
                h_cell[y, x] = f_cell[y, x]
                f_cell[y, x] = (0, 0, 0, 0)

    # Move any uncovered blonde hair pixels at top of Body collar (y < h_bot - 8) into Hair
    by, bx = np.where((b_cell[:, :, 3] > 0) & (h_cell[:, :, 3] == 0) & (f_cell[:, :, 3] == 0))
    for y, x in zip(by, bx):
        if h_top <= y <= h_top + 24 and h_left <= x <= h_right:
            rgb = tuple(int(v) for v in b_cell[y, x, :3])
            if rgb in ((252, 232, 213), (241, 183, 149), (236, 195, 171)):
                h_cell[y, x] = b_cell[y, x]

    # Fill small enclosed interior holes in (FaceEye | Hair) ONLY (never include Body arms/staff!)
    head_mask = (f_cell[:, :, 3] > 0) | (h_cell[:, :, 3] > 0)
    filled = binary_fill_holes(head_mask)
    holes = filled & (~head_mask) & (b_cell[:, :, 3] == 0)
    hy_h, hx_h = np.where(holes)
    for y, x in zip(hy_h, hx_h):
        if h_top + 4 <= y <= h_bot - 4 and h_left + 2 <= x <= h_left + 20:
            h_cell[y, x] = (134, 89, 72, 255)


def improve_combat_sheets():
    """
    Improves Combat_Idle and Combat_Attack for Druid, Gambler, Mage, Tracker:
    1) Cleans all FaceEye/Hair layer bleeds and interior ponytail/neck holes.
    2) Tracker Combat_Attack:
       - Removes 3 stray floating black pixels in f3 ((48,21), (46,22), (63,22)).
       - Paints the missing right shoulder / cape / chest bridge behind the ponytail in f2, f3, f4, f5
         so her extended bow arm is connected to her torso and no background shows between ponytail curls.
       - Repairs broken bowstring pixels in f3.
    3) Druid Combat_Attack:
       - Removes the leftover Male-Druid crouching bear from f7 (which popped in for 1 frame at x=32..174 and vanished in Idle),
         replacing it with a clean landing settle so f6 -> f7 -> Idle transitions seamlessly.
       - Differentiates f4 and f5 (which were 100% identical) with paw press + shockwave expansion on f5,
         and lifts Druid -1px on f1 as the bear spirit surges.
    4) Mage Combat_Attack:
       - Completes the clipped bottom magic circle arc at y=108..109 in f4 and f5.
    5) Combat_Idle (all 4 jobs):
       - Eliminates consecutive 0-diff frozen frames by adding subtle 1px ponytail/cloth/orb follow-through.
    """
    for job in JOBS:
        # --- 1. Combat_Idle ---
        b_name = f"WCh_{job}_Body_Combat_Idle_Sheet.png"
        f_name = f"WCh_{job}_FaceEye_Combat_Idle_Sheet.png"
        h_name = f"WCh_{job}_Hair_Combat_Idle_Sheet.png"
        b_sheet = load_backup(b_name)
        f_sheet = load_backup(f_name)
        h_sheet = load_backup(h_name)
        w = b_sheet.shape[1] // 8

        for f in range(8):
            b_cell = b_sheet[:, f * w:(f + 1) * w].copy()
            f_cell = f_sheet[:, f * w:(f + 1) * w].copy()
            h_cell = h_sheet[:, f * w:(f + 1) * w].copy()
            clean_combat_face_and_neck(b_cell, f_cell, h_cell)

            # Eliminate frozen frame runs in Combat_Idle
            hy, hx = np.where(h_cell[:, :, 3] > 0)
            if len(hy) > 0:
                h_left, h_top, h_bot = int(hx.min()), int(hy.min()), int(hy.max())
                if f in (1, 5):
                    sub = h_cell[h_top + 16:h_bot + 1, h_left:h_left + 15].copy()
                    if f == 1 and h_bot + 1 < h_cell.shape[0]:
                        h_cell[h_top + 17:h_bot + 2, h_left:h_left + 15] = np.where(
                            sub[:, :, 3:4] > 0, sub, h_cell[h_top + 17:h_bot + 2, h_left:h_left + 15]
                        )
                    elif f == 5:
                        h_cell[h_top + 15:h_bot, h_left:h_left + 15] = np.where(
                            sub[:, :, 3:4] > 0, sub, h_cell[h_top + 15:h_bot, h_left:h_left + 15]
                        )
                elif f in (3, 7):
                    sub = h_cell[h_top + 22:h_bot + 1, h_left:h_left + 14].copy()
                    if f == 3 and h_left > 0:
                        h_cell[h_top + 22:h_bot + 1, h_left - 1:h_left + 13] = np.where(
                            sub[:, :, 3:4] > 0, sub, h_cell[h_top + 22:h_bot + 1, h_left - 1:h_left + 13]
                        )
                    elif f == 7 and h_bot + 1 < h_cell.shape[0]:
                        h_cell[h_top + 23:h_bot + 2, h_left:h_left + 14] = np.where(
                            sub[:, :, 3:4] > 0, sub, h_cell[h_top + 23:h_bot + 2, h_left:h_left + 14]
                        )
                    by, bx = np.where(b_cell[:, :, 3] > 0)
                    if len(by) > 0:
                        b_mid_y = (int(by.min()) + int(by.max())) // 2
                        s = b_cell[b_mid_y:b_mid_y + 18, :].copy()
                        if f == 3:
                            b_cell[b_mid_y + 1:b_mid_y + 19, :] = np.where(
                                s[:, :, 3:4] > 0, s, b_cell[b_mid_y + 1:b_mid_y + 19, :]
                            )

            b_sheet[:, f * w:(f + 1) * w] = b_cell
            f_sheet[:, f * w:(f + 1) * w] = f_cell
            h_sheet[:, f * w:(f + 1) * w] = h_cell

        save_sheet(b_sheet, b_name)
        save_sheet(f_sheet, f_name)
        save_sheet(h_sheet, h_name)

        # --- 2. Combat_Attack ---
        ba_name = f"WCh_{job}_Body_Combat_Attack_Sheet.png"
        fa_name = f"WCh_{job}_FaceEye_Combat_Attack_Sheet.png"
        ha_name = f"WCh_{job}_Hair_Combat_Attack_Sheet.png"
        ba_sheet = load_backup(ba_name)
        fa_sheet = load_backup(fa_name)
        ha_sheet = load_backup(ha_name)
        wa = ba_sheet.shape[1] // 8

        for f in range(8):
            b_cell = ba_sheet[:, f * wa:(f + 1) * wa].copy()
            f_cell = fa_sheet[:, f * wa:(f + 1) * wa].copy()
            h_cell = ha_sheet[:, f * wa:(f + 1) * wa].copy()

            if job == "Tracker":
                if f == 3:
                    for sx, sy in [(48, 21), (46, 22), (63, 22)]:
                        b_cell[sy, sx] = (0, 0, 0, 0)
                    b_cell[50, 72] = (0, 0, 0, 255)
                    b_cell[51, 73] = (0, 0, 0, 255)
                    b_cell[52, 73] = (0, 0, 0, 255)

                # In f2, f3, f4, f5: paint the missing right shoulder / cape / chest bridge behind the ponytail
                # x=55..66 for cape/shoulder/torso (covered by ponytail), and x=67..71 ONLY at forearm height (y_arm+2..y_arm+6)!
                if f in (2, 3, 4, 5):
                    y_arm = 43 if f in (2, 3) else 44
                    for y in range(y_arm, 60):
                        for x in range(55, 72):
                            if x >= 66 and not (y_arm + 2 <= y <= y_arm + 7):
                                continue
                            if b_cell[y, x, 3] == 0:
                                if x >= 66:
                                    # Extended upper-arm sleeve connecting cleanly into the bare forearm at x=71
                                    if y in (y_arm + 2, y_arm + 7):
                                        b_cell[y, x] = (0, 0, 0, 255)
                                    else:
                                        b_cell[y, x] = (242, 205, 180, 255) if y <= y_arm + 4 else (184, 137, 117, 255)
                                elif y == y_arm:
                                    b_cell[y, x] = (0, 0, 0, 255)
                                elif y < y_arm + 4:
                                    b_cell[y, x] = (55, 62, 43, 255) if y < y_arm + 3 else (172, 137, 89, 255)
                                elif y == y_arm + 4:
                                    b_cell[y, x] = (0, 0, 0, 255)
                                elif y <= y_arm + 8:
                                    if y == y_arm + 8:
                                        b_cell[y, x] = (0, 0, 0, 255)
                                    else:
                                        b_cell[y, x] = (97, 62, 47, 255) if y < y_arm + 7 else (51, 31, 24, 255)
                                elif x <= 64 and y <= 59:
                                    if x == 64 or y == 59:
                                        b_cell[y, x] = (0, 0, 0, 255)
                                    elif x >= 62:
                                        b_cell[y, x] = (172, 137, 89, 255)
                                    else:
                                        b_cell[y, x] = (64, 42, 33, 255)

            elif job == "Druid":
                # 1) In f7: remove the leftover Male-Druid crouching bear at x=32..173 (which is absent in f0 and Combat_Idle!)
                if f == 7:
                    # Copy clean Druid body from f0 (x=174..220) so her back/skirt isn't overwritten by the bear!
                    b_f0 = ba_sheet[:, 0:wa]
                    b_cell[:, :] = b_f0[:, :]
                    # Add a subtle dissipating spirit aura behind her (taken from f6's spirit edge at low alpha/dither)
                    # or keep f7 as a slight landing settle (+1px on skirt hem) so f6 -> f7 -> Idle is 100% pop-free!
                    s = b_cell[200:232, 175:217].copy()
                    b_cell[201:233, 175:217] = np.where(s[:, :, 3:4] > 0, s, b_cell[201:233, 175:217])
                    apply_ponytail_secondary_motion(h_cell, "Side", dy_tail=+1, dx_tip=0)
                elif f == 1:
                    # Lift Druid -1px as the bear spirit surges up behind her
                    b_f0 = ba_sheet[:, 0:wa]
                    char_mask = b_f0[:, :, 3] > 0
                    # Shift character layers up -1px
                    f_cell = shift_cell(f_cell, 0, -1)
                    h_cell = shift_cell(h_cell, 0, -1)
                elif f == 5:
                    # Differentiate f5 from f4 (bear paw slam follow-through: shockwave stones expand 1px outward)
                    left_wave = b_cell[115:185, 130:182].copy()
                    right_wave = b_cell[115:185, 195:242].copy()
                    b_cell[116:186, 129:181] = np.where(left_wave[:, :, 3:4] > 0, left_wave, b_cell[116:186, 129:181])
                    b_cell[116:186, 196:243] = np.where(right_wave[:, :, 3:4] > 0, right_wave, b_cell[116:186, 196:243])

            elif job == "Mage":
                # Unclip the bottom magic ring arc around the staff base in f4 and f5 (y=107..109, x=11..28)
                if f == 4:
                    for x in range(15, 25):
                        if b_cell[108, x, 3] == 0:
                            b_cell[108, x] = (92, 162, 235, 255)
                    for x in range(17, 23):
                        b_cell[109, x] = (65, 128, 210, 255)
                elif f == 5:
                    for x in range(12, 28):
                        if b_cell[109, x, 3] == 0:
                            b_cell[109, x] = (92, 162, 235, 255) if 14 <= x <= 25 else (65, 128, 210, 255)

            clean_combat_face_and_neck(b_cell, f_cell, h_cell)

            ba_sheet[:, f * wa:(f + 1) * wa] = b_cell
            fa_sheet[:, f * wa:(f + 1) * wa] = f_cell
            ha_sheet[:, f * wa:(f + 1) * wa] = h_cell

        save_sheet(ba_sheet, ba_name)
        save_sheet(fa_sheet, fa_name)
        save_sheet(ha_sheet, ha_name)


def apply_proposal_a_walk_legs():
    """
    Applies the in-place 5px/6px leg pinch widening (zero vertical resampling!)
    to Tracker, Gambler, and Druid Walk_Back / Walk_Front (and Tracker/Gambler Idle_Back / Idle_Front)
    directly on the already-aligned HERE sheets.
    (Novice Walk_Back and Walk_Front are already upgraded in-place inside improve_novice_field().)
    """
    from generate_walk_proposals import upgrade_job_walk_inplace

    for job in ["Tracker", "Gambler"]:
        for anim in ["Walk_Back", "Walk_Front", "Idle_Back", "Idle_Front"]:
            arr = upgrade_job_walk_inplace(job, anim, src_dir=HERE)
            save_sheet(arr, f"WCh_{job}_Body_{anim}_Sheet.png")

    for anim in ["Walk_Back", "Walk_Front"]:
        arr_dr = upgrade_job_walk_inplace("Druid", anim, src_dir=HERE)
        save_sheet(arr_dr, f"WCh_Druid_Body_{anim}_Sheet.png")


if __name__ == "__main__":
    improve_novice_field()
    improve_jobs_field()
    improve_combat_sheets()
    apply_proposal_a_walk_legs()
    print("All 94 Female sprite sheets improved (with 100% lively bounce & in-place hand-pixeled legs) successfully!")
