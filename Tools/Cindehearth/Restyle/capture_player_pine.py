import unreal,json,time
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0);assert pawn
if STAGE=='position':
    pawn.set_actor_location(unreal.Vector(540,-6770,53),False,True)
    print('PINE PLAYER CAMERA POSITIONED')
else:
    manager=pc.player_camera_manager
    out=(root/'pine_player.png').resolve().as_posix()
    unreal.SystemLibrary.execute_console_command(w,'HighResShot 1440x900 filename='+out,pc)
    (root/'pine_player_camera.json').write_text(json.dumps(dict(position=list(manager.get_camera_location().to_tuple()),rotation=list(manager.get_camera_rotation().to_tuple()),fov=manager.get_fov_angle(),capture=out,actual_game_view=True),indent=2),encoding='utf-8')
    print('HIGH RES GAME CAPTURE REQUESTED',out)
