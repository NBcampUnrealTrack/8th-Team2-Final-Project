"""V8-only grade plus separately switchable Windows depth of field."""
import unreal,json
from pathlib import Path
OUT=Path(unreal.Paths.project_dir()).resolve()/'Docs/Libregrove/Level_V8_20260908'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Libregrove' and not unreal.WidgetService.is_pie_running()
vols=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.PostProcessVolume)];assert len(vols)==1
pp=vols[0];settings=pp.get_editor_property('settings')
params={'auto_exposure_min_brightness':1.,'auto_exposure_max_brightness':1.,'auto_exposure_bias':.30,'bloom_intensity':.40,'bloom_threshold':1.10,'vignette_intensity':.13,'motion_blur_amount':0.,'scene_fringe_intensity':0.,'film_grain_intensity':0.,'depth_of_field_fstop':16.,'depth_of_field_focal_distance':0.,'color_saturation':unreal.Vector4(.98,.98,.98,1.),'ambient_occlusion_intensity':.35,'ambient_occlusion_radius':75.}
backup={k:str(settings.get_editor_property(k)) for k in params}
for k,v in params.items():settings.set_editor_property('override_'+k,True);settings.set_editor_property(k,v)
pp.set_editor_property('settings',settings);pp.set_actor_label('LG8_PP_ArtGrade');pp.set_editor_property('priority',10.)
dof=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());dof.set_actor_label('LG8_PP_DOF_Optional_Windows');dof.set_folder_path('Libregrove/Environment')
dof.set_editor_property('unbound',True);dof.set_editor_property('priority',20.);dof.set_editor_property('blend_weight',1.)
settings=dof.get_editor_property('settings');values={'depth_of_field_fstop':2.8,'depth_of_field_sensor_width':72.,'depth_of_field_focal_distance':1900.}
for k,v in values.items():settings.set_editor_property('override_'+k,True);settings.set_editor_property(k,v)
dof.set_editor_property('settings',settings)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
with (OUT/'postprocess_setup.json').open('x',encoding='utf8') as f:json.dump({'original':backup,'grade':{k:str(v) for k,v in params.items()},'optional_dof':values,'mobile_switch':'Set LG8_PP_DOF_Optional_Windows BlendWeight to 0; platform qualification remains pending'},f,indent=2)
print('Grade and optional DOF saved. No project renderer changes.')
