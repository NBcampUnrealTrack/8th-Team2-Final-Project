import unreal
import json
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert w
d=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NUCindehearthDirector)[0]
pawn=unreal.GameplayStatics.get_player_pawn(w,0)
if STAGE=='camera':
    pawn.set_actor_location(unreal.Vector(-1125,-1880,52),False,True)
    d.set_night(False)
elif STAGE=='capture':
    cam=unreal.GameplayStatics.get_player_controller(w,0).player_camera_manager
    pos=cam.get_camera_location();rot=cam.get_camera_rotation();fov=cam.get_fov_angle()
    unreal.ViewportService.set_camera_location(pos);unreal.ViewportService.set_camera_rotation(rot)
    print(unreal.ScreenshotService.capture_from_pose(pos,rot,'CH_Gorm_Repaired_player_camera',fov,False,False))
elif STAGE=='epilogue':
    d.set_editor_property('auto_play_epilogue',False)
    d.set_chapter_progress(12)
    d.set_night(True)
    hidden=[]
    for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.StaticMeshActor):
        if a.get_actor_label() in ['CH_GormHouse_Stone','CH_GormHouse_Roof','CH_GormHouse_Window']:
            hidden.append(not a.static_mesh_component.get_editor_property('visible'))
    assert len(hidden)==3 and all(hidden)
    assert d.play_epilogue()
    print('P2 EPILOGUE STARTED')
elif STAGE=='verify':
    pc=unreal.GameplayStatics.get_player_controller(w,0)
    data=dict(played=d.epilogue_played,view_target_restored=pc.get_view_target()==pawn,move_input_restored=not pc.is_move_input_ignored(),look_input_restored=not pc.is_look_input_ignored())
    assert all(data.values()),data
    out=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Pipeline/verification/repair_epilogue_runtime.json'
    out.write_text(json.dumps(data,indent=2),encoding='utf-8')
    print(data)
