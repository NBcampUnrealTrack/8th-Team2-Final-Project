import unreal,json
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
bridge=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.StaticMeshActor) if a.get_actor_label()=='FB5_WoodBridge')
p=unreal.GameplayStatics.get_player_pawn(w,0);assert p
p.character_movement.stop_movement_immediately()
loc=bridge.get_actor_location()+bridge.get_actor_forward_vector()*260+unreal.Vector(0,0,53)
p.set_actor_location(loc,False,True)
print(json.dumps({'pawn_target':loc.to_tuple()}))
