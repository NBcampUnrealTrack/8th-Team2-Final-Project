import unreal,json
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
pawn=unreal.GameplayStatics.get_player_pawn(w,0);pc=unreal.GameplayStatics.get_player_controller(w,0)
d=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NUCindehearthDirector)[0]
if STAGE=='prepare':
    pawn.set_actor_location(unreal.Vector(540,-3500,52),False,True);d.set_night(False)
else:
    cam=pc.player_camera_manager;p=cam.get_camera_location();rot=cam.get_camera_rotation();f=cam.get_fov_angle()
    arm=pawn.get_component_by_class(unreal.SpringArmComponent)
    report=dict(position=list(p.to_tuple()),rotation=list(rot.to_tuple()),fov=f,arm_length=arm.target_arm_length,arm_rotation=list(arm.get_editor_property('relative_rotation').to_tuple()),collision_test=arm.do_collision_test)
    assert abs(f-32)<.01 and abs(rot.pitch+25)<.01,report
    unreal.ViewportService.set_camera_location(p);unreal.ViewportService.set_camera_rotation(rot);unreal.ViewportService.set_fov(f)
    (Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003/verification/player_camera.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(report)
