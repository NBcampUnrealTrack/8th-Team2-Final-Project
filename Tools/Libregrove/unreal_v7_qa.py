"""Read-only Unreal mesh/material/texture audit; never forces material recompilation."""
import unreal,json,math
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();dest=root/'Docs/Libregrove/Unreal_Textured_V7_20260908'
rows=json.loads((dest/'export_manifest.json').read_text(encoding='utf-8'))
sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for row in rows:
    if ASSET_ID!='all' and row['id']!=ASSET_ID:continue
    receipt=json.loads((dest/'unreal_jobs'/row['id']/'result.json').read_text(encoding='utf-8'));assert receipt['status']=='finished'
    obs=[unreal.load_asset(x['path']) for x in receipt['objects']];assert all(obs)
    meshes=[o for o in obs if isinstance(o,unreal.StaticMesh)];textures=[o for o in obs if isinstance(o,unreal.Texture2D)]
    assert len(meshes)==1 and len(textures)==1
    mesh=meshes[0];box=mesh.get_bounding_box();dims=(box.max-box.min).to_tuple()
    assert all(math.isfinite(v) for v in dims)
    assert max(abs(a-b) for a,b in zip(sorted(dims),sorted(row['dimensions_cm'])))<.01
    assert abs(dims[2]-row['dimensions_cm'][2])<.01
    assert abs(box.min.z)<.01
    slots=mesh.get_editor_property('static_materials');assert len(slots)==1
    materials=[]
    for slot in slots:
        mat=slot.material_interface;assert mat
        used=unreal.MaterialEditingLibrary.get_material_used_textures(mat)
        assert {t.get_path_name() for t in textures}<={t.get_path_name() for t in used}
        graph=json.loads(unreal.MaterialNodeService.export_material_graph(mat.get_path_name()))
        materials.append({'path':mat.get_path_name(),'referenced_textures':[t.get_path_name() for t in used],'graph':graph})
    tex=[]
    for t in textures:
        assert t.get_editor_property('srgb')
        tex.append({'path':t.get_path_name(),'size':[t.blueprint_get_size_x(),t.blueprint_get_size_y()],'srgb':t.get_editor_property('srgb'),'compression':str(t.get_editor_property('compression_settings')),'filter':str(t.get_editor_property('filter'))})
    build=sub.get_lod_build_settings(mesh,0);nanite=sub.get_nanite_settings(mesh)
    assert not nanite.enabled and build.use_full_precision_u_vs
    assert sub.get_num_uv_channels(mesh,0)==1
    tris=mesh.get_num_triangles(0);expected=row['output_report']['geometry']['triangles']
    assert 0<=expected-tris<=max(3,expected*.001),(row['id'],expected,tris)
    report={'id':row['id'],'path':mesh.get_path_name(),'dimensions_cm_xyz':dims,'bounds_min':box.min.to_tuple(),'bounds_max':box.max.to_tuple(),'triangles':tris,'source_triangles':expected,'removed_degenerate_triangles':expected-tris,'render_vertices':mesh.get_num_vertices(0),'uv_channels':sub.get_num_uv_channels(mesh,0),'full_precision_uv':bool(build.use_full_precision_u_vs),'nanite':bool(nanite.enabled),'materials':materials,'textures':tex,'collision_shapes':sub.get_simple_collision_count(mesh),'collision_complexity':str(sub.get_collision_complexity(mesh)),'passed':True}
    p=dest/'unreal_qa'/(row['id']+'.json');p.parent.mkdir(parents=True,exist_ok=True)
    with p.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
    print(json.dumps({k:v for k,v in report.items() if k!='materials'}))
