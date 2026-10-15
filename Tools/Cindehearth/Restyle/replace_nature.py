"""Fit inspected P2 assets; rebuild cliffs as low overlapping rock strata."""
import unreal,json,random,math
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle'
review=root/'verification';assert not (review/'nature_replaced.json').exists()
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors={a.get_actor_label():a for a in AS.get_all_level_actors()}
base='/Game/Cindehearth/HD2D_v002';S=unreal.MaterialService;L=unreal.EditorAssetLibrary
rng=random.Random(90526);meshes={};mats={};changes=[]
for key,tint in [('Rock',(1.13,1.12,1.10)),('Pine',(1.18,1.20,1.10))]:
    receipt=json.loads((review/('import_'+key+'.json')).read_text(encoding='utf-8'));assert receipt['status']=='finished'
    mesh=unreal.load_asset(receipt['meshes'][0]['path']);meshes[key]=mesh
    color=next(p for p in receipt['assets'] if p.endswith('/Color.Color'))
    normal=next(p for p in receipt['assets'] if p.endswith('/Normal.Normal'))
    t=unreal.load_asset(color);t.set_editor_property('filter',unreal.TextureFilter.TF_NEAREST);L.save_loaded_asset(t)
    name='MI_CH_P2_'+key;mi=base+'/Materials/'+name
    assert not L.does_asset_exist(mi)
    assert S.create_instance('/Game/Cindehearth/Tripo/GormHouse_P2_v001/MI_CH_GormP2_Pixel',name,base+'/Materials').success
    assert S.set_instance_texture_parameter(mi,'Albedo',color)
    assert S.set_instance_texture_parameter(mi,'NormalMap',normal)
    for k,v in [('PixelGrid',256),('NormalStrength',.04),('Roughness',.97),('AmbientLift',.045)]:assert S.set_instance_scalar_parameter(mi,k,v)
    assert S.set_instance_vector_parameter(mi,'Tint',*tint);assert S.save_instance(mi)
    mats[key]=unreal.load_asset(mi)

def hide(a):
    a.modify();a.static_mesh_component.set_visibility(False,True);a.static_mesh_component.set_hidden_in_game(True,True)
    a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True);a.set_actor_enable_collision(False)

def spawn(name,key,pos,size,yaw=0,collision=False,mat=None):
    assert name not in actors
    mesh=meshes[key];b=mesh.get_bounding_box();lo=b.min.to_tuple();hi=b.max.to_tuple()
    scale=[size[i]/(hi[i]-lo[i]) for i in range(3)]
    # Mesh local center and lowest point, transformed through yaw.
    cx=(lo[0]+hi[0])*.5*scale[0];cy=(lo[1]+hi[1])*.5*scale[1];ang=math.radians(yaw)
    p=unreal.Vector(pos[0]-cx*math.cos(ang)+cy*math.sin(ang),pos[1]-cx*math.sin(ang)-cy*math.cos(ang),pos[2]-lo[2]*scale[2])
    a=AS.spawn_actor_from_class(unreal.StaticMeshActor,p,unreal.Rotator(yaw=yaw));a.set_actor_label(name)
    a.set_folder_path('Cindehearth/HD2D/Nature');a.tags=['Cindehearth','CH.HD2D'];c=a.static_mesh_component
    assert c.set_static_mesh(mesh);a.set_actor_scale3d(unreal.Vector(*scale))
    c.set_material(0,mat or mats[key]);c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    c.set_visibility(True,True);c.set_hidden_in_game(False,True);a.set_actor_hidden_in_game(False);a.set_is_temporarily_hidden_in_editor(False)
    actors[name]=a;return a

with unreal.ScopedEditorTransaction('Cindehearth P2 rock and pine environment'):
    for a in list(actors.values()):
        if not isinstance(a,unreal.StaticMeshActor):continue
        mesh=a.static_mesh_component.static_mesh
        if not mesh:continue
        label=a.get_actor_label();path=mesh.get_path_name();p=a.get_actor_location();s=a.get_actor_scale3d();b=mesh.get_bounding_box()
        if 'SM_Rock_A.' in path or 'SM_Cliff_Block.' in path:
            oldsize=[(b.max.to_tuple()[i]-b.min.to_tuple()[i])*s.to_tuple()[i] for i in range(3)]
            iscliff='Cliff' in label;isridge='RidgeMass' in label
            yaw=a.get_actor_rotation().yaw
            if iscliff or isridge:
                # Broader, lower layers replace the former repeated vertical slabs.
                oldsize[0]*=1.20;oldsize[1]*=1.08;oldsize[2]*=.56 if isridge else .66
                if label.startswith('CH_North') or isridge and p.y>4000:
                    oldsize[0]=min(oldsize[0],430) if 0<=p.x<=1150 else oldsize[0]
                    yaw=rng.uniform(-12,12)
                else:yaw=90+rng.uniform(-12,12)
            spawn(label+'_HD2D','Rock',p.to_tuple(),oldsize,yaw,True)
            hide(a);changes.append(label)
        elif label.startswith('CH_Pine_') and label.endswith('_Pine'):
            # Broad front silhouette faces the south-facing HD2D camera; slight variation only.
            size=[240*rng.uniform(.8,1.15),105,905*s.z]
            spawn(label+'_HD2D','Pine',p.to_tuple(),size,rng.uniform(-18,18),False)
            hide(a);trunk=actors.get(label[:-5]+'_Timber')
            # Keep the slender trunk collider, hide only its old visible mesh.
            if trunk:
                trunk.static_mesh_component.set_visibility(False,True);trunk.static_mesh_component.set_hidden_in_game(True,True)
            changes.append(label)
    # Sparse flat scree piles soften retaining walls without intruding on stair/ramp gaps.
    for row,y,z in [(0,-1310,0),(1,1390,157.5)]:
        for i,x in enumerate(range(-3100,3101,145)):
            if abs(x-540)<260 or abs(x+810)<300 or abs(x-1600)<220:continue
            spawn(f'CH_HD2D_WallScree_{row}_{i}','Rock',(x+rng.uniform(-35,35),y-12+rng.uniform(-18,18),z-8),(rng.uniform(85,155),rng.uniform(40,90),rng.uniform(24,65)),rng.uniform(-25,25))
    # Distant ridge layers are outside playable town/fields/dungeon; no collision.
    for layer,y in enumerate([11500,15300]):
        for i,x in enumerate(range(-17000,17001,1800)):
            height=2600+1100*math.sin(i*.83+layer)+rng.uniform(0,500)
            a=spawn(f'CH_HD2D_DistantRidge_{layer}_{i}','Rock',(x,y+rng.uniform(-550,550),-1000),(3600,3100,height),rng.uniform(-15,15))
            a.static_mesh_component.set_material(0,unreal.load_asset(base+'/Materials/MI_CH_Rock'))
    # Forest edge silhouettes and irregular skirts hide the edge of the square field floor.
    for side,x in enumerate([-2250,2640]):
        for i,y in enumerate(range(-8900,-4899,260)):
            spawn(f'CH_HD2D_ForestEdge_{side}_{i}','Pine',(x+rng.uniform(-150,150),y+rng.uniform(-70,70),-8),(rng.uniform(235,320),130,rng.uniform(420,590)),rng.uniform(-18,18))
    # Broad landform skirts are below walkable ground, never substituted for route collision.
    for i,(p,size) in enumerate([((-5300,-5300,-1150),(5800,5000,1050)),((4500,-5600,-1100),(4200,6000,1000)),((-5000,2000,-1500),(5500,6900,1300)),((5000,1000,-1300),(4000,8000,1150)),((0,-10200,-1300),(6600,5000,1180))]):
        spawn(f'CH_HD2D_LandSkirt_{i}','Rock',p,size,0,False,unreal.load_asset(base+'/Materials/MI_CH_Rock'))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(review/'nature_replaced.json').write_text(json.dumps(dict(replaced=changes,added=[n for n in actors if n.startswith('CH_HD2D_')],materials={k:v.get_path_name() for k,v in mats.items()}),indent=2),encoding='utf-8')
print('NATURE REPLACED',len(changes))
