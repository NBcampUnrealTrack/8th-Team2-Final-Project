import unreal,json
P='/Game/Fernbrook/CharacterLighting/Materials/MI_FB_Player_Lit'
m=unreal.EditorAssetLibrary.load_asset(P);assert m
result=unreal.MaterialService.set_instance_vector_parameter(P,'Tint',1.3,1.3,1.3,1)
v=unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(m,'Tint')
assert max(abs(x-y) for x,y in zip([v.r,v.g,v.b,v.a],[1.3,1.3,1.3,1]))<0.0001
assert unreal.EditorAssetLibrary.save_asset(P)
print(json.dumps({'tint':[v.r,v.g,v.b,v.a],'ambient':0.45,'setter_return':result}))
