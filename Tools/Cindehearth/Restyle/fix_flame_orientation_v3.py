import unreal,json,time
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
changed=[]
for a in AS.get_all_level_actors():
    if a.get_actor_label().startswith(('CH_HD2D_HearthFire','CH_HD2D_ForgeFire')):
        a.modify(); r=a.get_actor_rotation()
        a.set_actor_rotation(unreal.Rotator(pitch=r.pitch,yaw=r.yaw,roll=270),False)
        changed.append(a.get_actor_label())
assert len(changed)==5,changed
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
p=unreal.Vector(1550,-1500,930);r=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(-300,0,160))
unreal.ViewportService.set_game_view(True)
unreal.ViewportService.set_camera_location(p)
unreal.ViewportService.set_camera_rotation(r)
unreal.ViewportService.set_fov(43)
state={'start':time.monotonic()}
def tick(dt):
    if time.monotonic()-state['start']<3:return
    unreal.unregister_slate_post_tick_callback(state['handle'])
    result=unreal.ScreenshotService.capture_from_pose(p,r,'CH_Restyle_flame_v3',43,False,False)
    (root/'fire_orientation_v3.json').write_text(json.dumps(dict(changed=changed,capture=json.loads(result)),indent=2),encoding='utf-8')
state['handle']=unreal.register_slate_post_tick_callback(tick)
print('FIRE UPRIGHT ROTATION SAVED; CAPTURE QUEUED')
