"""Read-only asset reuse inventory and unforced material graph inspection."""
import unreal,json
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Docs/Libregrove/Level_V8_20260908'
reg=unreal.AssetRegistryHelpers.get_asset_registry()
data=[]
for folder in ['/Game/Village','/Game/Cindehearth','/Game/Libregrove/Textured_V7']:
    for a in reg.get_assets_by_path(folder,recursive=True):
        typ=str(a.asset_class_path.asset_name)
        if typ not in ['StaticMesh','Material','MaterialInstanceConstant','Blueprint']:continue
        rec={'path':str(a.package_name),'type':typ}
        if typ=='StaticMesh':
            obj=a.get_asset();b=obj.get_bounds()
            rec.update(origin=list(b.origin.to_tuple()),extent=list(b.box_extent.to_tuple()),materials=[s.material_interface.get_path_name() if s.material_interface else None for s in obj.static_materials])
        data.append(rec)
with (OUT/'reuse_inventory.json').open('x',encoding='utf8') as f:json.dump(data,f,indent=2)
print(json.dumps({'count':len(data),'village':[d for d in data if d['path'].startswith('/Game/Village')],'cindehearth_meshes':[d['path'] for d in data if d['path'].startswith('/Game/Cindehearth') and d['type']=='StaticMesh']},indent=2))
print('DEST_EXISTS',unreal.EditorAssetLibrary.does_asset_exist('/Game/Libregrove/Level_V8/Maps/L_Libregrove'))
