"""Settled actual lit viewport captures, with chapter presentation variants."""
import unreal,json,time
from pathlib import Path
class PolishViews:
 def __init__(self):
  self.root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003/verification';self.rows=[];self.i=0;self.phase=0;self.start=0
  self.d=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.NUCindehearthDirector))
  self.poses=[
   ('village_day',(0,-8300,4300),(0,-200,250),53,False,0),
   ('square_day',(1550,-1500,930),(-300,0,160),43,False,0),
   ('homes_day',(-900,-4150,1050),(-900,-2050,120),43,False,0),
   ('mine',(1650,2600,1040),(540,4070,480),50,False,0),
   ('cemetery',(4900,2050,1200),(5050,3020,650),48,False,0),
   ('ashslope',(-6600,-1600,1850),(-6500,700,400),48,False,0),
   ('logging',(-1650,-7180,540),(-900,-6525,115),42,False,0),
   ('pine',(1300,-8800,850),(300,-6650,200),47,False,0),
   ('rings',(540,5410,900),(540,6250,185),50,False,8),
   ('hollowcore',(1100,8490,430),(540,9290,100),47,False,8),
   ('sky',(0,-4700,580),(0,5600,2300),64,False,0),
   ('square_night',(1550,-1500,930),(-300,0,160),43,True,0),
   ('square_hushed',(1550,-1500,930),(-300,0,160),43,True,9),
   ('square_recovered',(1550,-1500,930),(-300,0,160),43,True,12)]
  self.refined=globals().get('STAGE')=='refined'
  if self.refined:self.poses=[x for x in self.poses if x[0] in ['village_day','mine','rings','hollowcore']]
  unreal.ViewportService.set_game_view(True);self.handle=unreal.register_slate_post_tick_callback(self.tick)
 def tick(self,dt):
  if self.i==len(self.poses):
   unreal.unregister_slate_post_tick_callback(self.handle);self.d.set_chapter_progress(0);self.d.set_night(False)
   (self.root/('refined_captures.json' if self.refined else 'final_captures.json')).write_text(json.dumps(self.rows,indent=2));unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level();return
  n,p,t,f,night,q=self.poses[self.i];p=unreal.Vector(*p);rot=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(*t))
  if self.phase==0:
   self.d.set_night(night);self.d.set_chapter_progress(q);unreal.ViewportService.set_camera_location(p);unreal.ViewportService.set_camera_rotation(rot);unreal.ViewportService.set_fov(f);self.phase=1;self.start=time.monotonic()
  elif self.phase==1 and time.monotonic()-self.start>2:
   unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Polish_final_warmup',f,False,False);self.phase=2;self.start=time.monotonic()
  elif self.phase==2 and time.monotonic()-self.start>1:
   d=json.loads(unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Polish_final_'+n,f,False,False));assert d['success'];self.rows.append(dict(name=n,night=night,quest=q,capture=d));self.i+=1;self.phase=0
polish_views=PolishViews();print('FINAL VIEWS QUEUED')
