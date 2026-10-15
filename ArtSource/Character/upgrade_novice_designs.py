# -*- coding: utf-8 -*-
"""
Upgrades the visual design of Male Novice (MCh_Body_*) and Female Novice (WCh_Body_*)
across all 6 Field animations (Idle/Walk x Front/Side/Back, 8 frames each):

Male Novice (MCh_Body) Design Improvements:
1. Heroic Royal Navy & Gold-Trimmed Adventurer Tunic Vest (y <= 55 + dy):
   - Replaces the drab monochrome gray vest with a rich Royal Navy tunic featuring
     gold shoulder/collar piping, double gold chest clasps, and a warm Chestnut Leather
     shoulder/back X-harness with a Gold center ring.
   - Replaces the distracting white horizontal bar under the back belt (y=55) with a tailored
     Royal Navy & Gold-piped tunic waist hem, and adds neat front tunic points below the buckle.
2. Crimson/Scarlet Adventurer Cravat (Neckerchief) + Gold Gem Pin at the V-neck collar,
   plus Royal Navy & Gold cuff bands on the rolled-up sleeves.
3. Tailored Deep Slate-Charcoal Full Breeches + High-Cuff Buckled Leather Boots:
   - Eliminates the juvenile baggy boxer-shorts + bare orange knees + white tube socks look!
   - Tapers the outer thigh silhouette by 1px and dynamically transforms each leg column's
     (Knee + Sock) segment per frame into sleek fitted adventurer breeches tucked into
     folded-cuff Chestnut Leather & Brass-Buckled mid-calf adventurer boots (preserving lifted
     and striding feet across all 8 Walk frames!).

Female Novice (WCh_Body) Design Improvements:
1. Royal Navy & Gold-Laced Corset Bodice (y = 35..51 + dy):
   - Replaces the muddy dark-brown vest top so the warm Chestnut Leather cross-body satchel
     strap pops in 3D relief across Front, Side, and Back.
   - Cute Crimson-Rose Ribbon Bow + Gold Star Brooch at the V-neck blouse collar, plus soft
     Navy & Gold sleeve ribbon cuffs (eliminating the harsh black slash across her puff sleeves).
2. Brighter Sapphire-Navy & Gold Embroidered Pleated Skirt + Back Hem Outline Fix:
   - Adds a delicate gold/navy ribbon border along the white front apron hem (y=61..62 + dy).
   - Fixes the missing skirt-hem outline at y=64 + dy in Back view so the white ruffle hem
     no longer merges into the bare thighs below it.
3. Dynamic Per-Leg Zettai Ryouiki (절대영역 허벅지) + Frilly White Lace & Ribbon Stocking Tops
   + Glossy Navy-Charcoal Knee-High Stockings & Lace-Up Chestnut Boots across all Idle & Walk frames.
"""

import os
import shutil
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MALE_DIR = os.path.join(ROOT, "ArtSource", "Character", "Novice")
MALE_PART_DIR = os.path.join(ROOT, "ArtSource", "Character", "Male")
FEMALE_DIR = os.path.join(ROOT, "ArtSource", "Character", "Female")
MALE_BACKUP = os.path.join(ROOT, "Saved", "MaleSpriteBackup_20261014")
FEMALE_BACKUP = os.path.join(ROOT, "Saved", "FemaleNoviceOrig_20261014")
ARTIFACT_DIR = r"C:\Users\Ndus_072\.gemini\antigravity\brain\be1275f5-376f-4165-9ea2-1b959606f5ad"

FIELD_ANIMS = ["Idle_Front", "Walk_Front", "Idle_Side", "Walk_Side", "Idle_Back", "Walk_Back"]
CW, CH = 60, 100

MALE_DY = {
    "Idle_Front": [0, 0, 0, 1, 1, 1, 0, 0],
    "Idle_Side":  [0, 0, 0, 1, 1, 1, 0, 0],
    "Idle_Back":  [0, 0, 0, 1, 1, 1, 0, 0],
    "Walk_Front": [0, 1, 0, -1, 0, 1, 0, -1],
    "Walk_Side":  [0, 1, 0, -1, 0, 1, 0, -1],
    "Walk_Back":  [0, 1, 0, -1, 0, 1, 0, -1],
}

FEMALE_DY = {
    "Idle_Front": [0, 0, 1, 1, 1, 0, 0, 0],
    "Idle_Side":  [0, 0, 1, 1, 1, 0, 0, 0],
    "Idle_Back":  [0, 0, 1, 1, 1, 0, 0, 0],
    "Walk_Front": [0, 1, 0, -1, 0, 1, 0, -1],
    "Walk_Side":  [0, 1, 0, -1, 0, 1, 0, -1],
    "Walk_Back":  [0, 1, 0, -1, 0, 1, 0, -1],
}


def hex_to_rgba(h, a=255):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


def rgb_to_hex(rgb):
    return "#{:02x}{:02x}{:02x}".format(int(rgb[0]), int(rgb[1]), int(rgb[2]))


# ==============================================================================
# 1. MALE NOVICE UPGRADE PALETTE & TRANSFORMATION
# ==============================================================================
# Royal Navy Tunic (Vest)
NAVY_1 = hex_to_rgba("#182038")  # Deepest navy shadow
NAVY_2 = hex_to_rgba("#243256")  # Dark royal navy
NAVY_3 = hex_to_rgba("#324676")  # Mid royal navy
NAVY_4 = hex_to_rgba("#425c96")  # Bright royal navy
NAVY_5 = hex_to_rgba("#5470ac")  # Highlight royal navy

# Tailored Slate-Charcoal Adventurer Breeches (Pants)
PANT_1 = hex_to_rgba("#161924")  # Deepest slate shadow
PANT_2 = hex_to_rgba("#212636")  # Dark slate-navy
PANT_3 = hex_to_rgba("#2c3347")  # Mid slate-navy
PANT_4 = hex_to_rgba("#38415a")  # Light slate-navy
PANT_5 = hex_to_rgba("#45506c")  # Highlight slate-navy

# Gold / Brass Trim & Clasps
GOLD_1 = hex_to_rgba("#9c6b24")  # Dark antique gold
GOLD_2 = hex_to_rgba("#cf9638")  # Mid brass gold
GOLD_3 = hex_to_rgba("#f2c45a")  # Bright gold highlight
GOLD_4 = hex_to_rgba("#ffdf85")  # Specular gold glint

# Crimson Adventurer Scarf / Cravat
SCARF_1 = hex_to_rgba("#6e1824")  # Deep crimson shadow
SCARF_2 = hex_to_rgba("#a82434")  # Mid crimson
SCARF_3 = hex_to_rgba("#d93b4b")  # Bright scarlet highlight

# Warm Chestnut Leather (Belt, Pouches, Boots, Straps)
LEATH_1 = hex_to_rgba("#2e1810")
LEATH_2 = hex_to_rgba("#4a2719")
LEATH_3 = hex_to_rgba("#6b3923")
LEATH_4 = hex_to_rgba("#8c4d2d")
LEATH_5 = hex_to_rgba("#b0663c")
LEATH_6 = hex_to_rgba("#d48753")

# Folded Cream Boot-Liner Cuff
CUFF_1 = hex_to_rgba("#b89e82")
CUFF_2 = hex_to_rgba("#dfcbb0")
CUFF_3 = hex_to_rgba("#f7ebd9")

# Male Gray Fabric Colors (across Idle and Walk_Side)
MALE_GRAY_DARKEST = {"#262022", "#272121"}
MALE_GRAY_DARK    = {"#393131", "#3c3333"}
MALE_GRAY_MID     = {"#443c39"}
MALE_GRAY_LIGHT   = {"#544a46", "#514744"}
MALE_GRAY_BRIGHT  = {"#5d534d"}
MALE_GRAY_ALL     = MALE_GRAY_DARKEST | MALE_GRAY_DARK | MALE_GRAY_MID | MALE_GRAY_LIGHT | MALE_GRAY_BRIGHT

# Male Knee Skin + White Sock colors in y >= 70
MALE_KNEE_SOCK_HEX = {
    "#e57a4b", "#f8a876", "#fbb78c", "#d1a286", "#e4bb9d", "#eac8b0",
    "#fde1bd", "#fdf3db", "#fedeb4", "#fee5be", "#fceacb", "#fceacc",
    "#d9a78d", "#f0c5aa", "#fae2cd", "#fdf3dd", "#ca947c", "#b4806c",
    "#c8907b", "#ba8676",
}


def is_male_gray_fabric(r, g, b, hx):
    if hx in MALE_GRAY_ALL:
        return True
    mx, mn = max(int(r), int(g), int(b)), min(int(r), int(g), int(b))
    return (32 <= mx <= 95) and ((mx - mn) <= 12)


def is_male_knee_or_sock(r, g, b, hx):
    if hx in MALE_KNEE_SOCK_HEX:
        return True
    return (int(r) >= 175 and int(g) >= 115 and int(b) >= 70)


def upgrade_male_sheet(anim):
    src_path = os.path.join(MALE_BACKUP, f"MCh_Body_{anim}_Sheet.png")
    im = Image.open(src_path).convert("RGBA")
    arr = np.array(im)
    out = arr.copy()
    direction = anim.split("_")[1]  # Front, Side, Back
    dys = MALE_DY[anim]

    for f in range(8):
        x0 = f * CW
        dy = dys[f]
        cell = arr[:, x0:x0 + CW, :]
        new_cell = out[:, x0:x0 + CW, :]

        # Step 1: Remap upper Tunic Vest (ry <= 55) and lower Breeches (ry >= 56)
        for y in range(33, 96):
            ry = y - dy
            for x in range(CW):
                r, g, b, a = cell[y, x]
                if a == 0:
                    continue
                hx = rgb_to_hex((r, g, b))

                # 1A. Upper Tunic Vest (ry <= 55): Royal Navy
                if ry <= 55:
                    if hx in MALE_GRAY_DARKEST:
                        new_cell[y, x] = NAVY_1
                    elif hx in MALE_GRAY_DARK:
                        new_cell[y, x] = NAVY_2
                    elif hx in MALE_GRAY_MID:
                        new_cell[y, x] = NAVY_3
                    elif hx in MALE_GRAY_LIGHT:
                        new_cell[y, x] = NAVY_4
                    elif hx in MALE_GRAY_BRIGHT:
                        new_cell[y, x] = NAVY_5
                    elif is_male_gray_fabric(r, g, b, hx):
                        new_cell[y, x] = NAVY_3

                    # Warm up shoulder & back leather straps / belt
                    elif hx in ("#533d31", "#432f24"):
                        new_cell[y, x] = LEATH_3
                    elif hx in ("#7b533f", "#623d2c", "#674032", "#513a2c"):
                        new_cell[y, x] = LEATH_4
                    elif hx in ("#9d613b", "#814c31", "#815341", "#9c6754"):
                        new_cell[y, x] = LEATH_5
                    elif hx in ("#c38557", "#ab866b"):
                        new_cell[y, x] = GOLD_2 if (ry in (52, 53, 54) and 27 <= x <= 32) else LEATH_6
                    elif hx == "#e9b780":
                        new_cell[y, x] = GOLD_3

                # 1B. Lower Breeches (ry >= 56): Tailored Slate-Charcoal
                else:
                    if hx in MALE_GRAY_DARKEST:
                        new_cell[y, x] = PANT_1
                    elif hx in MALE_GRAY_DARK:
                        new_cell[y, x] = PANT_2
                    elif hx in MALE_GRAY_MID:
                        new_cell[y, x] = PANT_3
                    elif hx in MALE_GRAY_LIGHT:
                        new_cell[y, x] = PANT_4
                    elif hx in MALE_GRAY_BRIGHT:
                        new_cell[y, x] = PANT_5
                    elif is_male_gray_fabric(r, g, b, hx):
                        new_cell[y, x] = PANT_3

                    # Enhance existing shoe leather & buckles
                    elif ry >= 76:
                        if hx in ("#35231b", "#211612", "#2e1614", "#322118"):
                            new_cell[y, x] = LEATH_1
                        elif hx in ("#4a2b1e", "#432f24", "#533d31", "#4d2b22", "#513a2c"):
                            new_cell[y, x] = LEATH_2
                        elif hx in ("#5a331f", "#623d2c", "#674032"):
                            new_cell[y, x] = LEATH_3
                        elif hx in ("#7b533f", "#814c31", "#815341"):
                            new_cell[y, x] = LEATH_4
                        elif hx in ("#9d613b", "#ab866b", "#9c6754"):
                            new_cell[y, x] = LEATH_5
                        elif hx in ("#c38557", "#d1a286", "#b4806c"):
                            new_cell[y, x] = GOLD_2
                        elif hx in ("#e9b780", "#eac8b0", "#fde1bd", "#fdf3db", "#fae2cd", "#fdf3dd"):
                            new_cell[y, x] = GOLD_3 if ry <= 91 else LEATH_5

        # Step 2: Clean up Back & Side waist belt undershirt bands (ry = 48..58)
        # In original Back view, the vest stopped at y=48 leaving white undershirt bands
        # both ABOVE the belt (y=49..51) and BELOW the belt (y=55..57). Extend the Royal Navy
        # tunic seamlessly to the belt and turn the underside into a tailored Navy/Gold hem!
        if direction == "Back":
            for y in range(48, 59):
                ry = y - dy
                for x in range(23, 37):
                    r, g, b, a = cell[y, x]
                    if a == 0:
                        continue
                    hx = rgb_to_hex((r, g, b))
                    if hx in ("#fdf3db", "#fde1bd", "#eac8b0", "#e4bb9d", "#d1a286", "#ab866b"):
                        if y <= 52:
                            new_cell[y, x] = NAVY_4 if (27 <= x <= 32) else NAVY_3
                        else:
                            # Underside of belt: neat Navy tunic hem with subtle gold piping
                            new_cell[y, x] = GOLD_2 if (y == 55 + dy and 26 <= x <= 33) else NAVY_2
        elif direction == "Front":
            for ry in (55, 56, 57):
                y = ry + dy
                for x in range(25, 35):
                    if cell[y, x, 3] == 0:
                        continue
                    hx = rgb_to_hex(cell[y, x, :3])
                    if hx in MALE_GRAY_ALL:
                        if ry == 56 and x in (26, 27, 32, 33):
                            new_cell[y, x] = GOLD_2
                        elif ry == 55 and x in (25, 26, 27, 32, 33, 34):
                            new_cell[y, x] = NAVY_3
                        elif x in (29, 30):
                            new_cell[y, x] = NAVY_1

        # Step 3: Taper overly wide outer corners of the shorts by 1px in Front/Back (ry = 63..73)
        if direction in ("Front", "Back"):
            for ry in range(63, 74):
                y = ry + dy
                if not (0 <= y < CH):
                    continue
                xs = [x for x in range(16, 44) if new_cell[y, x, 3] > 0]
                if len(xs) >= 21:
                    xl, xr = xs[0], xs[-1]
                    if xl <= 18:
                        new_cell[y, xl] = (0, 0, 0, 0)
                        new_cell[y, xl + 1] = (12, 8, 6, 255)
                        new_cell[y, xl + 2] = PANT_2
                    if xr >= 40:
                        new_cell[y, xr] = (0, 0, 0, 0)
                        new_cell[y, xr - 1] = (12, 8, 6, 255)
                        new_cell[y, xr - 2] = PANT_2

        # Step 4: Dynamic Per-Column Knee + Sock Transformation (measuring UP from the shoe!)
        # By measuring dist_from_boot = y_bot - y (distance up from the top of the shoe in column x),
        # the 2 rows directly above the shoe are ALWAYS the Leather Boot Shaft, the 1-2 rows above that
        # are ALWAYS the Cream Boot Cuff, and everything above that up to the shorts is ALWAYS Breeches!
        for x in range(14, 46):
            ks_ys = []
            for y in range(max(68, 70 + dy), 86):
                r, g, b, a = cell[y, x]
                if a == 0:
                    continue
                hx = rgb_to_hex((r, g, b))
                if is_male_knee_or_sock(r, g, b, hx):
                    ks_ys.append(y)
                elif ks_ys and (y - ks_ys[-1] == 1) and hx in ("#000000", "#0c0806", "#211612", "#35231b", "#120708", "#050504", "#9d613b"):
                    if y + 1 < 86 and cell[y + 1, x, 3] > 0:
                        hx_next = rgb_to_hex(cell[y + 1, x, :3])
                        if is_male_knee_or_sock(cell[y + 1, x, 0], cell[y + 1, x, 1], cell[y + 1, x, 2], hx_next):
                            ks_ys.append(y)

            if not ks_ys:
                continue

            y_top, y_bot = ks_ys[0], ks_ys[-1]
            span = y_bot - y_top + 1

            # Erase the 1px dark horizontal line between shorts and bare knee (y_top - 1)
            for y_join in (y_top - 1, y_top - 2):
                if y_join >= 68 and cell[y_join, x, 3] > 0 and cell[y_join - 1, x, 3] > 0:
                    hx_j = rgb_to_hex(cell[y_join, x, :3])
                    hx_above = rgb_to_hex(cell[y_join - 1, x, :3])
                    if hx_j in ("#000000", "#0c0806", "#211612", "#120708", "#050504", "#9d613b") and is_male_gray_fabric(
                        cell[y_join - 1, x, 0], cell[y_join - 1, x, 1], cell[y_join - 1, x, 2], hx_above
                    ):
                        new_cell[y_join, x] = PANT_3

            for y in range(y_top, y_bot + 1):
                if cell[y, x, 3] == 0:
                    continue
                hx_orig = rgb_to_hex(cell[y, x, :3])
                if hx_orig in ("#000000", "#050504"):
                    left_empty = (x - 1 < 0) or (cell[y, x - 1, 3] == 0)
                    right_empty = (x + 1 >= CW) or (cell[y, x + 1, 3] == 0)
                    if left_empty or right_empty:
                        continue

                dist_from_boot = y_bot - y
                is_center = (x in (23, 24, 25, 34, 35, 36)) if direction != "Side" else (28 <= x <= 32)
                is_inner_edge = (x in (21, 22, 27, 32, 37, 38)) if direction != "Side" else (x in (25, 26, 34, 35))

                # Bottom 2 rows above shoe: High Leather Boot Shaft + Brass Buckle
                if dist_from_boot in (0, 1):
                    if dist_from_boot == 0 and ((direction != "Side" and x in (24, 35)) or (direction == "Side" and x in (30, 31))):
                        new_cell[y, x] = GOLD_3
                    elif is_center:
                        new_cell[y, x] = LEATH_5
                    elif is_inner_edge:
                        new_cell[y, x] = LEATH_2
                    else:
                        new_cell[y, x] = LEATH_4
                # Next 1-2 rows above boot shaft: Folded Cream Boot-Liner Cuff
                elif (span >= 6 and dist_from_boot in (2, 3)) or (span < 6 and dist_from_boot == 2):
                    if is_center:
                        new_cell[y, x] = CUFF_3
                    elif is_inner_edge:
                        new_cell[y, x] = CUFF_1
                    else:
                        new_cell[y, x] = CUFF_2
                # All upper rows: Tailored Slate-Charcoal Breeches extending over the knee!
                else:
                    if is_center:
                        new_cell[y, x] = PANT_4
                    elif is_inner_edge:
                        new_cell[y, x] = PANT_2
                    else:
                        new_cell[y, x] = PANT_3

        # Step 5: Add Heroic Details to Upper Torso (Crimson Cravat + Gold Clasps + Sleeve Cuffs)
        if direction == "Front":
            new_cell[36 + dy, 28] = SCARF_2
            new_cell[36 + dy, 31] = SCARF_2
            new_cell[37 + dy, 28] = SCARF_2
            new_cell[37 + dy, 29] = SCARF_3
            new_cell[37 + dy, 30] = SCARF_3
            new_cell[37 + dy, 31] = SCARF_1
            new_cell[38 + dy, 28] = SCARF_1
            new_cell[38 + dy, 29] = GOLD_4
            new_cell[38 + dy, 30] = GOLD_2
            new_cell[38 + dy, 31] = SCARF_1
            new_cell[39 + dy, 29] = SCARF_3
            new_cell[39 + dy, 30] = SCARF_2
            new_cell[40 + dy, 29] = SCARF_2
            new_cell[40 + dy, 30] = SCARF_1

            for ry_c in (43, 47):
                new_cell[ry_c + dy, 28] = GOLD_2
                new_cell[ry_c + dy, 29] = GOLD_3
                new_cell[ry_c + dy, 30] = GOLD_2
                new_cell[ry_c + dy, 31] = GOLD_1

            for sx in (24, 25, 34, 35):
                if cell[36 + dy, sx, 3] > 0:
                    new_cell[36 + dy, sx] = GOLD_2

            for ry_s in (44, 45):
                y_s = ry_s + dy
                for sx in list(range(17, 22)) + list(range(38, 43)):
                    if cell[y_s, sx, 3] > 0:
                        hx_s = rgb_to_hex(cell[y_s, sx, :3])
                        if hx_s in ("#d1a286", "#ab866b", "#e4bb9d", "#eac8b0"):
                            new_cell[y_s, sx] = GOLD_2 if ry_s == 44 else NAVY_3

        elif direction == "Side":
            for ry_s, sx, col in [
                (37, 33, SCARF_3), (37, 34, SCARF_2),
                (38, 33, GOLD_3),  (38, 34, SCARF_3), (38, 35, SCARF_1),
                (39, 34, SCARF_2), (39, 35, SCARF_1),
            ]:
                if cell[ry_s + dy, sx, 3] > 0:
                    new_cell[ry_s + dy, sx] = col

            for ry_c in (43, 47):
                if cell[ry_c + dy, 34, 3] > 0:
                    new_cell[ry_c + dy, 34] = GOLD_3
                if cell[ry_c + dy, 35, 3] > 0:
                    new_cell[ry_c + dy, 35] = GOLD_2

            for sx in range(25, 32):
                y_s = 45 + dy
                if cell[y_s, sx, 3] > 0 and cell[y_s - 1, sx, 3] > 0:
                    hx_s = rgb_to_hex(cell[y_s, sx, :3])
                    hx_up = rgb_to_hex(cell[y_s - 1, sx, :3])
                    if hx_s in ("#d1a286", "#ab866b", "#917362", "#c8a189") and hx_up in ("#fdf3db", "#fef2de", "#fff3e0", "#fde1bd"):
                        new_cell[y_s, sx] = GOLD_2

        elif direction == "Back":
            new_cell[41 + dy, 29] = GOLD_3
            new_cell[41 + dy, 30] = GOLD_4
            new_cell[42 + dy, 29] = GOLD_2
            new_cell[42 + dy, 30] = GOLD_3
            for sx in range(28, 32):
                if cell[35 + dy, sx, 3] > 0:
                    new_cell[35 + dy, sx] = SCARF_2
            for ry_s in (44, 45):
                y_s = ry_s + dy
                for sx in list(range(17, 22)) + list(range(38, 43)):
                    if cell[y_s, sx, 3] > 0:
                        hx_s = rgb_to_hex(cell[y_s, sx, :3])
                        if hx_s in ("#d1a286", "#ab866b", "#e4bb9d"):
                            new_cell[y_s, sx] = GOLD_2 if ry_s == 44 else NAVY_3

        out[:, x0:x0 + CW, :] = new_cell

    dst_path = os.path.join(MALE_DIR, f"MCh_Body_{anim}_Sheet.png")
    Image.fromarray(out).save(dst_path)
    return out


# ==============================================================================
# 2. FEMALE NOVICE UPGRADE PALETTE & TRANSFORMATION
# ==============================================================================
F_NAVY_1 = hex_to_rgba("#181c30")
F_NAVY_2 = hex_to_rgba("#252d4a")
F_NAVY_3 = hex_to_rgba("#35426b")
F_NAVY_4 = hex_to_rgba("#4a5d91")
F_NAVY_5 = hex_to_rgba("#637ab5")

ROSE_1 = hex_to_rgba("#6e1828")
ROSE_2 = hex_to_rgba("#a8243c")
ROSE_3 = hex_to_rgba("#e0405a")
ROSE_4 = hex_to_rgba("#ff7a8e")

F_SKIN_1 = hex_to_rgba("#d49b82")
F_SKIN_2 = hex_to_rgba("#f1b795")
F_SKIN_3 = hex_to_rgba("#f9dccb")
F_SKIN_4 = hex_to_rgba("#fde8da")

STOCK_1 = hex_to_rgba("#171824")
STOCK_2 = hex_to_rgba("#232536")
STOCK_3 = hex_to_rgba("#31354d")
STOCK_4 = hex_to_rgba("#434966")

WHITE_1 = hex_to_rgba("#c8b6ab")
WHITE_2 = hex_to_rgba("#e6d5c8")
WHITE_3 = hex_to_rgba("#f7ece2")
WHITE_4 = hex_to_rgba("#fffaf5")


def upgrade_female_sheet(anim):
    src_path = os.path.join(FEMALE_BACKUP, f"WCh_Body_{anim}_Sheet.png")
    im = Image.open(src_path).convert("RGBA")
    arr = np.array(im)
    out = arr.copy()
    direction = anim.split("_")[1]
    dys = FEMALE_DY[anim]

    for f in range(8):
        x0 = f * CW
        dy = dys[f]
        cell = arr[:, x0:x0 + CW, :]
        new_cell = out[:, x0:x0 + CW, :]

        for y in range(34, 96):
            ry = y - dy
            for x in range(CW):
                r, g, b, a = cell[y, x]
                if a == 0:
                    continue
                hx = rgb_to_hex((r, g, b))

                # 1A. Upper Bodice & Sleeves (35 <= ry <= 50)
                if 35 <= ry <= 50:
                    is_strap = False
                    if direction == "Front" and (x - (ry - 36) * 0.65) >= 24.5 and hx in ("#653c2c", "#754938", "#905c41", "#976853", "#cb956d", "#dea671"):
                        is_strap = True
                    elif direction == "Side" and hx in ("#653c2c", "#754938", "#905c41", "#976853", "#cb956d", "#dea671"):
                        is_strap = True
                    elif direction == "Back" and abs((x - 24) - (ry - 36) * 0.7) <= 3.2 and hx in ("#653c2c", "#754938", "#905c41", "#976853", "#cb956d", "#dea671"):
                        is_strap = True

                    if is_strap:
                        if hx == "#653c2c":
                            new_cell[y, x] = LEATH_3
                        elif hx == "#754938":
                            new_cell[y, x] = LEATH_4
                        elif hx in ("#905c41", "#976853"):
                            new_cell[y, x] = LEATH_5
                        elif hx in ("#cb956d", "#dea671"):
                            new_cell[y, x] = GOLD_3
                    else:
                        if hx in ("#261916", "#301b17", "#1b1314"):
                            if 21 <= x <= 38:
                                new_cell[y, x] = F_NAVY_1
                        elif hx in ("#331f18", "#39241c", "#302730"):
                            if 21 <= x <= 38:
                                new_cell[y, x] = F_NAVY_2
                        elif hx == "#442d25":
                            if 21 <= x <= 38:
                                new_cell[y, x] = F_NAVY_3
                        elif hx in ("#503730", "#644c4a"):
                            if 21 <= x <= 38:
                                new_cell[y, x] = F_NAVY_4
                        elif hx in ("#653c2c", "#754938") and 22 <= x <= 37 and ry <= 48:
                            new_cell[y, x] = F_NAVY_3
                        elif hx in ("#cb956d", "#dea671"):
                            new_cell[y, x] = GOLD_3

                        if 44 <= ry <= 47 and ((x <= 22) or (x >= 37) or (direction == "Side" and 25 <= x <= 33)):
                            if hx in ("#000000", "#261916", "#331f18") and ry == 45:
                                left_in = (x > 0 and cell[y, x - 1, 3] > 0)
                                right_in = (x + 1 < CW and cell[y, x + 1, 3] > 0)
                                if left_in and right_in:
                                    new_cell[y, x] = F_NAVY_3 if (x % 2 == 0) else GOLD_2

                # 1B. Waist Belt, Satchel Bag, & Pleated Skirt (51 <= ry <= 64)
                elif 51 <= ry <= 64:
                    if hx == "#1f1f2f":
                        new_cell[y, x] = F_NAVY_1
                    elif hx == "#302730":
                        new_cell[y, x] = F_NAVY_2
                    elif hx == "#303451":
                        new_cell[y, x] = F_NAVY_3
                    elif hx == "#424b72":
                        new_cell[y, x] = F_NAVY_4
                    elif hx == "#6b5f66":
                        new_cell[y, x] = F_NAVY_5
                    elif hx == "#cb956d":
                        new_cell[y, x] = GOLD_2
                    elif hx == "#dea671":
                        new_cell[y, x] = GOLD_3

                # 1C. Enhance Chestnut Lace-Up Boots (only on warm leather pixels in ry >= 75)
                if ry >= 75:
                    if hx == "#653c2c":
                        new_cell[y, x] = LEATH_3
                    elif hx == "#754938":
                        new_cell[y, x] = LEATH_4
                    elif hx in ("#905c41", "#976853"):
                        new_cell[y, x] = LEATH_5
                    elif hx == "#cb956d":
                        new_cell[y, x] = GOLD_2
                    elif hx == "#dea671":
                        new_cell[y, x] = GOLD_3

        # Step 2: Dynamic Per-Column Stocking & Zettai Ryouiki Transformation
        # In Side view (especially Walk_Side diagonal strides), the thigh/stocking border is
        # always at ry = 67 (bare thigh) and ry = 68 (white lace top), while ry >= 69 is stocking!
        for x in range(15, 45):
            st_ys = []
            for y in range(max(66, 67 + dy), 86):
                r, g, b, a = cell[y, x]
                if a == 0:
                    continue
                hx = rgb_to_hex((r, g, b))
                if y >= 73 and hx in ("#fdf3e7", "#fce8d5", "#ebc9b4", "#d6b7aa", "#ecc3ab", "#cb956d", "#dea671", "#905c41", "#976853", "#754938", "#653c2c"):
                    break
                if hx in ("#644c4a", "#56403f", "#503730", "#442d25", "#302730"):
                    st_ys.append(y)
                elif hx == "#261916" and st_ys:
                    st_ys.append(y)

            if not st_ys:
                continue

            y_st_top, y_st_bot = st_ys[0], st_ys[-1]
            span = y_st_bot - y_st_top + 1

            is_center = (x in (24, 25, 34, 35)) if direction != "Side" else (x in (29, 30, 31))
            is_edge = (x in (22, 23, 27, 28, 32, 33, 37, 38)) if direction != "Side" else (x in (26, 27, 33, 34))

            for y in st_ys:
                hx_orig = rgb_to_hex(cell[y, x, :3])

                ry = y - dy
                if direction == "Side":
                    # In Side view, keep the thigh/lace line horizontal under the skirt (ry=67 thigh, ry=68 lace)
                    # so diagonal striding shins in Walk_Side don't get white/peach edges along the lower calf!
                    if ry <= 67:
                        new_cell[y, x] = F_SKIN_4 if is_center else (F_SKIN_2 if is_edge else F_SKIN_3)
                    elif ry == 68:
                        new_cell[y, x] = ROSE_3 if x == 30 else (WHITE_2 if is_edge else WHITE_4)
                    else:
                        if y == y_st_bot and hx_orig == "#261916":
                            new_cell[y, x] = STOCK_1
                        elif is_center:
                            new_cell[y, x] = STOCK_4
                        elif is_edge:
                            new_cell[y, x] = STOCK_2
                        else:
                            new_cell[y, x] = STOCK_3
                else:
                    rel = y - y_st_top
                    n_thigh_ext = 2 if span >= 11 else 1
                    if rel < n_thigh_ext:
                        if is_center:
                            new_cell[y, x] = F_SKIN_4
                        elif is_edge:
                            new_cell[y, x] = F_SKIN_2
                        else:
                            new_cell[y, x] = F_SKIN_3
                    elif rel == n_thigh_ext:
                        if x in (25, 35):
                            new_cell[y, x] = ROSE_3
                        elif is_edge:
                            new_cell[y, x] = WHITE_2
                        else:
                            new_cell[y, x] = WHITE_4
                    else:
                        if y == y_st_bot and hx_orig == "#261916":
                            new_cell[y, x] = STOCK_1
                        elif is_center:
                            new_cell[y, x] = STOCK_4
                        elif is_edge:
                            new_cell[y, x] = STOCK_2
                        else:
                            new_cell[y, x] = STOCK_3

        # Step 3: Add Delicate Details (Collar Ribbon Bow, Corset Lacing, Apron Trim, Back Skirt Fix)
        if direction == "Front":
            new_cell[36 + dy, 28] = ROSE_3
            new_cell[36 + dy, 31] = ROSE_3
            new_cell[37 + dy, 28] = ROSE_2
            new_cell[37 + dy, 29] = GOLD_4
            new_cell[37 + dy, 30] = GOLD_3
            new_cell[37 + dy, 31] = ROSE_2
            new_cell[38 + dy, 28] = ROSE_3
            new_cell[38 + dy, 29] = ROSE_2
            new_cell[38 + dy, 30] = ROSE_2
            new_cell[38 + dy, 31] = ROSE_1
            new_cell[39 + dy, 28] = ROSE_2
            new_cell[39 + dy, 31] = ROSE_2

            new_cell[41 + dy, 29] = GOLD_3
            new_cell[41 + dy, 30] = GOLD_2
            new_cell[44 + dy, 29] = GOLD_3
            new_cell[44 + dy, 30] = GOLD_2

            for ax in range(26, 35):
                if cell[61 + dy, ax, 3] > 0:
                    hx_a = rgb_to_hex(cell[61 + dy, ax, :3])
                    if hx_a in ("#fdf3e7", "#fce8d5", "#ebc9b4", "#d6b7aa"):
                        new_cell[61 + dy, ax] = GOLD_2 if (ax % 2 == 0) else F_NAVY_3

        elif direction == "Side":
            for ry_s, sx, col in [
                (37, 33, ROSE_3), (37, 34, GOLD_3),
                (38, 33, ROSE_2), (38, 34, ROSE_3), (38, 35, ROSE_1),
                (39, 34, ROSE_2),
            ]:
                if cell[ry_s + dy, sx, 3] > 0:
                    new_cell[ry_s + dy, sx] = col

        elif direction == "Back":
            for sx in range(21, 39):
                if cell[62 + dy, sx, 3] > 0 and rgb_to_hex(cell[62 + dy, sx, :3]) in ("#303451", "#424b72", "#302730"):
                    if sx % 3 == 0:
                        new_cell[62 + dy, sx] = GOLD_2
                if cell[63 + dy, sx, 3] > 0 and rgb_to_hex(cell[63 + dy, sx, :3]) in ("#fdf3e7", "#fce8d5", "#f9dccb", "#ecc3ab", "#d6b7aa"):
                    new_cell[63 + dy, sx] = WHITE_3
                if cell[64 + dy, sx, 3] > 0 and rgb_to_hex(cell[64 + dy, sx, :3]) in ("#fdf3e7", "#fce8d5", "#f9dccb", "#ecc3ab", "#d6b7aa", "#f5d0b5", "#f2cdb4"):
                    new_cell[64 + dy, sx] = F_NAVY_1

            new_cell[42 + dy, 29] = GOLD_3
            new_cell[42 + dy, 30] = GOLD_2

        out[:, x0:x0 + CW, :] = new_cell

    dst_path = os.path.join(FEMALE_DIR, f"WCh_Body_{anim}_Sheet.png")
    Image.fromarray(out).save(dst_path)
    return out


# ==============================================================================
# 3. GENERATE BEFORE vs AFTER COMPARISON BOARD
# ==============================================================================
def composite_character(body_sheet, hair_sheet, eye_sheet, frame=0):
    x0 = frame * CW
    canvas = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
    # Layer order: Body -> FaceEye -> Hair (so bangs layer cleanly over the forehead/brows!)
    for sheet in (body_sheet, eye_sheet, hair_sheet):
        if sheet is not None:
            part = sheet.crop((x0, 0, x0 + CW, CH))
            canvas.alpha_composite(part)
    return canvas


def load_part_sheet(gender, kind, idx, anim):
    if gender == "Male":
        if kind == "Hair":
            p = os.path.join(MALE_DIR, f"MCh_Hair{idx}_{anim}_Sheet.png")
            return Image.open(p).convert("RGBA")
        elif kind == "Eye":
            pref = "MCh_FaceEye" if idx == 1 else f"MCh_FaceEye{idx}"
            p = os.path.join(MALE_DIR, f"{pref}_{anim}_Sheet.png")
            return Image.open(p).convert("RGBA")
    else:
        if kind == "Hair":
            pref = "WCh_Hair" if idx == 1 else f"WCh_Hair{idx}"
            p = os.path.join(FEMALE_DIR, f"{pref}_{anim}_Sheet.png")
            return Image.open(p).convert("RGBA")
        elif kind == "Eye":
            if anim.endswith("_Back"):
                return None
            pref = "WCh_FaceEye" if idx == 1 else f"WCh_FaceEye{idx}"
            p = os.path.join(FEMALE_DIR, f"{pref}_{anim}_Sheet.png")
            return Image.open(p).convert("RGBA")
    return None


def load_font(size, bold=False):
    candidates = [
        r"C:\Windows\Fonts\malgunbd.ttf" if bold else r"C:\Windows\Fonts\malgun.ttf",
        r"C:\Windows\Fonts\arialbd.ttf" if bold else r"C:\Windows\Fonts\arial.ttf",
    ]
    for p in candidates:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def generate_showcase():
    W, H = 1920, 1060
    board = Image.new("RGBA", (W, H), (18, 20, 28, 255))
    draw = ImageDraw.Draw(board)

    f_title = load_font(28, True)
    f_sec = load_font(20, True)
    f_lbl = load_font(15, True)
    f_sub = load_font(14, False)

    draw.text((36, 22), "남·여 노비스(Novice) 캐릭터 의상 & 비율 디자인 업그레이드 (Before vs After)", font=f_title, fill=(255, 240, 195))

    panels = [
        ("남자 노비스 (Male): 네이비 금장 튜닉 + 크림슨 스카프 + 슬림 브리치즈 & 롱부츠", 30, 72, 915, 960, (110, 175, 255)),
        ("여자 노비스 (Female): 네이비 코르셋 보디스 + 크림슨 리본 브로치 + 레이스 절대영역 니삭스", 975, 72, 915, 960, (255, 150, 190)),
    ]

    for title, px, py, pw, ph, accent in panels:
        draw.rounded_rectangle([px, py, px + pw, py + ph], radius=14, fill=(26, 30, 42, 255), outline=accent, width=2)
        draw.text((px + 22, py + 14), title, font=f_sec, fill=accent)

    dirs = [("Front", "정면"), ("Side", "측면"), ("Back", "후면")]

    for g_idx, (gender, backup_dir, body_dir, prefix, px) in enumerate([
        ("Male", MALE_BACKUP, MALE_DIR, "MCh", 30),
        ("Female", FEMALE_BACKUP, FEMALE_DIR, "WCh", 975),
    ]):
        for d_idx, (d_en, d_ko) in enumerate(dirs):
            card_x = px + 22 + d_idx * 295
            card_y = 124
            card_w, card_h = 280, 420
            draw.rounded_rectangle([card_x, card_y, card_x + card_w, card_y + card_h], radius=10, fill=(34, 39, 54, 255), outline=(65, 75, 100), width=1)

            anim_name = f"Idle_{d_en}"
            orig_body = Image.open(os.path.join(backup_dir, f"{prefix}_Body_{anim_name}_Sheet.png")).convert("RGBA")
            new_body = Image.open(os.path.join(body_dir, f"{prefix}_Body_{anim_name}_Sheet.png")).convert("RGBA")
            hair1 = load_part_sheet(gender, "Hair", 1, anim_name)
            eye1 = load_part_sheet(gender, "Eye", 1, anim_name)

            comp_orig = composite_character(orig_body, hair1, eye1, 0)
            comp_new = composite_character(new_body, hair1, eye1, 0)

            z3_orig = comp_orig.crop((10, 8, 50, 96)).resize((120, 264), Image.Resampling.NEAREST)
            z3_new = comp_new.crop((10, 8, 50, 96)).resize((120, 264), Image.Resampling.NEAREST)

            draw.text((card_x + 28, card_y + 10), f"기존 ({d_en})", font=f_lbl, fill=(175, 185, 205))
            draw.text((card_x + 156, card_y + 10), f"개선 ({d_en})", font=f_lbl, fill=(255, 225, 130))

            board.alpha_composite(z3_orig, (card_x + 12, card_y + 32))
            board.alpha_composite(z3_new, (card_x + 146, card_y + 32))

            z4_orig = comp_orig.crop((15, 35, 45, 95)).resize((120, 120), Image.Resampling.NEAREST)
            z4_new = comp_new.crop((15, 35, 45, 95)).resize((120, 120), Image.Resampling.NEAREST)
            board.alpha_composite(z4_orig, (card_x + 12, card_y + 292))
            board.alpha_composite(z4_new, (card_x + 146, card_y + 292))

        combo_y = 562
        draw.text((px + 22, combo_y), f"{'남자' if gender == 'Male' else '여자'} 노비스 신규 의상 × 헤어 3종 & 표정 조합 프리뷰", font=f_sec, fill=(235, 240, 255))

        combos = [
            (1, 1, "헤어 1 + 표정 1 (기본)"),
            (1, 3, "헤어 1 + 표정 3 (> <)"),
            (2, 2, "헤어 2 + 표정 2 (고양이)"),
            (2, 4, "헤어 2 + 표정 4 (도도)"),
            (3, 3, "헤어 3 + 표정 3 (> <)"),
            (3, 5, "헤어 3 + 표정 5 (초롱)"),
        ]

        for h_idx in range(3):
            c_x = px + 22 + h_idx * 295
            c_y = combo_y + 34
            c_w, c_h = 280, 412
            draw.rounded_rectangle([c_x, c_y, c_x + c_w, c_h + c_y], radius=10, fill=(34, 39, 54, 255), outline=(65, 75, 100), width=1)

            for sub_i in range(2):
                hid, eid, label = combos[h_idx * 2 + sub_i]
                new_body = Image.open(os.path.join(body_dir, f"{prefix}_Body_Idle_Front_Sheet.png")).convert("RGBA")
                hair_s = load_part_sheet(gender, "Hair", hid, "Idle_Front")
                eye_s = load_part_sheet(gender, "Eye", eid, "Idle_Front")
                comp = composite_character(new_body, hair_s, eye_s, 0)
                z3 = comp.crop((10, 8, 50, 96)).resize((120, 264), Image.Resampling.NEAREST)
                board.alpha_composite(z3, (c_x + 14 + sub_i * 134, c_y + 18))

            draw.text((c_x + 18, c_y + 294), f"헤어 {h_idx + 1} + 표정 {combos[h_idx*2][1]} / 표정 {combos[h_idx*2+1][1]}", font=f_sub, fill=(210, 220, 240))

    out_path = os.path.join(ARTIFACT_DIR, "novice_design_upgrade_showcase.png")
    board.save(out_path)
    print(f"Saved: {out_path}")


def main():
    for anim in FIELD_ANIMS:
        upgrade_male_sheet(anim)
        upgrade_female_sheet(anim)
    print("Upgraded all 12 Male & Female Novice Field Body sheets!")
    generate_showcase()


if __name__ == "__main__":
    main()
