import unreal,json
from pathlib import Path
p='/Game/Cindehearth/HD2D_v002/Materials/M_CH_PixelFire';N=unreal.MaterialNodeService
graph=json.loads(N.export_material_graph(p))
(Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification/fire_graph.json').write_text(json.dumps(graph,indent=2),encoding='utf-8')
node=next(n for n in graph['expressions'] if n.get('custom_input_names')=='UV,Time,Phase')
code=node['hlsl_code'].replace('float h=1.0-q.y;','float h=q.y;')
assert N.set_expression_property(p,node['id'],'Code',code)
assert unreal.MaterialService.compile_material(p)
assert N.get_material_diagnostics(p).is_compiled_ok
assert unreal.EditorAssetLibrary.save_asset(p)
print('FLAME UV HEIGHT CORRECTED; COMPILED')
