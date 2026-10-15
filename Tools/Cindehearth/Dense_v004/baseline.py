import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Tools/Cindehearth/Dense_v004/verification';out.mkdir(parents=True,exist_ok=True)
assert not unreal.WidgetService.is_pie_running();assert not (out/'level_before.json').exists()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
backup=root/'Saved/CindehearthDenseBackup/20260905_v004';backup.mkdir(parents=True,exist_ok=False)
shutil.copy2(root/'Content/Maps/L_Cindehearth.umap',backup/'L_Cindehearth.umap')
rows=[];actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for a in actors:
 r=dict(label=a.get_actor_label(),path=a.get_path_name(),class_name=a.get_class().get_name(),location=list(a.get_actor_location().to_tuple()),rotation=list(a.get_actor_rotation().to_tuple()),scale=list(a.get_actor_scale3d().to_tuple()),hidden=a.get_editor_property('hidden'),editor_hidden=a.is_temporarily_hidden_in_editor(),actor_collision=a.get_actor_enable_collision(),tags=[str(t) for t in a.tags])
 if isinstance(a,unreal.StaticMeshActor):
  c=a.static_mesh_component;m=c.static_mesh;o,e=a.get_actor_bounds(False)
  r.update(mesh=m.get_path_name() if m else None,materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],visible=c.get_editor_property('visible'),component_hidden=c.get_editor_property('hidden_in_game'),collision=str(c.get_collision_profile_name()),bounds_center=list(o.to_tuple()),bounds_extent=list(e.to_tuple()))
 rows.append(r)
 if isinstance(a,unreal.NUCindehearthDirector):
  route=[dict(location=list(t.translation.to_tuple()),rotation=list(t.rotation.rotator().to_tuple()),scale=list(t.scale3d.to_tuple())) for t in a.get_editor_property('epilogue_camera_route')]
  (out/'director_before.json').write_text(json.dumps(dict(route=route),indent=2),encoding='utf-8')
(out/'level_before.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print('BASELINE',len(rows),str(backup))
