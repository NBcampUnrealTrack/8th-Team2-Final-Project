import unreal,json
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
p=unreal.GameplayStatics.get_player_pawn(w,0);assert p
s=next(c for c in p.get_components_by_class(unreal.PaperFlipbookComponent) if c.get_name()=='Sprite')
if not hasattr(unreal,'_fb_lighting_mid'):
 unreal._fb_lighting_original=s.get_material(0)
 unreal._fb_lighting_mid=s.create_dynamic_material_instance(0,unreal._fb_lighting_original)
unreal._fb_lighting_mid.set_scalar_parameter_value('AmbientLift',float(STAGE))
print(json.dumps({'ambient':float(STAGE),'material':s.get_material(0).get_path_name()}))
