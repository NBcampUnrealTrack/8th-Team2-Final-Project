"""Independent readback and final review of the repaired asset."""
import unreal, json
from pathlib import Path

assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Cindehearth'
root = Path(unreal.Paths.project_dir())
base = '/Game/Cindehearth/Tripo/GormHouse_P2_Repair_v006'
mesh = unreal.load_asset(base + '/CH_GormP2_Clean_v006/StaticMeshes/CH_GormP2_Clean_v006')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == 'CH_GormHouse_P2')
component = actor.static_mesh_component
assert component.static_mesh == mesh
assert mesh.get_num_triangles(0) == 1994
assert not unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_nanite_settings(mesh).enabled
assert component.get_num_materials() == 2
assert mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag') == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
assert str(component.get_collision_profile_name()) == 'BlockAll'
assert component.get_material(1).get_path_name().startswith(base + '/MI_CH_RepairedRoof_Pixel')
old_shell = [a for a in actors.get_all_level_actors() if a.get_actor_label() in ['CH_GormHouse_Stone','CH_GormHouse_Roof','CH_GormHouse_Window']]
assert len(old_shell) == 3 and all(not a.static_mesh_component.get_editor_property('visible') for a in old_shell)
director = next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.NUCindehearthDirector))
if STAGE == 'night':
    director.set_night(True)
    pos = unreal.Vector(-720,-2350,660)
    rot = unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(-1125,-1620,120))
    print(unreal.ScreenshotService.capture_from_pose(pos,rot,'CH_Gorm_repaired_v006_night',55,False,False))
elif STAGE == 'finish':
    director.set_chapter_progress(0)
    director.set_night(False)
    pos = unreal.Vector(-1125,-3365.4021031123584,318.9096501049541)
    rot = unreal.Rotator(pitch=-8,yaw=90)
    unreal.ViewportService.set_camera_location(pos)
    unreal.ViewportService.set_camera_rotation(rot)
    unreal.ViewportService.set_game_view(True)
    actors.select_nothing()
    print(unreal.ScreenshotService.capture_from_pose(pos,rot,'CH_Gorm_repaired_v006_player_camera',30,False,False))
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    data=dict(passed=True,mesh=mesh.get_path_name(),triangles=1994,nanite=False,
              location=list(actor.get_actor_location().to_tuple()),rotation=list(actor.get_actor_rotation().to_tuple()),
              original_shell_hidden=True,material_slots=2,collision='BlockAll / ComplexAsSimple',
              saved_map='/Game/Maps/L_Cindehearth',saved_preview='day, chapter 0',pie_running=False)
    (root/'Tools/Cindehearth/Pipeline/verification/repair_unreal_readback.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    print(json.dumps(data))
    print('Repaired mesh editor opened:',unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([mesh]))
