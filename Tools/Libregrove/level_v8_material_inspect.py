import unreal,json
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Docs/Libregrove/Level_V8_20260908'
records=[]
for name in ['VillagePixelLandscape2','HD2D_Sky','HD2D_Water','PixelPropWA2']:
    path='/Game/Village/Materials/M_'+name
    graph=json.loads(unreal.MaterialNodeService.export_material_graph(path))
    with (OUT/('source_graph_'+name+'.json')).open('x',encoding='utf8') as f:json.dump(graph,f,indent=2)
    mat=unreal.load_asset(path)
    mi=unreal.load_asset('/Game/Village/Materials/MI_'+name) if name!='PixelPropWA2' else unreal.load_asset('/Game/Village/Materials/Props/MI_Prop_Stone')
    rec={'material':path,'instance':mi.get_path_name() if mi else None,'scalars':{},'vectors':{},'textures':{}}
    if mi:
        for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat):rec['scalars'][str(n)]=unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi,n)
        for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(mat):rec['vectors'][str(n)]=str(unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(mi,n))
        for n in unreal.MaterialEditingLibrary.get_texture_parameter_names(mat):
            t=unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mi,n);rec['textures'][str(n)]=t.get_path_name() if t else None
    records.append(rec)
with (OUT/'source_material_parameters.json').open('x',encoding='utf8') as f:json.dump(records,f,indent=2)
print(json.dumps(records,indent=2))
