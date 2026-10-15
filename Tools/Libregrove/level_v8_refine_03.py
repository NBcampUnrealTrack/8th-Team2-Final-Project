"""Tone down the river bed visibility identified in actual bridge/waterfront views."""
import unreal,json
from pathlib import Path
OUT=Path(unreal.Paths.project_dir()).resolve()/'Docs/Libregrove/Level_V8_20260908';assert not unreal.WidgetService.is_pie_running()
rows=json.loads((OUT/'HD2D/instance_mapping.json').read_text(encoding='utf8'))
r=next(r for r in rows if 'MI_LG8_River.' in r['source']);m=unreal.load_asset(r['instance']);E=unreal.MaterialEditingLibrary
before={};params={'OpacityBase':.84,'HighlightStrength':.36,'ShallowDistance':120.}
for k,v in params.items():before[k]=E.get_material_instance_scalar_parameter_value(m,k);E.set_material_instance_scalar_parameter_value(m,k,v);assert abs(E.get_material_instance_scalar_parameter_value(m,k)-v)<.0001
colors={'DeepColor':[.008,.085,.13,1.],'ShallowColor':[.035,.245,.29,1.]};old={}
for k,v in colors.items():
 c=E.get_material_instance_vector_parameter_value(m,k);old[k]=[c.r,c.g,c.b,c.a];E.set_material_instance_vector_parameter_value(m,k,unreal.LinearColor(*v))
assert unreal.EditorAssetLibrary.save_loaded_asset(m)
with (OUT/'refinement_03.json').open('x',encoding='utf8') as f:json.dump({'material':m.get_path_name(),'old_scalars':before,'new_scalars':params,'old_colors':old,'new_colors':colors},f,indent=2)
print('River opacity/depth color refined; other water instances unchanged')
