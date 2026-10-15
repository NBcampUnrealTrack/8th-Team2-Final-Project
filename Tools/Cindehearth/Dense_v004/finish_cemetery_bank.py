"""Cemetery trail/plateau support: 16 NoCollision meshes, no floor replacement.

Authoring proposal only; this agent has not executed it in Unreal.
STAGE defaults to 'review'. The coordinator may use STAGE='apply' after review.
--offline-preview prints the geometric plan without importing Unreal.
"""
import json
import math
import sys
from pathlib import Path

STAGE=globals().get('STAGE','review')
PREFIX='CH_D4_CemeteryBank_'


def compact(p):
    return (140.4+.74*p[0],.67*p[1],p[2])


def rotation_columns(pitch,yaw):
    p,y=math.radians(pitch),math.radians(yaw)
    cp,sp,cy,sy=math.cos(p),math.sin(p),math.cos(y),math.sin(y)
    return ((cp*cy,cp*sy,sp),(-sy,cy,0),(-sp*cy,-sp*sy,cp))


def rotated(point,pitch,yaw):
    axes=rotation_columns(pitch,yaw)
    return [sum(axes[j][i]*point[j] for j in range(3)) for i in range(3)]


def build_plan():
    # The original layout beam runs between these design endpoints.
    start=compact((66*45,48*45,7*45))
    end=compact((94*45,64*45,14*45))
    dx,dy,dz=(end[i]-start[i] for i in range(3))
    run=math.hypot(dx,dy)
    length=math.hypot(run,dz)
    yaw=math.degrees(math.atan2(dy,dx))
    pitch=math.degrees(math.atan2(dz,run))
    ux,uy=dx/run,dy/run
    side=(-uy,ux)
    rows=[]

    def add(name,kind,ceiling,size,yaw=0,pitch=0,purpose=''):
        rows.append(dict(label=PREFIX+name,kind=kind,ceiling_center=list(ceiling),size=list(size),
                         yaw=yaw,pitch=pitch,collision=False,purpose=purpose))

    midpoint=[(start[i]+end[i])*.5 for i in range(3)]
    midpoint[2]-=36
    # A narrow buried spine guarantees continuity beneath the path even between
    # irregular source-rock surfaces. Both its ends and sides sit inside rock.
    add('BuriedTrailCore','Core',midpoint,(length+40,90,1000),yaw,pitch,
        'Buried spine below the sloping path; never the visible outer silhouette')
    for i,t in enumerate((1/6,.5,5/6)):
        for sign in (-1,1):
            point=[start[j]+(end[j]-start[j])*t for j in range(3)]
            point[0]+=side[0]*sign*74
            point[1]+=side[1]*sign*74
            point[2]-=10
            # The full rock bounding-box ceiling follows the same inclined
            # plane as the path. It cannot protrude onto the walking surface.
            depth=(point[2]+280)/math.cos(math.radians(pitch))
            add('Trail_%02d_%s'%(i,'L' if sign<0 else 'R'),'Rock',point,
                (500+(12 if (i+sign)%2 else 0),316,depth),yaw,pitch,
                'Overlapping sloped bedrock from below the basin floor to just beneath the trail')

    # CemeteryHill: original centre(115,70,13.6), size(45,40,.8), then F.
    cx,cy,cz=compact((115*45,70*45,13.6*45))
    halfx,halfy=45*45*.74*.5,40*45*.67*.5
    xmin,xmax,ymin,ymax=cx-halfx,cx+halfx,cy-halfy,cy+halfy
    top=cz+.8*45*.5
    add('BuriedPlateauCore','Core',(cx,cy,top-26),(2*halfx-210,2*halfy-210,900),purpose=
        'Inner volume buried behind perimeter rocks; independent of the unchanged cemetery floor')
    for i,y in enumerate((ymin+195,ymin+603,ymax-195)):
        add('WestFace_%02d'%i,'Rock',(xmin+25,y,top-3-(i%2)*4),(470,570,830),yaw=(-4 if i%2 else 3),purpose=
            'Support the visible western lip and conceal the elevated rectangular slab edge')
    for i,x in enumerate((xmin+230,cx,xmax-230)):
        add('SouthFace_%02d'%i,'Rock',(x,ymin+25,top-3-(i%2)*3),(650,480,830),yaw=(3 if i%2 else -2),purpose=
            'Irregular southern bank below the cemetery graves and village vista')
    # Wide rear buttresses complete the island support. Their ceilings remain
    # below the terrain; the front/player-facing side uses six smaller pieces.
    add('NorthButtress','Rock',(cx,ymax-25,top-6),(2*halfx+170,480,830),purpose=
        'Conceal the northern inner-core face below the cemetery edge')
    add('EastButtress','Rock',(xmax-25,cy,top-6),(480,2*halfy+170,830),purpose=
        'Conceal the eastern inner-core face below the cemetery edge')
    assert len(rows)==16
    return dict(status='proposal_only',trail=dict(start=start,end=end,pitch=pitch,yaw=yaw,
                horizontal_run_cm=run),platform=dict(center=(cx,cy,cz),bounds=(xmin,xmax,ymin,ymax),top=top),
                placements=rows,count=len(rows),collision=False,
                notes=['No terrain, path, grave, vista, gate or source collider is moved.',
                       'Rock ceilings stay 10cm below the trail centre-plane, or at least3cm below the plateau top.',
                       'Existing thin trail/slab remain the walking surfaces. New meshes only provide visual support.',
                       'Runtime and player-camera acceptance remain for the coordinator.'])


if '--offline-preview' in sys.argv:
    print(json.dumps(build_plan(),ensure_ascii=False,indent=2))
    raise SystemExit(0)

import unreal
import importlib

root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004'
out=root/'verification'
sys.path.insert(0,str(root.parent/'Polish_v003'))
import ue_common as C
importlib.reload(C)
assert not unreal.WidgetService.is_pie_running(), 'Stop PIE before the visual bank is applied'
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
assert not (out/'cemetery_bank_applied.json').exists(), 'Inspect existing application rather than replaying it'
assert not any(label.startswith(PREFIX) for label in C.ACTORS), 'Inspect partially placed bank actors before proceeding'
plan=build_plan()
trail=C.ACTORS['CH_CemeteryTrail_Path']
platform=C.ACTORS['CH_CemeteryHill_Soil']
center,extent=platform.get_actor_bounds(False)
assert abs(center.x-plan['platform']['center'][0])<1 and abs(center.y-plan['platform']['center'][1])<1
assert abs(center.z+extent.z-plan['platform']['top'])<1, 'The platform no longer matches the inspected compact layout'
assert abs(trail.get_actor_scale3d().x-.333)<.001 and abs(trail.get_actor_scale3d().y-.3015)<.001

rock_receipt=json.loads((root.parent/'Restyle/verification/import_Rock.json').read_text(encoding='utf-8'))
assert rock_receipt['status']=='finished'
rock=unreal.load_asset(rock_receipt['meshes'][0]['path'])
rock_material=unreal.load_asset('/Game/Cindehearth/HD2D_v002/Materials/MI_CH_P2_Rock')
core=unreal.load_asset('/Engine/BasicShapes/Cube')
core_material=unreal.load_asset('/Game/Cindehearth/HD2D_v002/Materials/MI_CH_Rock')
assert rock and rock_material and core and core_material
plan['real_path_bounds']=[list(v.to_tuple()) for v in trail.get_actor_bounds(False)]
plan['real_platform_bounds']=[list(v.to_tuple()) for v in platform.get_actor_bounds(False)]
(out/'cemetery_bank_proposal.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
print('CEMETERY BANK REVIEW',len(plan['placements']),'NoCollision meshes')

if STAGE=='apply':
    made=[]
    with unreal.ScopedEditorTransaction('Cindehearth continuous cemetery slope and plateau bank'):
        for row in plan['placements']:
            mesh=core if row['kind']=='Core' else rock
            material=core_material if row['kind']=='Core' else rock_material
            bounds=mesh.get_bounding_box();lo=bounds.min.to_tuple();hi=bounds.max.to_tuple()
            scale=[row['size'][i]/(hi[i]-lo[i]) for i in range(3)]
            local_ceiling=[(lo[0]+hi[0])*.5*scale[0],(lo[1]+hi[1])*.5*scale[1],hi[2]*scale[2]]
            offset=rotated(local_ceiling,row['pitch'],row['yaw'])
            origin=[row['ceiling_center'][i]-offset[i] for i in range(3)]
            actor=C.AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*origin),
                  unreal.Rotator(pitch=row['pitch'],yaw=row['yaw']))
            actor.set_actor_label(row['label']);actor.set_folder_path('Cindehearth/Dense_v004/CemeteryBank')
            actor.tags=['Cindehearth','CH.HD2D','CH.Dense_v004','CH.CemeteryBank']
            component=actor.static_mesh_component;assert component.set_static_mesh(mesh)
            actor.set_actor_scale3d(unreal.Vector(*scale))
            for i in range(component.get_num_materials()):component.set_material(i,material)
            component.set_collision_profile_name('NoCollision');actor.set_actor_enable_collision(False)
            assert str(component.get_collision_profile_name())=='NoCollision'
            C.ACTORS[row['label']]=actor
            made.append(dict(label=row['label'],origin=origin,scale=scale,pitch=row['pitch'],yaw=row['yaw'],
                             bounds=[list(v.to_tuple()) for v in actor.get_actor_bounds(False)],collision=False))
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    (out/'cemetery_bank_applied.json').write_text(json.dumps(dict(status='applied_pending_player_camera_review',
             count=len(made),actors=made,plan=plan),ensure_ascii=False,indent=2),encoding='utf-8')
    print('CEMETERY BANK APPLIED:16 visual support meshes. Player-camera review still required.')
else:
    print('REVIEW ONLY: no editor actors changed')
