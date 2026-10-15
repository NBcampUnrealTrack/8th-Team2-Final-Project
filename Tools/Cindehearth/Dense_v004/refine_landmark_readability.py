import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';out=root/'verification'
assert not unreal.WidgetService.is_pie_running();assert not (out/'landmark_readability.json').exists()
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);a={x.get_actor_label():x for x in sub.get_all_level_actors()};rows=[]
with unreal.ScopedEditorTransaction('Tavern evening warmth and doorway sightlines'):
 for label,pos,f in [('CH_Lettering_Tavern',[-126,-1910,252],.55),('CH_Lettering_Smithy',[-285,522,295],.45)]:
  x=a[label];x.modify();x.set_actor_location(unreal.Vector(*pos),False,True);x.set_actor_scale3d(x.get_actor_scale3d()*f);rows.append(dict(label=label,position=pos,sign_scale=f))
 for name,pos,color,intensity,radius,night in [('TavernDayBounce',[-126,-2230,390],[.73,.77,.84],220,800,False),('TavernDoorWarm',[-126,-1970,185],[1,.57,.27],520,600,True),('TavernLoftWarm',[-260,-1910,280],[1,.61,.33],160,420,True)]:
  label='CH_D4_'+name;assert label not in a
  x=sub.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*pos));x.set_actor_label(label);x.set_folder_path('Cindehearth/Dense_v004/Lighting');x.tags=['Cindehearth','CH.Dense_v004']+(['CH.Window'] if night else [])
  c=x.point_light_component;c.set_intensity(intensity);c.set_light_color(unreal.LinearColor(*color));c.set_attenuation_radius(radius);c.set_editor_property('source_radius',45.);c.set_cast_shadows(False);rows.append(dict(label=label,position=pos,intensity=intensity,night=night))
 mi='/Game/Cindehearth/Dense_v004/Materials/MI_CH_Tavern'
 assert unreal.MaterialService.set_instance_scalar_parameter(mi,'AmbientLift',.085)
 assert unreal.MaterialService.set_instance_vector_parameter(mi,'Tint',1.28,1.27,1.20);assert unreal.MaterialService.save_instance(mi)
 d=a['CH_LevelDirector'];d.set_night(False);d.set_chapter_progress(0)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(out/'landmark_readability.json').write_text(json.dumps(rows,indent=2));print('Signs moved clear of player silhouette; inn day/night practical light added')
