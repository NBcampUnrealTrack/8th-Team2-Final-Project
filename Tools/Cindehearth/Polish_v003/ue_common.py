import unreal,json,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003'
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ACTORS={a.get_actor_label():a for a in AS.get_all_level_actors()}
MESH={};MAT={}
def resources(key):
 if key not in MESH:
  r=json.loads((ROOT/'verification'/('import_'+key+'.json')).read_text());assert r['status']=='finished'
  MESH[key]=unreal.load_asset(r['meshes'][0]['path']);MAT[key]=unreal.load_asset('/Game/Cindehearth/Polish_v003/Materials/MI_CH_'+key);assert MESH[key] and MAT[key]
 return MESH[key],MAT[key]
def hide(a,keep_collision=False):
 a.modify();a.static_mesh_component.set_visibility(False,True);a.static_mesh_component.set_hidden_in_game(True,True)
 a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True)
 if not keep_collision:a.set_actor_enable_collision(False)
def spawn(label,key,center_bottom,size,yaw=0,tags=None,collision=False):
 assert label not in ACTORS,label
 mesh,mat=resources(key);b=mesh.get_bounding_box();lo=b.min.to_tuple();hi=b.max.to_tuple()
 scale=[size[i]/(hi[i]-lo[i]) for i in range(3)];cx=(lo[0]+hi[0])*.5*scale[0];cy=(lo[1]+hi[1])*.5*scale[1];ang=math.radians(yaw)
 p=[center_bottom[0]-cx*math.cos(ang)+cy*math.sin(ang),center_bottom[1]-cx*math.sin(ang)-cy*math.cos(ang),center_bottom[2]-lo[2]*scale[2]]
 a=AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),unreal.Rotator(yaw=yaw));a.set_actor_label(label);a.set_folder_path('Cindehearth/Polish_v003/'+key)
 a.tags=['Cindehearth','CH.HD2D','CH.Polish_v003']+list(tags or []);c=a.static_mesh_component;c.set_static_mesh(mesh)
 a.set_actor_scale3d(unreal.Vector(*scale));c.set_material(0,mat);c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
 ACTORS[label]=a;return a
def save_result(name,data):
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 p=ROOT/'verification'/(name+'.json');assert not p.exists();p.write_text(json.dumps(data,indent=2),encoding='utf-8')
