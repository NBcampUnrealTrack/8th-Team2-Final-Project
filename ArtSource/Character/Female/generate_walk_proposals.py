import os
from PIL import Image, ImageDraw
import numpy as np

BASE_DIR = r"c:\Users\Ndus_072\RevDevTool\RevStudio-ProjectM"
BACKUP_DIR = os.path.join(BASE_DIR, "Saved", "FemaleSpriteBackup_20261014")
ARTIFACT_DIR = r"C:\Users\Ndus_072\.gemini\antigravity\brain\be1275f5-376f-4165-9ea2-1b959606f5ad"

FW, FH = 60, 100

# ==============================================================================
# AUTHENTIC PALETTE FROM WCh_Body_Idle_Back_Sheet.png & WCh_Body_Idle_Front_Sheet.png
# ==============================================================================
BLK  = (0, 0, 0, 255)         # '#'
# Back-of-thigh skin & petticoat (Idle_Back y=65..67)
SK_S = (249, 220, 203, 255)   # 's' petticoat warm cream
SK_Y = (254, 223, 204, 255)   # 'y' light skin
SK_1 = (251, 192, 157, 255)   # '1' mid skin
SK_0 = (252, 179, 139, 255)   # '0' warm skin
SK_Z = (231, 165, 136, 255)   # 'z' shaded skin
SK_A = (245, 208, 181, 255)   # 'a' soft shaded back-of-thigh skin
SK_6 = (51, 31, 24, 255)      # '6' dark skin-to-skirt shadow
SK_3 = (27, 19, 20, 255)      # '3' deep inner thigh shadow
SK_B = (48, 27, 23, 255)      # 'B' deep inner thigh shadow R
SK_C = (57, 36, 28, 255)      # 'C' outer thigh shadow R

# Thigh-high stockings (Idle_Back & Idle_Front y=68..82)
ST_G = (38, 25, 22, 255)      # 'g' deep crease
ST_F = (80, 55, 48, 255)      # 'f' dark brown stocking edge
ST_J = (100, 76, 74, 255)     # 'j' matte taupe-grey stocking body

# White fur cuff & Leather boot (Idle_Back & Idle_Front y=83..95)
FR_7 = (253, 243, 231, 255)   # '7' bright white fur
FR_L = (252, 232, 213, 255)   # 'l' cream fur
FR_M = (236, 195, 171, 255)   # 'm' warm fur shadow
FR_W = (214, 183, 170, 255)   # 'w' outer fur shadow

BT_G = (38, 25, 22, 255)      # 'g' dark crease
BT_K = (48, 39, 48, 255)      # 'k' dark heel shadow
BT_R = (68, 45, 37, 255)      # 'r' dark leather
BT_F = (80, 55, 48, 255)      # 'f' warm dark leather
BT_Q = (101, 60, 44, 255)     # 'q' rich mid-dark leather
BT_N = (117, 73, 56, 255)     # 'n' mid leather / sole
BT_X = (144, 92, 65, 255)     # 'x' warm mid leather
BT_P = (151, 104, 83, 255)    # 'p' light leather / heel block
BT_O = (203, 149, 109, 255)   # 'o' collar highlight / tan
BT_S = (222, 166, 113, 255)   # 'S' gold ankle strap / buckle

CHAR_MAP = {
    '#': BLK,
    's': SK_S, 'y': SK_Y, '1': SK_1, '0': SK_0, 'z': SK_Z, 'a': SK_A,
    '6': SK_6, '3': SK_3, 'B': SK_B, 'C': SK_C,
    'g': ST_G, 'f': ST_F, 'j': ST_J,
    '7': FR_7, 'l': FR_L, 'm': FR_M, 'w': FR_W,
    'k': BT_K, 'r': BT_R, 'q': BT_Q, 'n': BT_N, 'x': BT_X, 'p': BT_P, 'o': BT_O, 'S': BT_S,
}

def put_str(frame, y, x_start, pat):
    if not (0 <= y < FH):
        return
    for i, ch in enumerate(pat):
        if ch == '.':
            continue
        x = x_start + i
        if 0 <= x < FW:
            frame[y, x] = CHAR_MAP[ch]


# ==============================================================================
# 1. NOVICE WALK_BACK: IN-PLACE UPGRADE WITH AUTHENTIC PALETTE & SLEEK 1PX OUTLINES
# ==============================================================================
# 13-row planted back-view boot (f0/f4 y=80..92, f1/f5 y=83..95)
# Uses Idle_Back's exact colors with Walk_Front's clean 1px '#' outlines and 8px->7px->8px contour
BACK_BOOT_13_L = [
    ".#wml77l#.",  # r0: 8px white fur cuff top
    "..#glllo#.",  # r1: 7px cream fur cuff bottom
    "..#ggggg#.",  # r2: 7px dark crease under fur
    ".#ffnopn#.",  # r3: 8px upper leather collar
    ".#nngggg#.",  # r4: 8px collar shadow
    "..#grqrr#.",  # r5: 7px mid boot shaft
    "..#rfnqg#.",  # r6: 7px ankle taper
    ".#ffkfrf#.",  # r7: 8px heel counter upper
    ".#rrSSSr#.",  # r8: 8px gold ankle strap
    ".#rpppgg#.",  # r9: 8px heel block upper
    ".#rrnnng#.",  # r10: 8px heel block lower
    ".#rrnnng#.",  # r11: 8px boot sole
    ".########.",  # r12: 8px bottom black outline
]

BACK_BOOT_13_R = [
    ".#l77lmw#.",  # r0
    ".#olllg#..",  # r1
    ".#ggggg#..",  # r2
    ".#qoonff#.",  # r3
    ".#ggggnn#.",  # r4
    ".#rqqrr#..",  # r5
    ".#gnnfr#..",  # r6
    ".#frfkff#.",  # r7
    ".#rSSSrr#.",  # r8
    ".#ggpppr#.",  # r9
    ".#gnnnrr#.",  # r10
    ".#gnnnrr#.",  # r11
    ".########.",  # r12
]

# 12-row back-view boot (f2/f6 planted y=81..92, f3/f7 lower airborne leg y=79..90)
BACK_BOOT_12_L = BACK_BOOT_13_L[:10] + BACK_BOOT_13_L[11:]
BACK_BOOT_12_R = BACK_BOOT_13_R[:10] + BACK_BOOT_13_R[11:]

# 10-row airborne back-view boot (f3/f7 higher airborne leg y=81..90)
BACK_BOOT_10_L = [
    ".#wml77l#.",  # r0: 8px white fur top
    "..#glllo#.",  # r1: 7px cream fur bottom
    "..#ggggg#.",  # r2: 7px dark crease
    ".#ffnopn#.",  # r3: 8px leather collar
    "..#grqrr#.",  # r4: 7px mid shaft
    "..#rfnqg#.",  # r5: 7px ankle taper
    ".#rrSSSr#.",  # r6: 8px gold ankle strap
    ".#rpppgg#.",  # r7: 8px heel block
    ".#rrnnng#.",  # r8: 8px sole
    ".########.",  # r9: 8px bottom outline
]
BACK_BOOT_10_R = [
    ".#l77lmw#.",
    ".#olllg#..",
    ".#ggggg#..",
    ".#qoonff#.",
    ".#rqqrr#..",
    ".#gnnfr#..",
    ".#rSSSrr#.",
    ".#ggpppr#.",
    ".#gnnnrr#.",
    ".########.",
]

# 9-row lifted back-kick boot (f0/f4 y=78..86 and f2/f6 y=79..87)
# Foreshortened shaft + gold strap + upturned sole/heel facing the viewer
BACK_LIFTED_BOOT_9_L = [
    ".#wml77l#.",  # r0: 8px white fur cuff top
    "..#glllo#.",  # r1: 7px cream fur cuff bottom
    "..#fnopn#.",  # r2: 7px foreshortened collar
    "..#rSSSr#.",  # r3: 7px gold ankle strap
    ".#rpppgg#.",  # r4: 8px upturned heel top edge
    ".#rnnnqr#.",  # r5: 8px upturned sole/heel block
    ".#rqqqqr#.",  # r6: 8px upturned sole arch
    "..#rnnnr#.",  # r7: 7px rounded boot toe/sole tip
    "..#######.",  # r8: 7px bottom black outline
]
BACK_LIFTED_BOOT_9_R = [
    ".#l77lmw#.",
    ".#olllg#..",
    ".#qoonf#..",
    ".#rSSSr#..",
    ".#ggpppr#.",
    ".#rqnnnr#.",
    ".#rqqqqr#.",
    ".#rnnnr#..",
    ".#######..",
]

# 8-row high back-kick boot (f1/f5 y=78..85 highest kick!)
BACK_LIFTED_BOOT_8_L = [
    ".#wml77l#.",  # r0: 8px white fur cuff top
    "..#glllo#.",  # r1: 7px cream fur cuff bottom
    "..#rSSSr#.",  # r2: 7px gold ankle strap
    ".#rpppgg#.",  # r3: 8px upturned heel top edge
    ".#rnnnqr#.",  # r4: 8px upturned sole/heel block
    ".#rqqqqr#.",  # r5: 8px upturned sole arch
    "..#rnnnr#.",  # r6: 7px rounded boot toe/sole tip
    "..#######.",  # r7: 7px bottom black outline
]
BACK_LIFTED_BOOT_8_R = [
    ".#l77lmw#.",
    ".#olllg#..",
    ".#rSSSr#..",
    ".#ggpppr#.",
    ".#rqnnnr#.",
    ".#rqqqqr#.",
    ".#rnnnr#..",
    ".#######..",
]


def draw_back_leg_authentic(frame, is_left, sk_bot, fur_y, bot_y):
    """
    Draws one leg in Walk_Back aligned at x0=21 (Left: x=22..29) and x0=30 (Right: x=31..38),
    leaving a sleek 1px inner gap at x=30 matching Walk_Front.
    """
    x0 = 21 if is_left else 30

    # 1. Shaded back-of-thigh skin glimpse right below white petticoat (1 row: sk_bot + 1)
    if is_left:
        put_str(frame, sk_bot + 1, x0, ".#6aaaa3#.")
    else:
        put_str(frame, sk_bot + 1, x0, ".#BaaaaC#.")

    # 2. Dark stocking top band (sk_bot + 2) & shapely 8px->7px matte taupe stocking (down to fur_y - 1)
    st_start = sk_bot + 2
    for y in range(st_start, fur_y):
        i = y - st_start
        rem = fur_y - 1 - y
        if i == 0:
            pat = ".#ffffff#."
        elif rem >= 3:
            if i <= 3:
                pat = ".#fjjjjf#."
            else:
                pat = ".#jjjjjf#." if is_left else ".#fjjjjj#."
        elif rem == 2:
            pat = "..#fjjjf#." if is_left else ".#fjjjf#.."
        elif rem == 1:
            pat = "..#jjjjf#." if is_left else ".#fjjjj#.."
        else:
            pat = "..#gfjfg#." if is_left else ".#gfjfg#.."
        put_str(frame, y, x0, pat)

    # 3. Boot (from fur_y down to bot_y) — exact hand-pixeled template for each height H
    H = bot_y - fur_y + 1
    if H >= 13:
        tmpl = BACK_BOOT_13_L if is_left else BACK_BOOT_13_R
    elif H == 12:
        tmpl = BACK_BOOT_12_L if is_left else BACK_BOOT_12_R
    elif H == 10:
        tmpl = BACK_BOOT_10_L if is_left else BACK_BOOT_10_R
    elif H == 9:
        tmpl = BACK_LIFTED_BOOT_9_L if is_left else BACK_LIFTED_BOOT_9_R
    else:
        tmpl = BACK_LIFTED_BOOT_8_L if is_left else BACK_LIFTED_BOOT_8_R

    for i, pat in enumerate(tmpl):
        put_str(frame, fur_y + i, x0, pat)


def upgrade_novice_walk_back_inplace():
    """
    Upgrades WCh_Body_Walk_Back_Sheet.png in-place:
    - Preserves 100% of the original running bounce (topY=[3,5,5,2]), twisting skirt pleats, swinging arms & bag.
    - Adds missing 1px black silhouette outlines on the lower skirt/gloves/bag (y=53..sk_bot).
    - Restores the missing 1-row white petticoat frill at y=64 on airborne frames f3 and f7.
    - Replaces the unfinished 5px brown blockout legs with sleek 8px->7px stockings & fur/strap/heel boots.
    """
    src = np.array(Image.open(os.path.join(BACKUP_DIR, "WCh_Body_Walk_Back_Sheet.png")))
    out = src.copy()

    # Exact per-frame landmarks matching Walk_Front's running cycle 1:1:
    # f: (sk_bot, l_fur, l_bot, r_fur, r_bot)
    landmarks = {
        0: (65, 78, 86, 80, 92),  # L lifted (H=9), R planted (H=13)
        1: (67, 78, 85, 83, 95),  # L high back-kick (H=8), R deep planted (H=13)
        2: (66, 79, 87, 81, 92),  # L lifted (H=9), R push-off (H=12)
        3: (63, 79, 90, 81, 90),  # Airborne leap! L airborne (H=12), R airborne (H=10)
        4: (65, 80, 92, 78, 86),  # L planted (H=13), R lifted (H=9)
        5: (67, 83, 95, 78, 85),  # L deep planted (H=13), R high back-kick (H=8)
        6: (66, 81, 92, 79, 87),  # L push-off (H=12), R lifted (H=9)
        7: (63, 81, 90, 79, 90),  # Airborne leap! L airborne (H=10), R airborne (H=12)
    }

    dark_outlines = {(27, 19, 20, 255), (48, 39, 48, 255), (51, 31, 24, 255)}

    # Fix original artist's 2px vertical split-skirt bug at x=30..31 on f6 and f7 using f2 and f3's un-split skirt rows
    f2_frame = src[:, 2 * FW:3 * FW]
    f3_frame = src[:, 3 * FW:4 * FW]
    out[62:67, 6 * FW + 20:6 * FW + 42] = f2_frame[62:67, 20:42]
    out[59:64, 7 * FW + 20:7 * FW + 42] = f3_frame[59:64, 20:42]
    out[59, 7 * FW + 20] = (0, 0, 0, 0)

    for f in range(8):
        frame = out[:, f * FW:(f + 1) * FW]
        sk_bot, l_fur, l_bot, r_fur, r_bot = landmarks[f]

        # Clear old 5px leg blockout below sk_bot first (preserving bag/hand tips at x<=20 and x>=41 on y=sk_bot+1)
        frame[sk_bot + 1, 21:40] = (0, 0, 0, 0)
        frame[sk_bot + 2:97, 19:41] = (0, 0, 0, 0)

        # Upgrade un-outlined dark brown/navy silhouette edges on skirt/gloves/bag (y=53..sk_bot) to 1px black
        snap = frame.copy()
        for y in range(53, sk_bot + 1):
            for x in range(14, 46):
                p = tuple(int(v) for v in snap[y, x])
                if p in dark_outlines:
                    if (snap[y, x - 1, 3] == 0 or snap[y, x + 1, 3] == 0 or
                        snap[y - 1, x, 3] == 0 or snap[y + 1, x, 3] == 0):
                        frame[y, x] = BLK

        # Draw sleek 8px->7px stockings & fur/strap/heel boots at exact running landmarks
        draw_back_leg_authentic(frame, True,  sk_bot, l_fur, l_bot)
        draw_back_leg_authentic(frame, False, sk_bot, r_fur, r_bot)

    return out


# ==============================================================================
# 2. NOVICE WALK_FRONT: IN-PLACE UPGRADE PRESERVING 100% OF HAND-PIXELED BOOTS
# ==============================================================================
FRONT_LIFTED_BOOT_10_R = [
    ".#l77lmw#.",  # r0 (y=77): 8px clean white fur cuff top
    "..#olllg#.",  # r1 (y=78): 7px cream fur cuff bottom
    "..#rqqnf#.",  # r2 (y=79): 7px upper leather shaft
    ".#qxoqnn#.",  # r3 (y=80): 8px collar / strap rim
    "..#gfSoq#.",  # r4 (y=81): 7px gold buckle strap
    "..#rrxgg#.",  # r5 (y=82): 7px lower shaft shadow
    "..#gxnpp#.",  # r6 (y=83): 7px vamp highlight
    "..#m77xng#",  # r7 (y=84): 8px rounded toe highlight
    "..#pppnqg#",  # r8 (y=85): 8px rounded toe sole
    "...######.",  # r9 (y=86): 6px rounded toe bottom outline
]
FRONT_LIFTED_BOOT_10_L = [
    ".#wml77l#.",  # r0
    ".#glllo#..",  # r1
    ".#fnqqr#..",  # r2
    ".#nnqoxq#.",  # r3
    ".#qoSfg#..",  # r4
    ".#ggxrr#..",  # r5
    ".#ppnxg#..",  # r6
    "#gnx77m#..",  # r7
    "#gqnppp#..",  # r8
    ".######...",  # r9
]

FRONT_LIFTED_BOOT_9_R = [
    ".#l77lmw#.",  # r0 (y=77): 8px clean white fur cuff top
    "..#olllg#.",  # r1 (y=78): 7px cream fur cuff bottom
    "..#rqqnf#.",  # r2 (y=79): 7px upper leather shaft
    ".#qxoqnn#.",  # r3 (y=80): 8px collar / strap rim
    "..#gfSoq#.",  # r4 (y=81): 7px gold buckle strap
    "..#grxxm#.",  # r5 (y=82): 7px vamp
    "..#m77xng#",  # r6 (y=83): 8px rounded toe highlight
    "..#pppnqg#",  # r7 (y=84): 8px rounded toe sole
    "...######.",  # r8 (y=85): 6px rounded toe bottom outline
]
FRONT_LIFTED_BOOT_9_L = [
    ".#wml77l#.",
    ".#glllo#..",
    ".#fnqqr#..",
    ".#nnqoxq#.",
    ".#qoSfg#..",
    ".#mxxrg#..",
    "#gnx77m#..",
    "#gqnppp#..",
    ".######...",
]


def draw_front_stocking_authentic(frame, is_left, st_top, fur_y):
    x0 = 21 if is_left else 30
    for y in range(st_top, fur_y):
        i = y - st_top
        rem = fur_y - 1 - y
        if i == 0:
            pat = ".#ffffff#."
        elif rem >= 3:
            if i <= 3:
                pat = ".#fjjjjf#."
            else:
                pat = ".#jjjjjf#." if is_left else ".#fjjjjj#."
        elif rem == 2:
            pat = "..#fjjjf#." if is_left else ".#fjjjf#.."
        elif rem == 1:
            pat = "..#jjjjf#." if is_left else ".#fjjjj#.."
        else:
            pat = "..#gfjfg#." if is_left else ".#gfjfg#.."
        put_str(frame, y, x0, pat)


def upgrade_novice_walk_front_inplace():
    src = np.array(Image.open(os.path.join(BACKUP_DIR, "WCh_Body_Walk_Front_Sheet.png")))
    out = src.copy()

    # Fix original artist's 2px vertical split-apron gap at x=30..31 on f6 (y=67..68) and f7 (y=64..66)
    f2_frame = src[:, 2 * FW:3 * FW]
    f3_frame = src[:, 3 * FW:4 * FW]
    out[67:69, 6 * FW + 20:6 * FW + 42] = f2_frame[67:69, 20:42]
    out[64:67, 7 * FW + 20:7 * FW + 42] = f3_frame[64:67, 20:42]

    landmarks = {
        0: (68, 80, 92, False, 77, 86, True),
        1: (70, 83, 95, False, 77, 85, True),
        2: (70, 81, 92, False, 78, 87, True),
        3: (67, 81, 90, False, 79, 90, False),
        4: (68, 77, 86, True,  80, 92, False),
        5: (70, 77, 85, True,  83, 95, False),
        6: (70, 78, 87, True,  81, 92, False),
        7: (67, 79, 90, False, 81, 90, False),
    }

    for f in range(8):
        frame = out[:, f * FW:(f + 1) * FW]
        st_top, l_fur, l_bot, l_lifted, r_fur, r_bot, r_lifted = landmarks[f]

        frame[st_top:l_fur, 21:30] = (0, 0, 0, 0)
        draw_front_stocking_authentic(frame, True, st_top, l_fur)

        frame[st_top:r_fur, 30:40] = (0, 0, 0, 0)
        draw_front_stocking_authentic(frame, False, st_top, r_fur)

        if l_lifted:
            frame[l_fur:l_bot + 1, 21:30] = (0, 0, 0, 0)
            H = l_bot - l_fur + 1
            tmpl = FRONT_LIFTED_BOOT_10_L if H == 10 else FRONT_LIFTED_BOOT_9_L
            for i, pat in enumerate(tmpl):
                put_str(frame, l_fur + i, 21, pat)

        if r_lifted:
            frame[r_fur:r_bot + 1, 30:40] = (0, 0, 0, 0)
            H = r_bot - r_fur + 1
            tmpl = FRONT_LIFTED_BOOT_10_R if H == 10 else FRONT_LIFTED_BOOT_9_R
            for i, pat in enumerate(tmpl):
                put_str(frame, r_fur + i, 30, pat)

        if f == 3:
            put_str(frame, 88, 21, ".#m77xng#.")
            put_str(frame, 89, 21, ".#pppnqg#.")
            put_str(frame, 90, 21, ".########.")
        elif f == 7:
            put_str(frame, 88, 30, ".#gnx77m#.")
            put_str(frame, 89, 30, ".#gqnppp#.")
            put_str(frame, 90, 30, ".########.")

    return out


# ==============================================================================
# 3. JOB WALK_BACK / WALK_FRONT IN-PLACE PINCH WIDENING (0 RESAMPLING!)
# ==============================================================================
def upgrade_job_walk_inplace(job, anim, src_dir=None):
    if src_dir is None:
        src_dir = BACKUP_DIR
    fname = f"WCh_{job}_Body_{anim}_Sheet.png"
    src = np.array(Image.open(os.path.join(src_dir, fname)))
    out = src.copy()

    y_start = 68 if job in ("Tracker", "Gambler") else 78

    for f in range(8):
        frame = out[:, f * FW:(f + 1) * FW]
        for y in range(y_start, 95):
            row_l = frame[y, 20:30]
            nz_l = [x for x in range(10) if row_l[x, 3] > 0]
            if nz_l and 4 <= (nz_l[-1] - nz_l[0] + 1) <= 6:
                x_first, x_last = 20 + nz_l[0], 20 + nz_l[-1]
                w = x_last - x_first + 1
                if w == 5 and x_first > 20 and frame[y, x_first - 1, 3] == 0:
                    frame[y, x_first - 1] = frame[y, x_first]
                    frame[y, x_first] = frame[y, x_first + 1]
                if w <= 6 and x_last < 29 and frame[y, x_last + 1, 3] == 0:
                    frame[y, x_last + 1] = frame[y, x_last]
                    frame[y, x_last] = frame[y, x_last - 1]

            row_r = frame[y, 30:40]
            nz_r = [x for x in range(10) if row_r[x, 3] > 0]
            if nz_r and 4 <= (nz_r[-1] - nz_r[0] + 1) <= 6:
                x_first, x_last = 30 + nz_r[0], 30 + nz_r[-1]
                w = x_last - x_first + 1
                if w == 5 and x_last < 39 and frame[y, x_last + 1, 3] == 0:
                    frame[y, x_last + 1] = frame[y, x_last]
                    frame[y, x_last] = frame[y, x_last - 1]
                if w <= 6 and x_first > 30 and frame[y, x_first - 1, 3] == 0:
                    frame[y, x_first - 1] = frame[y, x_first]
                    frame[y, x_first] = frame[y, x_first + 1]

    return out


def render_comparison():
    wb_orig = np.array(Image.open(os.path.join(BACKUP_DIR, "WCh_Body_Walk_Back_Sheet.png")))
    wf_orig = np.array(Image.open(os.path.join(BACKUP_DIR, "WCh_Body_Walk_Front_Sheet.png")))
    wb_new = upgrade_novice_walk_back_inplace()
    wf_new = upgrade_novice_walk_front_inplace()

    scale = 4
    pad = 8
    header_h = 28
    row_h = FH * scale + header_h + pad
    col_w = FW * scale + 4
    W = col_w * 8 + pad * 2
    H = row_h * 4 + pad * 2

    canvas = Image.new("RGBA", (W, H), (24, 26, 32, 255))
    draw = ImageDraw.Draw(canvas)

    rows = [
        ("1. Walk_Back BEFORE (Original: Lively Bounce, but 5px Un-outlined Stick Legs & Blockout Boots)", wb_orig, (255, 130, 130, 255)),
        ("2. Walk_Back AFTER (Authentic Idle_Back Palette + 100% Original Running Bounce & Crisp 8px->7px Boots)", wb_new, (120, 255, 160, 255)),
        ("3. Walk_Front BEFORE (Original: Lively Bounce, but 7px Pipe Stockings & Pinched Lifted Boot)", wf_orig, (255, 130, 130, 255)),
        ("4. Walk_Front AFTER (Authentic Idle_Front Stockings + 100% Original Planted Boots & Clean Lifted Boot)", wf_new, (120, 255, 160, 255)),
    ]

    for r_idx, (title, sheet, color) in enumerate(rows):
        y0 = pad + r_idx * row_h
        draw.text((pad + 4, y0 + 6), title, fill=color)
        for f in range(8):
            x0 = pad + f * col_w
            fy = y0 + header_h
            draw.rectangle([x0, fy, x0 + FW * scale, fy + FH * scale], fill=(38, 42, 52, 255), outline=color, width=1)
            f_img = Image.fromarray(sheet[:, f * FW:(f + 1) * FW]).resize((FW * scale, FH * scale), Image.NEAREST)
            canvas.alpha_composite(f_img, (x0, fy))

    out_path = os.path.join(ARTIFACT_DIR, "lively_inplace_walk_comparison.png")
    canvas.save(out_path)

    gif_frames = []
    g_scale = 6
    gw = FW * g_scale * 4 + 40
    gh = FH * g_scale + 50
    for f in range(8):
        gf = Image.new("RGBA", (gw, gh), (28, 30, 36, 255))
        gd = ImageDraw.Draw(gf)
        items = [
            ("Back BEFORE", wb_orig[:, f * FW:(f + 1) * FW], (255, 140, 140, 255)),
            ("Back AFTER",  wb_new[:, f * FW:(f + 1) * FW],  (120, 255, 160, 255)),
            ("Front BEFORE", wf_orig[:, f * FW:(f + 1) * FW], (255, 140, 140, 255)),
            ("Front AFTER",  wf_new[:, f * FW:(f + 1) * FW],  (120, 255, 160, 255)),
        ]
        for idx, (lbl, arr, col) in enumerate(items):
            x = 8 + idx * (FW * g_scale + 8)
            gd.text((x + 8, 8), f"{lbl} (f{f})", fill=col)
            im = Image.fromarray(arr).resize((FW * g_scale, FH * g_scale), Image.NEAREST)
            gf.alpha_composite(im, (x, 30))
        gif_frames.append(gf)

    gif_path = os.path.join(ARTIFACT_DIR, "lively_inplace_walk_anim.gif")
    gif_frames[0].save(gif_path, save_all=True, append_images=gif_frames[1:], duration=110, loop=0)
    print("Saved:", out_path, gif_path)


if __name__ == "__main__":
    render_comparison()
