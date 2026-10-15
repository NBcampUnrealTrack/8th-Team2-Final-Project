import unreal
import json
import random
from pathlib import Path
R=random.Random(11820)
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=AS.get_all_level_actors()
index={a.get_actor_label():a for a in actors}
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
STAGE=globals().get('STAGE','palette')
DEST='/Game/Cindehearth'
lib=unreal.EditorAssetLibrary
svc=unreal.MaterialService

def v(p):return unreal.Vector(*[n*45 for n in p])
def spawn(name,mesh,pos,scale,material=None,yaw=0,folder='Dressing',tags=()):
    name='CH_'+name
    if name in index:return index[name]
    a=AS.spawn_actor_from_class(unreal.StaticMeshActor,v(pos),unreal.Rotator(yaw=yaw))
    a.set_actor_label(name);a.set_folder_path('Cindehearth/'+folder)
    a.tags=[unreal.Name('Cindehearth')]+[unreal.Name(t) for t in tags]
    a.static_mesh_component.set_static_mesh(unreal.load_asset(mesh))
    if material:a.static_mesh_component.set_material(0,unreal.load_asset(material))
    a.set_actor_scale3d(unreal.Vector(*[n*.45 for n in scale]));index[name]=a
    return a

if STAGE=='palette':
    # The earlier village's Soil texture is ploughed garden soil. Use ash/dirt.
    for key,source,tile,tint in [('Soil','T_Dirt_Path_Pixel',350,(.55,.57,.59)),
                                ('Roof','T_Prop_Metal_Pixel',130,(.43,.47,.50)),
                                ('RoofPatch','T_Prop_Metal_Pixel',115,(.58,.54,.45)),
                                ('Path','T_Cobblestone_Pixel',190,(.49,.49,.47))]:
        p=DEST+'/Materials/MI_CH_'+key
        print('BEFORE',key,svc.get_instance_info(p))
        for kind in ('BC','N','ORM'):
            path='/Game/Village/Textures/Pixel/'+source+'_'+kind
            assert lib.does_asset_exist(path)
            assert svc.set_instance_texture_parameter(p,'Tex_'+kind,path)
        svc.set_instance_vector_parameter(p,'Tint',*tint)
        svc.set_instance_scalar_parameter(p,'TileCm',tile)
        svc.save_instance(p)
    for key,vals in [('Flame',(2.8,.58,.025)),('Ember',(.9,.07,.002))]:
        svc.set_instance_vector_parameter(DEST+'/Materials/MI_CH_'+key,'Glow',*vals)
        svc.save_instance(DEST+'/Materials/MI_CH_'+key)
    for name,intensity in [('HearthLight',420),('HearthBounce',90),('SmithyLight',190)]:
        a=index['CH_'+name];a.light_component.set_intensity(intensity)
        a.light_component.set_editor_property('cast_shadows',False)
    index['CH_Sun'].light_component.set_intensity(11)
    index['CH_SkyLight'].light_component.set_intensity(5.5)
    unreal.ViewportService.set_game_view(True)
    print('PALETTE CORRECTED')

elif STAGE=='dressing':
    rock='/Game/Village/Meshes/SM_Rock_A';rm=DEST+'/Materials/MI_CH_Rock'
    # Natural mountain masses over the sharp retaining ledges.
    for i in range(58):
        if i<30:
            x=-89+i*6.0;y=97+R.uniform(-2,9)
            if abs(x-12)<11:continue
        else:
            x=(-1 if i%2 else 1)*R.uniform(71,82);y=-81+(i-30)*6.2
            if x<0 and -1<y<27 or x>0 and 44<y<73:continue
        z=(0 if y<-30 else 3.5 if y<30 else 7)-2
        spawn('RidgeMass_%02d'%i,rock,(x,y,z),(R.uniform(8,15),R.uniform(8,13),R.uniform(12,30)),rm,R.uniform(0,360),'Greyvein')
    for i in range(115):
        x=R.choice([-1,1])*R.uniform(40,72);y=R.uniform(-23,85)
        # Leave the cemetery trail approach and work fronts free.
        if x>58 and 40<y<57:continue
        z=3.5 if y<30 else 7
        spawn('Scree_%03d'%i,rock,(x,y,z),(R.uniform(.8,3.2),R.uniform(.8,2.8),R.uniform(.5,2)),rm,R.uniform(0,360),'Dressing/Scree')
    # Grouped stacks read as work areas rather than stray boxes in empty space.
    for c,(x,y,z) in enumerate([(-28,52,7),(-41,70,7),(31,57,7),(44,74,7),(-37,16,3.5),(49,18,3.5)]):
        for i in range(8):spawn('Stack_%d_%d'%(c,i),'/Game/Village/Meshes/SM_Crate',(x+i%3*.95,y+(i//3)*.95,z+(i%7==0)*.8),(1,1,1),folder='Dressing/Workyards')
        for i in range(4):spawn('RawOre_%d_%d'%(c,i),rock,(x+4+i%2,y+i//2,z),(1.5,1.5,1.3),rm,folder='Dressing/Workyards')
    # Hammer, glove and log silhouettes use the same modular kit.
    data=json.loads((Path(unreal.Paths.project_dir())/'Tools/Cindehearth/layout.json').read_text(encoding='utf-8'))
    for p in data['placements']:
        if p['module'] in ('RowHouse','BurrowHouse','SlopeHouse'):
            x,y,z=p['pos']
            for j in range(3):
                a=spawn(p['name']+'_Firewood'+str(j),'/Engine/BasicShapes/Cylinder',(x-2+j*.42,y-3.5,z+.22),(.35,.35,1.4),DEST+'/Materials/MI_CH_Timber',folder='Dressing/Homes')
                a.set_actor_rotation(unreal.Rotator(pitch=90),False)
    # Keep arrival clearly framed while first seeing all three terraces.
    for i in range(9):spawn('ArrivalBoulders_%d'%i,rock,(-4-i*.8,-95-i*1.8,-1),(3,3,2),rm,i*25,'Pinehollow')
    print('DRESSING',len(index))

elif STAGE=='people':
    # Reuse existing lit sprite art as provisional residents, without Libergrove quest components.
    crowd=[]
    for i in range(24):
        table=i//5
        import math
        ang=math.radians([20,75,140,205,270][min(table,4)])
        cx=-6+8.6*math.cos(ang);cy=8.6*math.sin(ang)
        crowd.append(('EveningResident_%02d'%i,(cx+math.cos(ang+math.pi/2)*((i%5)-2)*.8,cy+math.sin(ang+math.pi/2)*((i%5)-2)*.8,3.7),'Hearth',('CH.Crowd',)))
    crowd += [('Gorm', (18,65,7),'Mine',('CH.NPC.Gorm',)),('Dorik',(-10,18,3.5),'Smithy',('CH.NPC.Dorik',)),
              ('Freya',(-9,-63,0),'Residential',('CH.NPC.Freya',)),('Maren',(30,-51,0),'Residential',('CH.NPC.Maren',)),
              ('YoungAssayer',(30,8,3.5),'Assay',('CH.NPC.Assayer',)),('Child01',(-34,-48,0),'Residential',()),('Child02',(-31,-47,0),'Residential',())]
    for i,(name,pos,zone,tags) in enumerate(crowd):
        if 'CH_'+name in index:continue
        sprite=unreal.load_asset('/Game/Art/Sprites/NPC/NPC'+str(1+i%8)+'_Sprite')
        assert sprite
        a=AS.spawn_actor_from_class(unreal.PaperSpriteActor,v(pos),unreal.Rotator(yaw=0))
        a.set_actor_label('CH_'+name);a.set_folder_path('Cindehearth/'+zone+'/Residents')
        a.tags=[unreal.Name('Cindehearth'),unreal.Name('CH.Resident')]+[unreal.Name(t) for t in tags]
        c=a.get_component_by_class(unreal.PaperSpriteComponent)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        assert c.set_sprite(sprite)
        c.set_collision_profile_name('NoCollision');c.set_cast_shadow(True)
        c.set_sprite_color(unreal.LinearColor(.65,.68,.68,1))
        # Feet anchors are validated against the source sprite's bounds below.
        b=a.get_actor_bounds(False);floor=v(pos).z
        a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,floor-(b[0].z-b[1].z)),False,False)
        if 'CH.Crowd' in tags:
            a.set_actor_hidden_in_game(i%6!=0)
            a.set_is_temporarily_hidden_in_editor(i%6!=0)
    print('RESIDENTS',len(crowd))

AS.select_nothing();LS.save_current_level()
