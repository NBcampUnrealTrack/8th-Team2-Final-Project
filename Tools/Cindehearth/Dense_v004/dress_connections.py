"""Compact Cindehearth connection dressing. Default is review; STAGE='apply' mutates.

Run with --offline-preview outside UE to inspect the planned 106 placements using
the v004 baseline and the coordinator's terrain transform. No files are changed.
"""
import importlib
import json
import math
from pathlib import Path
import sys

PREFIX = 'CH_D4_Link_'
STAGE = globals().get('STAGE', 'review')


def compact(p):
    return (540 + (p[0] - 540) * .74, p[1] * .67, p[2])


def build_plan(bounds):
    """bounds maps existing actor labels to their live (center, extent)."""
    plan = []

    def put(group, suffix, mesh, p, size, yaw=0):
        plan.append(dict(label=PREFIX + group + '_' + suffix, group=group,
                         mesh=mesh, center_bottom=list(p), size=list(size), yaw=yaw,
                         collision=False))

    # Retain the actual compacted stair widths and clear each full tread width.
    stairs = (
        ('WestLower', 'CH_Stairs_-30_-18_Stone'),
        ('WestUpper', 'CH_Stairs_30_-18_Stone'),
        ('EastLower', 'CH_Stairs_-30_44_Stone'),
        ('EastUpper', 'CH_Stairs_30_44_Stone'),
    )
    for group, label in stairs:
        c, e = bounds[label]
        ymin, ymax, zbase = c[1]-e[1], c[1]+e[1], c[2]-e[2]
        for side in (-1, 1):
            # Four short rising courses read as masonry along the stair stringer.
            # The bottom follows each section's first tread, never hovers over it.
            for i in range(4):
                t = (i+.5)/4
                p = (c[0] + side*(e[0]+18), ymin+2*e[1]*t, zbase+2*e[2]*(i/4)-8)
                put(group, 'Curb_%s_%02d' % ('L' if side < 0 else 'R', i),
                    'Wall', p, (26, 2*e[1]/4+4, 58))
            put(group, 'LandingPier_' + ('L' if side < 0 else 'R'), 'Wall',
                (c[0]+side*(e[0]+24), ymax+22, zbase+2*e[2]-4), (38, 48, 68))
        if group.startswith('West'):
            # A small supply bay on the outer side of the upper landing.
            z = zbase+2*e[2]
            put(group, 'SupplyCrate', 'Crate', (c[0]-e[0]-107, ymax+78, z), (48, 48, 44), -8)
            put(group, 'WaterBarrel', 'Barrel', (c[0]-e[0]-160, ymax+113, z), (42, 42, 50), 17)
            put(group, 'PierTools', 'ToolRack', (c[0]-e[0]-49, ymax+22, z+12), (12, 32, 42), 180)

    # The rail ramp includes both rail and pedestrian strips. All new pieces
    # sit outside the transformed full 10-design-metre bed (x423.45..756.45).
    rail_min, rail_max = compact((382.5, 0, 0))[0], compact((832.5, 0, 0))[0]
    for group, old_y0, old_y1, z0 in (
        ('RailLower', -1800, -900, 0), ('RailUpper', 900, 1800, 157.5),
    ):
        y0, y1 = old_y0*.67, old_y1*.67
        for side, edge in ((-1, rail_min), (1, rail_max)):
            for i in range(4):
                t = (i+.5)/4
                put(group, 'Edge_%s_%02d' % ('L' if side < 0 else 'R', i), 'Wall',
                    (edge+side*24, y0+(y1-y0)*t, z0+157.5*t-22), (22, 100, 40))
            for end, y, z in (('Foot', y0, z0), ('Head', y1, z0+157.5)):
                put(group, end+'Pier_'+('L' if side < 0 else 'R'), 'Wall',
                    (edge+side*33, y, z-4), (34, 58, 64))

    # The west connector's live path surface remains a usable open corridor.
    c, e = bounds['CH_AshslopeConnection_Path']
    floor = c[2]+e[2]
    mouth_x = c[0]+e[0]-44
    for side in (-1, 1):
        for i in range(3):
            put('Ashslope', 'BrokenCourse_%s_%02d' % ('N' if side > 0 else 'S', i), 'Wall',
                (mouth_x-i*96, c[1]+side*(e[1]+22), floor-9), (24, 78, 42), 90)
        # Low natural rubble beside the old retaining-wall ends.
        put('Ashslope', 'Rubble_'+('N' if side > 0 else 'S'), 'Ridge',
            (mouth_x-300, c[1]+side*(e[1]+55), floor-16), (110, 76, 56), 18*side)
    bay = (mouth_x-34, c[1]-e[1]-102, floor)
    put('Ashslope', 'OreCrate', 'Crate', bay, (60, 52, 48), -6)
    put('Ashslope', 'StackedCrate', 'Crate', (bay[0], bay[1], floor+46), (40, 38, 34), 7)
    put('Ashslope', 'Barrel', 'Barrel', (bay[0]-67, bay[1]+4, floor), (45, 45, 54), 23)
    put('Ashslope', 'DiscardedCrate', 'Crate', (bay[0]-124, bay[1]+13, floor), (44, 40, 30), -19)

    # Quiet cemetery approach: stone only, no festive or industrial staging.
    start, end = compact((66*45, 48*45, 7*45)), compact((94*45, 64*45, 14*45))
    dx, dy = end[0]-start[0], end[1]-start[1]
    length = math.hypot(dx, dy)
    normal = (-dy/length, dx/length)
    yaw = math.degrees(math.atan2(dy, dx))-90
    for side in (-1, 1):
        for i, t in enumerate((.055, .18, .32)):
            p = (start[0]+dx*t+normal[0]*side*99,
                 start[1]+dy*t+normal[1]*side*99,
                 start[2]+(end[2]-start[2])*t-24)
            put('Cemetery', 'TrailCourse_%s_%02d' % ('L' if side < 0 else 'R', i), 'Wall', p, (25, 72, 42), yaw)
        for i, t in enumerate((.025, .24)):
            p = (start[0]+dx*t+normal[0]*side*137,
                 start[1]+dy*t+normal[1]*side*137,
                 start[2]+(end[2]-start[2])*t-35)
            put('Cemetery', 'StoneFooting_%s_%02d' % ('L' if side < 0 else 'R', i), 'Ridge', p, (105, 85, 62), yaw+27*side)

    # A low logging rest bay marks the stone-to-pine transition. The route's
    # 166.5-cm full road width remains open; table and supplies stay to its east.
    x, y, z = compact((12*45, -102*45, 0))
    for side in (-1, 1):
        for i in range(2):
            put('Pinehollow', 'ThresholdCourse_%s_%02d' % ('L' if side < 0 else 'R', i), 'Wall',
                (x+side*119, y-45-i*100, z-10), (25, 68, 32))
        put('Pinehollow', 'LowRubble_'+('L' if side < 0 else 'R'), 'Ridge',
            (x+side*155, y-278, z-18), (112, 92, 55), 30*side)
    put('Pinehollow', 'RestTable', 'Table', (x+267, y-111, z), (150, 94, 48), 4)
    put('Pinehollow', 'ToolStand', 'ToolRack', (x+320, y-21, z), (24, 62, 70), 90)
    for i, (ox, oy, yaw) in enumerate(((378,-112,-8), (411,-170,17), (-184,-96,4), (-220,-158,-14))):
        put('Pinehollow', 'SupplyCrate_%02d' % i, 'Crate', (x+ox,y+oy,z), (44, 42, 42), yaw)
    for i, (ox, oy) in enumerate(((372,-215), (-180,-216))):
        put('Pinehollow', 'Barrel_%02d' % i, 'Barrel', (x+ox,y+oy,z), (42, 42, 49), 15*i)
    assert len(plan) == 106, len(plan)
    assert len({r['label'] for r in plan}) == len(plan)
    assert all(r['collision'] is False and max(r['size']) <= 170 for r in plan)
    return plan


if '--offline-preview' in sys.argv:
    directory = Path(__file__).resolve().parent
    baseline = json.loads((directory/'verification/level_before.json').read_text(encoding='utf-8'))
    bounds = {r['label']: (compact(r['bounds_center']),
                          (r['bounds_extent'][0]*.74, r['bounds_extent'][1]*.67, r['bounds_extent'][2]))
              for r in baseline if 'bounds_center' in r}
    plan = build_plan(bounds)
    from collections import Counter
    print(json.dumps(dict(count=len(plan), groups=dict(Counter(r['group'] for r in plan)),
                          placements=plan), ensure_ascii=False, indent=2))
else:
    import unreal
    directory = Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004'
    sys.path.insert(0, str(directory.parent/'Polish_v003'))
    import ue_common as C
    importlib.reload(C)
    assert not unreal.WidgetService.is_pie_running(), 'Stop PIE before connection dressing'
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().startswith('/Game/Maps/L_Cindehearth.'), world.get_path_name()
    bounds = {label: tuple(v.to_tuple() for v in a.get_actor_bounds(False))
              for label, a in C.ACTORS.items() if isinstance(a, unreal.StaticMeshActor)}
    # Refuse to dress the old uncompressed layout by accident.
    assert abs(bounds['CH_Stairs_-30_-18_Stone'][0][0]-(-399)) < 8, 'Apply layout compaction and west stair clearance first'
    plan = build_plan(bounds)
    receipt = directory/'verification/connections_applied.json'
    review = directory/'verification/connections_proposal.json'
    review.write_text(json.dumps(dict(status='review', count=len(plan), placements=plan), indent=2), encoding='utf-8')
    if STAGE != 'apply':
        print('CONNECTION DRESSING REVIEW', str(review), len(plan))
    else:
        assert not receipt.exists(), 'A connection dressing receipt already exists; inspect it before proceeding'
        assert not any(label.startswith(PREFIX) for label in C.ACTORS), 'Existing/partial connection dressing must be inspected'
        # Load every dependency before the transaction creates the first actor.
        for key in sorted({r['mesh'] for r in plan}):
            C.resources(key)
        result = dict(status='started', count=0, placements=[])
        receipt.write_text(json.dumps(result, indent=2), encoding='utf-8')
        try:
            with unreal.ScopedEditorTransaction('Cindehearth compact stair and field connection detail'):
                for r in plan:
                    a = C.spawn(r['label'], r['mesh'], r['center_bottom'], r['size'], r['yaw'],
                                tags=['CH.Dense_v004', 'CH.ConnectionDressing'], collision=False)
                    a.set_folder_path('Cindehearth/Dense_v004/Connections/'+r['group'])
                    a.set_actor_enable_collision(False)
                    assert min(a.get_actor_bounds(False)[1].to_tuple()) > 0
                    result['placements'].append(r)
            assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
            result.update(status='finished', count=len(result['placements']))
        except Exception as exc:
            result.update(status='partial_error', count=len(result['placements']), error=str(exc))
            raise
        finally:
            receipt.write_text(json.dumps(result, indent=2), encoding='utf-8')
        print('CONNECTION DRESSING APPLIED', len(result['placements']))
