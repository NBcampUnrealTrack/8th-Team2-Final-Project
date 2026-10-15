"""Replace all rejected pine meshes, retaining natural proportions and base positions."""
import unreal,json,random,math
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle'
out=root/'verification/pine_volume_replaced.json';assert not out.exists()
review=json.loads((root/'verification/import_PineVolume.json').read_text(encoding='utf-8'))
old=json.loads((root/'verification/import_Pine.json').read_text(encoding='utf-8'))['meshes'][0]['path']
mesh=unreal.load_asset(review['meshes'][0]['path']);assert mesh
b=mesh.get_bounding_box();lo=b.min;hi=b.max;size=hi-lo
assert min(size.x,size.y)/max(size.x,size.y)>.65
L=unreal.EditorAssetLibrary;S=unreal.MaterialService;dest='/Game/Cindehearth/HD2D_v002/Materials'
color=next(p for p in review['assets'] if p.endswith('/Color.Color'))
normal=next(p for p in review['assets'] if p.endswith('/Normal.Normal'))
t=unreal.load_asset(color);t.set_editor_property('filter',unreal.TextureFilter.TF_NEAREST);L.save_loaded_asset(t)
mi=dest+'/MI_CH_P2_PineVolume';assert not L.does_asset_exist(mi)
assert S.create_instance('/Game/Cindehearth/Tripo/GormHouse_P2_v001/MI_CH_GormP2_Pixel','MI_CH_P2_PineVolume',dest).success
assert S.set_instance_texture_parameter(mi,'Albedo',color)
assert S.set_instance_texture_parameter(mi,'NormalMap',normal)
for k,v in [('PixelGrid',256),('NormalStrength',.015),('Roughness',.97),('AmbientLift',.065)]:assert S.set_instance_scalar_parameter(mi,k,v)
assert S.set_instance_vector_parameter(mi,'Tint',1.26,1.28,1.13);assert S.save_instance(mi)
mat=unreal.load_asset(mi);AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
trees=[a for a in AS.get_all_level_actors() if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_path_name()==old]
assert len(trees)==95,len(trees)
rows=[];rng=random.Random(905263)
with unreal.ScopedEditorTransaction('Replace flat pines with volumetric P2 pine'):
    for a in sorted(trees,key=lambda a:a.get_actor_label()):
        c=a.static_mesh_component;ob=c.static_mesh.get_bounding_box();s=a.get_actor_scale3d();p=a.get_actor_location()
        height=(ob.max.z-ob.min.z)*s.z
        oldrot=a.get_actor_rotation();oldscale=list(s.to_tuple());oldpos=list(p.to_tuple())
        basez=p.z+ob.min.z*s.z
        # Preserve world-space center/base while replacing the old flattened dimensions.
        oldang=math.radians(oldrot.yaw);oldcx=(ob.min.x+ob.max.x)*.5*s.x;oldcy=(ob.min.y+ob.max.y)*.5*s.y
        oc=unreal.Vector(p.x+oldcx*math.cos(oldang)-oldcy*math.sin(oldang),p.y+oldcx*math.sin(oldang)+oldcy*math.cos(oldang),basez)
        scale=height/size.z;yaw=rng.uniform(-180,180);ang=math.radians(yaw)
        cx=(lo.x+hi.x)*.5*scale;cy=(lo.y+hi.y)*.5*scale
        pos=unreal.Vector(oc.x-cx*math.cos(ang)+cy*math.sin(ang),oc.y-cx*math.sin(ang)-cy*math.cos(ang),basez-lo.z*scale)
        a.modify();assert c.set_static_mesh(mesh);c.set_material(0,mat)
        a.set_actor_scale3d(unreal.Vector(scale,scale,scale));a.set_actor_rotation(unreal.Rotator(yaw=yaw),False);a.set_actor_location(pos,False,False)
        c.set_collision_profile_name('NoCollision')
        rows.append(dict(label=a.get_actor_label(),previous=dict(mesh=old,scale=oldscale,location=oldpos,yaw=oldrot.yaw),scale=scale,height_cm=height,location=list(pos.to_tuple()),yaw=yaw))
assert all(max(a.get_actor_scale3d().to_tuple())-min(a.get_actor_scale3d().to_tuple())<1e-6 for a in trees)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
out.write_text(json.dumps(dict(mesh=mesh.get_path_name(),material=mi,count=len(rows),dimensions_cm=list(size.to_tuple()),horizontal_depth_ratio=min(size.x,size.y)/max(size.x,size.y),uniform_scale=True,trees=rows),indent=2),encoding='utf-8')
print('VOLUMETRIC PINES REPLACED',len(rows),'ALL UNIFORMLY SCALED',list(size.to_tuple()))
