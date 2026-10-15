import unreal,shutil,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
dest=root/'Saved/CindehearthRestyleBackup/20260905_v001'
assert not dest.exists()
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
dest.mkdir(parents=True)
for file in ['Content/Maps/L_Cindehearth.umap','Source/ProjectM/Field/NUCindehearthDirector.h','Source/ProjectM/Field/NUCindehearthDirector.cpp']:
    target=dest/file;target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/file,target)
print('RESTYLE BACKUP',str(dest))
