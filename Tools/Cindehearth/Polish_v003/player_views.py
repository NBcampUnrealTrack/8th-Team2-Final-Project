import unreal,json,time
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003/verification'
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
d=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NUCindehearthDirector)[0]
class PlayerViews:
 def __init__(self):
  self.poses=[('square',(0,-350,215),False,0),('square_night',(0,-350,215),True,0),('homes',(-1030,-1900,52),False,0),('logging',(-900,-6690,52),False,5),('depot',(540,6240,186),False,8),('cocoon',(540,9150,6),False,8)]
  self.retake=globals().get('STAGE') in ['depot','logging']
  self.retake_name=globals().get('STAGE','all')
  if self.retake:self.poses=[x for x in self.poses if x[0]==self.retake_name]
  self.i=0;self.phase=0;self.started=0;self.rows=[];self.handle=unreal.register_slate_post_tick_callback(self.tick)
 def tick(self,dt):
  if self.i==len(self.poses):
   unreal.unregister_slate_post_tick_callback(self.handle);d.set_chapter_progress(0);d.set_night(False)
   (root/('player_'+self.retake_name+'_retake.json' if self.retake else 'player_captures.json')).write_text(json.dumps(self.rows,indent=2));return
  n,p,night,q=self.poses[self.i]
  if self.phase==0:
   d.set_editor_property('auto_play_epilogue',False);d.set_chapter_progress(q);d.set_night(night);pawn.set_actor_location(unreal.Vector(*p),False,True);pawn.character_movement.stop_movement_immediately();self.started=time.monotonic();self.phase=1
  elif self.phase==1 and time.monotonic()-self.started>3:
   dest=(root/('player_'+n+'.png')).resolve().as_posix();unreal.SystemLibrary.execute_console_command(w,'HighResShot 1440x900 filename='+dest,pc)
   self.rows.append(dict(name=n,file=dest,actual_game_view=True,position=list(pc.player_camera_manager.get_camera_location().to_tuple()),fov=pc.player_camera_manager.get_fov_angle()));self.started=time.monotonic();self.phase=2
  elif self.phase==2 and time.monotonic()-self.started>2:self.i+=1;self.phase=0
player_views=PlayerViews();print('ACTUAL PLAYER CAPTURES QUEUED')
