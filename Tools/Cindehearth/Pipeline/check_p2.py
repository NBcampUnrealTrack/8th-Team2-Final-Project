import unreal
b='/Game/Cindehearth/Tripo/GormHouse_P2_v001'
m=unreal.load_asset(b+'/CH_GormP2_Game_v001/StaticMeshes/CH_GormP2_Game_v001')
print('TRIANGLES',m.get_num_triangles(0))
print('NANITE',m.get_editor_property('nanite_settings'))
d=unreal.MaterialNodeService.get_material_diagnostics(b+'/M_CH_P2_Pixel')
print('DIAG',d)
print('P2_ACTORS',[(a.get_actor_label(),str(a.get_actor_location())) for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.actor_has_tag('CH.P2SmartMesh')])
