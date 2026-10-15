import unreal,json
from pathlib import Path
p='/Game/Cindehearth/HD2D_v002/Materials/M_CH_PixelFire'
# The old Ember material group also contains a bright solid flame core.
for world in [unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()]:
    if not world:continue
    for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor):
        if a.get_actor_label()=='CH_LivingEmbers_Ember':
            a.modify();a.static_mesh_component.set_visibility(False,True);a.static_mesh_component.set_hidden_in_game(True,True);a.set_actor_hidden_in_game(True);a.set_actor_enable_collision(False)
        if a.get_actor_label().startswith('CH_HD2D_HearthFire'):
            # Lower the origin so the pointed lower edge stays inside the ember bed.
            a.modify();pos=a.get_actor_location();pos.z=249;a.set_actor_location(pos,False,False)
            a.static_mesh_component.set_cast_shadow(False)
unreal.ViewportService.set_game_view(True)
print('REMOVED SOLID EMBER FLAME CORE')
