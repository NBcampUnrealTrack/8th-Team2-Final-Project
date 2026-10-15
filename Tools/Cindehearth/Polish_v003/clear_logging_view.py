import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003/verification'
assert not unreal.WidgetService.is_pie_running();assert not (root/'logging_sightline.json').exists()
rows=[]
with unreal.ScopedEditorTransaction('Cindehearth logging reveal clearing'):
 for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
  if a.get_actor_label().startswith('CH_Pine_04_'):
   before=a.get_actor_location();after=before+unreal.Vector(-1100,0,0);a.set_actor_location(after,False,True)
   rows.append(dict(actor=a.get_actor_label(),before=list(before.to_tuple()),after=list(after.to_tuple())))
assert rows;assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(root/'logging_sightline.json').write_text(json.dumps(rows,indent=2));print('TREE AND MATCHING TRUNK MOVED',len(rows))
