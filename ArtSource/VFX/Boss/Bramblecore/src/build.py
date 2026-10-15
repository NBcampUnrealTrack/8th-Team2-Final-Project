"""Render all effects -> sheets/ (PNG + JSON) and inspect/ contact strips."""
import sys, time
from core import *
import fx

OUT = sys.argv[1] if len(sys.argv) > 1 else "out/sheets"
only = sys.argv[2:]
BG = np.array((0.16, 0.20, 0.17), f32)


def contact(add_imgs, tr_imgs, path, step=1):
    imgs = []
    n = len(add_imgs or tr_imgs)
    for i in range(0, n, step):
        w, h = (add_imgs or tr_imgs)[i].size
        c = np.ones((h, w, 3), f32) * BG
        if tr_imgs:
            t = np.asarray(tr_imgs[i]).astype(f32) / 255
            c = c * (1 - t[..., 3:]) + t[..., :3] * t[..., 3:]
        if add_imgs:
            a = np.asarray(add_imgs[i]).astype(f32) / 255
            c = c + a[..., :3] * a[..., 3:]
        imgs.append(Image.fromarray((np.clip(c, 0, 1) * 255).astype(np.uint8)))
    cols = 8
    w, h = imgs[0].size
    rows = math.ceil(len(imgs) / cols)
    sheet = Image.new("RGB", (cols * w, rows * h), (10, 10, 10))
    for i, im in enumerate(imgs):
        sheet.paste(im, ((i % cols) * w, (i // cols) * h))
    sc = min(1.0, 3000 / sheet.width)
    sheet.resize((int(sheet.width * sc), int(sheet.height * sc)), Image.LANCZOS).save(path)


os.makedirs("out/inspect", exist_ok=True)
for name, (fn, fps, piv, scale, loop, note) in fx.EFFECTS.items():
    if only and name not in only:
        continue
    t0 = time.time()
    res = fn()
    add_imgs = tr_imgs = None
    if isinstance(res, tuple):
        add_imgs = [to_img(f.add_rgba()) for f in res[0]]
        tr_imgs = [to_img(f.trans_rgba()) for f in res[1]]
        save_sheet(OUT, f"T_Boss2_{name}_Glow", add_imgs, fps, piv, scale, loop, "Additive", note)
        save_sheet(OUT, f"T_Boss2_{name}_Dust", tr_imgs, fps, piv, scale, loop, "Translucent", note)
    elif "Dust" in name or "Leaves" in name:
        tr_imgs = [to_img(f.trans_rgba()) for f in res]
        save_sheet(OUT, f"T_Boss2_{name}", tr_imgs, fps, piv, scale, loop, "Translucent", note)
    else:
        add_imgs = [to_img(f.add_rgba()) for f in res]
        save_sheet(OUT, f"T_Boss2_{name}", add_imgs, fps, piv, scale, loop, "Additive", note)
    contact(add_imgs, tr_imgs, f"out/inspect/{name}.png")
    print(f"{name}: {len(add_imgs or tr_imgs)}f  {time.time()-t0:.1f}s", flush=True)
