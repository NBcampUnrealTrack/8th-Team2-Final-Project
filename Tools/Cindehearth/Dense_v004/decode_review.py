"""Correct the ScreenshotService RGB channel order; no art changes."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parent
project=root.parents[2]
for name in ('village','square','homes'):
    source=project/'Saved/Revvy/Screenshots'/('CH_Dense_review_'+name+'.png')
    im=Image.open(source).convert('RGB')
    Image.merge('RGB',tuple(reversed(im.split()))).save(root/'verification'/('final_'+name+'.png'))
    print(name,source.stat().st_mtime)
