# -*- coding: utf-8 -*-
"""Pictures for the job / starter Naia selection stage that come from existing art (run before apply_select).

  ArtSource/UI/HUD2026/select/T_HUD_JobEmblem_<JobKey>.png   job emblems (ArtSource/UI/JobEmblems_20260927, 1254 px ivory line art)
      at 256 px, turned white with the art's alpha, so the widget tints them with the job colour.
The reward and element icons are made by ArtSource/UI/HUD2026/reward_icons/finish.py (GPT pictures, item-icon tone).
Run: python Tools/HUDRenewal/make_select_art.py
"""
import pathlib

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / 'ArtSource' / 'UI' / 'JobEmblems_20260927'
OUT = ROOT / 'ArtSource' / 'UI' / 'HUD2026' / 'select'

OUT.mkdir(parents=True, exist_ok=True)
for path in sorted(SRC.glob('T_JobEmblem_*.png')):
    key = path.stem.replace('T_JobEmblem_', '')
    im = Image.open(path).convert('RGBA').resize((256, 256), Image.LANCZOS)
    a = np.asarray(im).copy()
    a[..., :3] = 255
    Image.fromarray(a).save(OUT / ('T_HUD_JobEmblem_%s.png' % key))
    print('emblem', key)
