import sys,time,hashlib
import level_v8_client as c
tag=sys.argv[1];target=c.DEST/'Preview'/(tag+'.png');assert not target.exists()
cmd='HighResShot 1600x900 filename='+str(target).replace('\\','/')
code='''import unreal,json
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
pc=unreal.GameplayStatics.get_player_controller(w,0);assert pc
pcm=pc.player_camera_manager
print(json.dumps({'camera':pcm.get_camera_location().to_tuple(),'rotation':pcm.get_camera_rotation().to_tuple(),'fov':pcm.get_fov_angle(),'pawn':unreal.GameplayStatics.get_player_pawn(w,0).get_actor_location().to_tuple()}))
unreal.SystemLibrary.execute_console_command(w,COMMAND,pc)
'''
r=c.call('execute_python_code',{'code':'COMMAND='+repr(cmd)+'\n'+code},'V8_game_shot_'+tag+'_request');print(r)
for i in range(30):
    time.sleep(1)
    if target.exists() and target.stat().st_size>1000:break
else:raise RuntimeError('Game viewport screenshot did not arrive')
c.save('Preview/'+tag+'_receipt.json',{'path':str(target),'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'live_camera':r,'method':'SystemLibrary.ExecuteConsoleCommand specific player HighResShot'})
print(target)
