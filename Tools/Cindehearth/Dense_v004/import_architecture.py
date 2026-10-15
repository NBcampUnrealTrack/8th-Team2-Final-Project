"""Import preserved Blender P2 exports, keeping body/roof/repair origins intact."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';out=root/'verification';holder={}
keys=['Tavern','TavernWalls','Smithy','SmithyWalls','SmithyStructure','SmithyRoof']
if globals().get('STAGE')=='bodies':keys=keys[:-1]
if globals().get('STAGE')=='walls_inset':keys=['TavernWallsInset','TavernWallsFrame']
entries=[json.loads((root/'Architecture/verification'/f'{k}_export.json').read_text(encoding='utf-8')) for k in keys]
assert all(Path(r['source']).is_file() for r in entries)
assert not unreal.WidgetService.is_pie_running()
def tick(dt):
 unreal.unregister_slate_post_tick_callback(holder['h'])
 for r in entries:
  dest='/Game/Cindehearth/Dense_v004/Models/'+r['name'];receipt=out/('import_'+r['name']+'.json')
  if receipt.exists():assert json.loads(receipt.read_text())['status']=='finished';continue
  assert not unreal.EditorAssetLibrary.list_assets(dest,True,False)
  receipt.write_text(json.dumps(dict(status='started',source=r['source'])))
  try:
   task=unreal.AssetImportTask();task.filename=r['source'];task.destination_path=dest;task.automated=True;task.replace_existing=False;task.save=True
   unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);assets=list(task.imported_object_paths);meshes=[]
   for path in assets:
    a=unreal.load_asset(path)
    if isinstance(a,unreal.StaticMesh):
     sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sub.get_nanite_settings(a);n.enabled=False;sub.set_nanite_settings(a,n,True)
     a.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
     unreal.EditorAssetLibrary.save_loaded_asset(a);b=a.get_bounding_box();meshes.append(dict(path=path,triangles=a.get_num_triangles(0),min=list(b.min.to_tuple()),max=list(b.max.to_tuple())))
   assert len(meshes)==1
   receipt.write_text(json.dumps(dict(status='finished',source=r['source'],assets=assets,meshes=meshes),indent=2))
   unreal.log('DENSE IMPORT '+r['name'])
  except Exception as e:
   receipt.write_text(json.dumps(dict(status='failed',error=str(e))));raise
holder['h']=unreal.register_slate_post_tick_callback(tick);print('QUEUED',keys)
