"""Actual lit level viewport with settled camera history, not SceneCapture lighting."""
import unreal,json,time
from pathlib import Path
class FinalViews:
    def __init__(self):
        self.root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'
        self.d=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.NUCindehearthDirector))
        self.poses=[
            ('pine_front',(1300,-8800,850),(300,-6650,200),47,False,0),
            ('pine_side',(-1900,-7200,800),(0,-6500,200),47,False,0),
            ('pine_close',(650,-7250,490),(1450,-6650,245),43,False,0),
            ('village_day',(0,-8300,4300),(0,-200,250),53,False,0),
            ('square_day',(1550,-1500,930),(-300,0,160),43,False,0),
            ('homes_day',(-900,-4150,1050),(-900,-2050,120),43,False,0),
            ('sky',(0,-4700,580),(0,5600,2300),64,False,0),
            ('square_night',(1550,-1500,930),(-300,0,160),43,True,0),
            ('square_hushed',(1550,-1500,930),(-300,0,160),43,True,9),
            ('square_recovered',(1550,-1500,930),(-300,0,160),43,True,12)]
        self.i=0;self.phase=0;self.start=0;self.results=[]
        unreal.ViewportService.set_game_view(True)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
    def tick(self,dt):
        if self.i==len(self.poses):
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.d.set_chapter_progress(0);self.d.set_night(False)
            (self.root/'final_viewport_captures.json').write_text(json.dumps(self.results,indent=2),encoding='utf-8')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level();return
        n,p,t,f,night,q=self.poses[self.i];p=unreal.Vector(*p);rot=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(*t))
        if self.phase==0:
            self.d.set_night(night);self.d.set_chapter_progress(q)
            unreal.ViewportService.set_camera_location(p);unreal.ViewportService.set_camera_rotation(rot);unreal.ViewportService.set_fov(f)
            self.start=time.monotonic();self.phase=1
        elif self.phase==1 and time.monotonic()-self.start>2:
            unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Restyle_final_warmup',f,False,False)
            self.start=time.monotonic();self.phase=2
        elif self.phase==2 and time.monotonic()-self.start>1:
            data=json.loads(unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Restyle_final_'+n,f,False,False));assert data['success']
            self.results.append(dict(name=n,night=night,quest=q,capture=data))
            self.i+=1;self.phase=0
final_views=FinalViews()
print('FINAL ACTUAL VIEWPORT CAPTURES QUEUED')
