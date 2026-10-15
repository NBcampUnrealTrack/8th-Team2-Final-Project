from PIL import Image, ImageDraw
import numpy as np
from improve_female_sprites import (
    separate_novice_sheet,
    improve_novice_field,
    FIELD_ANIMS,
)

base = "ArtSource/Character/Female"

def tint_hair_srgb(hair_rgba, hex_color):
    # Simulate M_NUCharacterHairTint
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

# 1. Generate and save all 16 separated Female Novice sheets (6 Body, 6 Hair, 4 FaceEye)
improve_novice_field()

# 2. Generate crisp square 48x48 PartIcons for Female Hair and FaceEye
h_front0 = np.array(Image.open(f"{base}/WCh_Hair_Idle_Front_Sheet.png").convert("RGBA"))[:, :60]
f_front0 = np.array(Image.open(f"{base}/WCh_FaceEye_Idle_Front_Sheet.png").convert("RGBA"))[:, :60]

# Hair bounding box is x=5..42 (38px), y=2..47 (46px) -> center neatly inside 48x48 icon
hair_icon = np.zeros((48, 48, 4), dtype=np.uint8)
hair_icon[1:47, 5:43] = h_front0[2:48, 5:43]
Image.fromarray(hair_icon).save(f"{base}/T_PartIcon_Hair_Female_1_Front.png")

# FaceEye bounding box is x=22..38 (17px), y=24..34 (11px) -> center neatly inside 28x28 icon
face_icon = np.zeros((28, 28, 4), dtype=np.uint8)
face_icon[8:20, 5:23] = f_front0[23:35, 21:39]
Image.fromarray(face_icon).save(f"{base}/T_PartIcon_Face_Female_1_Front.png")

# 3. Render diagnostic verification sheet
scale = 5
w_cell = 60 * scale
h_cell = 70 * scale
canvas = Image.new("RGBA", (8 * w_cell, len(FIELD_ANIMS) * 2 * h_cell), (28, 32, 40, 255))
draw = ImageDraw.Draw(canvas)

for ai, anim in enumerate(FIELD_ANIMS):
    has_face = "Back" not in anim
    b_arr = np.array(Image.open(f"{base}/WCh_Body_{anim}_Sheet.png").convert("RGBA"))
    h_arr = np.array(Image.open(f"{base}/WCh_Hair_{anim}_Sheet.png").convert("RGBA"))
    f_arr = np.array(Image.open(f"{base}/WCh_FaceEye_{anim}_Sheet.png").convert("RGBA")) if has_face else np.zeros_like(b_arr)

    h_blue = tint_hair_srgb(h_arr, "3F6FB5")
    b_img = Image.fromarray(b_arr)
    f_img = Image.fromarray(f_arr)
    hb_img = Image.fromarray(h_blue)
    comp_blue = Image.alpha_composite(Image.alpha_composite(b_img, f_img), hb_img)

    for f in range(8):
        x0 = f * 60
        b_crop = b_img.crop((x0, 0, x0 + 60, 70)).resize((w_cell, h_cell), Image.Resampling.NEAREST)
        c_crop = comp_blue.crop((x0, 0, x0 + 60, 70)).resize((w_cell, h_cell), Image.Resampling.NEAREST)
        r0 = ai * 2
        canvas.alpha_composite(b_crop, (f * w_cell, r0 * h_cell))
        canvas.alpha_composite(c_crop, (f * w_cell, (r0 + 1) * h_cell))
        draw.rectangle([f * w_cell, r0 * h_cell, (f + 1) * w_cell - 1, (r0 + 2) * h_cell - 1], outline=(65, 70, 85, 255))
    draw.text((6, ai * 2 * h_cell + 4), f"{anim} - Headless Body", fill=(180, 255, 160, 255))
    draw.text((6, (ai * 2 + 1) * h_cell + 4), f"{anim} - Composite w/ Blue Hair Tint (#3F6FB5)", fill=(120, 220, 255, 255))

out_path = "C:/Users/Ndus_072/.gemini/antigravity/brain/be1275f5-376f-4165-9ea2-1b959606f5ad/scratch_novice_separated_test.png"
canvas.save(out_path)
print("Saved 16 separated Novice sheets + 2 PartIcons +", out_path)

