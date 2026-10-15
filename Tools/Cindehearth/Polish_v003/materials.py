import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';S=unreal.MaterialService;L=unreal.EditorAssetLibrary
dest='/Game/Cindehearth/Polish_v003/Materials';rows=[]
for p in sorted((root/'verification').glob('import_*.json')):
 d=json.loads(p.read_text());assert d['status']=='finished';name=p.stem[7:];mi=dest+'/MI_CH_'+name
 if L.does_asset_exist(mi):continue
 color=next(x for x in d['assets'] if x.endswith('/Color.Color'));normal=next(x for x in d['assets'] if x.endswith('/Normal.Normal'))
 tex=unreal.load_asset(color);tex.set_editor_property('filter',unreal.TextureFilter.TF_NEAREST);L.save_loaded_asset(tex)
 assert S.create_instance('/Game/Cindehearth/Tripo/GormHouse_P2_v001/MI_CH_GormP2_Pixel','MI_CH_'+name,dest).success
 assert S.set_instance_texture_parameter(mi,'Albedo',color);assert S.set_instance_texture_parameter(mi,'NormalMap',normal)
 for key,value in [('PixelGrid',128 if name in ('Crate','Barrel','AshNaia','ToolRack') else 256),('NormalStrength',.025),('Roughness',.97),('AmbientLift',.03)]:assert S.set_instance_scalar_parameter(mi,key,value)
 assert S.set_instance_vector_parameter(mi,'Tint',1.08,1.08,1.06);assert S.save_instance(mi)
 rows.append(name)
print('PIXEL MATERIALS',rows)
