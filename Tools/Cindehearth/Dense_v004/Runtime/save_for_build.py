import json
from pathlib import Path
import unreal

assert not unreal.WidgetService.is_pie_running()
out = Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004/Runtime'
utility = unreal.EditorLoadingAndSavingUtils
dirty = list(utility.get_dirty_map_packages()) + list(utility.get_dirty_content_packages())
names = [p.get_name() for p in dirty]
unexpected = [n for n in names if not (n.startswith('/Game/Cindehearth/') or n == '/Game/Maps/L_Cindehearth')]
report = dict(status='inspected', dirty_packages=names, unexpected_packages=unexpected)
(out/'prebuild_dirty.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
assert not unexpected, 'Unrelated dirty packages: ' + repr(unexpected)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith('/Game/Maps/L_Cindehearth.')
if dirty:
    assert utility.save_packages(dirty, True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
report['status'] = 'saved_requesting_orderly_exit'
(out/'prebuild_dirty.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('SAVED FOR BUILD', names)
unreal.SystemLibrary.quit_editor()
