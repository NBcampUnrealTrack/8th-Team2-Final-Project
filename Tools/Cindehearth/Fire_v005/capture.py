"""Fixed gameplay-camera captures, with time samples for the material loop."""
import unreal,json,time
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Fire_v005';out=root/'verification'
stage=globals().get('STAGE','after')
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
d=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NUCindehearthDirector)[0]
class FireViews:
 def __init__(self):
  self.poses=[('hearth_day',[-59.4,-345,215],False),('hearth_night',[-59.4,-345,215],True),('forge',[-92.7,660,215],False)]
  if stage!='before':self.poses.extend([('hearth_motion_a',[-59.4,-345,215],False),('hearth_motion_b',[-59.4,-345,215],False)])
  if stage=='forge_final':self.poses=[('forge',[-92.7,660,215],False)]
  self.i=0;self.phase=0;self.started=0;self.rows=[];self.handle=unreal.register_slate_post_tick_callback(self.tick)
 def tick(self,dt):
  if self.i==len(self.poses):
   unreal.unregister_slate_post_tick_callback(self.handle);d.set_chapter_progress(0);d.set_night(False)
   (out/(stage+'_captures.json')).write_text(json.dumps(self.rows,indent=2),encoding='utf-8');return
  name,pos,night=self.poses[self.i]
  if self.phase==0:
   d.set_editor_property('auto_play_epilogue',False);d.set_chapter_progress(0);d.set_night(night);pawn.set_actor_location(unreal.Vector(*pos),False,True);pawn.character_movement.stop_movement_immediately();self.started=time.monotonic();self.phase=1
  elif self.phase==1 and time.monotonic()-self.started>3:
   r=pc.player_camera_manager.get_camera_rotation();assert abs(r.pitch+25)<.1 and abs(r.yaw-90)<.1
   dest=(out/(stage+'_'+name+'.png')).resolve().as_posix();unreal.SystemLibrary.execute_console_command(w,'HighResShot 1440x900 filename='+dest,pc)
   self.rows.append(dict(name=name,file=dest,actual_game_view=True,camera_rotation=[r.pitch,r.yaw,r.roll],fov=pc.player_camera_manager.get_fov_angle(),pawn=list(pawn.get_actor_location().to_tuple()),captured_time=unreal.GameplayStatics.get_time_seconds(w)));self.started=time.monotonic();self.phase=2
  elif self.phase==2 and time.monotonic()-self.started>2:self.i+=1;self.phase=0
fire_views=FireViews();print('FIRE CAPTURES QUEUED',stage,len(fire_views.poses))
