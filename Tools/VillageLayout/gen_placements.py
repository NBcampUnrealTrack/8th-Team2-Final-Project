"""Generate concrete placement lists (cm) from layout_village.json.
Outputs placements.json + plan_topdown.png (visual sanity check). Deterministic (seeded).
Run: python gen_placements.py
"""
import json, math, random, os
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
L = json.load(open(os.path.join(HERE, 'layout_village.json'), encoding='utf-8'))
rng = random.Random(4242)
S = float(L.get('world_scale', 1.0))   # layout metres -> world size
CM = 100.0 * S                          # layout metres -> world centimetres

FACING_YAW = {'N': 0.0, 'E': -90.0, 'S': 180.0, 'W': 90.0}  # yaw such that mesh +Y (front) points to that compass dir (UE yaw is CW from +X when viewed from above; +Y front at yaw 0 -> N)
# NOTE: UE: yaw rotates about Z; +X is yaw 0; +Y is yaw 90. A mesh whose FRONT is +Y at yaw 0 faces north (+Y).
# rotating by yaw -90 makes +Y -> +X (east). rotating +90 makes +Y -> -X (west). 180 -> -Y (south).

FOOTPRINT = {'SM_House_A': (6, 5), 'SM_House_B': (7, 6), 'SM_House_C': (12, 8), 'SM_House_D': (5, 4)}

# ---------- masks for exclusion ----------
def rot_footprint(w, d, yaw):
    # footprint half extents after yaw (only 0/90/180/270 used)
    if abs(yaw) % 180 == 90:
        return d / 2, w / 2
    return w / 2, d / 2

house_rects = []  # (minx,miny,maxx,maxy) in m with margin
for h in L['houses']:
    w, d = FOOTPRINT[h['mesh']]
    w *= h['scale']; d *= h['scale']
    yaw = FACING_YAW[h['facing']]
    hx, hy = rot_footprint(w, d, yaw)
    cx, cy = h['center']
    m = 2.5
    house_rects.append((cx - hx - m, cy - hy - m, cx + hx + m, cy + hy + m, h['id']))

def dist_point_seg(px, py, ax, ay, bx, by):
    abx, aby = bx - ax, by - ay
    t = 0.0 if (abx == 0 and aby == 0) else max(0.0, min(1.0, ((px - ax) * abx + (py - ay) * aby) / (abx * abx + aby * aby)))
    qx, qy = ax + t * abx, ay + t * aby
    return math.hypot(px - qx, py - qy)

def on_road(px, py, margin=2.0):
    for r in L['roads']:
        if r['type'] == 'line':
            if dist_point_seg(px, py, *r['from'], *r['to']) < r['width'] / 2 + margin:
                return True
        elif r['type'] == 'circle':
            if math.hypot(px - r['center'][0], py - r['center'][1]) < r['radius'] + margin:
                return True
        elif r['type'] == 'rect':
            if r['min'][0] - margin <= px <= r['max'][0] + margin and r['min'][1] - margin <= py <= r['max'][1] + margin:
                return True
    return False

def in_house(px, py):
    return any(a <= px <= c and b <= py <= d for a, b, c, d, _ in house_rects)

def in_props(px, py, margin=2.0):
    for p in L['props']:
        if math.hypot(px - p['pos'][0], py - p['pos'][1]) < margin + 2.0:
            return True
    return False

def inside_plateau(px, py, half=None):
    half = half or L['plateau']['flat_half_size']
    cr = L['plateau']['corner_round']
    ax, ay = abs(px), abs(py)
    if ax > half or ay > half:
        return False
    if ax > half - cr and ay > half - cr:
        return math.hypot(ax - (half - cr), ay - (half - cr)) <= cr
    return True

# ---------- trees (poisson-ish) ----------
tz = L['trees']['perimeter_zone']
ir = tz['inner_rect']
trees = []
def in_inner(px, py):
    return ir[0] <= px <= ir[2] and ir[1] <= py <= ir[3]
def in_corridor(px, py):
    return abs(px) < tz['exclude_corridor_halfwidth'] and py < ir[1] + 2
def free(px, py, pts, spacing):
    return all(math.hypot(px - x, py - y) >= spacing for x, y, *_ in pts)
outer = tz['outer_half']
attempts = 0
while attempts < 20000 and len(trees) < 400:
    attempts += 1
    px = rng.uniform(-outer, outer); py = rng.uniform(-outer, outer)
    if not inside_plateau(px, py, outer + 0.5): continue
    if in_inner(px, py) or in_corridor(px, py): continue
    if on_road(px, py, 2.5) or in_house(px, py) or in_props(px, py): continue
    if not free(px, py, trees, tz['spacing']): continue
    mesh = 'SM_Tree_A' if rng.random() < tz['mix']['SM_Tree_A'] else 'SM_Tree_B'
    trees.append((px, py, mesh))
for x, y, mesh in L['trees']['interior_points']:
    trees.append((x, y, mesh))

# ---------- bushes & rocks ----------
bushes = []
attempts = 0
while attempts < 30000 and len(bushes) < L['trees']['bushes']['count']:
    attempts += 1
    px = rng.uniform(-outer, outer); py = rng.uniform(-outer, outer)
    if not inside_plateau(px, py, outer): continue
    if on_road(px, py, 1.5) or in_house(px, py) or in_props(px, py, 1.0): continue
    if not free(px, py, bushes, L['trees']['bushes']['spacing']): continue
    if not free(px, py, trees, 2.2): continue
    bushes.append((px, py, 'SM_Bush'))
rocks = []
attempts = 0
while attempts < 30000 and len(rocks) < L['trees']['rocks']['count']:
    attempts += 1
    px = rng.uniform(-outer, outer); py = rng.uniform(-outer, outer)
    if not inside_plateau(px, py, outer): continue
    # prefer near the rim or near trees
    near_rim = max(abs(px), abs(py)) > 48
    if not near_rim and rng.random() < 0.7: continue
    if on_road(px, py, 1.5) or in_house(px, py) or in_props(px, py, 1.0): continue
    if not free(px, py, rocks, 4.0) or not free(px, py, trees, 1.8): continue
    rocks.append((px, py, 'SM_Rock_A' if rng.random() < 0.5 else 'SM_Rock_B'))

# ---------- linear props along polylines ----------
def along(polylines, seg_len, mesh, extra_yaw=0.0):
    out = []
    for pl in polylines:
        pts = pl['points']
        for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
            length = math.hypot(bx - ax, by - ay)
            n = max(1, int(round(length / seg_len)))
            step = length / n
            ux, uy = (bx - ax) / length, (by - ay) / length
            yaw = math.degrees(math.atan2(uy, ux))  # segment direction; mesh length along +X
            for i in range(n):
                cx = ax + ux * (i + 0.5) * step; cy = ay + uy * (i + 0.5) * step
                out.append({'mesh': mesh, 'x': cx, 'y': cy, 'yaw': yaw + extra_yaw, 'scale': step / seg_len, 'group': pl['id']})
    return out
fences = along(L['fences'], 2.0, 'SM_Fence_Seg')
walls = along(L['stone_walls'], 2.0, 'SM_StoneWall_Seg')
hedges = along(L['hedges'], 2.0, 'SM_Hedge')

# ---------- lantern posts ----------
lp = L['lantern_posts']
lanterns = []
for x in lp['main_street_x']:
    for y in lp['main_street_y']:
        lanterns.append({'x': x, 'y': y, 'yaw': 90 if x < 0 else -90})
c = lp['plaza_ring']
for a in c['angles_deg']:
    lanterns.append({'x': c['center'][0] + c['radius'] * math.cos(math.radians(a)), 'y': c['center'][1] + c['radius'] * math.sin(math.radians(a)), 'yaw': a + 180})
for x, y in lp['extra']:
    lanterns.append({'x': x, 'y': y, 'yaw': 0})

# ---------- cliff rim (multi-ring) ----------
cr = L['cliff_rim']
rnd = cr['corner_round']
rim = []


def rim_path(half):
    h = half - rnd
    path = [(-h, -half), (h, -half)]

    def arc(cx, cy, a0, a1):
        n = 10
        return [(cx + rnd * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
                 cy + rnd * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(1, n + 1)]
    path += arc(h, -h, -90, 0)
    path += [(half, h)]
    path += arc(h, h, 0, 90)
    path += [(-h, half)]
    path += arc(-h, h, 90, 180)
    path += [(-half, -h)]
    path += arc(-h, -h, 180, 270)
    return path


for ring in cr['rings']:
    path = rim_path(ring.get('half', cr['half']))
    segs = list(zip(path, path[1:] + path[:1]))
    total = sum(math.hypot(bx - ax, by - ay) for (ax, ay), (bx, by) in segs)
    n = max(4, int(total / ring.get('spacing', cr['spacing'])))
    d_step = total / n
    smin, smax = ring.get('scale', [0.9, 1.25])
    for k in range(n):
        target = k * d_step
        run = 0.0
        for (ax, ay), (bx, by) in segs:
            sl = math.hypot(bx - ax, by - ay)
            if run + sl >= target:
                t = (target - run) / sl if sl else 0
                px, py = ax + (bx - ax) * t, ay + (by - ay) * t
                yaw = math.degrees(math.atan2(py, px))
                skip = False
                for g in cr.get('gaps', []):
                    if g.get('axis') == 'south' and abs(px) < g['half_width'] and py < g['beyond']:
                        skip = True
                if skip:
                    break
                rim.append({'mesh': 'SM_Cliff_Block',
                            'x': px + rng.uniform(-0.4, 0.4), 'y': py + rng.uniform(-0.4, 0.4),
                            'yaw': yaw + rng.uniform(-4, 4) - 90,
                            'scale': rng.uniform(smin, smax),
                            'z_offset': ring['z_offset_m'] + rng.uniform(-0.4, 0.25)})
                break
            run += sl

# ---------- paint plan (raster 127x127, 1 vertex = 1 m, vertex (0,0) = world (-63,-63)) ----------
N = 127
def world_to_vert(x, y):
    return x + 63.0, y + 63.0
ys, xs = np.mgrid[0:N, 0:N]
wx = xs - 63.0; wy = ys - 63.0   # landscape vertex == 1 layout metre (the landscape actor carries the world scale)
cob = np.zeros((N, N), np.float32); dirt = np.zeros((N, N), np.float32); moss = np.zeros((N, N), np.float32)
def seg_dist_grid(ax, ay, bx, by):
    abx, aby = bx - ax, by - ay
    denom = abx * abx + aby * aby
    t = np.clip(((wx - ax) * abx + (wy - ay) * aby) / denom, 0, 1) if denom else np.zeros_like(wx)
    return np.hypot(wx - (ax + t * abx), wy - (ay + t * aby))
noise = np.zeros((N, N), np.float32)
rs = np.random.RandomState(7)
for freq, amp in ((3, 1.0), (7, 0.5), (13, 0.25)):
    small = rs.rand(freq + 1, freq + 1)
    big = np.array(Image.fromarray((small * 255).astype(np.uint8)).resize((N, N), Image.BICUBIC), np.float32) / 255.0
    noise += (big - 0.5) * amp
noise /= 1.75
def soft(d, r, feather=1.2):
    return np.clip((r - d) / feather + 0.5, 0, 1)
for r in L['roads']:
    if r['type'] == 'line':
        d = seg_dist_grid(*r['from'], *r['to'])
        w = r['width'] / 2 + noise * 0.9
        m = soft(d, w)
        if 'verge_width' in r:
            v = soft(d, r['verge_width'] / 2 + noise * 1.6)
    elif r['type'] == 'circle':
        d = np.hypot(wx - r['center'][0], wy - r['center'][1])
        m = soft(d, r['radius'] + noise * 0.8)
        v = soft(d, r['verge_radius'] + noise * 1.5) if 'verge_radius' in r else None
    else:
        inside = (wx >= r['min'][0]) & (wx <= r['max'][0]) & (wy >= r['min'][1]) & (wy <= r['max'][1])
        m = inside.astype(np.float32)
        mg = r.get('verge_margin', 0)
        v = ((wx >= r['min'][0] - mg) & (wx <= r['max'][0] + mg) & (wy >= r['min'][1] - mg) & (wy <= r['max'][1] + mg)).astype(np.float32) if mg else None
    if r['layer'] == 'Cobblestone':
        cob = np.maximum(cob, m)
    else:
        dirt = np.maximum(dirt, m)
    if v is not None:
        dirt = np.maximum(dirt, v * 0.9)
# dirt yards around houses
for a, b, c_, d, _ in house_rects:
    inside = (wx >= a + 0.5) & (wx <= c_ - 0.5) & (wy >= b + 0.5) & (wy <= d - 0.5)
    dirt = np.maximum(dirt, inside.astype(np.float32) * (0.34 + noise * 0.25))
# moss zones
for z in L['moss_zones']:
    if z['type'] == 'ring':
        m = ((np.maximum(np.abs(wx), np.abs(wy)) >= z['inner_half'] + noise * 3)).astype(np.float32) * z['weight']
    else:
        m = ((wx >= z['min'][0]) & (wx <= z['max'][0]) & (wy >= z['min'][1]) & (wy <= z['max'][1])).astype(np.float32) * (z['weight'] + noise * 0.4)
    moss = np.maximum(moss, np.clip(m, 0, 1))
# moss under trees
for x, y, _ in trees:
    d = np.hypot(wx - x, wy - y)
    moss = np.maximum(moss, soft(d, 4.2 + noise * 1.6) * 0.95)
# roads win over moss/dirt; dirt wins over moss
moss = np.clip(moss - cob - dirt * 0.65, 0, 1)
dirt = np.clip(dirt - cob, 0, 1)
grass = np.clip(1 - cob - dirt - moss, 0, 1)

# plateau height mask (drop outside flat area)
P = L['plateau']
ax_, ay_ = np.abs(wx), np.abs(wy)
cr_ = P['corner_round']; fh = P['flat_half_size']
dx = np.clip(ax_ - (fh - cr_), 0, None); dy = np.clip(ay_ - (fh - cr_), 0, None)
edge_d = np.hypot(dx, dy) - cr_  # signed distance beyond the rounded flat square
drop = np.clip(edge_d / P['edge_blend'], 0, 1)
drop = drop * drop * (3 - 2 * drop)
height_delta_cm = -drop * P['drop_m'] * CM

out = {
    '_note': 'All coordinates in CENTIMETERS (world). yaw in degrees (UE). Paint rasters are 127x127 row-major, index = y*127 + x, vertex (0,0) = world (-6300,-6300), 1 vertex = 100 cm.',
    'houses': [{**h, 'x': h['center'][0] * CM, 'y': h['center'][1] * CM, 'yaw': FACING_YAW[h['facing']], 'scale': h['scale'] * S} for h in L['houses']],
    'props': [{**p, 'x': p['pos'][0] * CM, 'y': p['pos'][1] * CM, 'yaw': FACING_YAW[p['yaw_facing']], 'z_offset': p.get('z_offset', 0) * CM, 'scale': p.get('scale', 1.0) * S} for p in L['props']],
    'fences': [{**f, 'x': f['x'] * CM, 'y': f['y'] * CM, 'scale3': [f['scale'] * S, S, S]} for f in fences],
    'stone_walls': [{**f, 'x': f['x'] * CM, 'y': f['y'] * CM, 'scale3': [f['scale'] * S, S, S]} for f in walls],
    'hedges': [{**f, 'x': f['x'] * CM, 'y': f['y'] * CM, 'scale3': [f['scale'] * S, S, S]} for f in hedges],
    'lanterns': [{'mesh': 'SM_Lantern_Post', 'x': l['x'] * CM, 'y': l['y'] * CM, 'yaw': l['yaw'], 'scale': S} for l in lanterns],
    'trees': [{'mesh': m, 'x': x * CM, 'y': y * CM, 'yaw': rng.uniform(0, 360), 'scale': rng.uniform(0.85, 1.25) * S} for x, y, m in trees],
    'bushes': [{'mesh': m, 'x': x * CM, 'y': y * CM, 'yaw': rng.uniform(0, 360), 'scale': rng.uniform(0.8, 1.3) * S} for x, y, m in bushes],
    'rocks': [{'mesh': m, 'x': x * CM, 'y': y * CM, 'yaw': rng.uniform(0, 360), 'scale': rng.uniform(0.7, 1.4) * S} for x, y, m in rocks],
    'cliff_rim': [{**r, 'x': r['x'] * CM, 'y': r['y'] * CM, 'z_offset': r['z_offset'] * CM, 'scale': r['scale'] * S} for r in rim],
    'paint': {
        'Cobblestone': [round(float(v), 3) for v in cob.ravel()],
        'Dirt': [round(float(v), 3) for v in dirt.ravel()],
        'Moss': [round(float(v), 3) for v in moss.ravel()],
        'Grass': [round(float(v), 3) for v in grass.ravel()],
    },
    'height_delta_cm': [round(float(v), 1) for v in height_delta_cm.ravel()],
}
json.dump(out, open(os.path.join(HERE, 'placements.json'), 'w'), separators=(',', ':'))

# ---------- plan image ----------
S = 8
img = Image.new('RGB', (N * S, N * S), (30, 30, 30))
arr = np.zeros((N, N, 3), np.float32)
arr[..., 1] = grass * 0.55 + moss * 0.35; arr[..., 0] = grass * 0.35 + dirt * 0.6 + cob * 0.75 + moss * 0.25; arr[..., 2] = grass * 0.15 + cob * 0.6 + dirt * 0.3 + moss * 0.1
arr = (np.clip(arr, 0, 1) * 255).astype(np.uint8)
arr = (arr * (0.55 + 0.45 * (1 - drop))[..., None]).astype(np.uint8)
base = Image.fromarray(arr[::-1]).resize((N * S, N * S), Image.NEAREST)  # flip so +Y is up
img.paste(base, (0, 0))
d = ImageDraw.Draw(img)
def W(x, y):
    return (x / 100 + 63.5) * S, (63.5 - y / 100) * S
for h in out['houses']:
    w_, d_ = FOOTPRINT[h['mesh']]; w_ *= h['scale']; d_ *= h['scale']
    hx, hy = rot_footprint(w_, d_, h['yaw'])
    x0, y0 = W(h['x'] - hx * 100, h['y'] + hy * 100); x1, y1 = W(h['x'] + hx * 100, h['y'] - hy * 100)
    d.rectangle([x0, y0, x1, y1], fill=(150, 90, 60), outline=(255, 220, 160))
    d.text((x0 + 2, y0 + 2), h['id'], fill=(255, 255, 255))
for t in out['trees']:
    x, y = W(t['x'], t['y']); r = 2.2 * S * t['scale']
    d.ellipse([x - r, y - r, x + r, y + r], fill=(40, 110, 40), outline=(20, 60, 20))
for b in out['bushes']:
    x, y = W(b['x'], b['y']); r = 0.7 * S
    d.ellipse([x - r, y - r, x + r, y + r], fill=(70, 140, 60))
for r_ in out['rocks']:
    x, y = W(r_['x'], r_['y']); r = 0.6 * S
    d.ellipse([x - r, y - r, x + r, y + r], fill=(120, 120, 120))
for f in out['fences'] + out['stone_walls'] + out['hedges']:
    x, y = W(f['x'], f['y']); col = (230, 200, 120) if f['mesh'] == 'SM_Fence_Seg' else ((160, 160, 160) if f['mesh'] == 'SM_StoneWall_Seg' else (60, 160, 60))
    d.rectangle([x - S, y - S / 4, x + S, y + S / 4], fill=col)
for l in out['lanterns']:
    x, y = W(l['x'], l['y']); d.ellipse([x - 3, y - 3, x + 3, y + 3], fill=(255, 220, 80))
for p in out['props']:
    x, y = W(p['x'], p['y']); d.rectangle([x - 4, y - 4, x + 4, y + 4], fill=(220, 220, 255)); d.text((x + 5, y - 5), p['id'], fill=(200, 200, 255))
for c_ in out['cliff_rim']:
    x, y = W(c_['x'], c_['y']); d.rectangle([x - 5, y - 5, x + 5, y + 5], outline=(90, 80, 70))
img.save(os.path.join(HERE, 'plan_topdown.png'))
print('trees', len(trees), 'bushes', len(bushes), 'rocks', len(rocks), 'fences', len(fences), 'walls', len(walls), 'hedges', len(hedges), 'lanterns', len(lanterns), 'rim', len(rim))
print('paint coverage: cob %.1f%% dirt %.1f%% moss %.1f%% grass %.1f%%' % (cob.mean() * 100, dirt.mean() * 100, moss.mean() * 100, grass.mean() * 100))
print('height drop min cm', height_delta_cm.min())
