import unreal,json
A=unreal.EditorAssetLibrary;M=unreal.MaterialService
folder='/Game/Fernbrook/CharacterLighting/Materials';name='MI_FB_Player_Lit'
assert not A.does_asset_exist(folder+'/'+name)
r=M.create_instance('/Game/Village/Materials/MI_PixelSprite_Lit',name,folder);assert r.success,r.error_message
assert M.set_instance_scalar_parameter(r.asset_path,'AmbientLift',0.45)
m=A.load_asset(r.asset_path)
values={str(p.parameter_info.name):p.parameter_value for p in m.get_editor_property('scalar_parameter_values')}
assert abs(values['AmbientLift']-0.45)<0.0001
assert A.save_asset(r.asset_path)
print(json.dumps({'path':r.asset_path,'parent':m.get_editor_property('parent').get_path_name(),'scalars':values}))
