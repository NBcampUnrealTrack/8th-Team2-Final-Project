"""Saved-asset readback, no material compilation or gameplay mutation."""
import unreal,json,math
from pathlib import Path
dest=Path(unreal.Paths.project_dir()).resolve()/'Docs/Libregrove/Unreal_Textured_V7_20260908'
rows=json.loads((dest/'export_manifest.json').read_text(encoding='utf-8'))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Libregrove_AssetReview_V7'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
manager=unreal.InterchangeManager.get_interchange_manager_scripted()
assert not manager.is_interchange_active()
assert all(s['status']=='finished' for s in unreal._libregrove_v7_imports.values())
assert all(t.is_task_done() for t in unreal._libregrove_v7_screenshots.values())
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
review={a.get_actor_label():a for a in actors if a.get_actor_label().startswith('LGV7_')}
assert len(actors)==17 and len(review)==9
sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
results=[]
for row in rows:
    qa=json.loads((dest/'unreal_qa'/(row['id']+'.json')).read_text())
    mesh=unreal.load_asset(qa['path']);assert mesh
    dims=(mesh.get_bounding_box().max-mesh.get_bounding_box().min).to_tuple()
    assert max(abs(a-b) for a,b in zip(dims,row['dimensions_cm']))<.01
    assert mesh.get_num_triangles(0)==qa['triangles']==row['output_report']['geometry']['triangles']
    assert mesh.get_num_vertices(0)==qa['render_vertices']
    assert sub.get_num_uv_channels(mesh,0)==1
    slot=mesh.get_editor_property('static_materials');assert len(slot)==1
    mat=slot[0].material_interface;assert mat.get_path_name()==qa['materials'][0]['path']
    refs=[t.get_path_name() for t in unreal.MaterialEditingLibrary.get_material_used_textures(mat)]
    assert qa['textures'][0]['path'] in refs
    tex=unreal.load_asset(qa['textures'][0]['path']);assert tex.get_editor_property('srgb')
    assert [tex.blueprint_get_size_x(),tex.blueprint_get_size_y()]==qa['textures'][0]['size']
    a=review['LGV7_'+row['id']]
    assert a.static_mesh_component.static_mesh==mesh
    assert a.get_actor_scale3d().to_tuple()==(1,1,1)
    assert a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    results.append({'id':row['id'],'dimensions_cm':dims,'triangles':qa['triangles'],'render_vertices':qa['render_vertices'],'texture':tex.get_path_name(),'actor':a.get_path_name(),'passed':True})
result={'stage':STAGE,'level':world.get_path_name(),'actor_count':len(actors),'model_count':len(review),'interchange_active':False,'all_capture_tasks_done':True,'models':results,'passed':True,'scope':'Static mesh import, materials, source UV channel and game-scale dimensions. Not collision/interaction/LOD/cook/performance certification.'}
with (dest/('final_engine_audit_'+STAGE+'.json')).open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
print(json.dumps(result))
