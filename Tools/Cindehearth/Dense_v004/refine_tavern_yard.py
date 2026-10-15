import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';out=root/'verification'
assert not (out/'tavern_yard_refined.json').exists() and not unreal.WidgetService.is_pie_running()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
casks=sorted([a for a in actors if a.get_actor_label().startswith('CH_TavernCask_') and a.get_actor_label().endswith('_P3')],key=lambda a:a.get_actor_label());assert len(casks)==14
# A compact storage bay against the inn's eastern side; retain the road shoulder.
positions=[(232,-1600,0),(306,-1608,0),(380,-1596,0),(228,-1680,0),(302,-1688,0),(376,-1676,0),(226,-1760,0),(300,-1768,0),(374,-1756,0),(232,-1600,1),(306,-1608,1),(228,-1680,1),(302,-1688,1),(226,-1840,0)]
rows=[]
with unreal.ScopedEditorTransaction('Tavern compact barrel storage bay'):
 for i,(a,(x,y,layer)) in enumerate(zip(casks,positions)):
  c,e=a.get_actor_bounds(False);old=a.get_actor_location();z=2*e.z*layer
  a.modify();a.set_actor_location(old+unreal.Vector(x-c.x,y-c.y,z-(c.z-e.z)),False,True)
  rows.append(dict(label=a.get_actor_label(),center_bottom=[x,y,z],layer=layer))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(out/'tavern_yard_refined.json').write_text(json.dumps(rows,indent=2));print('14barrels regrouped into compact storage bay')
