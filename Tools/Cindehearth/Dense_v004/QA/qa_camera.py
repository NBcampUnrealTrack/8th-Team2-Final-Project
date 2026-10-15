"""Check the existing level-scoped camera after compact-layout play tests."""
import json
from pathlib import Path
import unreal

assert 'QA_OUT' in globals()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
controller = unreal.GameplayStatics.get_player_controller(world, 0)
director = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NUCindehearthDirector)[0]
if STAGE == 'prepare':
    pawn.character_movement.stop_movement_immediately()
    pawn.set_actor_location(unreal.Vector(540,-2345,52), False, True)
    director.set_night(False)
else:
    camera = controller.player_camera_manager
    rotation = camera.get_camera_rotation()
    arm = pawn.get_component_by_class(unreal.SpringArmComponent)
    report = dict(position=list(camera.get_camera_location().to_tuple()), rotation=list(rotation.to_tuple()),
                  fov=camera.get_fov_angle(), arm_length=arm.target_arm_length,
                  arm_rotation=list(arm.get_editor_property('relative_rotation').to_tuple()),
                  collision_test=arm.do_collision_test)
    assert abs(report['fov']-director.get_editor_property('village_camera_fov')) < .01, report
    assert abs(rotation.pitch-director.get_editor_property('village_camera_pitch')) < .01, report
    assert abs(report['arm_length']-director.get_editor_property('village_camera_distance')) < .01, report
    assert not report['collision_test'], report
    assert pawn.character_movement.current_floor.walkable_floor, 'Camera test pawn is not grounded'
    report['passed'] = True
    path = Path(QA_OUT)/'player_camera.json'
    assert not path.exists()
    path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('V004 GAME CAMERA PASS', report)
