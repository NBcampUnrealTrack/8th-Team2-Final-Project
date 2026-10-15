import unreal,json
from pathlib import Path
assert not unreal.WidgetService.is_pie_running()
U=unreal.EditorLoadingAndSavingUtils
dirty=list(U.get_dirty_map_packages())+list(U.get_dirty_content_packages())
names=[p.get_name() for p in dirty]
(Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification/prebuild_dirty.json').write_text(json.dumps(names,indent=2),encoding='utf-8')
assert all(n.startswith('/Game/Cindehearth/') or n=='/Game/Maps/L_Cindehearth' for n in names), names
if dirty:assert U.save_packages(dirty,True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('SAVED',names)
unreal.SystemLibrary.quit_editor()
