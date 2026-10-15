import unreal
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
p=unreal.GameplayStatics.get_player_pawn(w,0).get_actor_location()
print('PAWN',p)
for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.StaticMeshActor):
    c,e=a.get_actor_bounds(False)
    if a.get_actor_enable_collision() and abs(p.x-c.x)<e.x+120 and abs(p.y-c.y)<e.y+120 and abs(p.z-c.z)<e.z+120:
        print(a.get_actor_label(),c,e,a.static_mesh_component.get_collision_profile_name())
