"""Final, post-roundtrip-fix MCP delivery. Prior V2 candidates stay immutable."""
import json,sys
from concurrent.futures import ThreadPoolExecutor
from precision_workflow import DOC,OUT,EVIDENCE,call
from precision_stage import inspect
from precision_finish import render
from blender_mcp_client import Client

STAGES={1:'UV35',2:'UV35',3:'UV35',4:'UV35',5:'UV35',6:'UV27',7:'UV35',8:'UV35',9:'UV27'}
ASSETS={int(json.loads(p.read_text(encoding='utf-8'))['id'][3:5]):json.loads(p.read_text(encoding='utf-8'))['id'] for p in EVIDENCE.glob('review_handoff_*.json')}

def export(n):
    name=ASSETS[n];h=json.loads((DOC/(name+'_'+STAGES[n]+'.json')).read_text(encoding='utf-8'));assert h['uv_audit']['passed']
    before=inspect(h['output_blend'],h['output_object'],name+'_EXPORT_FINAL_BEFORE',uv=True);g=before['report']['geometry']
    assert before['uv_audit']['passed'];assert not any(r['glb_float32_uv_area']<=1e-12 for r in before['uv_export_precision']['risk_faces'])
    for f in ('non_manifold_edges','degenerate_faces','invalid_face_normals','non_finite_vertices','loose_vertices','loose_edges','zero_length_edges'):assert g[f]==0,(name,f)
    t=before['topology'];co=[tuple(sorted(tuple(t['vertices'][i]) for i in face)) for face in t['faces']];assert len(set(co))==len(co),'Coincident triangles remain'
    source=json.loads((EVIDENCE/('review_handoff_'+name+'.json')).read_text(encoding='utf-8'))
    compare=call('blender_precision_surface_compare',{'job_id':name+'_SURFACE_FINAL','blend_path':h['output_blend'],'object_name':h['output_object'],'reference_object_name':source['source_resized']},name+'_SURFACE_FINAL')
    c=Client('repair');exports={}
    try:
        for ext in ('glb','fbx'):
            dest=OUT/'exports'/(name+'_GAME_FINAL.'+ext)
            c.call('blender_repair_export',{'job_id':name+'_EXPORT_FINAL_'+ext,'blend_path':h['output_blend'],'object_name':h['output_object'],'expected_fingerprint':before['report']['fingerprint'],'output_path':str(dest)},name+'_EXPORT_FINAL_'+ext);exports[ext]=str(dest)
    finally:c.close()
    after=inspect(h['output_blend'],h['output_object'],name+'_EXPORT_FINAL_AFTER',uv=False);assert after['report']['fingerprint']==before['report']['fingerprint']
    r={**h,'final_inspection':before['report'],'final_uv_audit':before['uv_audit'],'surface_comparison':compare,'exports':exports,'expected_width_depth_height_cm':source['measured_width_depth_height_cm'],'coincident_triangle_count':0,'status':'MESH_UV_REPAIRED_NOT_ENGINE_OR_ART_SIGNOFF'}
    dest=DOC/(name+'_FINAL.json');assert not dest.exists();dest.write_text(json.dumps(r,indent=2),encoding='utf-8')
    print(json.dumps({'id':name,'triangles':g['triangles'],'vertices':g['vertices'],'exported':True}),flush=True)

def assemble():
    hs=[json.loads((DOC/(ASSETS[n]+'_FINAL.json')).read_text(encoding='utf-8')) for n in range(1,10)]
    for h in hs:
        assert json.loads((DOC/(h['id']+'_FINAL_GLB_QA.json')).read_text(encoding='utf-8'))['uv']['passed']
        assert json.loads((DOC/(h['id']+'_FINAL_FBX_QA.json')).read_text(encoding='utf-8'))['triangle_count_matches']
    specs=[{'blend_path':h['output_blend'],'object_name':h['output_object'],'expected_fingerprint':h['final_inspection']['fingerprint'],'display_name':'SM_'+h['id']} for h in hs]
    r=call('blender_assemble_clean_review',{'job_id':'LG_CLEAN_ASSEMBLY_FINAL_001','assets':specs,'output_blend':str(OUT/'Libregrove_GameMeshes_FINAL.blend'),'layout':'grid'},'precision_clean_assembly_final')
    (DOC/'clean_assembly_final.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print(json.dumps({'meshes':r['mesh_count'],'path':r['output_blend']}))

if __name__=='__main__':
    if sys.argv[1]=='assemble':assemble()
    elif sys.argv[1]=='export':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(export,[int(n) for n in sys.argv[2:]]))
    elif sys.argv[1]=='render':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(lambda v:render(*v),[(ASSETS[int(n)],STAGES[int(n)],view) for n in sys.argv[2:] for view in ('front_right','back_left')]))
