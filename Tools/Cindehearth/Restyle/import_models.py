import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle'
entries=json.loads((root/'import_models.json').read_text(encoding='utf-8'))
base='/Game/Cindehearth/HD2D_v002/Models'
holder={}
def on_tick(dt):
    unreal.unregister_slate_post_tick_callback(holder['handle'])
    for row in entries:
        receipt=root/'verification'/('import_'+row['name']+'.json')
        if receipt.exists():
            assert json.loads(receipt.read_text(encoding='utf-8'))['status']=='finished'
            continue
        dest=base+'/'+row['name']
        assert not unreal.EditorAssetLibrary.list_assets(dest,True,False)
        receipt.write_text(json.dumps(dict(status='started',source=row['source'])),encoding='utf-8')
        try:
            t=unreal.AssetImportTask();t.filename=row['source'];t.destination_path=dest
            t.automated=True;t.replace_existing=False;t.save=True
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
            paths=list(t.imported_object_paths);assert paths
            meshes=[]
            for path in paths:
                asset=unreal.load_asset(path)
                if isinstance(asset,unreal.StaticMesh):
                    sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
                    nanite=sub.get_nanite_settings(asset);nanite.enabled=False;sub.set_nanite_settings(asset,nanite,True)
                    body=asset.get_editor_property('body_setup')
                    body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                    unreal.EditorAssetLibrary.save_loaded_asset(asset)
                    meshes.append(dict(path=path,triangles=asset.get_num_triangles(0)))
            assert len(meshes)==1
            receipt.write_text(json.dumps(dict(status='finished',source=row['source'],assets=paths,meshes=meshes),indent=2),encoding='utf-8')
            unreal.log('RESTYLE IMPORT '+row['name'])
        except Exception as e:
            receipt.write_text(json.dumps(dict(status='failed',error=str(e)),indent=2),encoding='utf-8')
            unreal.log_error(str(e));break
holder['handle']=unreal.register_slate_post_tick_callback(on_tick)
print('QUEUED',len(entries),'model imports')
