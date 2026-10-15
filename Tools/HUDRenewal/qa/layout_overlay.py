"""Draw where every avatar tag would sit with a full party (4 per formation axis) on a battle screenshot, and report
overlaps. Input: Saved/HUDRenewal/battle_layout.json (qa_layout.py) + a PIE screenshot of the same frame.

    python Tools/HUDRenewal/qa/layout_overlay.py <screenshot.png> <out.png> [ally_w ally_h foe_w foe_h]

Tags are screen-space widget components centred on root +120 cm (NUBattleAvatar VitalBarWidget). Sizes are the
widgets' desired sizes in Slate units; the JSON records the viewport scale to convert them to pixels."""
import json
import pathlib
import sys

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[3]
data = json.loads((ROOT / 'Saved/HUDRenewal/battle_layout.json').read_text(encoding='utf-8'))
shot, out = sys.argv[1], sys.argv[2]
aw, ah, fw, fh = (float(v) for v in (sys.argv[3:7] if len(sys.argv) >= 7 else (124, 34, 116, 30)))

im = Image.open(shot).convert('RGBA')
vw, vh, scale = data['viewport'] or (im.width, im.height, 1.0)
# projections are in viewport pixels; the screenshot may be the window (same size) --- rescale if not
kx, ky = im.width / vw, im.height / vh
ov = Image.new('RGBA', im.size, (0, 0, 0, 0))
d = ImageDraw.Draw(ov)

rects = []
for axis in data['axes']:
    enemy = 'ENEMY' in axis['row'].upper()
    w, h = (fw, fh) if enemy else (aw, ah)
    w, h = w * scale * kx, h * scale * ky
    colour = (227, 112, 93) if enemy else ((166, 220, 242) if 'PLAYER' in axis['row'].upper() else (240, 215, 157))
    for slot in axis['counts']['4']:
        if not slot['head']:
            continue
        x, y = slot['head'][0] * kx, slot['head'][1] * ky
        r = (x - w / 2, y - h / 2, x + w / 2, y + h / 2)
        rects.append((axis['row'], r))
        d.rectangle(r, outline=colour + (255,), width=2, fill=colour + (60,))
        if slot['feet']:
            fx, fy = slot['feet'][0] * kx, slot['feet'][1] * ky
            d.ellipse((fx - 5, fy - 3, fx + 5, fy + 3), fill=colour + (255,))
            d.line((fx, fy, x, y + h / 2), fill=colour + (140,), width=1)

hits = []
for i in range(len(rects)):
    for j in range(i + 1, len(rects)):
        a, b = rects[i][1], rects[j][1]
        if a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]:
            hits.append((rects[i][0], rects[j][0]))
Image.alpha_composite(im, ov).convert('RGB').save(out)
print('tags', len(rects), 'overlaps', len(hits), hits[:6])
for axis in data['axes']:
    pts = [s['head'] for s in axis['counts']['4'] if s['head']]
    if len(pts) > 1:
        gaps = [round(abs(pts[i + 1][0] - pts[i][0]), 1) for i in range(len(pts) - 1)]
        print(axis['row'], 'spacing', axis['spacing'], 'screen x gaps', gaps, 'head y', [p[1] for p in pts])
