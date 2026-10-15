# -*- coding: utf-8 -*-
"""Cuts the window frame off a play capture, leaving the game viewport.  python crop_shot.py <shot.png>
Writes <shot>_view.png from the engine screenshot and, when the window grab <shot>.win.bmp exists, <shot>_win.png from that."""
import json
import pathlib
import sys
from PIL import Image

shot = pathlib.Path(sys.argv[1])
vw, vh = [int(round(v)) for v in json.loads(shot.with_suffix('.json').read_text())['viewport']]
for source, target in ((shot, shot.with_name(shot.stem + '_view.png')), (shot.with_suffix('.win.bmp'), shot.with_name(shot.stem + '_win.png'))):
    if not source.exists():
        continue
    im = Image.open(source)
    bx = (im.width - vw) // 2       # the frame is as thick at the bottom as at the sides; the title bar takes the rest
    im.crop((bx, im.height - vh - bx, bx + vw, im.height - bx)).convert('RGB').save(target)
    print(target, im.size, (vw, vh))
