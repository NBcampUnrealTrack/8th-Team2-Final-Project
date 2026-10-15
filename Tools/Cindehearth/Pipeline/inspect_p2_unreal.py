import unreal
import json
from pathlib import Path
base='/Game/Cindehearth/Tripo/GormHouse_P2_v001'
report=[]
for path in unreal.EditorAssetLibrary.list_assets(base,True,False):
    asset=unreal.load_asset(path)
    row=dict(path=path,type=asset.get_class().get_name())
    if isinstance(asset,unreal.StaticMesh):
        b=asset.get_bounds()
        row.update(center=list(b.origin.to_tuple()),extent=list(b.box_extent.to_tuple()),materials=[m.material_interface.get_path_name() if m.material_interface else None for m in asset.static_materials],collision=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_simple_collision_count(asset))
    if isinstance(asset,unreal.Texture2D):
        row.update(srgb=asset.get_editor_property('srgb'),compression=str(asset.get_editor_property('compression_settings')),filter=str(asset.get_editor_property('filter')),flip_green=asset.get_editor_property('flip_green_channel'))
    report.append(row)
print(json.dumps(report))
(Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Pipeline/verification/p2_unreal_assets.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
