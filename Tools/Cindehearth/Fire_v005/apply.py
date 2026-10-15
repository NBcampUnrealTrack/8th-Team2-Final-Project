"""Replace crossed silhouettes with two front-view fire cards."""
import unreal,json,math
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Fire_v005';out=root/'verification'
assert not unreal.WidgetService.is_pie_running()
assert not (out/'applied.json').exists()
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors={a.get_actor_label():a for a in AS.get_all_level_actors()}
oldnames=['CH_HD2D_HearthFire'+str(i) for i in range(3)]+['CH_HD2D_ForgeFire'+str(i) for i in range(2)]
assert all(n in actors and actors[n].static_mesh_component.is_visible() for n in oldnames)
dest='/Game/Cindehearth/Fire_v005/Materials';plane=unreal.load_asset('/Engine/BasicShapes/Plane')
plans=[dict(name='Hearth',center=(-59.4,30.43,249.25),scale=(1.9,1.6,1),roll=-115),dict(name='Forge',center=(-137.7,733,213),scale=(.75,.52,1),roll=-90)]
rows=[]
with unreal.ScopedEditorTransaction('Cindehearth fixed camera front fire and grounded forge flame'):
 for name in oldnames:
  a=actors[name];a.modify();c=a.static_mesh_component;c.set_visibility(False,True);c.set_hidden_in_game(True,True);a.set_actor_hidden_in_game(True);a.set_actor_enable_collision(False)
 for p in plans:
  a=AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p['center']),unreal.Rotator(roll=p['roll'],yaw=0,pitch=0));a.set_actor_label('CH_F5_FrontFire_'+p['name']);a.set_folder_path('Cindehearth/Fire_v005')
  a.tags=['Cindehearth','CH.HD2D','CH.Fire.Front'];c=a.static_mesh_component;assert c.set_static_mesh(plane)
  a.set_actor_scale3d(unreal.Vector(*p['scale']));m=unreal.load_asset(dest+'/MI_CH_FrontFire_'+p['name']);assert m;c.set_material(0,m)
  c.set_collision_profile_name('NoCollision');a.set_actor_enable_collision(False);c.set_cast_shadow(False)
  rows.append(dict(label=a.get_actor_label(),plan=p,material=m.get_path_name(),bounds=[list(v.to_tuple()) for v in a.get_actor_bounds(False)]))
 assert all(not actors[n].static_mesh_component.is_visible() for n in oldnames)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(out/'applied.json').write_text(json.dumps(dict(status='applied_pending_visual_review',hidden=oldnames,new=rows,camera_unchanged=True,light_unchanged=True,source_assets_unchanged=True),indent=2),encoding='utf-8')
print('FRONT FIRE APPLIED:5 crossed cards hidden,2 front cards, forge flame grounded at firebox')
