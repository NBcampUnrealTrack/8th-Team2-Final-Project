import unreal,json
from pathlib import Path
from collections import Counter
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003'
(root/'verification').mkdir(parents=True,exist_ok=True)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Cindehearth'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows=[]
for a in actors:
    r=dict(label=a.get_actor_label(),path=a.get_path_name(),class_name=a.get_class().get_name(),location=list(a.get_actor_location().to_tuple()),rotation=list(a.get_actor_rotation().to_tuple()),scale=list(a.get_actor_scale3d().to_tuple()),hidden=a.get_editor_property('hidden'),editor_hidden=a.is_temporarily_hidden_in_editor(),actor_collision=a.get_actor_enable_collision(),tags=[str(t) for t in a.tags])
    if isinstance(a,unreal.StaticMeshActor):
        c=a.static_mesh_component;m=c.static_mesh
        o,e=a.get_actor_bounds(False)
        r.update(mesh=m.get_path_name() if m else None,materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],visible=c.get_editor_property('visible'),component_hidden=c.get_editor_property('hidden_in_game'),collision=str(c.get_collision_profile_name()),bounds_center=list(o.to_tuple()),bounds_extent=list(e.to_tuple()))
    rows.append(r)
out=root/'verification/level_before.json';assert not out.exists()
out.write_text(json.dumps(rows,indent=2),encoding='utf-8')
visible=[r for r in rows if r.get('mesh') and r.get('visible') and not r['hidden'] and not r.get('component_hidden')]
print('TOTAL',len(rows),'VISIBLE_MESH',len(visible))
print('MESH_COUNTS',json.dumps(Counter(r['mesh'] for r in visible)))
