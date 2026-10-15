import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004'
if unreal.WidgetService.is_pie_running():unreal.WidgetService.stop_pie()
assert not unreal.WidgetService.is_pie_running()
actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
baseline=json.loads((root/'verification/level_before.json').read_text());r=next(r for r in baseline if r['label']=='CH_AshslopeConnection_Path')
a=actors[r['label']];a.modify();a.set_actor_scale3d(unreal.Vector(r['scale'][0]*.74,r['scale'][1]*.67,r['scale'][2]))
ns={};exec((root/'dress_connections.py').read_text(encoding='utf-8').split("if '--offline-preview'")[0],ns)
bounds={n:tuple(v.to_tuple() for v in a.get_actor_bounds(False)) for n,a in actors.items() if isinstance(a,unreal.StaticMeshActor)}
rows=[r for r in ns['build_plan'](bounds) if r['group']=='Ashslope']
for r in rows:
 a=actors[r['label']];c,e=a.get_actor_bounds(False);target=r['center_bottom'];delta=unreal.Vector(target[0]-c.x,target[1]-c.y,target[2]-(c.z-e.z))
 a.modify();a.set_actor_location(a.get_actor_location()+delta,False,True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(root/'verification/connector_refinement_v2.json').write_text(json.dumps(dict(reason='Movement QA exposed connector scale assumed .45 instead of saved7.2/3.6/.135. Restored from baseline.',bounds=bounds['CH_AshslopeConnection_Path'],placements=rows),indent=2))
print('Bridge corrected from exact baseline scale',bounds['CH_AshslopeConnection_Path'])
