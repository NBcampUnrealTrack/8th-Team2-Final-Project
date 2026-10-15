"""Six purposeful work structures and eighteen props. AUTHORING PROPOSAL ONLY.

Default STAGE='review' performs read-only UE preflight and writes a proposal.
STAGE='apply' is for the coordinator after reviewing it; this agent never ran it.
--offline-preview prints only the fixed plan and route-clearance calculation.
"""
import json
import math
import sys
from pathlib import Path

STAGE = globals().get('STAGE', 'review')
PREFIX = 'CH_D4_Work_'


def build_plan():
    structures = [
        dict(name='WestSorting', kind='workshop', center=(-1117.6,1742,315), yaw=-90,
             world_size=(300,220,210), old_yard='West', purpose='Covered ore grading and bagging bay'),
        dict(name='EastTimber', kind='workshop', center=(1657.4,1608,315), yaw=-90,
             world_size=(300,220,210), old_yard='East', purpose='Pit-prop timber repair and chain stores'),
        dict(name='ToolRepair', kind='workshop', center=(-1102.8,294.8,157.5), yaw=-90,
             world_size=(300,220,210), old_yard='Smithy', purpose='Cold tool repairs beside the forge square'),
        dict(name='CrewRoom', kind='store', center=(-1770,2250,315), yaw=180,
             house_factor=1.05, world_size=(292.1,255.2,245.7), purpose='Miners changing room close to the work terrace'),
        dict(name='LampStore', kind='store', center=(2050,2320,315), yaw=180,
             house_factor=.90, world_size=(250.3,218.7,210.6), purpose='Lamp fuel and spare equipment store'),
        dict(name='AssayPacking', kind='store', center=(1900,-260,157.5), yaw=90,
             house_factor=1.0, world_size=(243,278.1,234), purpose='Packing and outgoing sample store east of the assay'),
    ]
    props = []
    for row in structures:
        x,y,z = row['center']
        if row['kind'] == 'workshop':
            # The middle of the open front remains free; props flank its corners.
            values = [('Crate',(-108,-158,0),(58,54,46),-6),
                      ('Barrel',(106,-155,0),(46,46,54),12),
                      ('ToolRack',(-136,25,53),(14,36,44),0)]
        elif row['yaw'] == 180:
            values = [('Crate',(-110,-166,0),(54,50,48),8),
                      ('Barrel',(110,-160,0),(44,44,52),-8),
                      ('ToolRack',(83,-119,69),(14,34,43),-90)]
        else:
            values = [('Crate',(-168,-106,0),(54,50,48),90),
                      ('Barrel',(-162,108,0),(44,44,52),8),
                      ('ToolRack',(-119,80,69),(14,34,43),180)]
        for i,(key,offset,size,yaw) in enumerate(values):
            props.append(dict(label=PREFIX+row['name']+'_'+key+str(i), key=key,
                              center_bottom=(x+offset[0],y+offset[1],z+offset[2]),size=size,yaw=yaw))
    assert len(structures) == 6 and len(props) == 18
    return dict(structures=structures, props=props,
                source='Existing repaired P2 SmithyStructure/SmithyRoof/SmithyWalls and accepted Gorm-house base',
                scope='Three canopy replacements plus three non-quest storage/crew buildings; no additional dwellings or residents')


def segment_box(a,b,lo,hi):
    """True if a 2D segment enters an axis-aligned box, including its boundary."""
    first,last = 0.,1.
    for i in range(2):
        delta = b[i]-a[i]
        if abs(delta) < 1e-8:
            if a[i] < lo[i] or a[i] > hi[i]:
                return False
        else:
            t0,t1 = (lo[i]-a[i])/delta,(hi[i]-a[i])/delta
            if t0 > t1:
                t0,t1=t1,t0
            first,last=max(first,t0),min(last,t1)
            if first > last:
                return False
    return True


def check_routes(plan, root):
    namespace = {}
    exec((root/'QA/routes.py').read_text(encoding='utf-8'),namespace)
    conflicts = []
    for row in plan['structures']:
        x,y,z = row['center']; width,depth,_ = row['world_size']
        lo=(x-width/2-72,y-depth/2-72); hi=(x+width/2+72,y+depth/2+72)
        for name,route in namespace['ROUTES'].items():
            for i,(a,b) in enumerate(zip(route,route[1:])):
                if abs(a[2]-z) < 80 and abs(b[2]-z) < 80 and segment_box(a,b,lo,hi):
                    conflicts.append(dict(structure=row['name'],route=name,segment=i))
    return dict(conflicts=conflicts, clearance_margin_cm=72, route_count=len(namespace['ROUTES']),
                meaning='Analytical route-footprint clearance only; not a physics or visual QA result')


if '--offline-preview' in sys.argv:
    root = Path(__file__).resolve().parent
    plan = build_plan(); plan['route_clearance'] = check_routes(plan,root)
    print(json.dumps(plan,ensure_ascii=False,indent=2))
    raise SystemExit(0)

import importlib
import unreal

root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004'
out=root/'verification'
sys.path.insert(0,str(root.parent/'Polish_v003'))
import ue_common as C
importlib.reload(C)
assert not unreal.WidgetService.is_pie_running(), 'Review/apply in the editor world only'
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Cindehearth'
plan=build_plan()
plan['route_clearance']=check_routes(plan,root)
assert not plan['route_clearance']['conflicts'], plan['route_clearance']
assert (out/'architecture_applied.json').exists(), 'Finish the root landmark replacement before work-yard reuse'
assert not (out/'workyards_applied.json').exists(), 'Existing application needs inspection, not replay'

meshes={}
for key in ['Smithy','SmithyStructure','SmithyRoof','SmithyWalls']:
    receipt=json.loads((out/('import_'+key+'.json')).read_text(encoding='utf-8'))
    assert receipt['status']=='finished'
    meshes[key]=unreal.load_asset(receipt['meshes'][0]['path'])
    assert meshes[key],key
smithy_material=unreal.load_asset('/Game/Cindehearth/Dense_v004/Materials/MI_CH_Smithy')
stone_material=unreal.load_asset('/Game/Cindehearth/HD2D_v002/Materials/MI_CH_Stone')
assert smithy_material and stone_material
house=C.ACTORS['CH_GormHouse_P2']
house_mesh=house.static_mesh_component.static_mesh
house_materials=[house.static_mesh_component.get_material(i) for i in range(house.static_mesh_component.get_num_materials())]
house_scale=house.get_actor_scale3d().to_tuple()
assert max(house_scale)-min(house_scale)<.0001, 'Work stores must keep the accepted house proportions'
assert all(house_materials)

hidden=[]
for row in plan['structures']:
    yard=row.get('old_yard')
    if not yard:
        continue
    old='CH_HD2D_Yard_'+yard+'_'
    found=[]
    for label,actor in C.ACTORS.items():
        if label.startswith(old) and isinstance(actor,unreal.StaticMeshActor):
            # Remove the flat canopy and its mismatched inner crates. Preserve
            # the useful external timber bundles rather than deleting the yard.
            tail=label[len(old):]
            if tail.startswith(('Roof','Post','Crate')):
                found.append(label)
    assert any(label.endswith('_Roof') for label in found), yard
    hidden.extend(found)

# Only generic scree or this known non-quest crate may be locally concealed.
# The source and replacement family are both included so no hidden rock collider
# is left through the storage footprint. Quest anchors and all large rim pieces
# are excluded by the strict label allow-list.
clear_families=set()
for label,actor in C.ACTORS.items():
    if not isinstance(actor,unreal.StaticMeshActor):
        continue
    if not (label.startswith('CH_Scree_') or label.startswith('CH_Crate_15')):
        continue
    if any(str(tag).startswith(('CH.Q','Shadowvale.','Doren.','CH.Resident')) for tag in actor.tags):
        continue
    center,extent=actor.get_actor_bounds(False)
    for row in plan['structures']:
        if row['kind']!='store':
            continue
        x,y,z=row['center'];width,depth,_=row['world_size']
        if (abs(center.x-x)<width/2+extent.x+8 and abs(center.y-y)<depth/2+extent.y+8
                and abs(center.z-z)<extent.z+100):
            clear_families.add('_'.join(label.split('_')[:3]))
for label,actor in C.ACTORS.items():
    if isinstance(actor,unreal.StaticMeshActor) and any(label==family or label.startswith(family+'_') for family in clear_families):
        hidden.append(label)
hidden=sorted(set(hidden))
plan['hide_labels']=hidden
plan['clear_generic_families']=sorted(clear_families)
plan['status']='proposal_only'
(out/'workyards_proposal.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
print('WORK-YARD PROPOSAL: six structures, eighteen props;',len(hidden),'old canopy/crate/scree pieces to hide')
if STAGE!='apply':
    print('REVIEW ONLY; no editor actors changed')
else:
    created=[]
    snapshots=[]

    def fitted(mesh, point, scale, yaw):
        box=mesh.get_bounding_box();lo=box.min.to_tuple();hi=box.max.to_tuple()
        center=[(lo[i]+hi[i])*.5*scale[i] for i in range(2)];angle=math.radians(yaw)
        return [point[0]-center[0]*math.cos(angle)+center[1]*math.sin(angle),
                point[1]-center[0]*math.sin(angle)-center[1]*math.cos(angle),point[2]-lo[2]*scale[2]]

    def put(label,mesh,position,scale,yaw,materials,roof=False):
        assert label not in C.ACTORS,label
        actor=C.AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*position),unreal.Rotator(yaw=yaw))
        actor.set_actor_label(label);actor.set_folder_path('Cindehearth/Dense_v004/Workyards')
        actor.tags=['Cindehearth','CH.HD2D','CH.Dense_v004','CH.Workstore']+(['CH.InteriorRoof'] if roof else [])
        component=actor.static_mesh_component;assert component.set_static_mesh(mesh)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        for i in range(component.get_num_materials()):component.set_material(i,materials[min(i,len(materials)-1)])
        component.set_collision_profile_name('NoCollision' if roof else 'BlockAll')
        C.ACTORS[label]=actor;created.append(label)
        return actor

    with unreal.ScopedEditorTransaction('Cindehearth six purposeful work-yard structures'):
        for label in hidden:
            actor=C.ACTORS[label];component=actor.static_mesh_component
            snapshots.append(dict(label=label,hidden=actor.get_editor_property('hidden'),editor_hidden=actor.is_temporarily_hidden_in_editor(),
                                  collision=actor.get_actor_enable_collision(),visible=component.is_visible(),
                                  component_hidden=component.get_editor_property('hidden_in_game')))
            C.hide(actor)
        for row in plan['structures']:
            label=PREFIX+row['name'];point=row['center'];yaw=row['yaw']
            if row['kind']=='workshop':
                box=meshes['Smithy'].get_bounding_box();span=box.max-box.min
                # The source's long axis is Y, so yaw−90 produces world width300.
                width,depth,height=row['world_size']
                scale=(depth/span.x,width/span.y,height/span.z)
                position=fitted(meshes['Smithy'],point,scale,yaw)
                put(label,meshes['SmithyStructure'],position,scale,yaw,[smithy_material])
                put(label+'_Roof',meshes['SmithyRoof'],position,scale,yaw,[smithy_material],True)
                put(label+'_Walls',meshes['SmithyWalls'],position,scale,yaw,[stone_material])
            else:
                scale=[v*row['house_factor'] for v in house_scale]
                position=fitted(house_mesh,point,scale,yaw)
                put(label,house_mesh,position,scale,yaw,house_materials)
        for prop in plan['props']:
            actor=C.spawn(prop['label'],prop['key'],prop['center_bottom'],prop['size'],prop['yaw'],
                          tags=['CH.Dense_v004','CH.Workstore.Detail'],collision=False)
            actor.set_folder_path('Cindehearth/Dense_v004/Workyards');created.append(prop['label'])
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    (out/'workyards_applied.json').write_text(json.dumps(dict(status='applied_pending_visual_and_gameplay_QA',
         structure_count=6,prop_count=18,created=created,hidden_before=snapshots,plan=plan),ensure_ascii=False,indent=2),encoding='utf-8')
    print('SIX WORK STRUCTURES / EIGHTEEN PROPS APPLIED; visual and movement QA still required')
