"""Import the verified Blender export on a Slate tick, outside the MCP task stack."""
import unreal
import json
from pathlib import Path

def queue_p2_import():
    root=Path(unreal.Paths.project_dir())
    evidence=root/'Tools/Cindehearth/Pipeline/verification'
    ready=json.loads((evidence/'p2_ready_for_unreal.json').read_text(encoding='utf-8'))
    source=Path(ready['export_path'])
    dest='/Game/Cindehearth/Tripo/GormHouse_P2_v001'
    journal=evidence/'p2_unreal_import.json'
    assert source.is_file() and source.stat().st_size>0
    assert not unreal.WidgetService.is_pie_running()
    assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
    assert not unreal.EditorAssetLibrary.list_assets(dest,True,False), 'Inspect existing assets instead of importing twice'
    assert not journal.exists(), 'Inspect the recorded import before retrying'
    holder={}
    def run(_dt):
        unreal.unregister_slate_post_tick_callback(holder['handle'])
        try:
            task=unreal.AssetImportTask()
            task.filename=str(source)
            task.destination_path=dest
            task.automated=True
            task.replace_existing=False
            task.replace_existing_settings=False
            task.save=True
            task.async_=False
            if source.suffix.lower()=='.fbx':
                opt=unreal.FbxImportUI()
                opt.automated_import_should_detect_type=False
                opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
                opt.import_as_skeletal=False
                opt.import_animations=False
                opt.import_materials=True
                opt.import_textures=True
                opt.static_mesh_import_data.combine_meshes=True
                opt.static_mesh_import_data.generate_lightmap_u_vs=True
                opt.static_mesh_import_data.auto_generate_collision=True
                opt.static_mesh_import_data.convert_scene=True
                opt.static_mesh_import_data.convert_scene_unit=True
                opt.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
                task.options=opt
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            paths=list(task.imported_object_paths)
            assert paths,'No assets returned'
            journal.write_text(json.dumps(dict(status='finished',source=str(source),destination=dest,assets=paths),indent=2),encoding='utf-8')
            print('P2 IMPORT FINISHED',paths)
        except Exception as error:
            journal.write_text(json.dumps(dict(status='failed',error=str(error)),indent=2),encoding='utf-8')
            unreal.log_error(str(error))
    journal.write_text(json.dumps(dict(status='queued',source=str(source),destination=dest)),encoding='utf-8')
    holder['handle']=unreal.register_slate_post_tick_callback(run)
    print('P2 import queued; inspect journal and Content Browser for completion.')

queue_p2_import()
