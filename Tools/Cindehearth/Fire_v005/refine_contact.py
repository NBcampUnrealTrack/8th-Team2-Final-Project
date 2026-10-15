"""Contact correction from actual front-view captures."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Fire_v005';out=root/'verification'
assert not unreal.WidgetService.is_pie_running()
actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
plans=[('Hearth',(-59.4,-24.57,237.25),(1.9,1.6,1)),('Forge',(-91.7,733,282),(.62,.33,1))]
rows=[]
with unreal.ScopedEditorTransaction('Align front flames with the hearth logs and upper forge opening'):
 for name,loc,scale in plans:
  a=actors['CH_F5_FrontFire_'+name];a.modify();before=list(a.get_actor_location().to_tuple());a.set_actor_location(unreal.Vector(*loc),False,True);a.set_actor_scale3d(unreal.Vector(*scale))
  rows.append(dict(label=a.get_actor_label(),before=before,after=list(a.get_actor_location().to_tuple()),scale=list(a.get_actor_scale3d().to_tuple())))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(out/'contact_final.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print('CONTACT REFINED: hearth onto logs, forge upper opening')
