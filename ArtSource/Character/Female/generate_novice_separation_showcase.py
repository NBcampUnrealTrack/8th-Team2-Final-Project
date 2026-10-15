# -*- coding: utf-8 -*-
"""
Generates visual comparison artifacts for Female Novice Head/Hair & Body Separation:
  1) novice_head_body_separation.png:
     - Top Panel: 3-Layer Disjoint Separation Breakdown (Original Unseparated vs Headless Body + FaceEye + Hair + Tinted Composite) across Front, Side, Back (Idle & Walk)
     - Bottom Panel: 8 Hair Color Swatches applied via M_NUCharacterHairTint to Female Novice (Idle_Front, Walk_Front, Walk_Side, Walk_Back)
  2) novice_hair_tint_anim.gif:
     - Animated 8-frame GIF showing Headless Body, Tinted Hair Layer, and Tinted Composite across Idle & Walk (Front, Side, Back)
"""
from PIL import Image, ImageDraw
import numpy as np

BASE = "ArtSource/Character/Female"
ORIG = "Saved/FemaleSpriteBackup_20261014"
OUT_DIR = "C:/Users/Ndus_072/.gemini/antigravity/brain/be1275f5-376f-4165-9ea2-1b959606f5ad"


def tint_hair_srgb(hair_rgba, hex_color):
    if hex_color is None:
        return hair_rgba.copy()
    out = hair_rgba.copy()
    mask = out[:, :, 3] > 0
    if not np.any(mask):
        return out
    rgb_srgb = out[:, :, :3].astype(np.float32) / 255.0
    rgb_lin = np.where(rgb_srgb <= 0.04045, rgb_srgb / 12.92, ((rgb_srgb + 0.055) / 1.055) ** 2.4)
    tc = tuple(int(hex_color[i:i+2], 16) / 255.0 for i in (0, 2, 4))
    tint_lin = np.array([c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in tc], dtype=np.float32)

    luma = rgb_lin[:, :, 0] * 0.2126 + rgb_lin[:, :, 1] * 0.7152 + rgb_lin[:, :, 2] * 0.0722
    shade = luma / 0.60
    shade_sat = np.clip(shade, 0.0, 1.0)[:, :, None]
    over_sat = np.clip(shade - 1.0, 0.0, 1.0)[:, :, None]
    base_c = shade_sat * tint_lin[None, None, :]
    tinted_lin = base_c + (1.0 - base_c) * over_sat * 0.35
    tinted_srgb = np.where(tinted_lin <= 0.0031308, tinted_lin * 12.92, 1.055 * (np.clip(tinted_lin, 0.0, 1.0) ** (1.0 / 2.4)) - 0.055)
    out[:, :, :3] = np.clip(np.round(tinted_srgb * 255.0), 0, 255).astype(np.uint8)
    out[~mask] = 0
    return out


def make_static_showcase():
    scale = 4
    cw, ch = 60 * scale, 98 * scale
    anims = [
        ("Idle_Front", 0),
        ("Walk_Front", 1),
        ("Idle_Side", 0),
        ("Walk_Side", 1),
        ("Idle_Back", 0),
        ("Walk_Back", 1),
    ]
    cols = [
        "1. Headless Body\n(WCh_Body_*)",
        "2. FaceEye Layer\n(WCh_FaceEye_*)",
        "3. Hair Layer\n(WCh_Hair_*)",
        "4. Composite\n(Original Blonde)",
        "5. Composite\n(Blue Tint #3F6FB5)",
        "6. Composite\n(Crimson #B53B3B)",
        "7. Composite\n(Rose Pink #C96B8B)",
        "8. Composite\n(Silver #DCE0EC)",
    ]

    pad_top = 56
    pad_left = 150
    w_total = pad_left + len(cols) * cw + 20
    h_total = pad_top + len(anims) * ch + 20

    canvas = Image.new("RGBA", (w_total, h_total), (24, 28, 36, 255))
    draw = ImageDraw.Draw(canvas)

    for ci, col_title in enumerate(cols):
        x0 = pad_left + ci * cw
        draw.text((x0 + 8, 12), col_title, fill=(220, 230, 245, 255))

    for ri, (anim, f_idx) in enumerate(anims):
        y0 = pad_top + ri * ch
        has_face = "Back" not in anim
        b_sheet = np.array(Image.open(f"{BASE}/WCh_Body_{anim}_Sheet.png").convert("RGBA"))
        h_sheet = np.array(Image.open(f"{BASE}/WCh_Hair_{anim}_Sheet.png").convert("RGBA"))
        f_sheet = np.array(Image.open(f"{BASE}/WCh_FaceEye_{anim}_Sheet.png").convert("RGBA")) if has_face else np.zeros_like(b_sheet)

        fx0, fx1 = f_idx * 60, (f_idx + 1) * 60
        b_cell = Image.fromarray(b_sheet[:98, fx0:fx1])
        f_cell = Image.fromarray(f_sheet[:98, fx0:fx1])
        h_cell = Image.fromarray(h_sheet[:98, fx0:fx1])

        comp_orig = Image.alpha_composite(Image.alpha_composite(b_cell, f_cell), h_cell)
        comp_blue = Image.alpha_composite(Image.alpha_composite(b_cell, f_cell), Image.fromarray(tint_hair_srgb(h_sheet[:98, fx0:fx1], "3F6FB5")))
        comp_red = Image.alpha_composite(Image.alpha_composite(b_cell, f_cell), Image.fromarray(tint_hair_srgb(h_sheet[:98, fx0:fx1], "B53B3B")))
        comp_pink = Image.alpha_composite(Image.alpha_composite(b_cell, f_cell), Image.fromarray(tint_hair_srgb(h_sheet[:98, fx0:fx1], "C96B8B")))
        comp_silver = Image.alpha_composite(Image.alpha_composite(b_cell, f_cell), Image.fromarray(tint_hair_srgb(h_sheet[:98, fx0:fx1], "DCE0EC")))

        cells = [b_cell, f_cell, h_cell, comp_orig, comp_blue, comp_red, comp_pink, comp_silver]
        draw.text((12, y0 + ch // 2 - 12), f"{anim}\n(Frame {f_idx})", fill=(180, 235, 175, 255))

        for ci, cell_img in enumerate(cells):
            x0 = pad_left + ci * cw
            bg_color = (32, 37, 48, 255) if ci < 3 else (38, 44, 56, 255)
            draw.rectangle([x0, y0, x0 + cw - 1, y0 + ch - 1], fill=bg_color, outline=(62, 70, 88, 255))
            scaled = cell_img.resize((cw, ch), Image.Resampling.NEAREST)
            canvas.alpha_composite(scaled, (x0, y0))

    out_png = f"{OUT_DIR}/novice_head_body_separation.png"
    canvas.save(out_png)
    print("Saved", out_png)


def make_animated_gif():
    scale = 4
    cw, ch = 60 * scale, 98 * scale
    anims = ["Idle_Front", "Walk_Front", "Walk_Side", "Walk_Back"]
    cols = [
        ("Headless Body", None, "body"),
        ("Separated Hair", "3F6FB5", "hair"),
        ("Original Blonde", None, "comp"),
        ("Cobalt Blue (#3F6FB5)", "3F6FB5", "comp"),
        ("Crimson (#B53B3B)", "B53B3B", "comp"),
        ("Rose Pink (#C96B8B)", "C96B8B", "comp"),
    ]

    pad_top = 42
    pad_left = 110
    w_total = pad_left + len(cols) * cw + 12
    h_total = pad_top + len(anims) * ch + 12

    sheets = {}
    for anim in anims:
        has_face = "Back" not in anim
        b = np.array(Image.open(f"{BASE}/WCh_Body_{anim}_Sheet.png").convert("RGBA"))
        h = np.array(Image.open(f"{BASE}/WCh_Hair_{anim}_Sheet.png").convert("RGBA"))
        f = np.array(Image.open(f"{BASE}/WCh_FaceEye_{anim}_Sheet.png").convert("RGBA")) if has_face else np.zeros_like(b)
        sheets[anim] = (b, f, h)

    frames = []
    for f_idx in range(8):
        frame_img = Image.new("RGBA", (w_total, h_total), (24, 28, 36, 255))
        draw = ImageDraw.Draw(frame_img)
        for ci, (label, _, _) in enumerate(cols):
            x0 = pad_left + ci * cw
            draw.text((x0 + 6, 12), label, fill=(220, 230, 245, 255))

        fx0, fx1 = f_idx * 60, (f_idx + 1) * 60
        for ri, anim in enumerate(anims):
            y0 = pad_top + ri * ch
            draw.text((10, y0 + ch // 2 - 8), anim, fill=(180, 235, 175, 255))
            b_arr, f_arr, h_arr = sheets[anim]
            b_c = Image.fromarray(b_arr[:98, fx0:fx1])
            f_c = Image.fromarray(f_arr[:98, fx0:fx1])

            for ci, (_, hex_c, kind) in enumerate(cols):
                x0 = pad_left + ci * cw
                draw.rectangle([x0, y0, x0 + cw - 1, y0 + ch - 1], fill=(34, 39, 50, 255), outline=(60, 68, 84, 255))
                if kind == "body":
                    cell = b_c
                elif kind == "hair":
                    cell = Image.fromarray(tint_hair_srgb(h_arr[:98, fx0:fx1], hex_c))
                else:
                    h_t = Image.fromarray(tint_hair_srgb(h_arr[:98, fx0:fx1], hex_c))
                    cell = Image.alpha_composite(Image.alpha_composite(b_c, f_c), h_t)
                frame_img.alpha_composite(cell.resize((cw, ch), Image.Resampling.NEAREST), (x0, y0))

        frames.append(frame_img.convert("RGB"))

    out_gif = f"{OUT_DIR}/novice_hair_tint_anim.gif"
    frames[0].save(out_gif, save_all=True, append_images=frames[1:], duration=125, loop=0)
    print("Saved", out_gif)


if __name__ == "__main__":
    make_static_showcase()
    make_animated_gif()
