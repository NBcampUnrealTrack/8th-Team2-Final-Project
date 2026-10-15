"""Read back two local effects and their unchanged camera/night contract."""
import unreal,json,math
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Fire_v005';out=root/'verification'
pie=unreal.WidgetService.is_pie_running();sub=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
w=sub.get_game_world() if pie else sub.get_editor_world()
actors={a.get_actor_label():a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)}
d=next(a for a in actors.values() if isinstance(a,unreal.NUCindehearthDirector))
rows=[]
for key,p in [('Hearth',(-59.4,-24.57,237.25)),('Forge',(-91.7,733,282))]:
 a=actors['CH_F5_FrontFire_'+key];c=a.static_mesh_component;r=a.get_actor_rotation()
 assert c.is_visible() and not a.get_editor_property('hidden')
 assert (a.get_actor_location()-unreal.Vector(*p)).length()<.01
 assert not a.get_actor_enable_collision() and str(c.get_collision_profile_name())=='NoCollision'
 assert abs(r.yaw)<.01 and not c.get_editor_property('cast_shadow')
 assert 'Fire_v005' in c.get_material(0).get_path_name()
 rows.append(dict(label=a.get_actor_label(),location=list(a.get_actor_location().to_tuple()),rotation=[r.pitch,r.yaw,r.roll],scale=list(a.get_actor_scale3d().to_tuple()),material=c.get_material(0).get_path_name(),bounds=[list(v.to_tuple()) for v in a.get_actor_bounds(False)]))
old=['CH_HD2D_HearthFire'+str(i) for i in range(3)]+['CH_HD2D_ForgeFire'+str(i) for i in range(2)]
assert all(not actors[n].static_mesh_component.is_visible() and not actors[n].get_actor_enable_collision() for n in old)
assert d.get_editor_property('village_camera_pitch')==-25 and d.get_editor_property('village_camera_fov')==32 and d.get_editor_property('village_camera_distance')==2100
states=[]
if pie:
 pc=unreal.GameplayStatics.get_player_controller(w,0);r=pc.player_camera_manager.get_camera_rotation()
 assert abs(r.pitch+25)<.01 and abs(r.yaw-90)<.01 and abs(pc.player_camera_manager.get_fov_angle()-32)<.01
 for night in (False,True):
  d.set_night(night)
  lights=[]
  for a in actors.values():
   if 'CH.Light.Hearth' in [str(t) for t in a.tags]:
    c=a.get_component_by_class(unreal.PointLightComponent);assert c and abs(c.intensity-(580 if night else 240))<.1
    lights.append(a.get_actor_label())
  assert len(lights)==2
  assert all(actors[row['label']].static_mesh_component.is_visible() for row in rows)
  states.append(dict(night=night,fire_visible=True,light_count=len(lights)))
 d.set_night(False);d.set_chapter_progress(0)
else:
 assert not d.get_editor_property('night') and d.get_editor_property('completed_chapter_quest')==0
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
report=dict(passed=True,pie=pie,fire_cards=rows,hidden_crossed_cards=old,camera=[-25,90,32,2100],states=states)
if not pie:
 report['source_material_unchanged']=json.loads((out/'original_material.json').read_text())==json.loads(unreal.MaterialNodeService.export_material_graph('/Game/Cindehearth/HD2D_v002/Materials/M_CH_PixelFire'))
 assert report['source_material_unchanged']
(out/('runtime_verified.json' if pie else 'saved_verified.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
print('FRONT FIRE VERIFIED', 'PIE' if pie else 'SAVED', '2 cards / fixed camera / original source intact')
