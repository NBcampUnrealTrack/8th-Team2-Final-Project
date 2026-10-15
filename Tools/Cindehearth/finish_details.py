import unreal
import math
import json
from pathlib import Path
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lib=unreal.EditorAssetLibrary
svc=unreal.MaterialService
nodes=unreal.MaterialNodeService
index={a.get_actor_label():a for a in AS.get_all_level_actors()}
DEST='/Game/Cindehearth'
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
def vec(p):return unreal.Vector(*[v*45 for v in p])
def add(name,path,pos,scale,mat=None,yaw=0,tags=()):
    if 'CH_'+name in index:return index['CH_'+name]
    a=AS.spawn_actor_from_class(unreal.StaticMeshActor,vec(pos),unreal.Rotator(yaw=yaw))
    a.set_actor_label('CH_'+name);a.set_folder_path('Cindehearth/FinalDetails')
    a.tags=[unreal.Name('Cindehearth')]+[unreal.Name(t) for t in tags]
    a.static_mesh_component.set_static_mesh(unreal.load_asset(path));a.set_actor_scale3d(unreal.Vector(*[v*.45 for v in scale]))
    if mat:a.static_mesh_component.set_material(0,unreal.load_asset(mat))
    index['CH_'+name]=a
    return a

sky=DEST+'/Materials/MI_CH_Sky_Night'
if not lib.does_asset_exist(sky):
    assert svc.create_instance(DEST+'/Materials/MI_CH_Sky','MI_CH_Sky_Night',DEST+'/Materials').success
    for key,val in {'ZenithCol':(.008,.016,.030),'MidCol':(.024,.036,.055),'HorizonCol':(.09,.10,.12),
                    'GroundCol':(.008,.011,.015),'RidgeFar':(.025,.037,.055),'RidgeNear':(.015,.021,.03),
                    'CloudCol':(.065,.085,.12),'CloudShadow':(.02,.035,.065)}.items():svc.set_instance_vector_parameter(sky,key,*val)
    svc.save_instance(sky)

smoke=DEST+'/Materials/M_CH_ChimneySmoke'
if not lib.does_asset_exist(smoke):
    assert svc.create_material('M_CH_ChimneySmoke',DEST+'/Materials').success
    material=unreal.load_asset(smoke)
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('two_sided',False)
    col=nodes.create_parameter(smoke,'Vector','SmokeColor','','.16,.17,.18,1',-400,0)
    alpha=nodes.create_parameter(smoke,'Scalar','Density','','.16',-400,300)
    fresnel=nodes.create_expression(smoke,'Fresnel',-600,150)
    inv=nodes.create_expression(smoke,'OneMinus',-400,150)
    mul=nodes.create_expression(smoke,'Multiply',-150,180)
    assert nodes.connect_expressions(smoke,fresnel.id,'',inv.id,'')
    assert nodes.connect_expressions(smoke,inv.id,'',mul.id,'A')
    assert nodes.connect_expressions(smoke,alpha.id,'',mul.id,'B')
    assert nodes.connect_to_output(smoke,col.id,'','BaseColor')
    assert nodes.connect_to_output(smoke,mul.id,'','Opacity')
    # Persistent loops: soft silhouette and smooth shader-driven drift, no pops.
    t=nodes.create_expression(smoke,'Time',-650,520)
    sine=nodes.create_expression(smoke,'Sine',-450,520)
    wind=nodes.create_parameter(smoke,'Vector','Wind','','14,4,7,1',-450,680)
    motion=nodes.create_expression(smoke,'Multiply',-200,550)
    assert nodes.connect_expressions(smoke,t.id,'',sine.id,'')
    assert nodes.connect_expressions(smoke,sine.id,'',motion.id,'A')
    assert nodes.connect_expressions(smoke,wind.id,'',motion.id,'B')
    assert nodes.connect_to_output(smoke,motion.id,'','WorldPositionOffset')
    assert svc.compile_material(smoke);svc.save_material(smoke)

data=json.loads((Path(unreal.Paths.project_dir())/'Tools/Cindehearth/layout.json').read_text(encoding='utf-8'))
dims={'RowHouse':(7,6,3.5),'SlopeHouse':(6,7,4),'BurrowHouse':(7.5,5.5,3.25),'Smithy':(12,9,4.2),'Tavern':(14,10,6.6),'Office':(5,4,3.2),'Clinic':(6,5,3.3)}
for n,p in enumerate(data['placements']):
    if p['module'] not in dims or n%3==1:continue
    w,d,h=dims[p['module']];x,y,z=p['pos']
    for i in range(4):
        a=add(p['name']+'_Smoke'+str(i),'/Engine/BasicShapes/Sphere',(x+w*.3+i*.28,y+d*.2,z+h+3.55+i*.85),(.65+i*.45,.70+i*.4,1.4+i*.35),smoke,tags=('CH.Smoke',))
        a.static_mesh_component.set_collision_profile_name('NoCollision');a.static_mesh_component.set_cast_shadow(False)

# Public interiors have open entrances; external workshop geometry is retained.
for building in ('AssayOffice','FreyaTavern','MarenClinic'):
    for mat in ('Timber','Brass'):
        a=index.get('CH_'+building+'_'+mat)
        if a:
            a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True);a.set_actor_enable_collision(False)
    for mat in ('Roof','RoofPatch'):
        a=index.get('CH_'+building+'_'+mat)
        if a:
            a.tags=list(a.tags)+[unreal.Name('CH.InteriorRoof')]

for name,x in [('ClinicCot_28',29.5),('ClinicCot_34',32.5)]:
    for a in list(index.values()):
        if a.get_actor_label().startswith('CH_'+name+'_'):
            pos=a.get_actor_location();pos.x=x*45;a.set_actor_location(pos,False,False)

# Archive stair runs east-west into the cellar, outside the documentary worktable.
# A ladder is provided at the hatch; the director exposes a traversal helper for it.
for side in (-1,1):
    add('ArchiveLadderUpright'+str(side),DEST+'/Meshes/SM_CH_Block_Stone',(31+side*.52,7.35,1.4),(.08,.08,3.9),DEST+'/Materials/MI_CH_Iron')
for i in range(11):add('ArchiveLadderRung'+str(i),DEST+'/Meshes/SM_CH_Block_Stone',(31,7.35,-.3+i*.35),(1.1,.09,.07),DEST+'/Materials/MI_CH_Iron')

for mat in ('Paper','Seal'):
    a=add('DorenLetter_'+mat,DEST+'/Meshes/SM_CH_Letter_'+mat,(-23.8,-39.64,1.3),(1.6,1.6,1.6),tags=('CH.EpilogueOnly',))
    a.static_mesh_component.set_collision_profile_name('NoCollision')
    a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True)

if 'CH_Messenger' not in index:
    a=AS.spawn_actor_from_class(unreal.PaperSpriteActor,vec((-25.6,-40.8,0)))
    a.set_actor_label('CH_Messenger');a.set_folder_path('Cindehearth/Cinematics')
    a.tags=[unreal.Name('Cindehearth'),unreal.Name('CH.EpilogueOnly'),unreal.Name('CH.Resident'),unreal.Name('CH.Messenger')]
    c=a.get_component_by_class(unreal.PaperSpriteComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    assert c.set_sprite(unreal.load_asset('/Game/Art/Sprites/NPC/NPC7_Sprite'))
    c.set_sprite_color(unreal.LinearColor(.08,.10,.14,1));c.set_collision_profile_name('NoCollision')
    a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True)

AS.select_nothing();LS.save_current_level();print('FINISH DETAILS SAVED',len(AS.get_all_level_actors()))
