"""Native Unreal screenshots, one queued frame at a time (no pixel edits)."""
import sys,json,time,hashlib
import textured_handoff as h

ident=sys.argv[1]
angle=sys.argv[2]
tag=ident+'_'+angle
preview=h.DEST/'Preview'
preview.mkdir(exist_ok=True)
target=preview/(tag+'.png')
assert not target.exists()
context=h.ue_call('get_editor_context',{},'V7_capture_'+tag+'_context')
assert context['play_session']=='stopped' and context['level']['name']=='L_Libregrove_AssetReview_V7'
code='''import unreal
import json,math
from pathlib import Path
dest=Path(DEST)
info=unreal.ViewportService.get_viewport_info()
prior={'type':str(info.viewport_type),'location':info.location.to_tuple(),'rotation':info.rotation.to_tuple(),'fov':info.fov,'realtime':info.is_realtime,'game_view':info.is_game_view}
assert unreal.ViewportService.set_viewport_type('perspective')
assert unreal.ViewportService.set_fov(60.0)
assert unreal.ViewportService.set_view_mode('lit')
assert unreal.ViewportService.set_realtime(True)
assert unreal.ViewportService.set_exposure_game_settings()
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).select_nothing()
if IDENT=='Overview':
    refs=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label().startswith('LGV7_')]
    bounds=[a.get_actor_bounds(False) for a in refs];assert len(bounds)==9
    lower=[min(c.to_tuple()[i]-e.to_tuple()[i] for c,e in bounds) for i in range(3)]
    upper=[max(c.to_tuple()[i]+e.to_tuple()[i] for c,e in bounds) for i in range(3)]
    center=unreal.Vector(*[(a+b)/2 for a,b in zip(lower,upper)])
    radius=math.sqrt(sum(((b-a)/2)**2 for a,b in zip(lower,upper)))
    distance=radius/math.sin(math.atan(math.tan(math.radians(30))*960/1280))*1.12
    pitch=-55 if ANGLE=='Oblique' else -90
    rotation=unreal.Rotator(pitch=pitch,yaw=-90,roll=0)
    location=center-rotation.get_forward_vector()*distance
    framing={'combined_min':lower,'combined_max':upper,'distance':distance}
else:
    direction=unreal.ViewDirection.RIGHT if ANGLE.startswith('Front') else unreal.ViewDirection.LEFT
    view=unreal.ActorService.calculate_actor_view('LGV7_'+IDENT,direction,1.7)
    assert view.success
    rotation=unreal.Rotator(pitch=-18,yaw=view.camera_rotation.yaw-20,roll=0)
    radius=math.sqrt(sum(v*v for v in view.actor_extent.to_tuple()))
    distance=max(view.view_distance,radius/math.sin(math.atan(math.tan(math.radians(30))*960/1280))*1.07)
    location=view.actor_center-rotation.get_forward_vector()*distance
    framing={'actor_center':view.actor_center.to_tuple(),'actor_extent':view.actor_extent.to_tuple(),'distance':distance}
assert unreal.ActorService.set_viewport_camera(location,rotation)
scene=json.loads(unreal.ScreenshotService.describe_scene(location,rotation,use_current_camera=True,max_actors=50))
assert scene['success']
with (dest/'Preview'/(TAG+'_scene.json')).open('x',encoding='utf-8') as f:json.dump({'prior':prior,'framing':framing,'scene':scene},f,indent=2)
if not hasattr(unreal,'_libregrove_v7_screenshots'):unreal._libregrove_v7_screenshots={}
assert TAG not in unreal._libregrove_v7_screenshots
task=unreal.AutomationLibrary.take_high_res_screenshot(1280,960,TARGET,delay=0.5,force_game_view=True)
assert task
unreal._libregrove_v7_screenshots[TAG]=task
print(json.dumps({'queued':TAG,'location':location.to_tuple(),'rotation':rotation.to_tuple(),'target':TARGET}))
'''
values={'IDENT':ident,'ANGLE':angle,'TAG':tag,'DEST':str(h.DEST),'TARGET':str(target)}
code=code.replace('import json,math','import json,math\n'+''.join(k+'='+repr(v)+'\n' for k,v in values.items()))
print(json.dumps(h.ue_call('execute_python_code',{'code':code},'V7_capture_'+tag+'_queue'),indent=2),flush=True)
for attempt in range(20):
    time.sleep(1)
    if target.exists() and target.stat().st_size>1000:break
    if attempt==1:
        # Background viewports may not draw. Force a supported synchronous draw
        # in a later tool call; use ONLY the native screenshot for color QA.
        # This bridge's synchronous PNG output swaps R/B in the current build.
        warmup=preview/(tag+'_bridge_color_unreliable.png')
        draw='import unreal\nprint(unreal.ScreenshotService.capture_from_pose(unreal.Vector(),unreal.Rotator(),'+repr(str(warmup))+',use_current_camera=True))'
        h.ue_call('execute_python_code',{'code':draw},'V7_capture_'+tag+'_draw')
else:raise RuntimeError('Capture file not produced; do not blindly requeue')
code='import unreal\nprint(unreal._libregrove_v7_screenshots['+repr(tag)+'].is_task_done())'
done=h.ue_call('execute_python_code',{'code':code},'V7_capture_'+tag+'_done')
result={'path':str(target),'bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'native_task':done}
h.save('Preview/'+tag+'_receipt.json',result)
print(json.dumps(result,indent=2))
