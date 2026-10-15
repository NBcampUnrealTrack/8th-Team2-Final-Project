import sys, os, glob
from PIL import Image, ImageDraw, ImageFont
prev = sys.argv[1]
files = sorted(f for f in glob.glob(os.path.join(prev, 'SM_*.png')))
cell = 400; cols = 5
rows = (len(files) + cols - 1) // cols
sheet = Image.new('RGB', (cols * cell, rows * (cell + 26)), (30, 30, 34))
d = ImageDraw.Draw(sheet)
try:
    font = ImageFont.truetype("arial.ttf", 20)
except Exception:
    font = ImageFont.load_default()
for i, f in enumerate(files):
    im = Image.open(f).convert('RGB').resize((cell, cell), Image.LANCZOS)
    x = (i % cols) * cell; y = (i // cols) * (cell + 26)
    sheet.paste(im, (x, y))
    d.text((x + 6, y + cell + 3), os.path.splitext(os.path.basename(f))[0], fill=(240, 240, 240), font=font)
out = os.path.join(prev, '_contact.png')
sheet.save(out)
print("saved", out, sheet.size, len(files))
