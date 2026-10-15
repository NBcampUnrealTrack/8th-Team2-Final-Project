import unreal,json,time
from pathlib import Path
class Review:
 def __init__(self):
  self.root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003/verification';self.rows=[];self.i=0;self.phase=0;self.start=0
  self.poses=[('square',(1550,-1500,930),(-300,0,160),43),('tavern',(-400,-3950,800),(-400,-2420,200),43),('assay',(2050,-370,740),(1400,420,220),48),('mine',(1650,2600,1040),(540,4070,480),50)]
  unreal.ViewportService.set_game_view(True);self.handle=unreal.register_slate_post_tick_callback(self.tick)
 def tick(self,dt):
  if self.i==len(self.poses):
   unreal.unregister_slate_post_tick_callback(self.handle);(self.root/'architecture_captures.json').write_text(json.dumps(self.rows,indent=2));return
  n,p,t,f=self.poses[self.i];p=unreal.Vector(*p);rot=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(*t))
  if self.phase==0:
   unreal.ViewportService.set_camera_location(p);unreal.ViewportService.set_camera_rotation(rot);unreal.ViewportService.set_fov(f);self.phase=1;self.start=time.monotonic()
  elif self.phase==1 and time.monotonic()-self.start>2:
   unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Polish_warmup',f,False,False);self.phase=2;self.start=time.monotonic()
  elif self.phase==2 and time.monotonic()-self.start>1:
   d=json.loads(unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Polish_arch_'+n,f,False,False));assert d['success'];self.rows.append(dict(name=n,capture=d));self.i+=1;self.phase=0
review=Review();print('ARCHITECTURE REVIEW QUEUED')
