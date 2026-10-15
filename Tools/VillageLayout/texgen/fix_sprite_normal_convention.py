"""Flip R and G on the exported player normal maps so they match the NPC convention, and record the asset path for each."""
import glob
import json
import os

import numpy as np
from PIL import Image

SP = r"C:\Users\Ndus_072\AppData\Local\Temp\claude\C--Users-Ndus-072-RevDevTool-RevStudio-ProjectM\2d851b1b-c6b6-4a48-a0e8-226d39210886\scratchpad"
ROOT = os.path.join(SP, "fixnm")
OUT = os.path.join(SP, "fixnm_out")
os.makedirs(OUT, exist_ok=True)
for stale in glob.glob(os.path.join(OUT, "*.png")):
    os.remove(stale)

files = [p for p in glob.glob(os.path.join(ROOT, "Game", "**", "*"), recursive=True) if p.lower().endswith(".png")]
mapping = {}
before, after = [], []


def edge_bias(arr):
    h, w, _ = arr.shape
    return float(arr[: max(1, h // 5), :, 1].mean()), float(arr[:, : max(1, w // 5), 0].mean())


for i, f in enumerate(sorted(files)):
    rel = os.path.relpath(f, ROOT).replace(os.sep, "/")
    asset = "/" + os.path.splitext(rel)[0]          # -> /Game/Art/Sprites/Player/.../<name>_N
    name = os.path.basename(asset)
    a = np.asarray(Image.open(f).convert("RGB")).astype(np.int16)
    before.append(edge_bias(a.astype(np.float32)))
    flat = (np.abs(a[..., 0] - 128) < 2) & (np.abs(a[..., 1] - 128) < 2)
    a[..., 0] = 255 - a[..., 0]
    a[..., 1] = 255 - a[..., 1]
    a[flat] = [128, 128, 255]
    fixed = np.clip(a, 0, 255).astype(np.uint8)
    after.append(edge_bias(fixed.astype(np.float32)))
    dst = os.path.join(OUT, f"{i:03d}_{name}.png").replace(os.sep, "/")
    Image.fromarray(fixed).save(dst)
    mapping[dst] = {"asset": asset, "dir": os.path.dirname(asset), "name": name}

json.dump(mapping, open(os.path.join(OUT, "_map.json"), "w"), indent=0)
b, af = np.array(before), np.array(after)
print("mapped", len(mapping))
print("top-G   %.1f -> %.1f" % (b[:, 0].mean(), af[:, 0].mean()))
print("left-R  %.1f -> %.1f" % (b[:, 1].mean(), af[:, 1].mean()))
print("sample", list(mapping.values())[0])
