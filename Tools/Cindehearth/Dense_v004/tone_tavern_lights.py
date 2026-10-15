import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';assert not unreal.WidgetService.is_pie_running()
actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
rows=[]
for key,value in [('CH_D4_TavernDayBounce',90),('CH_D4_TavernDoorWarm',45),('CH_D4_TavernLoftWarm',18)]:
 a=actors[key];a.modify();a.point_light_component.set_intensity(value);rows.append(dict(label=key,intensity=value))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(root/'verification/tavern_lights_toned.json').write_text(json.dumps(rows,indent=2));print('Inn practical lights toned to prevent highlight clipping')
