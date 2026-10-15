"""Fixed 1440x900 scene captures, with real editor ticks between state and render."""
import unreal,json,time
from pathlib import Path
class RestyleCapture:
    def __init__(self):
        self.root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification/final_v2'
        self.root.mkdir(exist_ok=True)
        self.sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        self.d=next(a for a in self.sub.get_all_level_actors() if isinstance(a,unreal.NUCindehearthDirector))
        self.actor=self.sub.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(),unreal.Rotator())
        self.actor.set_actor_label('CH_TEMP_RestyleCapture')
        self.c=self.actor.get_component_by_class(unreal.SceneCaptureComponent2D)
        self.rt=unreal.RenderingLibrary.create_render_target2d(self.world,1440,900,unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        self.c.texture_target=self.rt;self.c.capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        self.c.capture_every_frame=False;self.c.capture_on_movement=False;self.c.always_persist_rendering_state=True
        self.c.post_process_blend_weight=1
        pp=next(a for a in self.sub.get_all_level_actors() if isinstance(a,unreal.PostProcessVolume))
        self.c.post_process_settings=pp.settings
        self.poses=[
            ('01_village_day',(0,-8300,4300),(0,-200,250),53,False,0),
            ('02_square_day',(1250,-1600,1030),(-270,80,220),45,False,0),
            ('03_homes_day',(-900,-4150,1050),(-900,-2050,120),43,False,0),
            ('04_mine_day',(1850,2000,1080),(450,3470,430),47,False,0),
            ('05_ashslope',(-6250,-1000,1800),(-6200,2000,480),49,False,0),
            ('06_pinehollow',(1400,-8400,1250),(200,-6400,30),48,False,0),
            ('07_sky',(0,-4700,580),(0,5600,2300),64,False,0),
            ('08_square_night',(1250,-1600,1030),(-270,80,220),45,True,0),
            ('09_square_hushed',(1250,-1600,1030),(-270,80,220),45,True,9),
            ('10_square_recovered',(1250,-1600,1030),(-270,80,220),45,True,12),
            ('11_homes_night',(-900,-4150,1050),(-900,-2050,120),43,True,12),
            ('12_fire_side',(-270,-950,340),(-270,0,270),42,False,0)]
        self.i=0;self.phase=0;self.start=0;self.results=[]
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
    def tick(self,dt):
        if self.i==len(self.poses):
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.sub.destroy_actor(self.actor)
            self.d.set_chapter_progress(0);self.d.set_night(False)
            (self.root/'captures.json').write_text(json.dumps(self.results,indent=2),encoding='utf-8')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level();return
        n,p,t,f,night,q=self.poses[self.i]
        if self.phase==0:
            self.d.set_night(night);self.d.set_chapter_progress(q)
            self.actor.set_actor_location(unreal.Vector(*p),False,False)
            self.actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*p),unreal.Vector(*t)),False)
            self.c.fov_angle=f;self.c.camera_cut_this_frame=True;self.c.capture_every_frame=True
            self.start=time.monotonic();self.phase=1
        elif self.phase==1 and time.monotonic()-self.start>1.5:
            self.c.capture_every_frame=False
            self.c.capture_scene()
            self.start=time.monotonic();self.phase=2
        elif self.phase==2 and time.monotonic()-self.start>.5:
            unreal.RenderingLibrary.export_render_target(self.world,self.rt,str(self.root),n+'.png')
            self.results.append(dict(name=n,position=p,target=t,fov=f,night=night,quest=q,resolution=[1440,900],method='SceneCapture2D / FinalColorLDR'))
            self.i+=1;self.phase=0
restyle_capture=RestyleCapture()
print('FINAL 12 SCENE CAPTURES QUEUED')
