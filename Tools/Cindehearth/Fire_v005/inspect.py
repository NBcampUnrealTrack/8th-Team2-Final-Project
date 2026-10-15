import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Fire_v005';out=root/'verification';out.mkdir(parents=True,exist_ok=True)
assert not unreal.WidgetService.is_pie_running()
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert w.get_name()=='L_Cindehearth'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();rows=[]
for a in actors:
 n=a.get_actor_label()
 if not any(s in n for s in ('Fire','Flame','Hearth','Forge','LivingEmbers','D4_DorikSmithy')):continue
 if not isinstance(a,unreal.StaticMeshActor):continue
 c=a.static_mesh_component;r=a.get_actor_rotation()
 rows.append(dict(label=n,location=list(a.get_actor_location().to_tuple()),rotation=[r.pitch,r.yaw,r.roll],scale=list(a.get_actor_scale3d().to_tuple()),visible=c.is_visible(),hidden=a.get_editor_property('hidden'),component_hidden=c.get_editor_property('hidden_in_game'),collision=a.get_actor_enable_collision(),collision_profile=str(c.get_collision_profile_name()),tags=[str(t) for t in a.tags],mesh=c.static_mesh.get_path_name() if c.static_mesh else None,bounds=[list(v.to_tuple()) for v in a.get_actor_bounds(False)],materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]))
(out/'before.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
p='/Game/Cindehearth/HD2D_v002/Materials/M_CH_PixelFire'
(out/'original_material.json').write_text(unreal.MaterialNodeService.export_material_graph(p),encoding='utf-8')
for r in rows:
 if ('Fire' in r['label'] or 'Forge' in r['label']) and r['visible']:print(json.dumps(r))
