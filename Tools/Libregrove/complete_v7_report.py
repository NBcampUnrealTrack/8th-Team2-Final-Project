"""Finish local reports from persisted final-save/readback receipts; no reimport."""
import json,hashlib,shutil,csv,collections
import textured_handoff as h

rows=json.loads((h.DEST/'export_manifest.json').read_text(encoding='utf-8'))
audit=json.loads((h.DEST/'final_engine_audit_saved.json').read_text())
assert audit['passed'] and len(audit['models'])==9
context=h.unwrap(json.loads((h.DEST/'evidence/V7_final_after_save_result.json').read_text()))
assert not context['level']['dirty']
baseline=json.loads((h.DEST/'blender_scene.json').read_text())
scene=h.unwrap(json.loads((h.DEST/'evidence/V7_final_blender_scene.json').read_text())[1])
lookup={o['name']:o for o in scene['objects']}
diffs={o['name']:{k:[v,lookup[o['name']][k]] for k,v in o.items() if v!=lookup[o['name']][k]} for o in baseline['objects']}
assert all(set(d)<= {'hidden','selected'} for d in diffs.values())
h.save('final_source_preservation.json',{'original_geometry_transform_fingerprints_preserved':9,'live_mesh_objects':len(scene['objects']),'ui_differences':diffs,'active_object':[baseline['active_object'],scene['active_object']],'passed':True,'note':'Visibility/selection is not identical. No typed restoration operation was available; source mesh data was not changed.'})
backup=json.loads((h.DEST/'backup_manifest.json').read_text())
assert hashlib.sha256(h.Path(backup['original_level']).read_bytes()).hexdigest()==backup['original_level_sha256']
working=h.EXCHANGE/'Libregrove_Textured_V7_WorkingCopy.blend'
local=h.DEST/'Libregrove_Textured_V7_WorkingCopy.blend';assert not local.exists();shutil.copy2(working,local)
objects=[];csvrows=[]
sources={s['object']['name']:s for s in json.loads((h.DEST/'source_meshes.json').read_text())}
for i,row in enumerate(rows):
    receipt=json.loads((h.DEST/'unreal_jobs'/row['id']/'result.json').read_text());assert receipt['status']=='finished'
    for n,obj in enumerate(receipt['objects']):
        saved=h.unwrap(json.loads((h.DEST/f'evidence/V7_FINAL_SAVE_{row["id"]}_{n:02d}_result.json').read_text()))
        assert saved['success'];objects.append(obj)
    assert hashlib.sha256(h.Path(row['glb_path']).read_bytes()).hexdigest()==row['sha256']
    qa=json.loads((h.DEST/'unreal_qa'/(row['id']+'.json')).read_text());g=sources[row['source_name']]['geometry'];uv=sources[row['source_name']]['uv_layers'][0]
    csvrows.append({'id':row['id'],'name_ko':row['name_ko'],'x_cm':round(qa['dimensions_cm_xyz'][0],4),'y_cm':round(qa['dimensions_cm_xyz'][1],4),'z_cm':round(qa['dimensions_cm_xyz'][2],4),'source_mesh_vertices':g['vertices'],'recommended_mesh_vertex_max':row['recommended_vertex_max'],'ue_render_vertices':qa['render_vertices'],'triangles':qa['triangles'],'initial_triangle_budget':row['initial_triangle_budget'],'above_recommended_budget':g['vertices']>row['recommended_vertex_max'],'basecolor_resolution':'x'.join(map(str,qa['textures'][0]['size'])),'source_boundary_edges':g['boundary_edges'],'source_nonmanifold_edges':g['non_manifold_edges'],'source_degenerate_faces':g['degenerate_faces'],'source_uv_degenerate_faces':uv['degenerate_faces'],'ue_collision_shapes':qa['collision_shapes'],'uv_channels':qa['uv_channels'],'engine_import_qa_pass':qa['passed']})
objects.append({'path':context['level']['path'],'class':'World'})
files=[]
for obj in objects:
    path=h.ROOT/'Content'/(obj['path'].split('.')[0].removeprefix('/Game/')+('.umap' if obj['class']=='World' else '.uasset'))
    assert path.is_file() and path.stat().st_size>0
    files.append({'asset_path':obj['path'],'class':obj['class'],'file':str(path),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
assert collections.Counter(o['class'] for o in files)=={'StaticMesh':9,'Texture2D':9,'Material':9,'World':1}
h.save('saved_content_manifest.json',{'files':files,'original_level_unchanged':True,'source_backup_sha256':backup['blend_copy_sha256'],'working_blend_sha256':hashlib.sha256(local.read_bytes()).hexdigest(),'passed':True})
with (h.DEST/'model_audit.csv').open('x',encoding='utf-8-sig',newline='') as f:
    w=csv.DictWriter(f,fieldnames=csvrows[0].keys());w.writeheader();w.writerows(csvrows)
previews=[]
for i,row in enumerate(rows):
    for angle in ['Front','Back']:
        name=row['id']+'_'+angle+('Final' if i<4 else 'Review')
        p=h.DEST/'Preview'/(name+'.png');receipt=json.loads((h.DEST/'Preview'/(name+'_receipt.json')).read_text())
        assert hashlib.sha256(p.read_bytes()).hexdigest()==receipt['sha256'] and receipt['native_task']['output']=='True'
        previews.append({'id':row['id'],'angle':angle,'file':str(p),'sha256':receipt['sha256']})
for angle in ['Top','Oblique']:
    name='Overview_'+angle;p=h.DEST/'Preview'/(name+'.png')
    receipt=json.loads((h.DEST/'Preview'/(name+'_receipt.json')).read_text());assert hashlib.sha256(p.read_bytes()).hexdigest()==receipt['sha256']
    previews.append({'id':'Overview','angle':angle,'file':str(p),'sha256':receipt['sha256']})
h.save('preview_manifest.json',previews)
summary={'status':'STATIC_IMPORT_SAVED_AND_VERIFIED_WITH_SOURCE_QUALITY_WARNINGS','models':9,'materials':9,'basecolor_textures':9,'review_maps':1,'source_mesh_vertices':sum(x['source_mesh_vertices'] for x in csvrows),'ue_render_vertices':sum(x['ue_render_vertices'] for x in csvrows),'triangles':sum(x['triangles'] for x in csvrows),'above_initial_budget':sum(x['above_recommended_budget'] for x in csvrows),'static_import_numeric_qa':True,'original_mesh_data_preserved':True,'original_blender_ui_preserved':False,'original_village_level_unchanged':True,'native_preview_files':20,'review_level':context['level']['path'],'scale_reference':'Project game cm, 88cm character. Not adult human architecture.','not_implemented':['Door articulation/interaction','Collision','LOD','Lightmap UV','Full village layout','Topology/UV repair or texture repainting','Cook/performance certification'],'warnings':['Source boundaries/nonmanifold/degenerate UV remain; see CSV','Visible roof faceting/gaps/self-shadow roughness; not fully diagnosed or repaired','Some back textures have less detail than front, including Naia bark and cottage ghost-door pattern','Source9 are merged Tripo static meshes, not V6 hinged-door simplified buildings'],'source_preservation_evidence':'final_source_preservation.json','save_evidence':'saved_content_manifest.json','engine_evidence':'final_engine_audit_saved.json'}
h.save('final_signoff.json',summary)
print(json.dumps(summary,ensure_ascii=True,indent=2))
