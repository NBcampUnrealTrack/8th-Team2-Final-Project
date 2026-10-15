"""Import the verified repair as new assets, outside the MCP task stack."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())
journal=root/'Tools/Cindehearth/Pipeline/verification/repair_unreal_import.json'
source=Path.home()/'AppData/Local/Revvy/BlenderExchange/Cindehearth/Export/CH_GormP2_Clean_v006.glb'
dest='/Game/Cindehearth/Tripo/GormHouse_P2_Repair_v006'
assert source.is_file()
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
assert not journal.exists()
assert not unreal.EditorAssetLibrary.list_assets(dest,True,False)
holder={}
def import_on_tick(_dt):
    unreal.unregister_slate_post_tick_callback(holder['handle'])
    try:
        task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=dest
        task.automated=True;task.replace_existing=False;task.replace_existing_settings=False;task.save=True;task.async_=False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        paths=list(task.imported_object_paths);assert paths
        journal.write_text(json.dumps(dict(status='finished',assets=paths,source=str(source),destination=dest),indent=2),encoding='utf-8')
        unreal.log('Cindehearth repaired model imported')
    except Exception as error:
        journal.write_text(json.dumps(dict(status='failed',error=str(error)),indent=2),encoding='utf-8')
        unreal.log_error(str(error))
journal.write_text(json.dumps(dict(status='queued',source=str(source),destination=dest)),encoding='utf-8')
holder['handle']=unreal.register_slate_post_tick_callback(import_on_tick)
print('Repair import queued')
