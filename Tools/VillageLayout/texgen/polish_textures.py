"""Texture polish pass 1: smaller cobbles, flat cloth banner, calmer roof moss, chunkier terrain dots."""
import os, math
import numpy as np
from PIL import Image, ImageEnhance

SP = r"C:\Users\Ndus_072\AppData\Local\Temp\claude\C--Users-Ndus-072-RevDevTool-RevStudio-ProjectM\2d851b1b-c6b6-4a48-a0e8-226d39210886\scratchpad"
OUT = os.path.join(SP, "texfix")
os.makedirs(OUT, exist_ok=True)


def tile(img, n, size):
    """Repeat a seamless texture n x n and resample to size."""
    w, h = img.size
    big = Image.new(img.mode, (w * n, h * n))
    for i in range(n):
        for j in range(n):
            big.paste(img, (i * w, j * h))
    return big.resize((size, size), Image.LANCZOS)


# ---------- 1. cobblestone: 3x3 so a 10 m tile shows ~21 stones (~48 cm each) ----------
for suffix, src_dir in (("BC", os.path.join(SP, "pack_cc")), ("N", os.path.join(SP, "pack1024", "Normal")),
                        ("ORM", os.path.join(SP, "pack1024", "ORM")), ("H", os.path.join(SP, "pack1024", "Height"))):
    p = os.path.join(src_dir, f"T_Cobblestone_Pixel_{suffix}.png")
    im = Image.open(p)
    out = tile(im, 3, 1024)
    if suffix == "N":  # renormalize after resample
        a = np.asarray(out.convert("RGB")).astype(np.float32) / 255 * 2 - 1
        a /= np.maximum(np.linalg.norm(a, axis=2, keepdims=True), 1e-5)
        out = Image.fromarray(((a + 1) / 2 * 255).round().clip(0, 255).astype(np.uint8))
    out.save(os.path.join(OUT, f"T_Cobblestone_Pixel_{suffix}.png"))
print("cobble 3x3 done")


# ---------- 2. terrain base colors: more contrast + palette quantization for chunkier dots ----------
def chunky(src, dst, colors=40, contrast=1.22, block=2):
    im = Image.open(src).convert("RGB")
    im = ImageEnhance.Contrast(im).enhance(contrast)
    small = im.resize((im.width // block, im.height // block), Image.BOX)
    q = small.quantize(colors=colors, method=Image.MEDIANCUT, dither=Image.Dither.NONE).convert("RGB")
    q.resize((im.width, im.height), Image.NEAREST).save(dst)


for name in ("T_Grass_Meadow_Pixel_BC", "T_Moss_ForestFloor_Pixel_BC", "T_Dirt_Path_Pixel_BC"):
    chunky(os.path.join(SP, "pack_cc", f"{name}.png"), os.path.join(OUT, f"{name}.png"))
chunky(os.path.join(OUT, "T_Cobblestone_Pixel_BC.png"), os.path.join(OUT, "T_Cobblestone_Pixel_BC.png"), colors=48)
chunky(os.path.join(SP, "pack1024", "BaseColor", "T_Cliff_Rock_Pixel_BC.png"),
       os.path.join(OUT, "T_Cliff_Rock_Pixel_BC.png"), colors=40, contrast=1.15)
print("terrain quantize done")


# ---------- 3. banner cloth: flat woven blue, cream band, dark border ----------
N = 256
S = 4  # art-pixel size
g = N // S
a = np.zeros((g, g, 3), np.uint8)
BLUE = (38, 58, 140); BLUE_D = (26, 40, 104); BLUE_L = (52, 76, 168)
CREAM = (216, 202, 168); CREAM_D = (186, 170, 136)
rs = np.random.RandomState(9)
a[:, :] = BLUE
weave = (np.add.outer(np.arange(g), np.arange(g)) % 8 == 0)
a[weave] = BLUE_D
a[(np.add.outer(np.arange(g), np.arange(g)) % 8 == 4)] = BLUE_L
band = slice(int(g * 0.38), int(g * 0.62))
a[band, :] = CREAM
a[band, :][(np.add.outer(np.arange(band.stop - band.start), np.arange(g)) % 8 == 0)] = CREAM_D
a[int(g * 0.38) - 1:int(g * 0.38) + 1, :] = BLUE_D
a[int(g * 0.62) - 1:int(g * 0.62) + 1, :] = BLUE_D
noise = rs.randint(-6, 7, (g, g, 1))
a = np.clip(a.astype(np.int16) + noise, 0, 255).astype(np.uint8)
Image.fromarray(a).resize((N, N), Image.NEAREST).save(os.path.join(OUT, "T_Prop_Cloth_Pixel_BC.png"))
print("cloth done")


# ---------- 4. roof shingles: moss becomes broad drifts, greys pulled to warm dark olive ----------
im = Image.open(os.path.join(SP, "proptex", "T_Prop_RoofShingle_Pixel_BC.png")).convert("RGB")
a = np.asarray(im).astype(np.int16)
r, gg, b = a[..., 0], a[..., 1], a[..., 2]
lime = (gg > r + 30) & (gg > 90)
# fbm mask for where moss may stay
rs = np.random.RandomState(31)
mask = np.zeros((N, N), np.float32)
for freq, amp in ((2, 1.0), (4, 0.5)):
    s = rs.rand(freq, freq)
    s = np.vstack([np.hstack([s, s]), np.hstack([s, s])])  # keep it tileable
    mask += np.asarray(Image.fromarray((s * 255).astype(np.uint8)).resize((N, N), Image.BICUBIC), np.float32) / 255 * amp
mask /= 1.5
keep = mask > 0.55
out = a.copy()
# lime outside the drifts -> shingle grey; lime inside -> deep olive
shingle = np.array([96, 88, 66])
out[lime & ~keep] = shingle + rs.randint(-10, 11, (int((lime & ~keep).sum()), 3))
out[lime & keep] = np.array([76, 96, 46]) + rs.randint(-8, 9, (int((lime & keep).sum()), 3))
# pull bright greys toward warm dark olive-brown
lum = a.mean(axis=2)
bright = (lum > 120) & ~lime
out[bright] = (a[bright] * np.array([0.70, 0.68, 0.60])).astype(np.int16)
Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).save(os.path.join(OUT, "T_Prop_RoofShingle_Pixel_BC.png"))
print("roof done")


# ---------- 5. stone brick: cooler and darker mortar contrast ----------
im = Image.open(os.path.join(SP, "proptex", "T_Prop_StoneBrick_Pixel_BC.png")).convert("RGB")
a = np.asarray(im).astype(np.float32)
a = (a - 128) * 1.15 + 128
a *= np.array([0.92, 0.95, 1.0])
Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(OUT, "T_Prop_StoneBrick_Pixel_BC.png"))

# ---------- 6. leaf: deeper olive palette with a darker underside band ----------
im = Image.open(os.path.join(SP, "proptex", "T_Prop_Leaf_Pixel_BC.png")).convert("RGB")
a = np.asarray(im).astype(np.float32)
a = (a - 110) * 1.18 + 100
a *= np.array([0.95, 0.92, 0.70])
Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(OUT, "T_Prop_Leaf_Pixel_BC.png"))
print("stone/leaf done")

# ---------- contact sheet ----------
names = ["T_Cobblestone_Pixel_BC", "T_Grass_Meadow_Pixel_BC", "T_Moss_ForestFloor_Pixel_BC", "T_Dirt_Path_Pixel_BC",
         "T_Cliff_Rock_Pixel_BC", "T_Prop_Cloth_Pixel_BC", "T_Prop_RoofShingle_Pixel_BC", "T_Prop_StoneBrick_Pixel_BC",
         "T_Prop_Leaf_Pixel_BC"]
cell = 250
sheet = Image.new("RGB", (cell * len(names), cell), (20, 20, 20))
for i, n in enumerate(names):
    im = Image.open(os.path.join(OUT, f"{n}.png")).convert("RGB")
    crop = im.crop((0, 0, min(512, im.width), min(512, im.height))).resize((cell - 6, cell - 6), Image.BOX)
    sheet.paste(crop, (i * cell + 3, 3))
sheet.save(os.path.join(OUT, "_contact.png"))
print("sheet:", os.path.join(OUT, "_contact.png"))
print("files:", sorted(os.listdir(OUT)))
