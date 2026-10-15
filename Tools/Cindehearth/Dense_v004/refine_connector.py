import unreal,json,sys
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
a=actors['CH_AshslopeConnection_Path'];base=next(r for r in json.loads((root/'verification/level_before.json').read_text()) if r['label']==a.get_actor_label());a.modify();a.set_actor_scale3d(unreal.Vector(base['scale'][0]*.74,base['scale'][1]*.67,base['scale'][2]))
# Rebuild only the affected plan from the final, compressed bridge bounds.
source=(root/'dress_connections.py').read_text(encoding='utf-8').split("if '--offline-preview'")[0]
ns={};exec(source,ns)
bounds={n:tuple(v.to_tuple() for v in a.get_actor_bounds(False)) for n,a in actors.items() if isinstance(a,unreal.StaticMeshActor)}
plan=ns['build_plan'](bounds);before=json.loads((root/'verification/connections_applied.json').read_text())
old={r['label']:r for r in before['placements']}
for row in plan:
 if row['group']!='Ashslope':continue
 a=actors[row['label']];p=a.get_actor_location();delta=[row['center_bottom'][i]-old[row['label']]['center_bottom'][i] for i in range(3)]
 a.modify();a.set_actor_location(p+unreal.Vector(*delta),False,True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(root/'verification/connector_refinement.json').write_text(json.dumps(dict(bridge_bounds=bounds['CH_AshslopeConnection_Path'],placements=[r for r in plan if r['group']=='Ashslope']),indent=2),encoding='utf-8')
print('Bridge geometry and12 side details aligned')
