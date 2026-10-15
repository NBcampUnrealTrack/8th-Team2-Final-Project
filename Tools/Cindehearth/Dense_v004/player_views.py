"""Actual PIE camera evidence; no scene-capture substitutes or art retouching."""
import unreal,json,time,sys
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';sys.path.insert(0,str(root))
import layout_transform as lt
out=root/'verification'
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
d=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NUCindehearthDirector)[0]
class PlayerViews:
 def __init__(self):
  self.poses=[('square',lt.cluster_point('Hearth',[0,-350,215]),False,0),('square_night',lt.cluster_point('Hearth',[0,-350,215]),True,0),('square_hushed',lt.cluster_point('Hearth',[0,-350,215]),True,9),('square_recovered',lt.cluster_point('Hearth',[0,-350,215]),True,12),
   ('tavern',[-126,-2110,52],False,0),('tavern_night',[-126,-2110,52],True,0),('smithy',[-92.7,510,215],False,0),('smithy_inside',[-92.7,690,215],False,0),
   ('homes',lt.cluster_point('GormHouse',[-1030,-1900,52]),False,0),('stair',[-399,-1240,52],False,0),('mine',lt.affine([540,3880,367]),False,0),
   ('ash_entry',lt.affine([-3400,400,210]),False,4),('pine_entry',lt.affine([540,-4740,52]),False,5),('logging',[-525.6,-4650,52],False,5),
   ('cemetery',lt.affine([78*45,55*45,10.2*45+52]),False,0),('depot',lt.affine([540,6240,186]),False,8),('cocoon',lt.affine([540,9150,6]),False,8)]
  stage=globals().get('STAGE','all');self.suffix=stage
  if stage!='all':self.poses=[x for x in self.poses if x[0] in stage.split(',')]
  assert self.poses
  self.i=0;self.phase=0;self.started=0;self.rows=[];self.handle=unreal.register_slate_post_tick_callback(self.tick)
 def tick(self,dt):
  if self.i==len(self.poses):
   unreal.unregister_slate_post_tick_callback(self.handle);d.set_chapter_progress(0);d.set_night(False)
   (out/('player_captures_'+self.suffix.replace(',','_')+'.json')).write_text(json.dumps(self.rows,indent=2));return
  n,p,night,q=self.poses[self.i]
  if self.phase==0:
   d.set_editor_property('auto_play_epilogue',False);d.set_chapter_progress(q);d.set_night(night);pawn.set_actor_location(unreal.Vector(*p),False,True);pawn.character_movement.stop_movement_immediately();self.started=time.monotonic();self.phase=1
  elif self.phase==1 and time.monotonic()-self.started>3:
   dest=(out/('player_'+n+'.png')).resolve().as_posix();unreal.SystemLibrary.execute_console_command(w,'HighResShot 1440x900 filename='+dest,pc)
   self.rows.append(dict(name=n,file=dest,actual_game_view=True,pawn_position=list(pawn.get_actor_location().to_tuple()),camera_position=list(pc.player_camera_manager.get_camera_location().to_tuple()),fov=pc.player_camera_manager.get_fov_angle()));self.started=time.monotonic();self.phase=2
  elif self.phase==2 and time.monotonic()-self.started>2:self.i+=1;self.phase=0
player_views=PlayerViews();print('ACTUAL PLAYER CAPTURES QUEUED',len(player_views.poses))
