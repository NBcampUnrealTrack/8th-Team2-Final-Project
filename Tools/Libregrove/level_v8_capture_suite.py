"""Actual gameplay views, global style A/B/reset and reversible night readability test."""
import unreal,json,time,runpy,traceback,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908';RUN=globals().get('RUN','001')
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
if hasattr(unreal,'_lg8_walk'):assert len(unreal._lg8_walk.rows)==len(unreal._lg8_walk.names)
p=unreal.GameplayStatics.get_player_pawn(w,0);pc=unreal.GameplayStatics.get_player_controller(w,0);assert p and pc
mpc=unreal.load_asset('/Game/Libregrove/Level_V8/HD2D/MPC_LG8_HD2D');M=unreal.MaterialLibrary
baseline=json.loads((OUT/'HD2D/default_preset.json').read_text(encoding='utf8'))
defaults={'scalars':[{'name':k,'default':v} for k,v in baseline['scalars'].items()]}
for r in defaults['scalars']:assert abs(M.get_scalar_parameter_value(w,mpc,r['name'])-r['default'])<.0001,r
mpc_file=ROOT/'Content/Libregrove/Level_V8/HD2D/MPC_LG8_HD2D.uasset';mpc_hash=hashlib.sha256(mpc_file.read_bytes()).hexdigest()
actors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)
sun=next(a for a in actors if a.get_actor_label()=='LG8_Sun');sky=next(a for a in actors if a.get_actor_label()=='LG8_SkyLight');dome=next(a for a in actors if a.get_actor_label()=='LG8_SkyDome')
lamps=[a.light_component for a in actors if a.get_actor_label().startswith('LG8_LanternLight_')]
sun_color=sun.light_component.get_light_color();sun_int=sun.light_component.intensity;sky_int=sky.light_component.intensity;lamp_values=[a.intensity for a in lamps];sky_material=dome.static_mesh_component.get_material(0)
def reset():
 for r in defaults['scalars']:M.set_scalar_parameter_value(w,mpc,r['name'],r['default'])
 sun.light_component.set_intensity(sun_int);sun.light_component.set_light_color(sun_color);sky.light_component.set_intensity(sky_int)
 for a,v in zip(lamps,lamp_values):a.set_intensity(v)
 dome.static_mesh_component.set_material(0,sky_material)
def style(values):
 for k,v in values.items():M.set_scalar_parameter_value(w,mpc,k,v);assert abs(M.get_scalar_parameter_value(w,mpc,k)-v)<.0001
def night():
 sun.light_component.set_intensity(1.1);sun.light_component.set_light_color(unreal.LinearColor(.48,.62,1.));sky.light_component.set_intensity(2.2)
 for a in lamps:a.set_intensity(110.)
 mid=dome.static_mesh_component.create_dynamic_material_instance(0)
 for k,v in {'ZenithCol':(.02,.04,.09),'MidCol':(.06,.10,.18),'HorizonCol':(.16,.20,.29),'GroundCol':(.03,.04,.065),'CloudCol':(.12,.18,.27),'SunCol':(.18,.26,.45)}.items():mid.set_vector_parameter_value(k,unreal.LinearColor(*v,1))
class Views:
 def __init__(self):
  self.poses=[('StyleOff',[0,350,90],{'StyleStrength':0.,'PixelBlend':0.},False),('StyleStrong',[0,350,90],{'StyleStrength':1.,'PixelBlend':1.,'TextureDensity':.35,'PaletteSteps':10.,'PosterizeStrength':1.},False),('Square',[0,350,90],{},False),('Waterfront',[1710,1350,105],{},False),('Workshops',[-1800,850,90],{},False),('Shrine',[0,2110,160],{},False),('CentralBridge',[0,-720,125],{},False),('SquareNight',[0,350,90],{},True)]
  self.poses.append(('Watermill',[1650,-80,90],{},False))
  if globals().get('SELECT'):self.poses=[q for q in self.poses if q[0] in globals()['SELECT']]
  self.i=0;self.phase=0;self.rows=[];self.handle=unreal.register_slate_post_tick_callback(self.tick)
 def finish(self,error=None):
  unreal.unregister_slate_post_tick_callback(self.handle);reset();p.set_actor_location(unreal.Vector(0,350,90),False,True);p.character_movement.stop_movement_immediately()
  with (OUT/('capture_suite_'+RUN+'.json')).open('x',encoding='utf8') as f:json.dump({'rows':self.rows,'error':error,'mpc_defaults_unchanged':hashlib.sha256(mpc_file.read_bytes()).hexdigest()==mpc_hash,'runtime_restored':all(abs(M.get_scalar_parameter_value(w,mpc,r['name'])-r['default'])<.0001 for r in defaults['scalars'])},f,indent=2)
  unreal.log('LG8 CAPTURE SUITE FINISHED '+RUN)
 def tick(self,dt):
  try:
   if self.i==len(self.poses):self.finish();return
   name,pos,values,is_night=self.poses[self.i];now=time.monotonic()
   if self.phase==0:
    reset();style(values)
    if is_night:night()
    p.set_actor_location(unreal.Vector(*pos),False,True);p.character_movement.stop_movement_immediately();self.started=now;self.phase=1
   elif self.phase==1 and now-self.started>3.:
    dest=OUT/'Preview'/('Game_'+name+'_Final_'+RUN+'.png');assert not dest.exists()
    pcm=pc.player_camera_manager;r=pcm.get_camera_rotation();assert abs(r.pitch+30)<.01 and abs(r.yaw-90)<.01
    unreal.SystemLibrary.execute_console_command(w,'HighResShot 1600x900 filename='+dest.as_posix(),pc)
    self.rows.append({'name':name,'path':str(dest),'pawn':p.get_actor_location().to_tuple(),'camera':pcm.get_camera_location().to_tuple(),'rotation':r.to_tuple(),'fov':pcm.get_fov_angle(),'night':is_night,'style_overrides':values,'actual_game_view':True});self.started=now;self.phase=2
   elif self.phase==2 and now-self.started>3.:
    dest=Path(self.rows[-1]['path']);assert dest.exists() and dest.stat().st_size>1000;self.i+=1;self.phase=0
  except Exception:self.finish(traceback.format_exc())
unreal._lg8_views=Views();print('Queued '+str(len(unreal._lg8_views.poses))+' actual game views; runtime-only style/light overrides restore at end')
