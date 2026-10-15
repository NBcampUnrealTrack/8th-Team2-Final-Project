import unreal
m=unreal.load_asset('/Game/Cindehearth/Tripo/GormHouse_P2_v001/CH_GormP2_Game_v001/StaticMeshes/CH_GormP2_Game_v001')
s=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
settings=s.get_nanite_settings(m)
assert settings.enabled
settings.enabled=False
s.set_nanite_settings(m,settings,True)
assert not s.get_nanite_settings(m).enabled
print('RENDER TRIANGLES',m.get_num_triangles(0))
assert m.get_num_triangles(0)==3363
unreal.EditorAssetLibrary.save_loaded_asset(m)
