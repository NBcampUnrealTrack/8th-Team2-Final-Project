import unreal
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
actors={a.get_actor_label():a for a in AS.get_all_level_actors()}
# The authored sky remains visible; a specified ambient capture gives repeatable
# shadow fill after editor restart and when real-time capture is unavailable.
sky=actors['CH_SkyLight'].light_component
sky.set_editor_property('real_time_capture',False)
sky.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
cube=unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap')
assert cube
sky.set_cubemap(cube)
sky.set_editor_property('lower_hemisphere_is_black',False)
actors['CH_LevelDirector'].set_night(False)
actors['CH_LevelDirector'].set_chapter_progress(0)
LS.save_current_level()
print('AMBIENT CUBEMAP',cube.get_path_name(),'INTENSITY',sky.get_editor_property('intensity'))
