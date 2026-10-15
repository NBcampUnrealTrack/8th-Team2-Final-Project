import unreal
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('Saved Cindehearth; requesting orderly editor exit for the compiled director.')
unreal.SystemLibrary.quit_editor()
