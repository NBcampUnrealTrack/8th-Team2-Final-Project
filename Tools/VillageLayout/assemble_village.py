# Unreal Editor Python — village assembly from placements.json.
# The caller prepends a line:  STAGE = 'heights' | 'paint' | 'houses' | 'props' | 'linear' | 'lanterns' | 'rim' | 'foliage' | 'clear_<stage>'
# and may override the constants below (FRONT_YAW_OFFSET etc.). Idempotent: each stage clears its own actors first.
import unreal, json, math, os

PLACEMENTS = r"C:\Users\Ndus_072\AppData\Local\Temp\claude\C--Users-Ndus-072-RevDevTool-RevStudio-ProjectM\2d851b1b-c6b6-4a48-a0e8-226d39210886\scratchpad\layout\placements.json"
MESH_DIR = '/Game/Village/Meshes'
LANDSCAPE = 'Landscape_Village'
N = 127
GROUND_MARGIN = 2.0          # cm sink into ground to avoid floating
try: FRONT_YAW_OFFSET
except NameError: FRONT_YAW_OFFSET = 0.0     # add so that a mesh's FRONT faces world +Y at placement yaw 0
try: LINEAR_YAW_OFFSET
except NameError: LINEAR_YAW_OFFSET = 0.0    # add so that a segment mesh's LENGTH axis lies along world +X at yaw 0
try: STAGE
except NameError: STAGE = 'none'

P = json.load(open(PLACEMENTS, 'r'))
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS = unreal.LandscapeService

_mesh_cache = {}
def mesh(name):
    if name not in _mesh_cache:
        m = unreal.load_asset(f'{MESH_DIR}/{name}')
        _mesh_cache[name] = m
    return _mesh_cache[name]

def ground_z(x, y):
    s = LS.get_height_at_location(LANDSCAPE, float(x), float(y))
    for attr in ('height', 'z', 'world_z', 'value', 'height_z'):
        if hasattr(s, attr):
            try:
                v = getattr(s, attr)
                return float(v)
            except Exception:
                pass
    try:
        return float(s)
    except Exception:
        return 100.0

def clear_folder(folder):
    n = 0
    for a in EAS.get_all_level_actors():
        try:
            if str(a.get_folder_path()) == folder or str(a.get_folder_path()).startswith(folder + '/'):
                EAS.destroy_actor(a); n += 1
        except Exception:
            pass
    return n

def spawn(mesh_name, x, y, yaw, scale=1.0, label='', folder='Village', z_offset=0.0):
    m = mesh(mesh_name)
    if m is None:
        return None
    z = ground_z(x, y) - GROUND_MARGIN + z_offset
    a = EAS.spawn_actor_from_object(m, unreal.Vector(float(x), float(y), float(z)), unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(yaw)))
    if a is None:
        return None
    if label: a.set_actor_label(label)
    a.set_folder_path(folder)
    if scale != 1.0: a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    a.set_mobility(unreal.ComponentMobility.STATIC)
    return a

report = {'stage': STAGE}

if STAGE == 'heights':
    hs = list(LS.get_height_in_region(LANDSCAPE, 0, 0, N, N))
    delta = P['height_delta_cm']
    assert len(hs) == N * N and len(delta) == N * N, (len(hs), len(delta))
    base = float(globals().get('BASE_Z', 0.0))
    new = [base + delta[i] for i in range(N * N)]   # absolute: safe to re-run
    ok = LS.set_height_in_region(LANDSCAPE, 0, 0, N, N, new)
    report.update({'ok': bool(ok), 'min_before': min(hs), 'max_before': max(hs), 'min_after': min(new), 'max_after': max(new)})

elif STAGE == 'paint':
    res = {}
    for layer in ('Cobblestone', 'Dirt', 'Moss', 'Grass'):
        w = P['paint'][layer]
        assert len(w) == N * N
        res[layer] = bool(LS.set_weights_in_region(LANDSCAPE, layer, 0, 0, N, N, w))
    # read back a few probes
    probes = {}
    for name, (x, y) in {'main_street': (0, 0), 'plaza': (0, 3600), 'forest_sw': (-5000, -5000), 'inn_yard': (2200, 1300)}.items():
        try:
            ws = LS.get_layer_weights_at_location(LANDSCAPE, float(x), float(y))
            probes[name] = {str(getattr(s, 'layer_name', '?')): round(float(getattr(s, 'weight', 0)), 2) for s in ws}
        except Exception as e:
            probes[name] = str(e)[:120]
    report.update({'written': res, 'probes': probes})

elif STAGE == 'houses':
    clear_folder('Village/Houses')
    n = 0
    for h in P['houses']:
        a = spawn(h['mesh'], h['x'], h['y'], h['yaw'] + FRONT_YAW_OFFSET, h['scale'], h['id'], 'Village/Houses')
        n += 1 if a else 0
    report['spawned'] = n

elif STAGE == 'props':
    clear_folder('Village/Props')
    n = 0
    for p in P['props']:
        a = spawn(p['mesh'], p['x'], p['y'], p['yaw'] + FRONT_YAW_OFFSET, p.get('scale', 1.0), p['id'], 'Village/Props', p.get('z_offset', 0.0))
        n += 1 if a else 0
    report['spawned'] = n

elif STAGE == 'linear':
    clear_folder('Village/Fences')
    n = 0
    for key, folder in (('fences', 'Village/Fences/Fence'), ('stone_walls', 'Village/Fences/StoneWall'), ('hedges', 'Village/Fences/Hedge')):
        for i, f in enumerate(P[key]):
            a = spawn(f['mesh'], f['x'], f['y'], f['yaw'] + LINEAR_YAW_OFFSET, 1.0, f"{f['group']}_{i:02d}", folder)
            if a and f.get('scale3'):
                s3 = f['scale3']
                a.set_actor_scale3d(unreal.Vector(s3[0], s3[1], s3[2]))
            elif a and abs(f.get('scale', 1.0) - 1.0) > 0.02:
                # stretch along the segment's length axis only (X in mesh space)
                a.set_actor_scale3d(unreal.Vector(f['scale'], 1.0, 1.0))
            n += 1 if a else 0
    report['spawned'] = n

elif STAGE == 'lanterns':
    clear_folder('Village/Lanterns')
    n = 0
    for i, l in enumerate(P['lanterns']):
        a = spawn(l['mesh'], l['x'], l['y'], l['yaw'] + FRONT_YAW_OFFSET, l.get('scale', 1.0), f'Lantern_{i:02d}', 'Village/Lanterns')
        n += 1 if a else 0
    report['spawned'] = n

elif STAGE == 'rim':
    clear_folder('Village/CliffRim')
    n = 0
    for i, r in enumerate(P['cliff_rim']):
        a = spawn(r['mesh'], r['x'], r['y'], r['yaw'] + FRONT_YAW_OFFSET, r['scale'], f'Cliff_{i:03d}', 'Village/CliffRim', r['z_offset'])
        n += 1 if a else 0
    report['spawned'] = n

elif STAGE == 'foliage':
    FS = unreal.FoliageService
    res = {}
    for key in ('trees', 'bushes', 'rocks'):
        by_mesh = {}
        scales = {}
        for e in P[key]:
            by_mesh.setdefault(e['mesh'], []).append(unreal.Vector(float(e['x']), float(e['y']), 5000.0))
            scales.setdefault(e['mesh'], []).append(float(e.get('scale', 1.0)))
        for mname, locs in by_mesh.items():
            smin, smax = min(scales[mname]), max(scales[mname])
            path = f'{MESH_DIR}/{mname}'
            try:
                FS.remove_all_foliage_of_type(path)
            except Exception:
                pass
            r = FS.add_foliage_instances(path, locs, smin, smax, False, True, True)
            res[mname] = {'requested': len(locs), 'result': str(r)[:160]}
    report['foliage'] = res

elif STAGE.startswith('clear_'):
    folder = {'houses': 'Village/Houses', 'props': 'Village/Props', 'linear': 'Village/Fences', 'lanterns': 'Village/Lanterns', 'rim': 'Village/CliffRim'}.get(STAGE[6:])
    report['cleared'] = clear_folder(folder) if folder else 'unknown stage'

elif STAGE == 'stats':
    counts = {}
    for a in EAS.get_all_level_actors():
        f = str(a.get_folder_path()); counts[f] = counts.get(f, 0) + 1
    report['folders'] = counts

print(json.dumps(report, default=str))
