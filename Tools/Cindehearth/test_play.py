import unreal
import json
STAGE=globals().get('STAGE','inspect')
if STAGE=='start':print('STARTED',unreal.WidgetService.start_pie())
elif STAGE=='stop':print('STOPPED',unreal.WidgetService.stop_pie())
else:
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert w,'No PIE world'
    pawn=unreal.GameplayStatics.get_player_pawn(w,0)
    print('WORLD',w.get_path_name(),'PAWN',pawn)
    if pawn:
        print('LOCATION',pawn.get_actor_location(),'BOUNDS',pawn.get_actor_bounds(False))
        if isinstance(pawn,unreal.Character):
            print('MOVEMENT',pawn.character_movement.movement_mode,'FLOOR',pawn.character_movement.current_floor)
    if STAGE.startswith('teleport:'):
        x,y,z=map(float,STAGE.split(':')[1:])
        print('TELEPORT',pawn.set_actor_location(unreal.Vector(x*45,y*45,z*45+52),False,True))
