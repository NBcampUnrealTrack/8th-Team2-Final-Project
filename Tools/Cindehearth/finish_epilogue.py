import unreal
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
idx={a.get_actor_label():a for a in AS.get_all_level_actors()}
sign=idx['CH_Lettering_DorenSeal']
sign.set_actor_location(unreal.Vector(-23.8*45,-39.75*45,1.37*45),False,False)
sign.set_actor_scale3d(unreal.Vector(.64*.45,.30*.45,.45))
seal=idx['CH_DorenLetter_Seal']
seal.set_actor_rotation(unreal.Rotator(yaw=90),False)
seal.set_actor_location(unreal.Vector(-23.848*45,-39.73*45,1.10*45),False,False)
name='CH_EpilogueFill'
a=idx.get(name)
if not a:
    a=AS.spawn_actor_from_class(unreal.PointLight,unreal.Vector(-23.8*45,-41.0*45,2.2*45))
    a.set_actor_label(name);a.set_folder_path('Cindehearth/Cinematics')
    a.tags=[unreal.Name('Cindehearth'),unreal.Name('CH.EpilogueOnly')]
    a.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
a.light_component.set_intensity(3.5)
a.light_component.set_attenuation_radius(180)
a.light_component.set_light_color(unreal.LinearColor(.55,.65,.80))
a.light_component.set_editor_property('cast_shadows',False)
a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True)
material='/Game/Cindehearth/Materials/MI_CH_LetterPaper'
if not unreal.EditorAssetLibrary.does_asset_exist(material):
    assert unreal.MaterialService.create_instance('/Game/Cindehearth/Materials/M_CH_Solid','MI_CH_LetterPaper','/Game/Cindehearth/Materials').success
    unreal.MaterialService.set_instance_vector_parameter(material,'Color',.65,.55,.39)
    unreal.MaterialService.save_instance(material)
idx['CH_DorenLetter_Paper'].static_mesh_component.set_material(0,unreal.load_asset(material))
LS.save_current_level();print('EPILOGUE LEGIBILITY REFINED')
