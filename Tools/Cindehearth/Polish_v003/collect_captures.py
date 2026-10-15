"""Correct the known ScreenshotService channel ordering only; no art edits."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parent
for p in (root.parents[2]/'Saved/Revvy/Screenshots').glob('CH_Polish_final_*.png'):
 if p.stem.endswith('warmup'):continue
 out=root/'verification'/(p.stem.replace('CH_Polish_','')+'.png')
 im=Image.open(p).convert('RGB');Image.merge('RGB',tuple(reversed(im.split()))).save(out)
print('Collected',len(list((root/'verification').glob('final_*.png'))),'actual viewport images')
