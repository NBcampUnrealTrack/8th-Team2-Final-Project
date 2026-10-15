"""One fresh asynchronous Interchange import; no existing content is replaced."""
import unreal,json,hashlib
from pathlib import Path

root=Path(unreal.Paths.project_dir()).resolve()
dest=root/'Docs/Libregrove/Unreal_Textured_V7_20260908'
ident=ASSET_ID
row=json.loads((dest/'exports'/(ident+'.json')).read_text(encoding='utf-8'))
source=Path(row['glb_path']);assert hashlib.sha256(source.read_bytes()).hexdigest()==row['sha256']
assert row['ue_destination']=='/Game/Libregrove/Textured_V7/Models/'+ident
registry=unreal.AssetRegistryHelpers.get_asset_registry()
assert not registry.get_assets_by_path(row['ue_destination'],True),'Existing destination; inspect instead of overwrite'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(),'PIE must be stopped'
manager=unreal.InterchangeManager.get_interchange_manager_scripted()
assert not manager.is_interchange_active(),'Finish the existing import first'
journal=dest/'unreal_jobs'/ident;journal.mkdir(parents=True,exist_ok=True)
assert not (journal/'request.json').exists(),'Never blindly repeat an import'
pipeline=unreal.InterchangeGenericAssetsPipeline(name='LGV7_'+ident+'_Pipeline')
pipeline.set_editor_property('asset_type_sub_folders',True)
pipeline.set_editor_property('scene_name_sub_folder',False)
pipeline.set_editor_property('use_source_name_for_asset',True)
mesh=pipeline.get_editor_property('mesh_pipeline')
mesh.set_editor_property('import_static_meshes',True)
mesh.set_editor_property('import_skeletal_meshes',False)
mesh.set_editor_property('import_geometry_caches',False)
mesh.set_editor_property('build_nanite',False)
mesh.set_editor_property('generate_lightmap_u_vs',False)
mesh.set_editor_property('collision',False)
common=pipeline.get_editor_property('common_meshes_properties')
common.set_editor_property('recompute_normals',False)
common.set_editor_property('recompute_tangents',True)
common.set_editor_property('use_mikk_t_space',True)
common.set_editor_property('remove_degenerates',True)
common.set_editor_property('use_full_precision_u_vs',True)
material=pipeline.get_editor_property('material_pipeline')
material.set_editor_property('import_materials',True)
material.set_editor_property('create_new_materials',True)
material.set_editor_property('reuse_existing_materials',False)
params=unreal.ImportAssetParameters();params.is_automated=True;params.replace_existing=False
params.override_pipelines=[unreal.SoftObjectPath(pipeline.get_path_name())]
source_data=unreal.InterchangeManager.create_source_data(str(source))
state={'pipeline':pipeline,'params':params,'source':source_data,'status':'prepared'}
if not hasattr(unreal,'_libregrove_v7_imports'):unreal._libregrove_v7_imports={}
assert ident not in unreal._libregrove_v7_imports
unreal._libregrove_v7_imports[ident]=state

def done(objects):
    paths=[{'path':o.get_path_name(),'class':o.get_class().get_name()} for o in objects if o]
    payload={'status':'finished' if paths else 'failed','id':ident,'destination':row['ue_destination'],'objects':paths}
    state['status']=payload['status']
    with (journal/'result.json').open('x',encoding='utf-8') as f:json.dump(payload,f,indent=2)
    unreal.log('LIBREGROVE IMPORT '+ident+' '+payload['status'])

callback=unreal.OnImportDoneDynamic();callback.bind_callable(done)
params.on_assets_import_done=callback;state['callback']=callback;state['done']=done
with (journal/'request.json').open('x',encoding='utf-8') as f:json.dump({'source':str(source),'sha256':row['sha256'],'destination':row['ue_destination'],'overwrite':False},f,indent=2)
started=manager.scripted_import_asset_async(row['ue_destination'],source_data,params)
state['started']=bool(started)
print(json.dumps({'id':ident,'started':bool(started),'journal':str(journal)}))
assert started,'Import not accepted; inspect journal and current state'
