import unreal
import json
from pathlib import Path
assert not unreal.WidgetService.is_pie_running()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Cindehearth'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
by_label={a.get_actor_label():a for a in actors}
d=next(a for a in actors if isinstance(a,unreal.NUCindehearthDirector))
assert d.get_editor_property('completed_chapter_quest')==0
assert not d.get_editor_property('night')
a=by_label['CH_GormHouse_P2']
c=a.static_mesh_component
m=c.static_mesh
center,extent=a.get_actor_bounds(False)
assert m.get_num_triangles(0)==3363
assert not m.get_editor_property('nanite_settings').enabled
assert all(abs(v-e)<.02 for v,e in zip((extent*2).to_tuple(),[315,270,260]))
assert abs(a.get_actor_rotation().yaw-180)<.01
assert c.get_editor_property('visible')
v=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Pipeline/verification'
shell=json.loads((v/'p2_replaced_shell.json').read_text(encoding='utf-8'))
rows=shell if isinstance(shell,list) else shell.get('actors',[])
assert len(rows)==12
for row in rows:
    assert not by_label[row['label']].static_mesh_component.get_editor_property('visible'),row['label']
walk=json.loads((v/'p2_walk.json').read_text(encoding='utf-8'))
assert walk['passed'] and walk['reached']==8
epilogue=json.loads((v/'p2_epilogue_runtime.json').read_text(encoding='utf-8'))
assert all(epilogue.values())
data=dict(map=world.get_path_name(),pie_running=False,chapter=0,night=False,actor=a.get_path_name(),mesh=m.get_path_name(),material=c.get_material(0).get_path_name(),location=list(a.get_actor_location().to_tuple()),yaw=a.get_actor_rotation().yaw,dimensions_cm=list((extent*2).to_tuple()),render_triangles=m.get_num_triangles(0),nanite=False,original_shell_hidden_count=12,sightline_infill_count=sum(x.get_actor_label().startswith('CH_GormStairInfill') for x in actors),supervisor_tool_parts=sum(x.get_actor_label().startswith('CH_GormP2_SupervisorTool') for x in actors),walk_waypoints_passed=8,epilogue=epilogue)
(v/'p2_unreal_signoff.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
print(json.dumps(data,indent=2))
