import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';out=root/'verification'
assert not unreal.WidgetService.is_pie_running();assert not (out/'tavern_walls_inset.json').exists()
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);a={x.get_actor_label():x for x in sub.get_all_level_actors()};old=a['CH_D4_FreyaTavern_Walls'];transform=old.get_actor_transform();mat=old.static_mesh_component.get_material(0);rows=[]
with unreal.ScopedEditorTransaction('Recess inn repair wall behind original door and frames'):
 for key in ['TavernWallsInset','TavernWallsFrame']:
  r=json.loads((out/('import_'+key+'.json')).read_text());assert r['status']=='finished';mesh=unreal.load_asset(r['meshes'][0]['path'])
  label='CH_D4_FreyaTavern_'+key;assert label not in a
  x=sub.spawn_actor_from_class(unreal.StaticMeshActor,transform.translation,transform.rotation.rotator());x.set_actor_label(label);x.set_folder_path('Cindehearth/Dense_v004/Architecture');x.tags=['Cindehearth','CH.HD2D','CH.Dense_v004']
  c=x.static_mesh_component;c.set_static_mesh(mesh);x.set_actor_scale3d(transform.scale3d)
  for i in range(c.get_num_materials()):c.set_material(i,mat)
  c.set_collision_profile_name('BlockAll');rows.append(dict(label=label,mesh=mesh.get_path_name()))
 old.modify();old.static_mesh_component.set_visibility(False,True);old.static_mesh_component.set_hidden_in_game(True,True);old.set_actor_hidden_in_game(True);old.set_is_temporarily_hidden_in_editor(True);old.set_actor_enable_collision(False)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(out/'tavern_walls_inset.json').write_text(json.dumps(dict(hidden=old.get_actor_label(),parts=rows,local_front_inset_m=.14,reason='Reveal original wooden door and first-floor frames'),indent=2));print('Inn original door and frames exposed by recessed repair wall')
