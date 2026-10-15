import unreal
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
stage=globals().get('STAGE','day')
d=next(a for a in AS.get_all_level_actors() if isinstance(a,unreal.NUCindehearthDirector))
d.set_chapter_progress(int(stage[5:]) if stage.startswith('night') else 0)
d.set_night(stage.startswith('night'))
for a in AS.get_all_level_actors():
    if a.actor_has_tag('CH.Resident'):a.set_actor_rotation(unreal.Rotator(yaw=0),False)
if stage=='seal':
    d.set_night(True)
    for a in AS.get_all_level_actors():
        if a.actor_has_tag('CH.EpilogueOnly'):
            a.set_actor_hidden_in_game(False);a.set_is_temporarily_hidden_in_editor(False)
    pose=d.get_editor_property('epilogue_camera_route')[-1]
    pos=pose.translation;rot=pose.rotation.rotator()
    unreal.ViewportService.set_camera_location(pos);unreal.ViewportService.set_camera_rotation(rot)
    unreal.ViewportService.set_game_view(True);AS.select_nothing()
    print(unreal.ScreenshotService.capture_from_pose(pos,rot,'CH_seal',38,False,False))
if stage=='day':LS.save_current_level()
print('PREVIEW',stage,d.get_square_mood())
