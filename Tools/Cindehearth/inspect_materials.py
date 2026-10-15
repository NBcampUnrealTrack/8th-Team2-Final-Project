import unreal
for name in ['Stone','Roof','Metal','Timber','Soil','Glow','Rock','Cliff','Cloth']:
    p='/Game/Village/Materials/Props/MI_Prop_'+name
    print(name, unreal.MaterialService.get_instance_info(p))
    print(unreal.MaterialService.list_instance_parameters(p))
print('SKY',unreal.MaterialService.list_instance_parameters('/Game/Village/Materials/MI_HD2D_Sky'))
print('BP', unreal.EditorAssetLibrary.list_assets('/Game/Art/Sprites', True, False)[:12])
