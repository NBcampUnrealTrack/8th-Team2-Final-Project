import unreal
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
d=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NUCindehearthDirector)[0]
pc=unreal.GameplayStatics.get_player_controller(w,0)
print('CINEMATIC',d.epilogue_played,pc.get_view_target(),pc.is_move_input_ignored(),pc.is_look_input_ignored(),pc.player_camera_manager.get_camera_location())
