import unreal,json
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
p=unreal.GameplayStatics.get_player_pawn(w,0);assert p
assert not hasattr(unreal,'_fb_char_lighting_qa') or unreal._fb_char_lighting_qa.done
p.character_movement.stop_movement_immediately()
p.set_actor_location(unreal.Vector(1350,-2380,120),False,True)
print('Runtime player returned to entry for QA')
