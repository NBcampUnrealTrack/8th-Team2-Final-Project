# -*- coding: utf-8 -*-
"""Reward (experience, gold) and Naia element icons, drawn by GPT in the item-icon tone (prompts/, raw pictures in _gen/),
finished with the item-icon pipeline (Tools/ItemIconsHD2D/finish_icons.py: magenta out, back to native dots, 48 grid, x3 = 144 px).
Run: python ArtSource/UI/HUD2026/reward_icons/finish.py"""
import importlib.util
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
spec = importlib.util.spec_from_file_location('finish_icons', ROOT / 'Tools' / 'ItemIconsHD2D' / 'finish_icons.py')
fi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fi)
fi.SRC = HERE / '_gen'
fi.OUT = HERE

names = sorted(p.stem for p in fi.SRC.glob('T_*.png'))
for n in names:
    try:
        b, size, nat = fi.finish(n)
        print('%-22s cell %.2f px  native %s  icon %s' % (n, b, nat, size))
    except Exception as e:
        print('%-22s FAILED %s' % (n, e))
fi.sheet(names)
