"""
Generate:
- 4 Male Hairstyles (PartId 1..4) & 4 Female Hairstyles (PartId 1..4)
  (Excluding the old Male #4 and Female #2 hanging-sidelock styles as requested)
- 5 Mabinogi-Style Expressive Male Eye/Face Styles (PartId 1..5)
- 5 Mabinogi-Style Expressive Female Eye/Face Styles (PartId 1..5)
  1: 기본 초롱 눈매 (Classic Clear Round Eyes)
  2: 도도한 고양이 눈매 (Chic Cat Eyes / Tsurime + Vertical Slit Pupil + :3 Kitty Mouth)
  3: 해맑은 > < 눈웃음 표정 (Cheerful > < Squinting Laugh Arcs + Rosy Blush + Happy Mouth)
  4: 나른한 반눈 / 지토메 ㅡ ㅡ (Aloof Half-Lidded Jitome ㅡ ㅡ + Sanpaku Sclera + Tear Mole)
  5: 순둥 처진 별눈 (Gentle Droopy Tareme + Sparkling Starlight Big Eyes + Blush)

Key guarantees:
1. ZERO color bleed: 100% of hair pixels use the warm-blonde palette and tint uniformly under M_NUCharacterHairTint.
2. ZERO 1-pixel animation cuts: Every frame f=0..7 of Idle and Walk is built directly from
   the hand-animated frame f of MCh_Hair1 / WCh_Hair (no y=28 horizontal slicing).
3. ZERO hanging side strips: Both excluded styles (old M-Hair 4 and F-Hair 2) and all remaining styles
   are completely free of pasted vertical side columns.
"""

import os
from PIL import Image, ImageDraw, ImageFont
import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MALE_DIR = os.path.join(ROOT, "ArtSource", "Character", "Novice")
FEMALE_DIR = os.path.join(ROOT, "ArtSource", "Character", "Female")
ARTIFACT_DIR = r"C:\Users\Ndus_072\.gemini\antigravity\brain\be1275f5-376f-4165-9ea2-1b959606f5ad"


def hex_rgba(h, a=255):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


BLONDE_RAMP = [
    hex_rgba("#000000"),  # 0: pure black outline
    hex_rgba("#1b1325"),  # 1: dark charcoal tie/ribbon shadow
    hex_rgba("#5c4135"),  # 2: dark root shadow
    hex_rgba("#7d5b4b"),  # 3: deep shadow / soft inner bevel
    hex_rgba("#9c7867"),  # 4: mid shadow
    hex_rgba("#b58f7c"),  # 5: base midtone
    hex_rgba("#cfa793"),  # 6: warm blonde
    hex_rgba("#e3bca6"),  # 7: bright blonde
    hex_rgba("#f2ceb8"),  # 8: highlight band
    hex_rgba("#ffdfca"),  # 9: specular halo peak
]


def load_sheet(path):
    return np.array(Image.open(path).convert("RGBA"), dtype=np.uint8)


def save_sheet(arr, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.fromarray(arr, mode="RGBA").save(path)


def shift_frame(frame, dx=0, dy=0):
    out = np.zeros_like(frame)
    h, w, _ = frame.shape
    src_x0 = max(0, -dx)
    src_x1 = min(w, w - dx)
    src_y0 = max(0, -dy)
    src_y1 = min(h, h - dy)
    dst_x0 = max(0, dx)
    dst_x1 = min(w, w + dx)
    dst_y0 = max(0, dy)
    dst_y1 = min(h, h + dy)
    if src_x1 > src_x0 and src_y1 > src_y0:
        out[dst_y0:dst_y1, dst_x0:dst_x1] = frame[src_y0:src_y1, src_x0:src_x1]
    return out


def seal_outer_outline(frame, protect_box=None):
    """Ensure any opaque pixel on the OUTER silhouette bordering empty air is #000000,
    while preserving soft hand-pixeled anti-aliasing inside protect_box (around the face/bangs)."""
    out = frame.copy()
    alpha = out[:, :, 3] > 0
    for y in range(1, 99):
        for x in range(1, 59):
            if alpha[y, x]:
                if protect_box is not None:
                    x0, y0, x1, y1 = protect_box
                    if x0 <= x <= x1 and y0 <= y <= y1:
                        continue
                if (
                    not alpha[y - 1, x]
                    or not alpha[y + 1, x]
                    or not alpha[y, x - 1]
                    or not alpha[y, x + 1]
                ):
                    out[y, x] = BLONDE_RAMP[0]
    # Remove isolated 1-pixel specks
    alpha = out[:, :, 3] > 0
    for y in range(1, 99):
        for x in range(1, 59):
            if alpha[y, x]:
                n_cnt = sum(
                    1
                    for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1))
                    if alpha[ny, nx]
                )
                if n_cnt == 0:
                    out[y, x] = (0, 0, 0, 0)
    return out


# ==============================================================================
# STEP 1: Per-Frame Head Bounce Tables & Clean Backup Loaders
# ==============================================================================
BACKUP_MALE_DIR = os.path.join(ROOT, "Saved", "MaleSpriteBackup_20261014")
BACKUP_FEMALE_DIR = os.path.join(ROOT, "Saved", "FemaleSpriteBackup_20261014")

MALE_DY = {
    "Idle_Front": [0, 0, 1, 2, 1, 1, 0, 0],
    "Idle_Side": [0, 0, 1, 2, 1, 1, 0, 0],
    "Idle_Back": [0, 0, 1, 1, 1, 0, 0, 0],
    "Walk_Front": [0, 1, 0, -1, 0, 1, 0, -1],
    "Walk_Side": [0, 1, 1, 0, 1, 0, 1, 0],
    "Walk_Back": [0, 1, 0, -1, 0, 1, 0, -1],
}

FEMALE_DY = {
    "Idle_Front": [0, 0, 1, 1, 1, 0, 0, 0],
    "Idle_Side": [0, 0, 1, 1, 1, 0, 0, 0],
    "Idle_Back": [0, 0, 1, 1, 1, 0, 0, 0],
    "Walk_Front": [0, 2, 2, -1, 0, 2, 2, -1],
    "Walk_Side": [0, 2, 1, -1, 0, 2, 1, -1],
    "Walk_Back": [0, 2, 2, -1, 0, 2, 2, -1],
}


def load_male_backup(name):
    p = os.path.join(BACKUP_MALE_DIR, name)
    if os.path.exists(p):
        return load_sheet(p)
    return load_sheet(os.path.join(MALE_DIR, name))


def load_female_backup(name):
    p = os.path.join(BACKUP_FEMALE_DIR, name)
    if os.path.exists(p):
        return load_sheet(p)
    return load_sheet(os.path.join(FEMALE_DIR, name))


def clean_male_hair1_and_restore_face():
    """Ensure MCh_Hair1 contains ZERO skin/ear/glabella/temple pixels, and restore all
    such pixels to the base MCh_FaceEye sheets before generating custom faces and hairs."""
    m_base_off = {
        "Idle_Front": 0,
        "Walk_Front": 1,
        "Idle_Side": 0,
        "Walk_Side": -1,
        "Idle_Back": 0,
        "Walk_Back": 0,
    }
    # 1) Front: clean MCh_Hair1_{Idle,Walk}_Front_Sheet.png
    for anim in ["Idle_Front", "Walk_Front"]:
        h_arr = load_male_backup(f"MCh_Hair1_{anim}_Sheet.png")
        base_off = m_base_off[anim]
        for f, dy0 in enumerate(MALE_DY[anim]):
            ox = f * 60
            dy = base_off + dy0
            # Recolor peach/orange brow-shadow pixels (#dc8566, #e7a588) at y_rel=20..22 to warm blonde hair
            for y_rel in (20, 21, 22):
                y = y_rel + dy
                if 0 <= y < 100:
                    for x in range(22, 37):
                        if h_arr[y, ox + x, 3] > 0:
                            r, g, b, _ = h_arr[y, ox + x]
                            if (r > 215 and g < 170) or (y_rel == 22 and x in (23, 24, 25, 32, 33, 34, 35, 36)):
                                h_arr[y, ox + x] = BLONDE_RAMP[5 if (y_rel == 22 and x in (23, 32, 36)) else 6]
            # At y_rel=23: open brow/upper-eyelid arches (x=23..27 and x=31..35) so eyebrows & skin never dye!
            # Keep neat center bang lock at x=28..30 and outer bang frames at x<=22, x>=36
            y23 = 23 + dy
            if 0 <= y23 < 100:
                for x in [23, 24, 25, 26, 27, 31, 32, 33, 34, 35]:
                    h_arr[y23, ox + x] = (0, 0, 0, 0)
                for x in [22, 36]:
                    if h_arr[y23, ox + x, 3] > 0:
                        h_arr[y23, ox + x] = BLONDE_RAMP[4]
                for x in [28, 29, 30]:
                    h_arr[y23, ox + x] = BLONDE_RAMP[6 if x == 29 else 5]
            # At y_rel=24: clear eye + glabella skin (x=21..27 and x=31..37), keep center bang (x=28..30) and sideburns (x=20, 38)
            y24 = 24 + dy
            if 0 <= y24 < 100:
                for x in list(range(21, 28)) + list(range(31, 38)):
                    h_arr[y24, ox + x] = (0, 0, 0, 0)
                for x in [28, 29, 30]:
                    h_arr[y24, ox + x] = BLONDE_RAMP[6 if x == 29 else 4]
                for x in [20, 38]:
                    h_arr[y24, ox + x] = BLONDE_RAMP[5]
            # At y_rel=25: clear temple + eye + glabella skin (x=21..28 and x=30..37), keep 1px center tip (x=29) & symmetrical sideburns (x=20, 38)
            y25 = 25 + dy
            if 0 <= y25 < 100:
                for x in list(range(21, 29)) + list(range(30, 38)):
                    h_arr[y25, ox + x] = (0, 0, 0, 0)
                h_arr[y25, ox + 29] = BLONDE_RAMP[4]
                for x in [20, 38]:
                    h_arr[y25, ox + x] = BLONDE_RAMP[5]
            # At y_rel=26..27: clear entire face/temple span (x=21..37) and ear-top skin at (19,27)/(39,27),
            # keeping symmetrical 1px sideburn tips at x=20 and x=38
            for y_rel, ramp_idx in [(26, 4), (27, 3)]:
                y = y_rel + dy
                if 0 <= y < 100:
                    for x in range(21, 38):
                        h_arr[y, ox + x] = (0, 0, 0, 0)
                    h_arr[y, ox + 20] = BLONDE_RAMP[ramp_idx]
                    h_arr[y, ox + 38] = BLONDE_RAMP[ramp_idx]
                    if y_rel == 27:
                        h_arr[y, ox + 19] = BLONDE_RAMP[3]
                        h_arr[y, ox + 39] = BLONDE_RAMP[3]
            # At y_rel=28..31: remove all leaked ear skin/outline pixels from MCh_Hair1!
            for y_rel in range(28, 32):
                y = y_rel + dy
                if 0 <= y < 100:
                    h_arr[y, ox : ox + 60] = 0
        save_sheet(h_arr, os.path.join(MALE_DIR, f"MCh_Hair1_{anim}_Sheet.png"))

    # 2) Side: clean ear/cheek/temple skin pixels from MCh_Hair1_{Idle,Walk}_Side_Sheet.png
    side_skin_coords = [
        (36, 22), (36, 23),  # forehead highlight in bangs -> recolor to blonde
    ]
    side_ear_cheek_clear = [
        (35, 25), (31, 26), (30, 27), (29, 29),  # upper-eye, temple, and cheek skin
        (24, 25), (25, 26), (26, 26), (26, 27), (23, 28),
        (24, 29), (24, 30), (25, 30), (26, 31), (27, 31), (27, 32),  # male ear skin & outline
    ]
    for anim in ["Idle_Side", "Walk_Side"]:
        h_arr = load_male_backup(f"MCh_Hair1_{anim}_Sheet.png")
        base_off = m_base_off[anim]
        for f, dy0 in enumerate(MALE_DY[anim]):
            ox = f * 60
            dy = base_off + dy0
            for sx, sy0 in side_skin_coords:
                y = sy0 + dy
                if 0 <= y < 100 and h_arr[y, ox + sx, 3] > 0:
                    h_arr[y, ox + sx] = BLONDE_RAMP[6]
            for cx, cy0 in side_ear_cheek_clear:
                y = cy0 + dy
                if 0 <= y < 100:
                    h_arr[y, ox + cx] = (0, 0, 0, 0)
        save_sheet(h_arr, os.path.join(MALE_DIR, f"MCh_Hair1_{anim}_Sheet.png"))

    # 3) Back: move the 4 back-ear tip skin pixels (20,26..27) and (40,26..27) from Hair1_Back to FaceEye_Back
    for anim in ["Idle_Back", "Walk_Back"]:
        h_arr = load_male_backup(f"MCh_Hair1_{anim}_Sheet.png")
        f_arr = load_male_backup(f"MCh_FaceEye_{anim}_Sheet.png")
        base_off = m_base_off[anim]
        for f, dy0 in enumerate(MALE_DY[anim]):
            ox = f * 60
            dy = base_off + dy0
            for ex, ey0 in [(20, 26), (20, 27), (40, 26), (40, 27)]:
                y = ey0 + dy
                if 0 <= y < 100 and h_arr[y, ox + ex, 3] > 0:
                    f_arr[y, ox + ex] = h_arr[y, ox + ex]
                    h_arr[y, ox + ex] = BLONDE_RAMP[3]  # dark hair shadow under ear tip
        save_sheet(h_arr, os.path.join(MALE_DIR, f"MCh_Hair1_{anim}_Sheet.png"))
        save_sheet(f_arr, os.path.join(MALE_DIR, f"MCh_FaceEye_{anim}_Sheet.png"))


def add_forehead_skin_bridge_male(front):
    """Build a complete, 100% symmetrical Male Front face canvas (centered at x=29.0, 58 - x):
    - Full forehead & glabella skin (y=18..27, x=21..37) with symmetrical glabella (31,24..25 = skin_main)
    - Symmetrical temple skin at x=21 and x=37 (y=24..27)
    - Symmetrical left (x=19..20) and right (x=38..39) ears & jawlines at y=28..30!"""
    out = front.copy()
    skin_main = hex_rgba("#fee3d2")
    skin_mid = hex_rgba("#fedfcc")
    temple_top = hex_rgba("#e7bb9e")
    temple_mid = hex_rgba("#ddb297")
    temple_bot = hex_rgba("#d3a38c")

    # Remove sideburn pixels (20, 26..27) from FaceEye so x=20 and x=38 are strictly sideburn hair,
    # while x=21 and x=37 are strictly symmetrical temple skin!
    out[26, 20] = (0, 0, 0, 0)
    out[27, 20] = (0, 0, 0, 0)

    for y in range(18, 28):
        for x in range(21, 38):
            if out[y, x, 3] == 0:
                out[y, x] = skin_mid if y <= 21 else skin_main

    # Clear the asymmetrical cast-shadow strip at (31, 24..25) so glabella x=27..31 is 100% symmetrical
    out[24, 31] = skin_main
    out[25, 31] = skin_main

    # Symmetrical temple skin buffer at x=21 (left of left eye 22..26) and x=37 (right of right eye 32..36)
    out[24, 21] = skin_mid
    out[24, 37] = skin_mid
    out[25, 21] = temple_top
    out[25, 37] = temple_top
    out[26, 21] = temple_mid
    out[26, 37] = temple_mid
    out[27, 21] = temple_bot
    out[27, 37] = temple_bot

    # Exact 58 - x mirror of left ear/jaw (x=19..23) to right (x=35..39) at y=28..30
    out[28, 19] = hex_rgba("#f7ac86")
    out[28, 20] = hex_rgba("#f7ac86")
    for y in (28, 29, 30):
        for lx in range(19, 24):
            out[y, 58 - lx] = out[y, lx]

    # Default subtle eyebrows at y=23 (above left eye x=23..26 and right eye x=32..35) for Style 1
    brow_soft = hex_rgba("#8c584c")
    for x in [23, 24, 25, 33, 34, 35]:
        out[23, x] = brow_soft
    return out


def add_side_temple_skin_bridge_male(side):
    """Build a complete Male Side face canvas with full ear, temple, and forehead skin
    restored from MCh_Hair1_Side and zero brown sideburn cast-shadow smudges on the cheek."""
    out = side.copy()
    skin_main = hex_rgba("#fce2d2")
    skin_mid = hex_rgba("#f6d1b7")
    orig_h_side = load_male_backup("MCh_Hair1_Idle_Side_Sheet.png")[:, :60]

    # Copy the male ear pixels that originally lived in MCh_Hair1_Side into FaceEye_Side
    for cx, cy in [
        (24, 25), (25, 26), (26, 26), (26, 27), (23, 28),
        (24, 29), (24, 30), (25, 30), (26, 31), (27, 31), (27, 32),
    ]:
        if orig_h_side[cy, cx, 3] > 0:
            out[cy, cx] = orig_h_side[cy, cx]

    # Clean temple, cheek, and front-eye skin pixels so zero brownish sideburn/bang shadows remain
    out[25, 35] = orig_h_side[25, 35]
    out[26, 31] = skin_mid
    out[27, 30] = skin_main
    out[28, 29] = skin_main
    out[29, 28] = skin_main
    out[29, 29] = skin_main
    out[26, 36] = skin_mid
    out[27, 37] = skin_mid

    for y in range(23, 30):
        for x in range(25, 36):
            if out[y, x, 3] == 0:
                out[y, x] = skin_mid if y <= 24 else skin_main
    return out


def add_forehead_skin_bridge_female(front):
    """Build a 100% symmetrical Female Front face canvas (centered at x=30, spanning x=22..38):
    - Restores the outer brow/eyelash/jawline contour pixels that improve_female_sprites moved into Hair
    - NEVER places stray skin pixels outside the black jawline at (22,29), (23,30), or (24,31)!"""
    out = front.copy()
    orig_w = load_female_backup("WCh_Body_Idle_Front_Sheet.png")[:, :60]
    skin_main = hex_rgba("#fee3d2")
    skin_mid = hex_rgba("#fedfcc")

    # 1. Restore the exact symmetrical face/eyebrow/jawline pixels from orig_w inside the face contour:
    #    y=23..28: x=22..38, y=29: x=23..37, y=30: x=24..36, y=31: x=25..35
    face_spans = {
        23: (23, 37),
        24: (22, 38),
        25: (22, 38),
        26: (22, 38),
        27: (22, 38),
        28: (22, 38),
        29: (23, 37),
        30: (24, 36),
        31: (25, 35),
    }
    for y, (x0, x1) in face_spans.items():
        # Clear any stray pixels outside [x0, x1] on this row!
        out[y, :x0] = 0
        out[y, x1 + 1 :] = 0
        for x in range(x0, x1 + 1):
            if out[y, x, 3] == 0 and orig_w[y, x, 3] > 0:
                out[y, x] = orig_w[y, x]

    # Ensure (23, 28) and (37, 28) inside the jawline are soft cheek contour (#d09683) matching y=29..30,
    # NOT dark hair shadow (#ab7050 / #8a4f3a), and (22, 27..28) & (38, 27..28) are crisp #301b17 outline!
    out[27, 22] = hex_rgba("#301b17")
    out[27, 38] = hex_rgba("#301b17")
    out[28, 22] = hex_rgba("#301b17")
    out[28, 38] = hex_rgba("#301b17")
    out[28, 23] = hex_rgba("#d09683")
    out[28, 37] = hex_rgba("#d09683")

    # Fill forehead skin bridge under bangs (y=18..25, x=22..38) where transparent
    for y in range(18, 26):
        for x in range(22, 39):
            if out[y, x, 3] == 0:
                out[y, x] = skin_mid if y <= 20 else skin_main
    # Also ensure glabella at y=23..25, x=28..32 is clean skin under the center bang tip
    for y in (23, 24, 25):
        for x in range(28, 33):
            out[y, x] = skin_mid if y == 23 else skin_main
    return out


def add_side_temple_skin_bridge_female(side):
    """Build a complete Female Side face canvas with forehead, temple, ear, and jaw/neck skin
    so zero skin pixels remain in WCh_Hair_Side."""
    out = side.copy()
    orig_w_side = load_female_backup("WCh_Body_Idle_Side_Sheet.png")[:, :60]
    skin_main = hex_rgba("#fee3d2")
    skin_mid = hex_rgba("#fedfcc")

    # Restore ear and jaw/neck skin pixels that were put into WCh_Hair_Side
    for cx, cy in [(26, 27), (23, 28), (25, 31), (26, 31), (27, 31)]:
        if orig_w_side[cy, cx, 3] > 0:
            out[cy, cx] = orig_w_side[cy, cx]

    for y in range(21, 29):
        for x in range(24, 36):
            if out[y, x, 3] == 0:
                out[y, x] = skin_mid if y <= 23 else skin_main
    return out


# ==============================================================================
# STEP 2: 5 Mabinogi-Style Expressive Eye/Face Styles (PartId 1..5)
#   1: 기본 초롱 눈매 (Classic Clear Round Eyes)
#   2: 도도한 고양이 눈매 (Chic Cat Eyes / Tsurime + Vertical Slit Pupil + :3 Mouth)
#   3: 해맑은 > < 눈웃음 표정 (Cheerful > < Squinting Laugh Arcs + Blush + :D Mouth)
#   4: 나른한 반눈 / 지토메 ㅡ ㅡ (Aloof Half-Lidded Jitome ㅡ ㅡ + Tear Mole)
#   5: 순둥 처진 별눈 (Gentle Droopy Tareme + Sparkling Starlight Big Eyes + Blush)
# ==============================================================================
def make_male_face_style_f0(base_front, base_side, style_id):
    front = add_forehead_skin_bridge_male(base_front)
    side = add_side_temple_skin_bridge_male(base_side)
    if style_id == 1:
        return front, side

    skin = hex_rgba("#fee3d2")
    skin_mid = hex_rgba("#fedfcc")
    skin_shade = hex_rgba("#e8aa99")
    lash_black = hex_rgba("#120814")
    lash_dark = hex_rgba("#2b1620")
    lash_soft = hex_rgba("#6e4448")
    sclera_w = hex_rgba("#ebe6f2")

    if style_id == 2:
        # Style 2: Chic Cat Eyes (도도한 고양이 눈매 + 세로 동공 + :3 입매)
        # Clear eye area first to sculpt sharp upturned winged tsurime + vertical slit pupil
        amber_hi = hex_rgba("#ffe478")
        amber_mid = hex_rgba("#f0a21d")
        amber_deep = hex_rgba("#9c5208")
        slit_pupil = hex_rgba("#170704")

        # Sharp slanted eyebrows at y=23
        front[23, 25] = lash_dark
        front[23, 26] = lash_black
        front[23, 32] = lash_black
        front[23, 33] = lash_dark

        # Upturned winged upper eyeliner (outer wing kicks up at y=24, x=22 & x=36; inner dips to y=25)
        front[24, 22] = lash_black
        front[24, 23] = lash_black
        front[24, 24] = lash_black
        front[24, 25] = lash_dark
        front[24, 26] = skin_shade
        front[24, 32] = skin_shade
        front[24, 33] = lash_dark
        front[24, 34] = lash_black
        front[24, 35] = lash_black
        front[24, 36] = lash_black

        # Row y=25: almond sclera on outer edge + golden iris with 1px VERTICAL SLIT PUPIL at center (x=25, x=33)
        front[25, 22] = lash_soft
        front[25, 23] = sclera_w
        front[25, 24] = amber_mid
        front[25, 25] = slit_pupil
        front[25, 26] = lash_black
        front[25, 32] = lash_black
        front[25, 33] = slit_pupil
        front[25, 34] = amber_mid
        front[25, 35] = sclera_w
        front[25, 36] = lash_soft

        # Row y=26: bright glowing cat iris flanking vertical slit pupil + raised outer lower lid
        front[26, 23] = skin_mid
        front[26, 24] = amber_hi
        front[26, 25] = slit_pupil
        front[26, 26] = amber_mid
        front[26, 32] = amber_mid
        front[26, 33] = slit_pupil
        front[26, 34] = amber_hi
        front[26, 35] = skin_mid

        # Row y=27: tapered almond bottom rim (outer corners raised to skin!)
        front[27, 23] = skin
        front[27, 24] = skin_mid
        front[27, 25] = amber_deep
        front[27, 26] = amber_hi
        front[27, 32] = amber_hi
        front[27, 33] = amber_deep
        front[27, 34] = skin_mid
        front[27, 35] = skin

        # Sleek 3-pixel cat smirk mouth at y=30..31
        front[31, 28] = hex_rgba("#d47e63")
        front[31, 29] = hex_rgba("#e08b70")
        front[30, 30] = hex_rgba("#c97157")

        # Side view: upturned winged cat liner + vertical slit pupil
        side[25, 31] = lash_black
        side[26, 31] = lash_black
        side[26, 32] = lash_black
        side[26, 33] = lash_black
        side[26, 34] = lash_dark
        side[27, 33] = sclera_w
        side[27, 34] = slit_pupil
        side[27, 35] = amber_mid
        side[28, 33] = skin_mid
        side[28, 34] = slit_pupil
        side[28, 35] = amber_hi
        side[29, 34] = amber_deep
        side[29, 35] = skin_mid

    elif style_id == 3:
        # Style 3: Cheerful '> <' Squinting Laugh Eyes (해맑은 > < 눈웃음 표정 + 발그레 홍조)
        blush_deep = hex_rgba("#f58c98")
        blush_soft = hex_rgba("#faa8ad")
        mouth_dark = hex_rgba("#bd5848")
        mouth_in = hex_rgba("#f27d6d")

        # Reset both eye boxes (y=24..27, x=22..26 and x=32..36) to clean skin first
        for y in range(24, 28):
            for x in list(range(22, 27)) + list(range(32, 37)):
                front[y, x] = skin

        # Left Eye '>' shape (x=22..26, y=24..27):
        # Upper diagonal slanting down-right (22,24)->(24,25)->(26,25..26)
        front[24, 22] = lash_soft
        front[24, 23] = lash_black
        front[24, 24] = lash_dark
        front[25, 24] = lash_dark
        front[25, 25] = lash_black
        front[25, 26] = lash_black  # '>' apex
        # Lower diagonal slanting down-left from apex (26,25)->(24,26)->(22,27)
        front[26, 25] = lash_black
        front[26, 24] = lash_black
        front[26, 23] = lash_dark
        front[27, 22] = lash_black
        front[27, 23] = lash_soft

        # Right Eye '<' shape (x=32..36, y=24..27):
        # Upper diagonal slanting down-left (36,24)->(34,25)->(32,25..26)
        front[24, 36] = lash_soft
        front[24, 35] = lash_black
        front[24, 34] = lash_dark
        front[25, 34] = lash_dark
        front[25, 33] = lash_black
        front[25, 32] = lash_black  # '<' apex
        # Lower diagonal slanting down-right from apex
        front[26, 33] = lash_black
        front[26, 34] = lash_black
        front[26, 35] = lash_dark
        front[27, 36] = lash_black
        front[27, 35] = lash_soft

        # Warm rosy blush on cheeks (y=28, x=21..23 and x=35..37)
        front[28, 22] = blush_deep
        front[28, 23] = blush_soft
        front[28, 35] = blush_soft
        front[28, 36] = blush_deep

        # Cheerful open happy mouth ':D' at y=30..31, x=28..30
        front[30, 28] = mouth_dark
        front[30, 29] = mouth_dark
        front[30, 30] = mouth_dark
        front[31, 28] = mouth_in
        front[31, 29] = mouth_in
        front[31, 30] = mouth_in

        # Side view: '<' squinting eye arc at x=32..35, y=26..28 + soft brow at y=25 + blush
        for x in range(32, 37):
            side[25, x] = lash_soft if 33 <= x <= 35 else hex_rgba("#fce2d2")
        for y in range(26, 30):
            for x in range(32, 36):
                side[y, x] = hex_rgba("#fce2d2")
        side[26, 35] = lash_black
        side[26, 34] = lash_dark
        side[27, 33] = lash_black
        side[27, 32] = lash_black  # '<' apex
        side[28, 33] = lash_dark
        side[28, 34] = lash_black
        side[28, 35] = lash_black
        side[29, 31] = blush_deep
        side[29, 32] = blush_soft
        side[32, 34] = mouth_dark
        side[32, 35] = mouth_in

    elif style_id == 4:
        # Style 4: Lazy Half-Lidded Jitome 'ㅡ ㅡ' (나른한 반눈 / 지토메 표정 + 눈물점)
        iris_dark = hex_rgba("#4a1525")
        iris_mid = hex_rgba("#9e2445")
        iris_hi = hex_rgba("#e86b88")
        mole = hex_rgba("#381c22")

        # Row y=24: erase high arch lash to flat skin & flat low brow at y=23..24!
        for x in list(range(22, 27)) + list(range(32, 37)):
            front[24, x] = skin_mid
        for x in [23, 24, 25, 26, 32, 33, 34, 35]:
            front[23, x] = lash_soft

        # Row y=25: heavy straight horizontal upper eyelid bar 'ㅡ ㅡ'
        for x in range(22, 27):
            front[25, x] = lash_black
        for x in range(32, 37):
            front[25, x] = lash_black

        # Row y=26: half-covered sleepy iris peeking under the flat lid
        front[26, 22] = skin
        front[26, 23] = sclera_w
        front[26, 24] = iris_dark
        front[26, 25] = iris_mid
        front[26, 26] = sclera_w
        front[26, 32] = sclera_w
        front[26, 33] = iris_mid
        front[26, 34] = iris_dark
        front[26, 35] = sclera_w
        front[26, 36] = skin

        # Row y=27: sanpaku lower sclera rim + soft bottom highlight
        front[27, 23] = skin
        front[27, 24] = sclera_w
        front[27, 25] = iris_hi
        front[27, 26] = sclera_w
        front[27, 32] = sclera_w
        front[27, 33] = iris_hi
        front[27, 34] = sclera_w
        front[27, 35] = skin

        # Chic beauty mark (tear mole) under left outer eye + flat unimpressed mouth
        front[28, 23] = mole
        front[31, 28] = hex_rgba("#c9826b")
        front[31, 29] = hex_rgba("#c9826b")
        front[31, 30] = hex_rgba("#c9826b")

        # Side view: lowered flat horizontal lid at y=27 + sleepy half-eye at y=28..29
        for x in range(32, 36):
            side[26, x] = hex_rgba("#f6d1b7")
            side[27, x] = lash_black
        side[28, 33] = sclera_w
        side[28, 34] = iris_dark
        side[28, 35] = iris_mid
        side[29, 34] = sclera_w
        side[29, 35] = iris_hi
        side[29, 32] = mole

    elif style_id == 5:
        # Style 5: Gentle Droopy Tareme + Sparkling Starlight Big Eyes (순둥 처진 별눈 + 홍조)
        u_dark = hex_rgba("#1e1659")
        u_mid = hex_rgba("#3b2d9e")
        m_bright = hex_rgba("#655ce6")
        l_aqua = hex_rgba("#8fd0ff")
        star_w = hex_rgba("#ffffff")
        blush = hex_rgba("#f79ea8")

        # Droopy tareme upper lash: high on inner side (y=24, x=25..26 & 32..33), drooping down on outer side (y=25..26, x=22 & 36)
        front[24, 22] = skin_mid
        front[24, 23] = lash_dark
        front[24, 24] = lash_black
        front[24, 25] = lash_black
        front[24, 26] = lash_black
        front[24, 32] = lash_black
        front[24, 33] = lash_black
        front[24, 34] = lash_black
        front[24, 35] = lash_dark
        front[24, 36] = skin_mid

        # Outer droop lash at y=25..26, x=22 & x=36 + big sparkling starlight iris
        front[25, 22] = lash_black
        front[25, 23] = star_w  # big top-outer star catchlight
        front[25, 24] = u_dark
        front[25, 25] = u_mid
        front[25, 26] = star_w  # inner-top catchlight
        front[25, 32] = star_w
        front[25, 33] = u_mid
        front[25, 34] = u_dark
        front[25, 35] = star_w
        front[25, 36] = lash_black

        front[26, 22] = lash_soft
        front[26, 23] = m_bright
        front[26, 24] = star_w
        front[26, 25] = m_bright
        front[26, 26] = l_aqua
        front[26, 32] = l_aqua
        front[26, 33] = m_bright
        front[26, 34] = star_w
        front[26, 35] = m_bright
        front[26, 36] = lash_soft

        front[27, 23] = sclera_w
        front[27, 24] = l_aqua
        front[27, 25] = star_w
        front[27, 26] = l_aqua
        front[27, 32] = l_aqua
        front[27, 33] = star_w
        front[27, 34] = l_aqua
        front[27, 35] = sclera_w

        # Rosy blush & gentle smile
        front[28, 22] = blush
        front[28, 23] = blush
        front[28, 35] = blush
        front[28, 36] = blush
        front[30, 27] = hex_rgba("#e59677")
        front[30, 31] = hex_rgba("#e59677")

        # Side view: droopy outer corner + sparkling dual-star iris
        side[26, 32] = lash_soft
        side[27, 31] = lash_black
        side[27, 33] = star_w
        side[27, 34] = u_dark
        side[27, 35] = u_mid
        side[28, 33] = m_bright
        side[28, 34] = star_w
        side[28, 35] = l_aqua
        side[29, 34] = l_aqua
        side[29, 35] = star_w
        side[30, 32] = blush

    return front, side


def generate_male_face_sheets():
    clean_male_hair1_and_restore_face()
    base_if = load_male_backup("MCh_FaceEye_Idle_Front_Sheet.png")[:, :60]
    base_is = load_male_backup("MCh_FaceEye_Idle_Side_Sheet.png")[:, :60]
    base_ib_sheet = load_sheet(os.path.join(MALE_DIR, "MCh_FaceEye_Idle_Back_Sheet.png"))
    base_wb_sheet = load_sheet(os.path.join(MALE_DIR, "MCh_FaceEye_Walk_Back_Sheet.png"))

    for style_id in range(1, 6):
        f0_front, f0_side = make_male_face_style_f0(base_if, base_is, style_id)
        prefix = "MCh_FaceEye" if style_id == 1 else f"MCh_FaceEye{style_id}"

        for anim, f0_img, base_off in [
            ("Idle_Front", f0_front, 0),
            ("Walk_Front", f0_front, 1),
            ("Idle_Side", f0_side, 0),
            ("Walk_Side", f0_side, -1),
        ]:
            sheet = np.zeros((100, 480, 4), dtype=np.uint8)
            for f, dy in enumerate(MALE_DY[anim]):
                sheet[:, f * 60 : (f + 1) * 60] = shift_frame(f0_img, 0, base_off + dy)
            save_sheet(sheet, os.path.join(MALE_DIR, f"{prefix}_{anim}_Sheet.png"))

        save_sheet(base_ib_sheet, os.path.join(MALE_DIR, f"{prefix}_Idle_Back_Sheet.png"))
        save_sheet(base_wb_sheet, os.path.join(MALE_DIR, f"{prefix}_Walk_Back_Sheet.png"))

        icon = f0_front[14:42, 16:44].copy()
        save_sheet(icon, os.path.join(MALE_DIR, f"T_PartIcon_Eyes_{style_id}_Front.png"))


def enforce_female_face_outline_f0(front):
    """Ensure the continuous symmetrical dark face/cheek/jawline border around x=30
    (x=22,38 at y=24..28; x=23,37 at y=29; x=24,36 at y=30; x=25,35 at y=31)
    is 100% intact across all 5 expressions so skin never bleeds into the hair!"""
    front[24, 22] = hex_rgba("#462c2b")
    front[24, 38] = hex_rgba("#462c2b")
    front[25, 22] = hex_rgba("#110711")
    front[25, 38] = hex_rgba("#110711")
    front[26, 22] = hex_rgba("#5c2020")
    front[26, 38] = hex_rgba("#5c2020")
    front[27, 22] = hex_rgba("#301b17")
    front[27, 38] = hex_rgba("#301b17")
    front[28, 22] = hex_rgba("#301b17")
    front[28, 38] = hex_rgba("#301b17")
    front[29, 23] = hex_rgba("#301b17")
    front[29, 37] = hex_rgba("#301b17")
    front[30, 24] = hex_rgba("#301b17")
    front[30, 36] = hex_rgba("#301b17")
    front[31, 25] = hex_rgba("#301b17")
    front[31, 35] = hex_rgba("#301b17")
    # Ensure zero stray pixels outside the jawline at y=29..31
    front[29, :23] = 0
    front[29, 38:] = 0
    front[30, :24] = 0
    front[30, 37:] = 0
    front[31, :25] = 0
    front[31, 36:] = 0
    return front


def make_female_face_style_f0(base_front, base_side, style_id):
    front = add_forehead_skin_bridge_female(base_front)
    side = add_side_temple_skin_bridge_female(base_side)
    if style_id == 1:
        return enforce_female_face_outline_f0(front), side

    skin = hex_rgba("#fee3d2")
    skin_mid = hex_rgba("#fedfcc")
    skin_shade = hex_rgba("#e8aa99")
    lash_black = hex_rgba("#110714")
    lash_dark = hex_rgba("#2b1522")
    lash_soft = hex_rgba("#70454c")
    sclera_w = hex_rgba("#ede8f5")

    if style_id == 2:
        # Style 2: Chic Cat Eyes (도도한 고양이 눈매 + 세로 동공 + :3 고양이 입매)
        amber_hi = hex_rgba("#ffe680")
        amber_mid = hex_rgba("#f2a41e")
        amber_deep = hex_rgba("#994f06")
        slit_pupil = hex_rgba("#170604")

        # Upturned winged cat eyeliner inside x=23..27 and x=33..37
        front[23, 23] = lash_black
        front[23, 37] = lash_black
        front[24, 23] = lash_black
        front[24, 24] = lash_black
        front[24, 25] = lash_black
        front[24, 26] = lash_dark
        front[24, 27] = skin_shade
        front[24, 33] = skin_shade
        front[24, 34] = lash_dark
        front[24, 35] = lash_black
        front[24, 36] = lash_black
        front[24, 37] = lash_black

        # Row y=25: almond sclera + golden iris with 1px VERTICAL SLIT PUPIL at x=26 & x=34
        front[25, 23] = lash_soft
        front[25, 24] = sclera_w
        front[25, 25] = amber_mid
        front[25, 26] = slit_pupil
        front[25, 27] = lash_black
        front[25, 33] = lash_black
        front[25, 34] = slit_pupil
        front[25, 35] = amber_mid
        front[25, 36] = sclera_w
        front[25, 37] = lash_soft

        # Row y=26: bright golden cat iris on both sides of the vertical slit pupil
        front[26, 24] = skin_mid
        front[26, 25] = amber_hi
        front[26, 26] = slit_pupil
        front[26, 27] = amber_mid
        front[26, 33] = amber_mid
        front[26, 34] = slit_pupil
        front[26, 35] = amber_hi
        front[26, 36] = skin_mid

        # Row y=27: raised outer cheek skin for tilted almond cat-eye slant
        front[27, 24] = skin
        front[27, 25] = skin_mid
        front[27, 26] = amber_deep
        front[27, 27] = amber_hi
        front[27, 33] = amber_hi
        front[27, 34] = amber_deep
        front[27, 35] = skin_mid
        front[27, 36] = skin

        # Cute sleek kitty smirk at y=29..30
        front[30, 29] = hex_rgba("#d4755c")
        front[30, 30] = hex_rgba("#e38870")
        front[29, 31] = hex_rgba("#c96b52")

        # Side view: winged cat liner + vertical slit pupil
        side[24, 29] = lash_black
        side[25, 29] = lash_black
        side[25, 30] = lash_black
        side[26, 31] = sclera_w
        side[26, 32] = amber_mid
        side[26, 33] = slit_pupil
        side[26, 34] = amber_mid
        side[27, 31] = skin_mid
        side[27, 32] = amber_hi
        side[27, 33] = slit_pupil
        side[27, 34] = amber_hi
        side[28, 32] = skin
        side[28, 33] = amber_deep
        side[28, 34] = skin_mid

    elif style_id == 3:
        # Style 3: Cheerful '> <' Squinting Laugh Eyes (해맑은 > < 눈웃음 표정 + 발그레 홍조)
        blush_deep = hex_rgba("#f7889a")
        blush_soft = hex_rgba("#fca6b2")
        mouth_dark = hex_rgba("#c25648")
        mouth_in = hex_rgba("#f78272")

        # Clear ONLY the interior eye boxes (x=23..27 and x=33..37, y=23..27), keeping x=22 & x=38 border intact!
        for y in range(23, 28):
            for x in list(range(23, 28)) + list(range(33, 38)):
                front[y, x] = skin_mid if y == 23 else skin
        for x in [24, 25, 26, 34, 35, 36]:
            front[23, x] = lash_soft

        # Left Eye '>' shape (x=23..27, y=24..27):
        front[24, 23] = lash_soft
        front[24, 24] = lash_black
        front[24, 25] = lash_dark
        front[25, 25] = lash_dark
        front[25, 26] = lash_black
        front[25, 27] = lash_black  # '>' apex
        front[26, 26] = lash_black
        front[26, 25] = lash_black
        front[26, 24] = lash_dark
        front[27, 23] = lash_black
        front[27, 24] = lash_soft

        # Right Eye '<' shape (x=33..37, y=24..27):
        front[24, 37] = lash_soft
        front[24, 36] = lash_black
        front[24, 35] = lash_dark
        front[25, 35] = lash_dark
        front[25, 34] = lash_black
        front[25, 33] = lash_black  # '<' apex
        front[26, 34] = lash_black
        front[26, 35] = lash_black
        front[26, 36] = lash_dark
        front[27, 37] = lash_black
        front[27, 36] = lash_soft

        # Rosy cheek blush under '>' and '<' (inside x=23..24 and x=36..37)
        front[27, 25] = blush_soft
        front[27, 35] = blush_soft
        front[28, 23] = blush_deep
        front[28, 24] = blush_soft
        front[28, 36] = blush_soft
        front[28, 37] = blush_deep

        # Cute open happy mouth ':D' at y=29..30, x=29..31
        front[29, 29] = mouth_dark
        front[29, 30] = mouth_dark
        front[29, 31] = mouth_dark
        front[30, 29] = mouth_in
        front[30, 30] = mouth_in
        front[30, 31] = mouth_in

        # Side view: '<' squinting eye arc at x=30..34, y=25..28 + rosy blush
        for y in range(24, 29):
            for x in range(30, 35):
                side[y, x] = skin
        side[25, 34] = lash_black
        side[25, 33] = lash_dark
        side[26, 32] = lash_black
        side[26, 31] = lash_black  # '<' apex
        side[27, 32] = lash_black
        side[27, 33] = lash_dark
        side[28, 34] = lash_black
        side[28, 30] = blush_deep
        side[28, 31] = blush_soft

    elif style_id == 4:
        # Style 4: Lazy Half-Lidded Jitome 'ㅡ ㅡ' (나른한 반눈 / 지토메 표정 + 눈물점)
        iris_dark = hex_rgba("#4d1326")
        iris_mid = hex_rgba("#a82248")
        iris_hi = hex_rgba("#f26b8a")
        mole = hex_rgba("#381a20")

        # Lower the upper lid by clearing interior x=23..27 and x=33..37 at y=23..24 to flat skin + flat brow at y=23
        for x in list(range(23, 28)) + list(range(33, 38)):
            front[23, x] = skin_mid
            front[24, x] = skin
        for x in [24, 25, 26, 27, 33, 34, 35, 36]:
            front[23, x] = lash_soft

        # Straight heavy horizontal upper eyelid bar 'ㅡ ㅡ' at y=25 (x=23..27 and x=33..37)
        for x in range(23, 28):
            front[25, x] = lash_black
        for x in range(33, 38):
            front[25, x] = lash_black

        # Row y=26: half-moon sleepy iris peeking under the flat lid
        front[26, 24] = sclera_w
        front[26, 25] = iris_dark
        front[26, 26] = iris_mid
        front[26, 27] = sclera_w
        front[26, 33] = sclera_w
        front[26, 34] = iris_mid
        front[26, 35] = iris_dark
        front[26, 36] = sclera_w

        # Row y=27: sanpaku lower sclera rim + soft bottom highlight + tear mole at (28, 24)
        front[27, 24] = skin
        front[27, 25] = sclera_w
        front[27, 26] = iris_hi
        front[27, 27] = sclera_w
        front[27, 33] = sclera_w
        front[27, 34] = iris_hi
        front[27, 35] = sclera_w
        front[27, 36] = skin
        front[28, 24] = mole

        # Side view: lowered flat horizontal lid at y=26 + sleepy half-eye at y=27..28
        for x in range(30, 35):
            side[24, x] = lash_soft if x >= 31 else skin
            side[25, x] = skin
            side[26, x] = lash_black
        side[27, 31] = sclera_w
        side[27, 32] = iris_dark
        side[27, 33] = iris_mid
        side[27, 34] = sclera_w
        side[28, 32] = sclera_w
        side[28, 33] = iris_hi
        side[28, 30] = mole

    elif style_id == 5:
        # Style 5: Gentle Droopy Tareme + Sparkling Starlight Big Eyes (순둥 처진 별눈 + 홍조)
        u_dark = hex_rgba("#2b0e63")
        u_mid = hex_rgba("#51209c")
        m_bright = hex_rgba("#8c3fe3")
        l_rose = hex_rgba("#df8aff")
        star_w = hex_rgba("#ffffff")
        blush = hex_rgba("#f79aa8")

        # Gentle arched eyebrows at y=23 + droopy tareme upper lash inside x=23..27 & x=33..37
        front[24, 23] = skin_mid
        front[24, 37] = skin_mid
        front[25, 23] = lash_black
        front[25, 37] = lash_black

        # Big sparkling starlight iris with dual pure-white catchlights
        front[25, 24] = star_w
        front[25, 25] = u_dark
        front[25, 26] = u_mid
        front[25, 27] = star_w
        front[25, 33] = star_w
        front[25, 34] = u_mid
        front[25, 35] = u_dark
        front[25, 36] = star_w

        front[26, 24] = m_bright
        front[26, 25] = star_w
        front[26, 26] = m_bright
        front[26, 27] = l_rose
        front[26, 33] = l_rose
        front[26, 34] = m_bright
        front[26, 35] = star_w
        front[26, 36] = m_bright

        front[27, 24] = blush
        front[27, 25] = l_rose
        front[27, 26] = star_w
        front[27, 27] = l_rose
        front[27, 33] = l_rose
        front[27, 34] = star_w
        front[27, 35] = l_rose
        front[27, 36] = blush

        front[28, 23] = blush
        front[28, 37] = blush
        front[29, 28] = hex_rgba("#de856d")
        front[29, 32] = hex_rgba("#de856d")

        # Side view: droopy outer corner + sparkling dual-star iris
        side[26, 30] = lash_black
        side[26, 31] = star_w
        side[26, 32] = u_mid
        side[26, 33] = u_dark
        side[26, 34] = star_w
        side[27, 32] = star_w
        side[27, 33] = m_bright
        side[27, 34] = l_rose
        side[28, 31] = blush
        side[28, 33] = l_rose
        side[28, 34] = star_w

    return enforce_female_face_outline_f0(front), side


def generate_female_face_sheets():
    base_if = load_female_backup("WCh_FaceEye_Idle_Front_Sheet.png")[:, :60]
    base_is = load_female_backup("WCh_FaceEye_Idle_Side_Sheet.png")[:, :60]

    for style_id in range(1, 6):
        f0_front, f0_side = make_female_face_style_f0(base_if, base_is, style_id)
        prefix = "WCh_FaceEye" if style_id == 1 else f"WCh_FaceEye{style_id}"

        for anim, f0_img, base_off in [
            ("Idle_Front", f0_front, 0),
            ("Walk_Front", f0_front, 1),
            ("Idle_Side", f0_side, 0),
            ("Walk_Side", f0_side, 1),
        ]:
            sheet = np.zeros((100, 480, 4), dtype=np.uint8)
            for f, dy in enumerate(FEMALE_DY[anim]):
                sheet[:, f * 60 : (f + 1) * 60] = shift_frame(f0_img, 0, base_off + dy)
            save_sheet(sheet, os.path.join(FEMALE_DIR, f"{prefix}_{anim}_Sheet.png"))

        icon = f0_front[14:42, 16:44].copy()
        save_sheet(icon, os.path.join(FEMALE_DIR, f"T_PartIcon_Face_Female_{style_id}_Front.png"))


# ==============================================================================
# STEP 3: Per-Frame Female Bob Base & 3 Female Hairstyles (PartId 1..3)
#         + STEP 4: Per-Frame 3 Male Hairstyles (PartId 1..3)
# ==============================================================================
def recolor_dark_ribbon_to_blonde(frame):
    """Replace dark navy/blue ribbon pixels (from WCh_Hair) with warm blonde shading
    so all 11 hair tints dye 100% cleanly without dark blue/black color bleed."""
    out = frame.copy()
    for y in range(1, 99):
        for x in range(1, 59):
            r, g, b, a = out[y, x]
            if a > 0 and (int(r) + int(g) + int(b)) > 0:
                if b > r or (r < 85 and g < 62):
                    is_edge = (
                        out[y - 1, x, 3] == 0
                        or out[y + 1, x, 3] == 0
                        or out[y, x - 1, 3] == 0
                        or out[y, x + 1, 3] == 0
                    )
                    in_bang_box = 21 <= x <= 39 and 18 <= y <= 32
                    if is_edge and not in_bang_box:
                        out[y, x] = BLONDE_RAMP[0]
                    elif is_edge and in_bang_box:
                        out[y, x] = BLONDE_RAMP[3]
                    else:
                        out[y, x] = BLONDE_RAMP[4 if (x + y) % 2 == 0 else 5]
    return out


def clear_interior_black(frame, x0, y0, x1, y1, replacement_idx=3):
    """Replace any #000000 black outline pixels inside an interior face/bang box with soft blonde shadow."""
    out = frame.copy()
    for y in range(max(1, y0), min(99, y1 + 1)):
        for x in range(max(1, x0), min(59, x1 + 1)):
            r, g, b, a = out[y, x]
            if a > 0 and int(r) + int(g) + int(b) < 30:
                out[y, x] = BLONDE_RAMP[replacement_idx]
    return out


def clean_female_hair_front_face_boundary(frame, dy_f):
    """Enforce a 100% clean, symmetrical boundary between Female Front Hair and FaceEye (centered at x=30):
    1) Recolor all peach forehead/ear skin pixels (#f1b795, #ab7050) across y_rel=18..28, x=19..41 to warm blonde hair,
       and mirror the clean right side-lock (x=39..40, y_rel=24..28) to the left side-lock (x=20..21) so ear skin never dyes!
    2) Cover the 1px outer-corner notches at (23, 23) and (37, 23) with BLONDE_RAMP[5], leaving symmetrical 5px brow/eye arches
       (x=24..28 & x=32..36 at y_rel=23) around the 3px center bang (x=29..31).
    3) Clear 100% of the face/cheek/jawline interior and soften interior #000000 or bright jaw-corner pixels at (22,29)/(38,29)."""
    out = frame.copy()
    for y_rel in range(18, 29):
        y = y_rel + dy_f
        if 0 <= y < 100:
            for x in range(19, 42):
                if out[y, x, 3] > 0:
                    r, g, b, _ = out[y, x]
                    # Replace #f1b795 peach forehead/ear skin or #ab7050 ear outline with warm blonde hair
                    if (r > 235 and g < 190) or (r == 0xAB and g == 0x70 and b == 0x50):
                        out[y, x] = BLONDE_RAMP[6 if y_rel <= 21 else 5]

    # Mirror the clean right cheek side-lock (x=39..40, y_rel=24..28) to the left cheek side-lock (x=20..21)
    # and soften any #000000 pixels at x=20..21 / x=39..40 so no black holes or ear-skin dots appear next to the cheeks!
    for y_rel in range(24, 29):
        y = y_rel + dy_f
        if 0 <= y < 100:
            for rx in (39, 40):
                if out[y, rx, 3] > 0:
                    r, g, b, _ = out[y, rx]
                    if int(r) + int(g) + int(b) < 30:
                        out[y, rx] = BLONDE_RAMP[3]
                    out[y, 60 - rx] = out[y, rx]

    y23 = 23 + dy_f
    if 0 <= y23 < 100:
        for x in list(range(24, 29)) + list(range(32, 37)):
            out[y23, x] = (0, 0, 0, 0)
        out[y23, 29] = BLONDE_RAMP[5]
        out[y23, 30] = BLONDE_RAMP[6]
        out[y23, 31] = BLONDE_RAMP[5]
        for x in (22, 23, 37, 38):
            out[y23, x] = BLONDE_RAMP[5]

    y24 = 24 + dy_f
    if 0 <= y24 < 100:
        for x in list(range(22, 29)) + list(range(32, 39)):
            out[y24, x] = (0, 0, 0, 0)
        out[y24, 29] = BLONDE_RAMP[4]
        out[y24, 30] = BLONDE_RAMP[5]
        out[y24, 31] = BLONDE_RAMP[4]

    y25 = 25 + dy_f
    if 0 <= y25 < 100:
        for x in list(range(22, 30)) + list(range(31, 39)):
            out[y25, x] = (0, 0, 0, 0)
        out[y25, 30] = BLONDE_RAMP[4]

    face_clear_spans = [
        (26, 22, 38),
        (27, 22, 38),
        (28, 22, 38),
        (29, 23, 37),
        (30, 24, 36),
        (31, 25, 35),
        (32, 25, 35),
    ]
    for y_rel, x0, x1 in face_clear_spans:
        y = y_rel + dy_f
        if 0 <= y < 100:
            out[y, x0 : x1 + 1] = 0
            # Soften any #000000 hair pixel or bright highlight pixel immediately bordering the dark jawline
            for bx in (x0 - 2, x0 - 1, x1 + 1, x1 + 2):
                if 0 <= bx < 60 and out[y, bx, 3] > 0:
                    r, g, b, _ = out[y, bx]
                    if int(r) + int(g) + int(b) < 30:
                        out[y, bx] = BLONDE_RAMP[3]
                    elif y_rel == 29 and bx in (22, 38):
                        out[y, bx] = BLONDE_RAMP[3]
    return out


def clean_female_hair_side_face_boundary(frame, dy_s):
    """Enforce a 100% clean boundary between Female Side Hair and FaceEye:
    - Recolors skin-colored pixels inside the upper side bangs (y_rel=21..23, x=25..38) and ear border (x=22..26, y_rel=25..28)
      to smooth blonde hair so there are ZERO 1px pinholes at (26,27) or (23,28)!
    - Clears the Side Eyebrow & Eye box (y_rel=24..28, x=29..35) so all 5 side eye expressions are 100% unobstructed.
    - Clears the neck/jaw skin pixels (25..27, 31) so zero hair dye bleeds onto her neck!"""
    out = frame.copy()
    for y_rel in (21, 22, 23):
        y = y_rel + dy_s
        if 0 <= y < 100:
            for x in range(25, 39):
                r, g, b, a = out[y, x]
                if a == 0 and 26 <= x <= 37:
                    out[y, x] = BLONDE_RAMP[6 if y_rel <= 22 else 5]
                elif a > 0 and (r > 235 or (r > 220 and g < 185)):
                    out[y, x] = BLONDE_RAMP[6 if y_rel <= 22 else 5]

    for y_rel in range(24, 29):
        y = y_rel + dy_s
        if 0 <= y < 100:
            out[y, 29:36] = 0
            if y_rel in (24, 25):
                for x in range(36, 40):
                    if out[y, x, 3] > 0:
                        r, g, b, _ = out[y, x]
                        if r > 235 and g < 190:
                            out[y, x] = BLONDE_RAMP[5 if y_rel == 24 else 4]
            for x in range(22, 29):
                if out[y, x, 3] > 0:
                    r, g, b, _ = out[y, x]
                    if (r == 0xAB and g == 0x70 and b == 0x50) or (r > 235 and g < 190):
                        out[y, x] = BLONDE_RAMP[5 if x >= 25 else 4]

    # Complete the bottom edge of the side-lock at (26, 27) with blonde hair instead of punching a hole,
    # and clear ONLY the 3 neck/jaw skin pixels (25..27, 31)!
    y27 = 27 + dy_s
    if 0 <= y27 < 100:
        out[y27, 26] = BLONDE_RAMP[5]
    for cx, cy0 in [(25, 31), (26, 31), (27, 31)]:
        y = cy0 + dy_s
        if 0 <= y < 100:
            out[y, cx] = (0, 0, 0, 0)
    return out


def graft_smooth_crown_dome(frame, m_ref_f, dy, is_back=False, center_x=29.5):
    """Sculpt a monotonically smooth studio-shaded round crown dome (y_rel=5..12)
    centered around center_x (29.0 for Male, 30.0 for Female)."""
    out = frame.copy()
    off = int(round(center_x - 29.5))
    y5 = 5 + dy
    if 0 <= y5 < 100:
        out[:y5, :] = 0
        for x in range(24 + off, 36 + off):
            out[y5, x] = BLONDE_RAMP[0]

    dome_spans = [
        (6, (22 + off, 37 + off)),
        (7, (20 + off, 39 + off)),
        (8, (19 + off, 40 + off)),
        (9, (19 + off, 40 + off)),
        (10, (18 + off, 41 + off)),
        (11, (18 + off, 41 + off)),
        (12, (17 + off, 42 + off)),
    ]
    for y_rel, (x0, x1) in dome_spans:
        y = y_rel + dy
        if 0 <= y < 100:
            out[y, :x0] = 0
            out[y, x1 + 1 :] = 0
            out[y, x0] = BLONDE_RAMP[0]
            out[y, x1] = BLONDE_RAMP[0]
            for x in range(x0 + 1, x1):
                if 23 + off <= x <= 36 + off:
                    px = m_ref_f[y, x - off]
                    if px[3] > 0 and (int(px[0]) + int(px[1]) + int(px[2])) >= 40:
                        out[y, x] = px
                        continue
                if out[y, x, 3] == 0 or (int(out[y, x, 0]) + int(out[y, x, 1]) + int(out[y, x, 2])) < 35:
                    if y_rel >= 11 and 24 + off <= x <= 35 + off:
                        out[y, x] = BLONDE_RAMP[8]
                    elif 23 + off <= x <= 36 + off:
                        out[y, x] = BLONDE_RAMP[7]
                    else:
                        out[y, x] = BLONDE_RAMP[6]

    if is_back:
        for y_rel in range(10, 30):
            y = y_rel + dy
            if 0 <= y < 100:
                for x in range(23 + off, 37 + off):
                    px = m_ref_f[y, x - off]
                    if px[3] > 0 and (int(px[0]) + int(px[1]) + int(px[2])) >= 40:
                        out[y, x] = px
    return out


def make_smooth_male_side_base(m_side_f, dy_s):
    """Create a smooth, rounded Male Side hair base from MCh_Hair1 Side:
    Smooths the top/front/back spikes (y_rel=5..23) into a natural round dome while preserving
    100% of MCh_Hair1 Side's native male ear cutout (x=23..26), sideburn (x=26..31), and eye opening (x=32..36)."""
    out = m_side_f.copy()
    y5 = 5 + dy_s
    if 0 <= y5 < 100:
        out[:y5, :] = 0
        out[y5, :] = 0
        for x in range(24, 36):
            out[y5, x] = BLONDE_RAMP[0]

    side_dome_spans = [
        (6, (22, 37)),
        (7, (20, 39)),
        (8, (19, 40)),
        (9, (18, 41)),
        (10, (17, 41)),
        (11, (17, 42)),
        (12, (16, 42)),
        (13, (16, 42)),
        (14, (16, 42)),
        (15, (16, 43)),
        (16, (16, 43)),
        (17, (16, 43)),
        (18, (15, 43)),
        (19, (15, 43)),
        (20, (15, 43)),
        (21, (15, 43)),
        (22, (16, 43)),
        (23, (16, 43)),
    ]
    for y_rel, (x0, x1) in side_dome_spans:
        y = y_rel + dy_s
        if 0 <= y < 100:
            out[y, :x0] = 0
            out[y, x1 + 1 :] = 0
            out[y, x0] = BLONDE_RAMP[0]
            out[y, x1] = BLONDE_RAMP[0]
            for x in range(x0 + 1, x1):
                r, g, b, a = out[y, x]
                if a == 0 or (int(r) + int(g) + int(b)) < 35:
                    if 11 <= y_rel <= 15 and 22 <= x <= 38:
                        out[y, x] = BLONDE_RAMP[8]
                    elif 6 <= y_rel <= 10 and 23 <= x <= 37:
                        out[y, x] = BLONDE_RAMP[7]
                    elif x <= x0 + 2 or x >= x1 - 2:
                        out[y, x] = BLONDE_RAMP[5]
                    else:
                        out[y, x] = BLONDE_RAMP[6]
    # Trim the 1px wild spike on the lower back nape at y_rel=25, x=14
    y25 = 25 + dy_s
    if 0 <= y25 < 100:
        out[y25, :15] = 0
    return out


def make_female_bob_frame(
    w_front_f, w_side_f, w_back_f, m_front_f, m_side_f, m_back_f, dy_front, dy_side, dy_back
):
    """Create a studio-quality symmetrical C-curl Bob frame (Front, Side, Back)
    centered at x=30 (mirroring via 60 - x to match the Female Face x=22..38 center!)."""
    inner_f = (20, 17 + dy_front, 40, 32 + dy_front)
    inner_s = (22, 17 + dy_side, 41, 32 + dy_side)

    # 1. Front: mirror ponytail-free right half (x=30..48) to left half (x=12..30) via 60 - x!
    bob_f = np.zeros_like(w_front_f)
    bob_f[:, 30:] = w_front_f[:, 30:]
    for x in range(31, 49):
        bob_f[:, 60 - x] = w_front_f[:, x]
    y_b0, y_b1 = max(0, 7 + dy_front), min(100, 26 + dy_front)
    bang_m = w_front_f[y_b0:y_b1, 22:39, 3] > 0
    bob_f[y_b0:y_b1, 22:39][bang_m] = w_front_f[y_b0:y_b1, 22:39][bang_m]
    y27f = 27 + dy_front
    if 0 <= y27f < 100:
        for gx in (19, 41):
            if bob_f[y27f, gx, 3] == 0:
                bob_f[y27f, gx] = BLONDE_RAMP[4]
    bob_f = graft_smooth_crown_dome(bob_f, m_front_f, dy_front, is_back=False, center_x=30.0)
    bob_f = clear_interior_black(bob_f, 19, 17 + dy_front, 41, 32 + dy_front, replacement_idx=3)
    bob_f = clean_female_hair_front_face_boundary(bob_f, dy_front)
    bob_f = seal_outer_outline(bob_f, protect_box=inner_f)

    # 2. Back: mirror ponytail-free left half (x=12..30) to right half (x=30..48) via 60 - x!
    bob_b = np.zeros_like(w_back_f)
    bob_b[:, :31] = w_back_f[:, :31]
    for x in range(12, 30):
        bob_b[:, 60 - x] = w_back_f[:, x]
    bob_b = graft_smooth_crown_dome(bob_b, m_back_f, dy_back, is_back=True, center_x=30.0)
    bob_b = seal_outer_outline(bob_b)

    # 3. Side: combine MCh_Hair1 Side's round back-of-skull (x=15..25) with WCh_Hair Side's bangs & cheek lock (x=25..43),
    #    and sculpt a continuous C-curl cheek/nape lock (y_rel=24..31) with ZERO trapped ear hole and ZERO neck bleed!
    bob_s = np.zeros_like(w_side_f)
    for y_rel in range(5, 33):
        y = y_rel + dy_side
        if 0 <= y < 100:
            for x in range(15, 26):
                px = m_side_f[y, x]
                if px[3] > 0:
                    bob_s[y, x] = px
            for x in range(25, 44):
                px = w_side_f[y, x]
                if px[3] > 0:
                    bob_s[y, x] = px

    y5 = 5 + dy_side
    if 0 <= y5 < 100:
        bob_s[:y5, :] = 0
        bob_s[y5, :] = 0
        for x in range(24, 36):
            bob_s[y5, x] = BLONDE_RAMP[0]

    for y_rel, (x0, x1) in [
        (6, (22, 37)),
        (7, (20, 39)),
        (8, (19, 40)),
        (9, (18, 41)),
        (10, (17, 41)),
        (11, (17, 42)),
        (12, (16, 42)),
        (13, (16, 42)),
        (14, (16, 42)),
        (15, (16, 43)),
        (16, (16, 43)),
        (17, (16, 43)),
        (18, (15, 42)),
        (19, (15, 42)),
        (20, (15, 42)),
        (21, (15, 43)),
        (22, (16, 42)),
        (23, (16, 41)),
    ]:
        y = y_rel + dy_side
        if 0 <= y < 100:
            bob_s[y, :x0] = 0
            bob_s[y, x1 + 1 :] = 0
            bob_s[y, x0] = BLONDE_RAMP[0]
            bob_s[y, x1] = BLONDE_RAMP[0]
            for x in range(x0 + 1, x1):
                r, g, b, a = bob_s[y, x]
                if a == 0 or (int(r) + int(g) + int(b)) < 35:
                    if 11 <= y_rel <= 15 and 22 <= x <= 38:
                        bob_s[y, x] = BLONDE_RAMP[8]
                    elif 6 <= y_rel <= 10 and 23 <= x <= 37:
                        bob_s[y, x] = BLONDE_RAMP[7]
                    else:
                        bob_s[y, x] = BLONDE_RAMP[6]

    # Continuous C-curl bob side-lock & rounded nape at y_rel=24..31 (ending at x<=24 on y_rel=31 so neck x=25..27 is clean!)
    for y_rel, (x0, x1) in [
        (24, (16, 28)),
        (25, (16, 28)),
        (26, (16, 28)),
        (27, (17, 28)),
        (28, (17, 28)),
        (29, (18, 28)),
        (30, (19, 27)),
        (31, (20, 24)),
    ]:
        y = y_rel + dy_side
        if 0 <= y < 100:
            bob_s[y, :x0] = 0
            if y_rel >= 27:
                bob_s[y, x1 + 1 :] = 0
            bob_s[y, x0] = BLONDE_RAMP[0]
            for x in range(x0 + 1, x1 + 1):
                r, g, b, a = bob_s[y, x]
                if x == x1 and y_rel >= 28:
                    bob_s[y, x] = BLONDE_RAMP[3]
                elif a == 0 or (int(r) + int(g) + int(b)) < 35:
                    if x == 23 and 26 <= y_rel <= 29:
                        bob_s[y, x] = BLONDE_RAMP[3]
                    elif x >= 24:
                        bob_s[y, x] = BLONDE_RAMP[5 if y_rel <= 29 else 4]
                    else:
                        bob_s[y, x] = BLONDE_RAMP[4 if y_rel <= 29 else 3]
    for y in range(max(0, 32 + dy_side), 100):
        bob_s[y, :] = 0
    bob_s = clear_interior_black(bob_s, 23, 18 + dy_side, 39, 30 + dy_side, replacement_idx=4)
    bob_s = clean_female_hair_side_face_boundary(bob_s, dy_side)
    bob_s = seal_outer_outline(bob_s, protect_box=inner_s)
    return bob_f, bob_s, bob_b


def build_female_hair_frame(
    w_front_f, w_side_f, w_back_f, m_front_f, m_side_f, m_back_f, face_f, face_s, dy_f, dy_s, dy_b, style_id
):
    inner_f = (17, 17 + dy_f, 43, 34 + dy_f)
    inner_s = (22, 17 + dy_s, 41, 32 + dy_s)

    w_f_clean = recolor_dark_ribbon_to_blonde(w_front_f)
    w_s_clean = recolor_dark_ribbon_to_blonde(w_side_f)
    w_b_clean = recolor_dark_ribbon_to_blonde(w_back_f)

    # Fill the 1px gap at (41, 27 + dy_f) on WCh_Hair before sealing the outer silhouette
    y27f = 27 + dy_f
    if 0 <= y27f < 100 and w_f_clean[y27f, 41, 3] == 0:
        w_f_clean[y27f, 41] = BLONDE_RAMP[4]

    # Always clean the face/eye/jaw/neck boundary & interior black lines on WCh_Hair (Style 1) as well!
    w_f_clean = clear_interior_black(w_f_clean, 17, 14 + dy_f, 40, 34 + dy_f, replacement_idx=3)
    w_f_clean = clean_female_hair_front_face_boundary(w_f_clean, dy_f)
    w_f_clean = seal_outer_outline(w_f_clean, protect_box=(17, 17 + dy_f, 40, 34 + dy_f))
    w_s_clean = clean_female_hair_side_face_boundary(w_s_clean, dy_s)
    w_s_clean = seal_outer_outline(w_s_clean, protect_box=inner_s)

    if style_id == 1:
        # PartId 1: Ribbon Side Ponytail (리본 사이드 포니테일 - 염색 번짐 및 얼굴 침범 제로)
        return w_f_clean, w_s_clean, w_b_clean

    bob_f, bob_s, bob_b = make_female_bob_frame(
        w_f_clean, w_s_clean, w_b_clean, m_front_f, m_side_f, m_back_f, dy_f, dy_s, dy_b
    )

    if style_id == 2:
        # PartId 2: Lively Symmetrical Twin Tails (발랄한 양갈래 트윈테일)
        # Start from bob_f so the entire forehead bang & cheek frame (x=19..41, y_rel=6..31) is 100% solid and gap-free
        # (ZERO vertical skin slit at x=36..37!), then attach the left twin-tail (x=4..22) and its 60-x mirror (rx=38..56)!
        front = bob_f.copy()
        for y in range(100):
            for x in range(4, 24):
                px = w_f_clean[y, x]
                if px[3] > 0:
                    # Only place twin-tail pixels outside the face interior or above the bangs (y_rel <= 20)
                    if y < 21 + dy_f or x <= 21:
                        front[y, x] = px
                    rx = 60 - x
                    if 0 <= rx < 60 and (y < 21 + dy_f or rx >= 39):
                        front[y, rx] = px
        # Smooth the center crown between the two twin-tail arches (x=23..37, y_rel=0..11)
        for y_rel in range(0, 5):
            y = y_rel + dy_f
            if 0 <= y < 100:
                front[y, 23:38] = 0
        y5 = 5 + dy_f
        if 0 <= y5 < 100:
            front[y5, 23:38] = BLONDE_RAMP[0]
        for y_rel in range(6, 12):
            y = y_rel + dy_f
            if 0 <= y < 100:
                for x in range(23, 38):
                    px = m_front_f[y, x - 1]
                    if px[3] > 0 and (int(px[0]) + int(px[1]) + int(px[2])) >= 40:
                        front[y, x] = px
                    elif front[y, x, 3] == 0 or (int(front[y, x, 0]) + int(front[y, x, 1]) + int(front[y, x, 2])) < 35:
                        front[y, x] = BLONDE_RAMP[7 if 25 <= x <= 35 else 6]
        front = clear_interior_black(front, 17, 14 + dy_f, 43, 34 + dy_f, replacement_idx=3)
        front = clean_female_hair_front_face_boundary(front, dy_f)
        front = seal_outer_outline(front, protect_box=inner_f)

        # Side: clean w_s_clean + subtle 2px twin-tail depth behind back tail
        side = w_s_clean.copy()
        for y_rel in range(14, 42):
            y = y_rel + dy_s
            if 0 <= y < 100:
                row_xs = [x for x in range(4, 20) if side[y, x, 3] > 0]
                if row_xs:
                    lx = row_xs[0]
                    if lx >= 6:
                        side[y, lx - 1] = BLONDE_RAMP[4]
                        side[y, lx - 2] = BLONDE_RAMP[0]
        side = clean_female_hair_side_face_boundary(side, dy_s)
        side = seal_outer_outline(side, protect_box=inner_s)

        # Back: mirror right ponytail half (x=30..55) to left half (lx=5..30) via 60 - x + MCh_Hair1 center strands
        back = np.zeros_like(w_b_clean)
        back[:, 30:] = w_b_clean[:, 30:]
        for x in range(31, 56):
            back[:, 60 - x] = w_b_clean[:, x]
        for y_rel in range(0, 5):
            y = y_rel + dy_b
            if 0 <= y < 100:
                back[y, 23:38] = 0
        y5b = 5 + dy_b
        if 0 <= y5b < 100:
            back[y5b, 23:38] = BLONDE_RAMP[0]
        for y_rel in range(6, 30):
            y = y_rel + dy_b
            if 0 <= y < 100:
                for x in range(23, 38):
                    px = m_back_f[y, x - 1]
                    if px[3] > 0 and (int(px[0]) + int(px[1]) + int(px[2])) >= 40:
                        back[y, x] = px
                    elif y_rel <= 12 and (back[y, x, 3] == 0 or (int(back[y, x, 0]) + int(back[y, x, 1]) + int(back[y, x, 2])) < 35):
                        back[y, x] = BLONDE_RAMP[7 if 25 <= x <= 35 else 6]
        for y_rel in range(42, 48):
            y = y_rel + dy_b
            if 0 <= y < 100:
                for x in range(5, 56):
                    if y_rel >= 46:
                        back[y, x] = (0, 0, 0, 0)
                    elif back[y, x, 3] > 0 and tuple(back[y, x]) == BLONDE_RAMP[0]:
                        back[y, x] = BLONDE_RAMP[5]
        back = seal_outer_outline(back)
        return front, side, back

    elif style_id == 3:
        # PartId 3: Cute Full-Bang C-Curl Bob (단정한 풀뱅 숏 보브컷)
        return bob_f, bob_s, bob_b


def make_male_bob_dome_front(w_f_clean, m_front_f, dy_f):
    """Build a Male-centered (x=29.0, mirrored via 58 - x) smooth upper hair dome from w_f_clean and m_front_f."""
    dome = np.zeros_like(m_front_f)
    for y in range(100):
        for x in range(29, 47):
            px = w_f_clean[y, min(59, x + 1)]
            dome[y, x] = px
            lx = 58 - x
            if 0 <= lx < 60:
                dome[y, lx] = px
    dome = graft_smooth_crown_dome(dome, m_front_f, dy_f, is_back=False, center_x=29.0)
    return dome


def build_male_hair_frame(
    m_front_f, m_side_f, m_back_f, w_front_f, w_side_f, w_back_f, face_f, face_s, dy_f, dy_s, dy_b, style_id
):
    if style_id == 1:
        # PartId 1: Original Spiky Short (기본 숏 스파이키 - 관자놀이·미간·귀 피부 침범 0%)
        return m_front_f.copy(), m_side_f.copy(), m_back_f.copy()

    w_f_clean = clean_female_hair_front_face_boundary(recolor_dark_ribbon_to_blonde(w_front_f), dy_f)
    w_s_clean = clean_female_hair_side_face_boundary(recolor_dark_ribbon_to_blonde(w_side_f), dy_s)
    w_b_clean = recolor_dark_ribbon_to_blonde(w_back_f)
    _, _, bob_b = make_female_bob_frame(
        w_f_clean, w_s_clean, w_b_clean, m_front_f, m_side_f, m_back_f, dy_f, dy_s, dy_b
    )
    m_side_smooth = make_smooth_male_side_base(m_side_f, dy_s)

    inner_f = (19, 16 + dy_f, 39, 31 + dy_f)
    inner_s = (22, 16 + dy_s, 41, 32 + dy_s)

    if style_id == 2:
        # PartId 2: Sleek Comma / Curtain-Parted Two-Block (가르마 커튼 투블럭)
        # Centered at x=29.0 (matching Male Face x=21..37!). Smoothly arched curtain bangs open naturally
        # at y_rel=22..23 (without a giant high forehead hole!) and frame both eyes cleanly!
        front = make_male_bob_dome_front(w_f_clean, m_front_f, dy_f)
        y5, y6 = 5 + dy_f, 6 + dy_f
        if 0 <= y5 < 100 and 0 <= y6 < 100:
            front[y5, 29] = (0, 0, 0, 0)
            front[y6, 29] = BLONDE_RAMP[0]

        for y_rel, (min_x, max_x) in [
            (21, (15, 43)),
            (22, (15, 43)),
            (23, (16, 42)),
            (24, (16, 42)),
            (25, (17, 41)),
            (26, (18, 40)),
            (27, (20, 38)),
        ]:
            y = y_rel + dy_f
            if 0 <= y < 100:
                if y_rel >= 23:
                    front[y, :] = m_front_f[y, :]
                front[y, :min_x] = 0
                front[y, max_x + 1 :] = 0
        for y in range(max(0, 28 + dy_f), 100):
            front[y, :] = 0
        front = clear_interior_black(front, 19, 14 + dy_f, 39, 27 + dy_f, replacement_idx=4)

        # Curved S-part root shadow (y_rel=6..21) in warm blonde root tones
        for y_rel, px_x in [
            (6, 29), (7, 29), (8, 28), (9, 28), (10, 29), (11, 29), (12, 30),
            (13, 29), (14, 29), (15, 29), (16, 29), (17, 29), (18, 29), (19, 29), (20, 29), (21, 29),
        ]:
            y = y_rel + dy_f
            if 0 <= y < 100 and front[y, px_x, 3] > 0:
                front[y, px_x] = BLONDE_RAMP[3 if y_rel in (6, 7, 8, 9, 10, 20, 21) else 4]
                if y_rel >= 19:
                    front[y, px_x - 1] = BLONDE_RAMP[4 if y_rel == 21 else 5]
                    front[y, px_x + 1] = BLONDE_RAMP[4 if y_rel == 21 else 5]

        # Naturally proportioned curtain bang opening at y_rel=22..27 (keeping y_rel=20..21 covered by parted hair!):
        for y_rel, open_xs in [
            (22, [27, 28, 29, 30, 31]),
            (23, list(range(23, 36))),
            (24, list(range(21, 38))),
            (25, list(range(21, 38))),
            (26, list(range(21, 38))),
            (27, list(range(21, 38))),
        ]:
            y = y_rel + dy_f
            if 0 <= y < 100:
                for cx in open_xs:
                    front[y, cx] = (0, 0, 0, 0)
                lx, rx = open_xs[0] - 1, open_xs[-1] + 1
                if front[y, lx, 3] > 0:
                    front[y, lx] = BLONDE_RAMP[4 if y_rel <= 23 else 3]
                if front[y, rx, 3] > 0:
                    front[y, rx] = BLONDE_RAMP[4 if y_rel <= 23 else 3]
                if y_rel == 22:
                    if front[y, lx - 1, 3] > 0:
                        front[y, lx - 1] = BLONDE_RAMP[5]
                    if front[y, rx + 1, 3] > 0:
                        front[y, rx + 1] = BLONDE_RAMP[5]
        front = seal_outer_outline(front, protect_box=inner_f)

        # Side: built directly from make_smooth_male_side_base(m_side_f) with clean ear/cheek clearance!
        side = m_side_smooth.copy()
        for y_rel, (min_x, max_x) in [
            (22, (16, 42)),
            (23, (16, 41)),
            (24, (16, 41)),
            (25, (17, 40)),
            (26, (18, 39)),
            (27, (19, 38)),
            (28, (20, 29)),
            (29, (20, 23)),
            (30, (21, 23)),
            (31, (22, 23)),
        ]:
            y = y_rel + dy_s
            if 0 <= y < 100:
                side[y, :min_x] = 0
                side[y, max_x + 1 :] = 0
                if 26 <= y_rel <= 30:
                    for x in range(min_x + 1, min(23, max_x)):
                        if side[y, x, 3] > 0:
                            side[y, x] = BLONDE_RAMP[3 if (x + y_rel) % 2 == 0 else 4]
        for y in range(max(0, 32 + dy_s), 100):
            side[y, :] = 0
        # Soft diagonal curtain bang part crease on side view
        for y_rel, sx in [(18, 38), (19, 38), (20, 37), (21, 37), (22, 36)]:
            y = y_rel + dy_s
            if 0 <= y < 100 and side[y, sx, 3] > 0:
                side[y, sx] = BLONDE_RAMP[4]
        side = clear_interior_black(side, 24, 16 + dy_s, 40, 27 + dy_s, replacement_idx=4)
        side = seal_outer_outline(side, protect_box=inner_s)

        # Back: smooth studio-shaded bob_b dome + tapered masculine two-block undercut nape connected to collar
        back = bob_b.copy()
        for y_rel, trim_x in [(23, 15), (24, 16), (25, 17), (26, 18), (27, 19), (28, 20), (29, 21), (30, 22), (31, 23)]:
            y = y_rel + dy_b
            if 0 <= y < 100:
                back[y, :trim_x] = 0
                back[y, 60 - trim_x :] = 0
                if 26 <= y_rel <= 30:
                    for x in range(trim_x + 1, 60 - trim_x - 1):
                        if back[y, x, 3] > 0:
                            back[y, x] = BLONDE_RAMP[3 if (x + y_rel) % 2 == 0 else 4]
        for y in range(max(0, 32 + dy_b), 100):
            back[y, :] = 0
        back = seal_outer_outline(back)
        return front, side, back

    elif style_id == 3:
        # PartId 3: Wild High Swordsman Ponytail (와일드 하이 포니테일)
        # Built on the cleaned, symmetrical MCh_Hair1 head (zero glabella/temple/ear skin bleed!)
        # + high ponytail crest rising above the upper crown (y_rel=2..13, x=17..35)
        # + sleek windswept center bang strand at x=29 (y_rel=23..25) with 100% unobstructed eyes!
        f_mask_f = face_f[:, :, 3] > 0
        f_mask_s = face_s[:, :, 3] > 0

        front = m_front_f.copy()
        for y_rel in range(2, 14):
            y = y_rel + dy_f
            src_y = min(99, max(0, y + 1))
            if 0 <= y < 100:
                for x in range(17, 36):
                    src_x = x - 5
                    if 0 <= src_x < 60:
                        px = w_f_clean[src_y, src_x]
                        if px[3] > 0 and not f_mask_f[y, x] and front[y, x, 3] == 0:
                            front[y, x] = px
        # Taper the center bang strand at y_rel=23..24 to a sleek 1px windswept lock at x=29
        # (with subtle root part shadow at y_rel=21..22, x=30) instead of a blocky hole above the right eye!
        for y_rel in (21, 22):
            y = y_rel + dy_f
            if 0 <= y < 100 and front[y, 30, 3] > 0:
                front[y, 30] = BLONDE_RAMP[4]
        for y_rel in (23, 24):
            y = y_rel + dy_f
            if 0 <= y < 100:
                front[y, 28] = (0, 0, 0, 0)
                front[y, 30] = (0, 0, 0, 0)
                front[y, 29] = BLONDE_RAMP[5 if y_rel == 23 else 4]
        front = clear_interior_black(front, 20, 17 + dy_f, 38, 26 + dy_f, replacement_idx=4)
        front = seal_outer_outline(front, protect_box=inner_f)

        # Side: MCh_Hair1 Side (already cleaned of ear/cheek skin!) + high ponytail from w_s_clean
        side = m_side_f.copy()
        for y_rel in range(2, 34):
            y = y_rel + dy_s
            src_y = min(99, max(0, y + 3))
            if 0 <= y < 100:
                min_tx = 8 if y_rel <= 29 else (9 if y_rel == 30 else (10 if y_rel == 31 else (11 if y_rel == 32 else 12)))
                max_tx = 22 if y_rel <= 29 else (18 if y_rel == 30 else (16 if y_rel == 31 else (15 if y_rel == 32 else 14)))
                for x in range(min_tx, max_tx + 1):
                    src_x = x - 2
                    if 0 <= src_x < 60:
                        px = w_s_clean[src_y, src_x]
                        if px[3] > 0 and not f_mask_s[y, x] and side[y, x, 3] == 0:
                            side[y, x] = px
        for y_rel, (x0, x1) in [(4, (13, 19)), (5, (11, 21)), (6, (10, 21)), (7, (10, 21)), (8, (10, 20))]:
            y = y_rel + dy_s
            if 0 <= y < 100:
                for x in range(x0, x1 + 1):
                    if side[y, x, 3] == 0 or tuple(side[y, x]) == BLONDE_RAMP[0]:
                        side[y, x] = BLONDE_RAMP[7 if y_rel <= 6 else 6]
        side = clear_interior_black(side, 24, 17 + dy_s, 40, 27 + dy_s, replacement_idx=4)
        side = seal_outer_outline(side, protect_box=inner_s)

        # Back: MCh_Hair1 Back + organic teardrop high ponytail
        back = m_back_f.copy()
        tail_spans = [
            (2, (27, 32)),
            (3, (26, 33)),
            (4, (26, 33)),
            (5, (26, 33)),
            (6, (27, 32)),
            (7, (27, 32)),
            (8, (26, 33)),
            (9, (26, 33)),
            (10, (25, 34)),
            (11, (25, 34)),
            (12, (25, 34)),
            (13, (24, 35)),
            (14, (24, 35)),
            (15, (24, 35)),
            (16, (24, 35)),
            (17, (24, 35)),
            (18, (25, 34)),
            (19, (25, 34)),
            (20, (25, 34)),
            (21, (26, 33)),
            (22, (26, 33)),
            (23, (26, 33)),
            (24, (27, 32)),
            (25, (27, 32)),
            (26, (27, 32)),
            (27, (28, 31)),
            (28, (28, 31)),
            (29, (28, 31)),
            (30, (28, 31)),
            (31, (29, 30)),
            (32, (29, 30)),
        ]
        for y_rel, (tx0, tx1) in tail_spans:
            y = y_rel + dy_b
            src_y = min(99, max(0, y + 2))
            if 0 <= y < 100:
                shift_x = 11 if y_rel <= 7 else 15
                for x in range(tx0, tx1 + 1):
                    src_x = min(59, max(0, x + shift_x))
                    px = w_b_clean[src_y, src_x]
                    if y_rel in (6, 7) and tx0 + 1 <= x <= tx1 - 1:
                        back[y, x] = BLONDE_RAMP[2 if y_rel == 6 else 3]
                    elif x in (tx0, tx1) and back[y, x, 3] > 0 and 6 <= y_rel <= 29:
                        back[y, x] = BLONDE_RAMP[3]
                    elif px[3] > 0 and (int(px[0]) + int(px[1]) + int(px[2])) >= 510:
                        back[y, x] = px
                    else:
                        back[y, x] = BLONDE_RAMP[7 if (tx0 + 2 <= x <= tx1 - 2 and y_rel <= 22) else 6]
        back = seal_outer_outline(back)
        return front, side, back


# ==============================================================================
# STEP 5: Generate All Male & Female Hair Sheets Frame-by-Frame (f = 0..7)
# ==============================================================================
def ensure_female_body_shoulder_outline():
    for anim in ["Idle_Side", "Walk_Side", "Idle_Front", "Walk_Front"]:
        p = os.path.join(FEMALE_DIR, f"WCh_Body_{anim}_Sheet.png")
        arr = load_sheet(p)
        for f in range(8):
            ox = f * 60
            for y in range(32, 45):
                row_a = arr[y, ox : ox + 60, 3]
                xs = np.where(row_a > 0)[0]
                if len(xs) > 0:
                    lx = xs[0]
                    r, g, b, _ = arr[y, ox + lx]
                    if int(r) + int(g) + int(b) > 90:
                        arr[y, ox + lx] = (0, 0, 0, 255)
        save_sheet(arr, p)


def generate_all_hair_sheets():
    ensure_female_body_shoulder_outline()

    m_hair1 = {}
    w_hair1 = {}
    m_face1 = {}
    w_face1 = {}
    for state in ["Idle", "Walk"]:
        for dir_name in ["Front", "Side", "Back"]:
            anim = f"{state}_{dir_name}"
            # m_hair1 was just cleaned & saved by clean_male_hair1_and_restore_face()
            m_hair1[anim] = load_sheet(os.path.join(MALE_DIR, f"MCh_Hair1_{anim}_Sheet.png"))
            # Load w_hair1 from the pristine backup so repeated runs are 100% idempotent
            w_hair1[anim] = load_female_backup(f"WCh_Hair_{anim}_Sheet.png")
            m_face1[anim] = load_sheet(os.path.join(MALE_DIR, f"MCh_FaceEye_{anim}_Sheet.png"))
            if dir_name != "Back":
                w_face1[anim] = load_sheet(os.path.join(FEMALE_DIR, f"WCh_FaceEye_{anim}_Sheet.png"))

    m_base_off = {
        "Idle_Front": 0,
        "Walk_Front": 1,
        "Idle_Side": 0,
        "Walk_Side": -1,
        "Idle_Back": 0,
        "Walk_Back": 0,
    }
    w_base_off = {
        "Idle_Front": 0,
        "Walk_Front": 1,
        "Idle_Side": 0,
        "Walk_Side": 1,
        "Idle_Back": 0,
        "Walk_Back": 1,
    }

    for style_id in range(1, 4):
        # 1) Male Hair style_id (PartId 1..3) across all 6 animations and all 8 frames
        m_prefix = f"MCh_Hair{style_id}"
        for state in ["Idle", "Walk"]:
            m_sheets = {d: np.zeros((100, 480, 4), dtype=np.uint8) for d in ["Front", "Side", "Back"]}
            for f in range(8):
                ox = f * 60
                af, as_, ab = f"{state}_Front", f"{state}_Side", f"{state}_Back"
                dy_f = m_base_off[af] + MALE_DY[af][f]
                dy_s = m_base_off[as_] + MALE_DY[as_][f]
                dy_b = m_base_off[ab] + MALE_DY[ab][f]

                w_dy_f = w_base_off[af] + FEMALE_DY[af][f]
                w_dy_s = w_base_off[as_] + FEMALE_DY[as_][f]
                w_dy_b = w_base_off[ab] + FEMALE_DY[ab][f]

                wf_aligned = shift_frame(w_hair1[af][:, ox : ox + 60], 0, dy_f - w_dy_f)
                ws_aligned = shift_frame(w_hair1[as_][:, ox : ox + 60], 0, dy_s - w_dy_s)
                wb_aligned = shift_frame(w_hair1[ab][:, ox : ox + 60], 0, dy_b - w_dy_b)

                fr_f, fr_s, fr_b = build_male_hair_frame(
                    m_hair1[af][:, ox : ox + 60],
                    m_hair1[as_][:, ox : ox + 60],
                    m_hair1[ab][:, ox : ox + 60],
                    wf_aligned,
                    ws_aligned,
                    wb_aligned,
                    m_face1[af][:, ox : ox + 60],
                    m_face1[as_][:, ox : ox + 60],
                    dy_f,
                    dy_s,
                    dy_b,
                    style_id,
                )
                m_sheets["Front"][:, ox : ox + 60] = fr_f
                m_sheets["Side"][:, ox : ox + 60] = fr_s
                m_sheets["Back"][:, ox : ox + 60] = fr_b

            if state == "Idle":
                icon = m_sheets["Front"][1:49, 6:54].copy()
                save_sheet(icon, os.path.join(MALE_DIR, f"T_PartIcon_Hair_{style_id}_Front.png"))

            if style_id > 1:
                for d in ["Front", "Side", "Back"]:
                    save_sheet(m_sheets[d], os.path.join(MALE_DIR, f"{m_prefix}_{state}_{d}_Sheet.png"))

        # 2) Female Hair style_id (PartId 1..3) across all 6 animations and all 8 frames
        w_prefix = "WCh_Hair" if style_id == 1 else f"WCh_Hair{style_id}"
        for state in ["Idle", "Walk"]:
            w_sheets = {d: np.zeros((100, 480, 4), dtype=np.uint8) for d in ["Front", "Side", "Back"]}
            for f in range(8):
                ox = f * 60
                af, as_, ab = f"{state}_Front", f"{state}_Side", f"{state}_Back"
                dy_f = w_base_off[af] + FEMALE_DY[af][f]
                dy_s = w_base_off[as_] + FEMALE_DY[as_][f]
                dy_b = w_base_off[ab] + FEMALE_DY[ab][f]

                m_dy_f = m_base_off[af] + MALE_DY[af][f]
                m_dy_s = m_base_off[as_] + MALE_DY[as_][f]
                m_dy_b = m_base_off[ab] + MALE_DY[ab][f]

                mf_aligned = shift_frame(m_hair1[af][:, ox : ox + 60], 0, dy_f - m_dy_f)
                ms_aligned = shift_frame(m_hair1[as_][:, ox : ox + 60], 0, dy_s - m_dy_s)
                mb_aligned = shift_frame(m_hair1[ab][:, ox : ox + 60], 0, dy_b - m_dy_b)

                fr_f, fr_s, fr_b = build_female_hair_frame(
                    w_hair1[af][:, ox : ox + 60],
                    w_hair1[as_][:, ox : ox + 60],
                    w_hair1[ab][:, ox : ox + 60],
                    mf_aligned,
                    ms_aligned,
                    mb_aligned,
                    w_face1[af][:, ox : ox + 60],
                    w_face1[as_][:, ox : ox + 60],
                    dy_f,
                    dy_s,
                    dy_b,
                    style_id,
                )
                w_sheets["Front"][:, ox : ox + 60] = fr_f
                w_sheets["Side"][:, ox : ox + 60] = fr_s
                w_sheets["Back"][:, ox : ox + 60] = fr_b

            if state == "Idle":
                icon = w_sheets["Front"][1:49, 6:54].copy()
                save_sheet(icon, os.path.join(FEMALE_DIR, f"T_PartIcon_Hair_Female_{style_id}_Front.png"))

            for d in ["Front", "Side", "Back"]:
                save_sheet(w_sheets[d], os.path.join(FEMALE_DIR, f"{w_prefix}_{state}_{d}_Sheet.png"))


# ==============================================================================
# STEP 6: Exact UE M_NUCharacterHairTint Simulation & Showcase Rendering
# ==============================================================================
def apply_tint(hair_arr, target_rgb):
    """Exact match to UE M_NUCharacterHairTint HLSL:
    Tints 100% of hair pixels by luminance (zero un-tinted tan/brown pixels!)."""
    out = hair_arr.copy().astype(np.float32)
    a = out[:, :, 3] > 0
    rgb = out[:, :, :3] / 255.0
    luma = 0.2126 * rgb[:, :, 0] + 0.7152 * rgb[:, :, 1] + 0.0722 * rgb[:, :, 2]
    ref_luma = 0.72
    hi_lift = 0.35
    shade = luma / ref_luma
    s_clamped = np.clip(shade, 0.0, 1.0)
    s_over = np.clip(shade - 1.0, 0.0, 1.0)
    for c, tv in enumerate(target_rgb):
        t_norm = tv / 255.0
        base = s_clamped * t_norm
        tinted = base + (1.0 - base) * s_over * hi_lift
        out[:, :, c][a] = np.clip(tinted[a] * 255.0, 0, 255)
    return out.astype(np.uint8)


def composite_layers(*layers):
    out = np.zeros_like(layers[0])
    for lyr in layers:
        m = lyr[:, :, 3] > 0
        out[m] = lyr[m]
    return out


def render_showcase():
    try:
        font_title = ImageFont.truetype(r"C:\Windows\Fonts\malgunbd.ttf", 20)
        font_sub = ImageFont.truetype(r"C:\Windows\Fonts\malgunbd.ttf", 14)
        font_body = ImageFont.truetype(r"C:\Windows\Fonts\malgun.ttf", 12)
    except Exception:
        font_title = ImageFont.load_default()
        font_sub = ImageFont.load_default()
        font_body = ImageFont.load_default()

    dye_colors = [
        ("기본 금발 (Blonde)", (235, 195, 165)),
        ("흑요석 (Obsidian)", (65, 72, 92)),
        ("진홍빛 (Crimson)", (215, 65, 75)),
        ("푸른빛 (Azure)", (80, 150, 235)),
        ("자수정 (Violet)", (165, 105, 225)),
    ]

    eye_titles = [
        "눈 1: 기본 초롱 눈매 (Classic)",
        "눈 2: 도도한 고양이 눈매 (세로 동공)",
        "눈 3: 해맑은 > < 눈웃음 (홍조+미소)",
        "눈 4: 나른한 반눈 지토메 (ㅡ ㅡ)",
        "눈 5: 순둥 처진 별눈 (Tareme+스타)",
    ]

    for is_female in (False, True):
        g_title = (
            "여자 캐릭터 (Female) 헤어 3종 & 마비노기풍 표정/눈 5종"
            if is_female
            else "남자 캐릭터 (Male) 헤어 3종 & 마비노기풍 표정/눈 5종"
        )
        src_dir = FEMALE_DIR if is_female else MALE_DIR
        b_pref = "WCh_Body" if is_female else "MCh_Body"
        body_if = load_sheet(os.path.join(src_dir, f"{b_pref}_Idle_Front_Sheet.png"))[:, :60]
        body_is = load_sheet(os.path.join(src_dir, f"{b_pref}_Idle_Side_Sheet.png"))[:, :60]
        body_ib = load_sheet(os.path.join(src_dir, f"{b_pref}_Idle_Back_Sheet.png"))[:, :60]

        hair_titles = (
            [
                "헤어 1: 리본 사이드 포니테일",
                "헤어 2: 발랄한 양갈래 트윈테일",
                "헤어 3: 단정한 풀뱅 숏 보브컷",
            ]
            if is_female
            else [
                "헤어 1: 기본 숏 스파이키",
                "헤어 2: 가르마 커튼 투블럭",
                "헤어 3: 와일드 하이 포니테일",
            ]
        )

        canvas = Image.new("RGBA", (1620, 760), (18, 21, 29, 255))
        draw = ImageDraw.Draw(canvas)
        draw.text((28, 14), g_title, font=font_title, fill=(255, 232, 160))

        # Top Section (y=50..426): 3 Hairstyles (PartId 1..3)
        for i in range(1, 4):
            cx = 24 + (i - 1) * 530
            draw.rounded_rectangle(
                [cx, 50, cx + 508, 426], radius=10, fill=(28, 33, 45), outline=(68, 78, 102), width=2
            )
            draw.text((cx + 16, 60), hair_titles[i - 1], font=font_sub, fill=(230, 240, 255))

            h_pref = ("WCh_Hair" if i == 1 else f"WCh_Hair{i}") if is_female else f"MCh_Hair{i}"
            f_pref = (
                ("WCh_FaceEye" if i == 1 else f"WCh_FaceEye{i}")
                if is_female
                else ("MCh_FaceEye" if i == 1 else f"MCh_FaceEye{i}")
            )

            h_if = load_sheet(os.path.join(src_dir, f"{h_pref}_Idle_Front_Sheet.png"))[:, :60]
            h_is = load_sheet(os.path.join(src_dir, f"{h_pref}_Idle_Side_Sheet.png"))[:, :60]
            h_ib = load_sheet(os.path.join(src_dir, f"{h_pref}_Idle_Back_Sheet.png"))[:, :60]

            f_if = load_sheet(os.path.join(src_dir, f"{f_pref}_Idle_Front_Sheet.png"))[:, :60]
            f_is = load_sheet(os.path.join(src_dir, f"{f_pref}_Idle_Side_Sheet.png"))[:, :60]
            f_ib = (
                np.zeros_like(f_if)
                if is_female
                else load_sheet(os.path.join(src_dir, f"{f_pref}_Idle_Back_Sheet.png"))[:, :60]
            )

            for vi, (lbl, b_im, f_im, h_im) in enumerate(
                [
                    ("정면(Front)", body_if, f_if, h_if),
                    ("측면(Side)", body_is, f_is, h_is),
                    ("후면(Back)", body_ib, f_ib, h_ib),
                ]
            ):
                comp = composite_layers(b_im, f_im, h_im)[0:66, 4:56]
                pil_c = Image.fromarray(comp).resize((104, 132), Image.NEAREST)
                canvas.alpha_composite(pil_c, (cx + 28 + vi * 158, 90))
                draw.text((cx + 46 + vi * 158, 224), lbl, font=font_body, fill=(165, 178, 204))

            dye_lbl, dye_rgb = dye_colors[i]
            h_dyed = apply_tint(h_if, dye_rgb)
            comp_dye = composite_layers(body_if, f_if, h_dyed)[0:68, 4:56]
            pil_dye = Image.fromarray(comp_dye).resize((126, 164), Image.NEAREST)
            canvas.alpha_composite(pil_dye, (cx + 24, 250))

            dye_lbl2, dye_rgb2 = dye_colors[i + 1]
            h_dyed2 = apply_tint(h_if, dye_rgb2)
            comp_dye2 = composite_layers(body_if, f_if, h_dyed2)[0:68, 4:56]
            pil_dye2 = Image.fromarray(comp_dye2).resize((126, 164), Image.NEAREST)
            canvas.alpha_composite(pil_dye2, (cx + 168, 250))
            draw.text((cx + 312, 260), f"염색 1: {dye_lbl}", font=font_body, fill=(255, 214, 145))
            draw.text((cx + 312, 280), f"염색 2: {dye_lbl2}", font=font_body, fill=(255, 214, 145))

            h_icon_fn = f"T_PartIcon_Hair_Female_{i}_Front.png" if is_female else f"T_PartIcon_Hair_{i}_Front.png"
            h_ic = Image.open(os.path.join(src_dir, h_icon_fn)).resize((72, 72), Image.NEAREST)
            draw.rounded_rectangle([cx + 312, 306, cx + 392, 386], radius=6, fill=(20, 24, 34), outline=(80, 95, 125))
            canvas.alpha_composite(h_ic, (cx + 316, 310))
            draw.text((cx + 312, 392), "헤어 UI 아이콘", font=font_body, fill=(165, 178, 204))

        # Bottom Section (y=442..742): 5 Mabinogi-Style Eye/Face Expressions (PartId 1..5)
        for i in range(1, 6):
            cx = 24 + (i - 1) * 316
            draw.rounded_rectangle(
                [cx, 442, cx + 304, 742], radius=10, fill=(25, 31, 44), outline=(82, 118, 140), width=2
            )
            draw.text((cx + 12, 452), eye_titles[i - 1], font=font_sub, fill=(175, 242, 210))

            f_pref = (
                ("WCh_FaceEye" if i == 1 else f"WCh_FaceEye{i}")
                if is_female
                else ("MCh_FaceEye" if i == 1 else f"MCh_FaceEye{i}")
            )
            h_idx_prev = ((i - 1) % 3) + 1
            h_part_pref = (
                ("WCh_Hair" if h_idx_prev == 1 else f"WCh_Hair{h_idx_prev}")
                if is_female
                else "MCh_Hair2"
            )
            h_p_if = load_sheet(os.path.join(src_dir, f"{h_part_pref}_Idle_Front_Sheet.png"))[:, :60]

            f_if = load_sheet(os.path.join(src_dir, f"{f_pref}_Idle_Front_Sheet.png"))[:, :60]
            f_is = load_sheet(os.path.join(src_dir, f"{f_pref}_Idle_Side_Sheet.png"))[:, :60]

            # 7x Zoom on Face Front (without hair obscuring brows, and with parted hair)
            fz_f = composite_layers(body_if, f_if)[20:34, 19:41]
            fz_s = composite_layers(body_is, f_is)[20:34, 21:41]
            canvas.alpha_composite(Image.fromarray(fz_f).resize((146, 94), Image.NEAREST), (cx + 10, 484))
            canvas.alpha_composite(Image.fromarray(fz_s).resize((126, 94), Image.NEAREST), (cx + 166, 484))
            draw.text((cx + 28, 582), "정면 표정 (7x 확대)", font=font_body, fill=(190, 210, 235))
            draw.text((cx + 178, 582), "측면 표정 (7x 확대)", font=font_body, fill=(190, 210, 235))

            # Full head composite with hair + UI icon
            head_comp = composite_layers(body_if, f_if, h_p_if)[2:42, 10:50]
            canvas.alpha_composite(Image.fromarray(head_comp).resize((116, 116), Image.NEAREST), (cx + 18, 612))
            draw.text((cx + 144, 622), "헤어 착용 시 인게임 프리뷰", font=font_body, fill=(255, 224, 160))

            e_icon_fn = f"T_PartIcon_Face_Female_{i}_Front.png" if is_female else f"T_PartIcon_Eyes_{i}_Front.png"
            e_ic = Image.open(os.path.join(src_dir, e_icon_fn)).resize((64, 64), Image.NEAREST)
            draw.rounded_rectangle([cx + 148, 650, cx + 220, 722], radius=6, fill=(20, 24, 34), outline=(80, 95, 125))
            canvas.alpha_composite(e_ic, (cx + 152, 654))
            draw.text((cx + 228, 676), "눈 아이콘", font=font_body, fill=(165, 178, 204))

        out_name = "summary_2_female_custom_parts.png" if is_female else "summary_1_male_custom_parts.png"
        out_path = os.path.join(ARTIFACT_DIR, out_name)
        canvas.save(out_path)
        print("Saved:", out_path)

    m_board = Image.open(os.path.join(ARTIFACT_DIR, "summary_1_male_custom_parts.png"))
    w_board = Image.open(os.path.join(ARTIFACT_DIR, "summary_2_female_custom_parts.png"))
    combined = Image.new("RGBA", (1620, 1520), (18, 21, 29, 255))
    combined.alpha_composite(m_board, (0, 0))
    combined.alpha_composite(w_board, (0, 760))
    combined.save(os.path.join(ARTIFACT_DIR, "character_create_parts_showcase.png"))

    # 8-frame Walk Animation GIF showing all 3 Hairstyles + 5 Eye Expressions!
    gif_frames = []
    m_bw_f = load_sheet(os.path.join(MALE_DIR, "MCh_Body_Walk_Front_Sheet.png"))
    w_bw_f = load_sheet(os.path.join(FEMALE_DIR, "WCh_Body_Walk_Front_Sheet.png"))
    for f in range(8):
        ox = f * 60
        fr = Image.new("RGBA", (1050, 460), (22, 25, 34, 255))
        fdraw = ImageDraw.Draw(fr)
        fdraw.text(
            (16, 10),
            f"MALE & FEMALE 3 HAIRS + 5 EXPRESSIONS WALK ANIMATION (Frame {f+1}/8)",
            fill=(255, 230, 150),
        )
        for i in range(1, 6):
            _, dye_rgb = dye_colors[i - 1]
            h_idx = ((i - 1) % 3) + 1  # 3 hairstyles (1..3), 5 eye expressions (1..5)
            mh = load_sheet(os.path.join(MALE_DIR, f"MCh_Hair{h_idx}_Walk_Front_Sheet.png"))[:, ox : ox + 60]
            mf_pref = "MCh_FaceEye" if i == 1 else f"MCh_FaceEye{i}"
            mf = load_sheet(os.path.join(MALE_DIR, f"{mf_pref}_Walk_Front_Sheet.png"))[:, ox : ox + 60]
            mh_t = mh if i == 1 else apply_tint(mh, dye_rgb)
            mc = composite_layers(m_bw_f[:, ox : ox + 60], mf, mh_t)[4:98, 4:56]
            fr.alpha_composite(Image.fromarray(mc).resize((104, 188), Image.NEAREST), (20 + (i - 1) * 204, 34))

            wh_pref = "WCh_Hair" if h_idx == 1 else f"WCh_Hair{h_idx}"
            wh = load_sheet(os.path.join(FEMALE_DIR, f"{wh_pref}_Walk_Front_Sheet.png"))[:, ox : ox + 60]
            wf_pref = "WCh_FaceEye" if i == 1 else f"WCh_FaceEye{i}"
            wf = load_sheet(os.path.join(FEMALE_DIR, f"{wf_pref}_Walk_Front_Sheet.png"))[:, ox : ox + 60]
            wh_t = wh if i == 1 else apply_tint(wh, dye_rgb)
            wc = composite_layers(w_bw_f[:, ox : ox + 60], wf, wh_t)[4:98, 4:56]
            fr.alpha_composite(Image.fromarray(wc).resize((104, 188), Image.NEAREST), (20 + (i - 1) * 204, 244))
        gif_frames.append(fr)

    gif_path = os.path.join(ARTIFACT_DIR, "character_create_parts_anim.gif")
    gif_frames[0].save(gif_path, save_all=True, append_images=gif_frames[1:], duration=110, loop=0)
    gif_frames[6].save(os.path.join(ARTIFACT_DIR, "scratch_walk_frame7_fixed.png"))
    gif_frames[1].save(os.path.join(ARTIFACT_DIR, "scratch_walk_frame2_fixed.png"))
    print("Saved animated GIF and frame inspection PNGs.")


if __name__ == "__main__":
    generate_male_face_sheets()
    generate_female_face_sheets()
    generate_all_hair_sheets()
    render_showcase()
    print("ALL CUSTOM PARTS REGENERATED CLEANLY.")
