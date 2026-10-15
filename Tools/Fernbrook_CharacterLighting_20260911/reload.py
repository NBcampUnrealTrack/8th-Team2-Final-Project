import unreal,json
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Fernbrook/Entrance_V1/Maps/L_Fernbrook_Entrance')
print('Saved Fernbrook map reloaded')
