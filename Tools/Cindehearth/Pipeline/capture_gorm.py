import unreal
import json
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
stage=STAGE
d=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.NUCindehearthDirector))
d.set_night('night' in stage)
pos=unreal.Vector(-720,-2350,660)
target=unreal.Vector(-1125,-1620,120)
rot=unreal.MathLibrary.find_look_at_rotation(pos,target)
unreal.ViewportService.set_camera_location(pos)
unreal.ViewportService.set_camera_rotation(rot)
unreal.ViewportService.set_game_view(True)
print(unreal.ScreenshotService.capture_from_pose(pos,rot,'CH_Gorm_'+stage,55,False,False))
