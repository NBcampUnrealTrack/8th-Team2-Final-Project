"""Import only completed bounded Blender exports, on the editor tick."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003'
entries=[json.loads(p.read_text(encoding='utf-8')) for p in sorted((root/'verification').glob('*_export.json'))]
base='/Game/Cindehearth/Polish_v003/Models';holder={}
assert not unreal.WidgetService.is_pie_running()
def on_tick(dt):
 unreal.unregister_slate_post_tick_callback(holder['handle'])
 for row in entries:
  receipt=root/'verification'/('import_'+row['name']+'.json')
  if receipt.exists():
   assert json.loads(receipt.read_text())['status']=='finished';continue
  dest=base+'/'+row['name'];assert not unreal.EditorAssetLibrary.list_assets(dest,True,False)
  receipt.write_text(json.dumps(dict(status='started',source=row['source'])),encoding='utf-8')
  try:
   t=unreal.AssetImportTask();t.filename=row['source'];t.destination_path=dest;t.automated=True;t.replace_existing=False;t.save=True
   unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);paths=list(t.imported_object_paths);assert paths
   meshes=[]
   for path in paths:
    asset=unreal.load_asset(path)
    if isinstance(asset,unreal.StaticMesh):
     sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sub.get_nanite_settings(asset);n.enabled=False;sub.set_nanite_settings(asset,n,True)
     asset.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
     unreal.EditorAssetLibrary.save_loaded_asset(asset);b=asset.get_bounding_box()
     meshes.append(dict(path=path,triangles=asset.get_num_triangles(0),min=list(b.min.to_tuple()),max=list(b.max.to_tuple())))
   assert len(meshes)==1
   receipt.write_text(json.dumps(dict(status='finished',source=row['source'],assets=paths,meshes=meshes),indent=2),encoding='utf-8')
   unreal.log('POLISH IMPORT '+row['name'])
  except Exception as e:
   receipt.write_text(json.dumps(dict(status='failed',error=str(e))),encoding='utf-8');unreal.log_error(str(e));break
holder['handle']=unreal.register_slate_post_tick_callback(on_tick)
print('QUEUED',len(entries),'exports')
