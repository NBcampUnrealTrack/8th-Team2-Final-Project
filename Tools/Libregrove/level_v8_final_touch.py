"""Scoped finishing inspection and reversible overview-only showflag."""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Docs/Libregrove/Level_V8_20260908'
assert not unreal.WidgetService.is_pie_running()
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert w.get_name()=='L_Libregrove'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=sub.get_all_level_actors()
stage=globals().get('STAGE','inspect')
if stage=='inspect':
 rows=[]
 for a in actors:
  if 'Watermill' in a.get_actor_label() and isinstance(a,unreal.StaticMeshActor):
   rows.append({'label':a.get_actor_label(),'location':a.get_actor_location().to_tuple(),'rotation':a.get_actor_rotation().to_tuple(),'scale':a.get_actor_scale3d().to_tuple(),'bounds':[x.to_tuple() for x in a.get_actor_bounds(False)],'material':a.static_mesh_component.get_material(0).get_path_name()})
 print(json.dumps(rows))
 print('DOF_SHOWFLAG',unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.DepthOfField'))
elif stage=='overview_start':
 assert not hasattr(unreal,'_lg8_final_showflag')
 unreal._lg8_final_showflag=unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.DepthOfField')
 unreal.SystemLibrary.execute_console_command(w,'ShowFlag.DepthOfField 0')
 print('Original DOF showflag',unreal._lg8_final_showflag)
elif stage=='overview_end':
 assert hasattr(unreal,'_lg8_final_showflag')
 unreal.SystemLibrary.execute_console_command(w,'ShowFlag.DepthOfField '+str(unreal._lg8_final_showflag))
 assert unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.DepthOfField')==unreal._lg8_final_showflag
 del unreal._lg8_final_showflag
 print('DOF showflag restored')
elif stage=='foundation':
 import shutil
 label='LG8_Watermill_RiverFoundation'
 assert not any(a.get_actor_label()==label for a in actors)
 source=ROOT/'Content/Libregrove/Level_V8/Maps/L_Libregrove.umap'
 backup=ROOT/'Saved/LibregroveV8Backup/BeforeMillFoundation_20260909.umap'
 assert not backup.exists();shutil.copy2(source,backup)
 footing=next(a for a in actors if a.get_actor_label()=='LG8_Watermill_Footing')
 loc=footing.get_actor_location();scale=footing.get_actor_scale3d()
 top=loc.z-10+2;bottom=-145.
 foundation=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(loc.x,loc.y,(top+bottom)/2),footing.get_actor_rotation())
 foundation.set_actor_label(label);foundation.set_folder_path('Libregrove/Architecture/Foundations')
 foundation.set_actor_scale3d(unreal.Vector(scale.x,scale.y,(top-bottom)/100))
 component=foundation.static_mesh_component
 assert component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
 component.set_material(0,footing.static_mesh_component.get_material(0))
 component.set_collision_profile_name('BlockAll')
 assert foundation.get_actor_bounds(False)[1].z>70
 next(a for a in actors if a.get_actor_label()=='LG8_PP_ArtGrade').set_folder_path('Libregrove/Environment')
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 row={'actor':label,'location':foundation.get_actor_location().to_tuple(),'scale':foundation.get_actor_scale3d().to_tuple(),'top':top,'bottom':bottom,'backup':str(backup),'original_model_unchanged':True}
 with (OUT/'mill_foundation.json').open('x',encoding='utf8') as f:json.dump(row,f,indent=2)
 print(json.dumps(row))
elif stage=='save_reload':
 level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
 assert level.save_current_level()
 dirty=[p for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() if p.get_name().startswith('/Game/Libregrove/Level_V8/')]
 for package in dirty:assert unreal.EditorAssetLibrary.save_asset(package.get_name(),False)
 print('V8 additional dirty packages saved',len(dirty))
 assert level.load_level('/Game/Libregrove/Level_V8/Maps/L_Libregrove')
 print('Reloaded saved V8 map')
elif stage=='final_view':
 data=json.loads((OUT/'capture_suite_002.json').read_text(encoding='utf8'))
 q=next(r for r in data['rows'] if r['name']=='Square')
 assert q['actual_game_view']
 roll,pitch,yaw=q['rotation']
 assert unreal.ViewportService.set_fov(q['fov'])
 assert unreal.ActorService.set_viewport_camera(unreal.Vector(*q['camera']),unreal.Rotator(pitch=pitch,yaw=yaw,roll=roll))
 sub.select_nothing()
 print('Editor view set to verified square gameplay-camera pose')
elif stage=='final_shot':
 target=OUT/'Preview/Editor_AfterReload_001.png'
 assert not target.exists()
 unreal._lg8_final_shot=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(target),delay=.5,force_game_view=True)
 assert unreal._lg8_final_shot
 print('Queued post-reload editor screenshot',str(target))
elif stage=='final_draw':
 print(unreal.ScreenshotService.capture_from_pose(unreal.Vector(),unreal.Rotator(),str(OUT/'Preview/Editor_AfterReload_bridge_color_unreliable.png'),use_current_camera=True))
 print('Native screenshot task done',unreal._lg8_final_shot.is_task_done())
