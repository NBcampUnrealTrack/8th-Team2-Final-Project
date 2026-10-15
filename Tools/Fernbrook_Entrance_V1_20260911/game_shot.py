import sys,time,hashlib
import client as c
tag=sys.argv[1];target=c.DOC/'Preview'/(tag+'.png');assert not target.exists()
cmd='HighResShot 1600x900 filename='+str(target).replace('\\','/')
code='''import unreal,json
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0);assert pc and pawn
pcm=pc.player_camera_manager;cap=pawn.get_component_by_class(unreal.CapsuleComponent)
print(json.dumps({'camera':pcm.get_camera_location().to_tuple(),'rotation':pcm.get_camera_rotation().to_tuple(),'fov':pcm.get_fov_angle(),'pawn':pawn.get_actor_location().to_tuple(),'capsule_half_height':cap.get_scaled_capsule_half_height(),'capsule_radius':cap.get_scaled_capsule_radius(),'grounded':bool(pawn.character_movement.current_floor.walkable_floor),'world':w.get_path_name()}))
unreal.SystemLibrary.execute_console_command(w,COMMAND,pc)
'''
r=c.call('execute_python_code',{'code':'import unreal\nCOMMAND='+repr(cmd)+'\n'+code},'FB1_game_shot_'+tag+'_request');print(r,flush=True)
for i in range(30):
    time.sleep(1)
    if target.exists() and target.stat().st_size>1000:break
else:raise RuntimeError('Game screenshot pending: do not requeue')
c.save('Preview/'+tag+'_receipt.json',{'path':str(target),'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'live_camera':r,'method':'specific player controller HighResShot; actual game view'})
print(target)
