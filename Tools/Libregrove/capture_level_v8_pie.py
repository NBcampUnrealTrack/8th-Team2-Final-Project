"""Native active PIE screenshot: no camera replacement, no image editing."""
import sys,time,json,hashlib
import level_v8_client as c
tag=sys.argv[1];target=c.DEST/'Preview'/(tag+'.png');assert not target.exists()
code='''import unreal,json
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
p=unreal.GameplayStatics.get_player_pawn(w,0);assert p
pcm=unreal.GameplayStatics.get_player_camera_manager(w,0)
print(json.dumps({'pawn':p.get_actor_location().to_tuple(),'camera':pcm.get_camera_location().to_tuple(),'rotation':pcm.get_camera_rotation().to_tuple(),'fov':pcm.get_fov_angle()}))
if not hasattr(unreal,'_lg8_pie_captures'):unreal._lg8_pie_captures={}
unreal._lg8_pie_captures[TAG]=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,TARGET,delay=.5,force_game_view=True)
assert unreal._lg8_pie_captures[TAG]
'''
code='TAG='+repr(tag)+'\nTARGET='+repr(str(target))+'\n'+code
print(c.call('execute_python_code',{'code':code},'V8_PIE_capture_'+tag+'_queue'))
for i in range(20):
    time.sleep(1)
    if target.exists() and target.stat().st_size>1000:break
    if i==2:
        code='import unreal\nprint(unreal.ScreenshotService.capture_from_pose(unreal.Vector(),unreal.Rotator(),'+repr(str(target.with_name(tag+'_bridge_color_unreliable.png')))+',use_current_camera=True))'
        c.call('execute_python_code',{'code':code},'V8_PIE_capture_'+tag+'_draw')
else:raise RuntimeError('Capture pending; do not requeue')
status=c.call('execute_python_code',{'code':'import unreal\nprint(unreal._lg8_pie_captures['+repr(tag)+'].is_task_done())'},'V8_PIE_capture_'+tag+'_done')
c.save('Preview/'+tag+'_receipt.json',{'native_file':str(target),'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'status':status})
print(target)
