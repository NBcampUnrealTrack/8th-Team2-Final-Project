"""Create ONLY a new Libregrove map; never overwrite an existing world."""
import unreal,json
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Docs/Libregrove/Level_V8_20260908'
LEVEL='/Game/Libregrove/Level_V8/Maps/L_Libregrove'
E=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.EditorAssetLibrary.does_asset_exist(LEVEL),'Refuse existing map overwrite'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith('/Game/Libregrove/Textured_V7/Maps/L_Libregrove_AssetReview_V7.')
assert L.new_level_from_template(LEVEL,'/Engine/Maps/Templates/Template_Default')
baseline=[{'name':a.get_name(),'label':a.get_actor_label(),'class':a.get_class().get_name()} for a in E.get_all_level_actors()]
with (OUT/'new_map_template.json').open('x',encoding='utf8') as f:json.dump(baseline,f,indent=2)
print(json.dumps(baseline))
