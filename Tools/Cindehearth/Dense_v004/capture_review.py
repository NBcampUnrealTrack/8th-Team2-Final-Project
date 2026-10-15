import unreal,json,time,sys
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';sys.path.insert(0,str(root))
import layout_transform as lt
class DenseViews:
 def __init__(self):
  self.rows=[];self.i=0;self.phase=0;self.start=0
  self.d=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.NUCindehearthDirector))
  # Retain previously validated v003 viewing rays, recenter on compacted subjects.
  self.poses=[]
  for n,p,t,f,k in [('village',(0,-8300,4300),(0,-200,250),53,None),('square',(1550,-1500,930),(-300,0,160),43,'Hearth'),('homes',(-900,-4150,1050),(-900,-2050,120),43,None)]:
   nt=lt.cluster_point(k,t) if k else lt.affine(t);scale=.74 if not k else 1
   np=[nt[i]+scale*(p[i]-t[i]) for i in range(3)];self.poses.append((n,np,nt,f))
  unreal.ViewportService.set_game_view(True);self.d.set_night(False);self.d.set_chapter_progress(0)
  self.handle=unreal.register_slate_post_tick_callback(self.tick)
 def tick(self,dt):
  if self.i==len(self.poses):
   unreal.unregister_slate_post_tick_callback(self.handle);(root/'verification/review_captures.json').write_text(json.dumps(self.rows,indent=2));return
  n,p,t,f=self.poses[self.i];p=unreal.Vector(*p);rot=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(*t))
  if self.phase==0:
   unreal.ViewportService.set_camera_location(p);unreal.ViewportService.set_camera_rotation(rot);unreal.ViewportService.set_fov(f);self.phase=1;self.start=time.monotonic()
  elif self.phase==1 and time.monotonic()-self.start>2:
   unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Dense_warmup',f,False,False);self.phase=2;self.start=time.monotonic()
  elif self.phase==2 and time.monotonic()-self.start>1:
   r=json.loads(unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Dense_review_'+n,f,False,False));assert r['success'];self.rows.append(dict(name=n,capture=r));self.i+=1;self.phase=0
dense_views=DenseViews();print('Review captures queued')
