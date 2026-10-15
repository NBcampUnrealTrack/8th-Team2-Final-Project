"""MCP-verified, explicit export selection and clean .blend assembly."""
import json,sys
from concurrent.futures import ThreadPoolExecutor
from precision_workflow import DOC,OUT,EVIDENCE,call
from precision_stage import inspect
from precision_finish import render
from blender_mcp_client import Client

STAGES={1:'UV28',2:'UV27',3:'UV28',4:'UV33',5:'UV32',6:'UV27',7:'UV32',8:'UV28',9:'UV27'}
ASSETS={int(json.loads(p.read_text(encoding='utf-8'))['id'][3:5]):json.loads(p.read_text(encoding='utf-8'))['id'] for p in sorted(EVIDENCE.glob('review_handoff_*.json'))}

def export_one(number):
    name=ASSETS[number];stage=STAGES[number];h=json.loads((DOC/(name+'_'+stage+'.json')).read_text(encoding='utf-8'))
    assert h['uv_audit']['passed'],name+' UV not passed'
    before=inspect(h['output_blend'],h['output_object'],name+'_EXPORT_V2_BEFORE',uv=True)
    g=before['report']['geometry'];assert before['uv_audit']['passed']
    for field in ('non_manifold_edges','degenerate_faces','invalid_face_normals','non_finite_vertices','loose_vertices','loose_edges','zero_length_edges'):assert g[field]==0,(name,field,g[field])
    source=json.loads((EVIDENCE/('review_handoff_'+name+'.json')).read_text(encoding='utf-8'))
    compare=call('blender_precision_surface_compare',{'job_id':name+'_SURFACE_V2','blend_path':h['output_blend'],'object_name':h['output_object'],'reference_object_name':source['source_resized']},name+'_SURFACE_V2')
    c=Client('repair');exports={};(OUT/'exports').mkdir(exist_ok=True)
    try:
        for extension in ('glb','fbx'):
            dest=OUT/'exports'/(name+'_GAME_V2.'+extension)
            c.call('blender_repair_export',{'job_id':name+'_EXPORT_V2_'+extension,'blend_path':h['output_blend'],'object_name':h['output_object'],'expected_fingerprint':before['report']['fingerprint'],'output_path':str(dest)},name+'_EXPORT_V2_'+extension)
            exports[extension]=str(dest)
    finally:c.close()
    after=inspect(h['output_blend'],h['output_object'],name+'_EXPORT_V2_AFTER',uv=False)
    assert after['report']['fingerprint']==before['report']['fingerprint']
    result={**h,'final_inspection':before['report'],'final_uv_audit':before['uv_audit'],'surface_comparison':compare,'exports':exports,'expected_width_depth_height_cm':source['measured_width_depth_height_cm'],'status':'BLENDER_MESH_UV_VALIDATED_ENGINE_PERFORMANCE_NOT_TESTED'}
    dest=DOC/(name+'_DELIVERY.json');assert not dest.exists();dest.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({'id':name,'triangles':g['triangles'],'vertices':g['vertices'],'source_surface_max_m':compare['reference_to_output']['max_distance_m'],'exports':exports}),flush=True)

def assemble():
    hs=[json.loads((DOC/(ASSETS[n]+'_DELIVERY.json')).read_text(encoding='utf-8')) for n in range(1,10)]
    spec=[{'blend_path':h['output_blend'],'object_name':h['output_object'],'expected_fingerprint':h['final_inspection']['fingerprint'],'display_name':'SM_'+h['id']} for h in hs]
    result=call('blender_assemble_clean_review',{'job_id':'LG_CLEAN_ASSEMBLY_V2_001','assets':spec,'output_blend':str(OUT/'Libregrove_GameMeshes_V2.blend'),'layout':'grid'},'precision_clean_assembly_v2')
    (DOC/'clean_assembly.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({'mesh_count':result['mesh_count'],'path':result['output_blend']}))

if __name__=='__main__':
    if sys.argv[1]=='assemble':assemble()
    elif sys.argv[1]=='export':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(export_one,[int(n) for n in sys.argv[2:]]))
    elif sys.argv[1]=='render':
        jobs=[(ASSETS[int(n)],STAGES[int(n)],v) for n in sys.argv[2:] for v in ('front_right','back_left')]
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(lambda x:render(*x),jobs))
