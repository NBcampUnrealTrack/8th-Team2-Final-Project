import unreal,json
from pathlib import Path
P='/Game/Village/Materials/M_PixelSprite_Lit'
graph=json.loads(unreal.MaterialNodeService.export_material_graph(P))
out=Path(unreal.Paths.project_dir()).resolve()/'Docs/Fernbrook_CharacterLighting_20260911'
with (out/'source_material_graph.json').open('x',encoding='utf8') as f:json.dump(graph,f,indent=2)
print(json.dumps(graph))
