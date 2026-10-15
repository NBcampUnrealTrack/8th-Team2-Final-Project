"""Unedited native UE frames. Uses measured bounds, not guessed scene coordinates."""
import sys,json,time,hashlib
import level_v8_client as c
tag=sys.argv[1];group=sys.argv[2] if len(sys.argv)>2 else 'Overview'
out=c.DEST/'Preview';out.mkdir(exist_ok=True);target=out/(tag+'.png');assert not target.exists()
code='''import unreal,json,math
from pathlib import Path
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
refs=[a for a in actors.get_all_level_actors() if str(a.get_folder_path()).startswith('Libregrove/Buildings')]
if GROUP!='Overview':refs=[a for a in refs if GROUP in str(a.get_folder_path())]
assert refs
bounds=[a.get_actor_bounds(False) for a in refs]
lo=[min(b[0].to_tuple()[i]-b[1].to_tuple()[i] for b in bounds) for i in range(3)]
hi=[max(b[0].to_tuple()[i]+b[1].to_tuple()[i] for b in bounds) for i in range(3)]
center=unreal.Vector(*[(l+h)/2 for l,h in zip(lo,hi)])
radius=math.sqrt(sum(((h-l)/2)**2 for l,h in zip(lo,hi)))
fov=45;pitch=-65 if GROUP=='Overview' else -32
rotation=unreal.Rotator(pitch=pitch,yaw=90,roll=0)
distance=radius/math.sin(math.atan(math.tan(math.radians(fov/2))*1200/1600))*1.05
loc=center-rotation.get_forward_vector()*distance
assert unreal.ViewportService.set_viewport_type('perspective')
assert unreal.ViewportService.set_fov(fov)
assert unreal.ViewportService.set_view_mode('lit')
assert unreal.ViewportService.set_realtime(True)
assert unreal.ViewportService.set_game_view(True)
assert unreal.ViewportService.set_exposure_game_settings()
actors.select_nothing();assert unreal.ActorService.set_viewport_camera(loc,rotation)
if not hasattr(unreal,'_lg8_captures'):unreal._lg8_captures={}
task=unreal.AutomationLibrary.take_high_res_screenshot(1600,1200,TARGET,delay=.5,force_game_view=True)
assert task;unreal._lg8_captures[TAG]=task
print(json.dumps({'bounds_min':lo,'bounds_max':hi,'location':loc.to_tuple(),'rotation':rotation.to_tuple(),'group':GROUP}))
'''
code='TAG='+repr(tag)+'\nGROUP='+repr(group)+'\nTARGET='+repr(str(target))+'\n'+code
print(c.call('execute_python_code',{'code':code},'V8_capture_'+tag+'_queue'),flush=True)
for i in range(15):
    time.sleep(1)
    if target.exists() and target.stat().st_size>1000:break
    if i==1:
        draw='import unreal\nprint(unreal.ScreenshotService.capture_from_pose(unreal.Vector(),unreal.Rotator(),'+repr(str(out/(tag+'_bridge_color_unreliable.png')))+',use_current_camera=True))'
        c.call('execute_python_code',{'code':draw},'V8_capture_'+tag+'_draw')
else:raise RuntimeError('Native capture did not finish; do not requeue')
done=c.call('execute_python_code',{'code':'import unreal\nprint(unreal._lg8_captures['+repr(tag)+'].is_task_done())'},'V8_capture_'+tag+'_done')
c.save('Preview/'+tag+'_receipt.json',{'path':str(target),'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'task':done})
print(target)
