import unreal
import json
p='/Game/Cindehearth/Materials/M_CH_ChimneySmoke'
print(unreal.MaterialNodeService.export_material_graph(p))
