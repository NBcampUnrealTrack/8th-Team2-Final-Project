import unreal
import json
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
print('WORLD', world.get_path_name(), 'ACTORS', len(actors))
print('PIE', unreal.WidgetService.is_pie_running())
print('TARGET_EXISTS', unreal.EditorAssetLibrary.does_asset_exist('/Game/Maps/L_Cindehearth'))
for actor in actors:
    if isinstance(actor, (unreal.DirectionalLight, unreal.SkyLight, unreal.PostProcessVolume, unreal.PlayerStart)) or 'SkyDome' in actor.get_actor_label():
        print(actor.get_actor_label(), actor.get_class().get_name(), str(actor.get_actor_location()))
        if isinstance(actor, unreal.PostProcessVolume):
            print('POSTPROCESS', actor.get_editor_property('settings'))
print('GAMEMODE', world.get_world_settings().get_editor_property('default_game_mode'))
for name in ['SM_Rock_A', 'SM_Cliff_Block', 'SM_Crate', 'SM_Barrel', 'SM_Steps_Stone']:
    mesh = unreal.load_asset('/Game/Village/Meshes/' + name)
    print(name, str(mesh.get_bounds()), [(str(s.material_slot_name), s.material_interface.get_path_name() if s.material_interface else '') for s in mesh.static_materials])
