import unreal,json
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
p=unreal.GameplayStatics.get_player_pawn(w,0);assert p
s=next(c for c in p.get_components_by_class(unreal.PaperFlipbookComponent) if c.get_name()=='Sprite')
m=s.create_dynamic_material_instance(0,unreal.load_asset('/Game/Fernbrook/CharacterLighting/Materials/MI_FB_Player_Lit'))
v=float(STAGE);m.set_vector_parameter_value('Tint',unreal.LinearColor(v,v,v,1))
print(json.dumps({'ambient':0.45,'tint':v,'material':s.get_material(0).get_path_name()}))
