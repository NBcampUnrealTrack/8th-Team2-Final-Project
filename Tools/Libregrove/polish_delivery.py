"""Export final V3 single-asset source files via typed Blender MCP only."""
import hashlib,json,sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from polish_v3 import ASSETS,DOC,OUT,call,record,render

def write(p,v):
    assert not p.exists(),str(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2),encoding='utf-8')

def export(n):
    name=ASSETS[n]['id'];h=json.loads((DOC/(name+'_ACCEPTED.json')).read_text(encoding='utf-8'));label=f'V3_{name}_DELIVERY'
    before=call('precision','blender_precision_inspect',{'job_id':label+'_BEFORE','blend_path':h['output_blend'],'object_name':h['output_object'],'include_uv_overlap':True},label+'_BEFORE')
    assert before['report']['fingerprint']==h['fingerprint'] and before['uv_audit']['passed']
    compare=call('precision','blender_precision_surface_compare',{'job_id':label+'_COMPARE','blend_path':h['output_blend'],'object_name':h['output_object'],'reference_object_name':ASSETS[n]['output_object']},label+'_COMPARE')
    assert max(compare[d]['max_distance_m'] for d in ('reference_to_output','output_to_reference'))<=.02,(name,compare)
    clean=OUT/'clean'/(name+'.blend');clean.parent.mkdir(exist_ok=True);obj='SM_'+name
    spec={'blend_path':h['output_blend'],'object_name':h['output_object'],'expected_fingerprint':h['fingerprint'],'display_name':obj}
    assembly=call('precision','blender_assemble_clean_review',{'job_id':label+'_CLEAN','assets':[spec],'output_blend':str(clean),'layout':'origin'},label+'_CLEAN')
    checked=call('precision','blender_precision_inspect',{'job_id':label+'_REOPEN','blend_path':str(clean),'object_name':obj,'include_uv_overlap':True},label+'_REOPEN')
    for key in ('vertices','faces'):assert checked['topology'][key]==before['topology'][key]
    assert checked['uv_audit']==before['uv_audit'] and checked['report']['uv_layers']==before['report']['uv_layers']
    assert checked['report']['object']['location']==[0,0,0]
    exports={}
    for ext in ('glb','fbx'):
        dest=OUT.parent/'exports'/(name+'_TEXTURING_V3.'+ext)
        receipt=call('repair','blender_repair_export',{'job_id':label+'_EXPORT_'+ext,'blend_path':str(clean),'object_name':obj,'expected_fingerprint':checked['report']['fingerprint'],'output_path':str(dest)},label+'_EXPORT_'+ext)
        assert dest.is_file();exports[ext]=str(dest)
    after=call('precision','blender_precision_inspect',{'job_id':label+'_AFTER','blend_path':str(clean),'object_name':obj,'include_uv_overlap':False},label+'_AFTER')
    assert after['report']['fingerprint']==checked['report']['fingerprint']
    dims=checked['report']['object']['dimensions'];game_cm=[dims[1]*100,dims[0]*100,dims[2]*100]
    previous=ASSETS[n]['expected_width_depth_height_cm'];drift=max(abs(a-b) for a,b in zip(game_cm,previous));assert drift<=1.,(name,'Size drift',drift)
    result={**h,'clean_blend':str(clean),'clean_object':obj,'clean_fingerprint':checked['report']['fingerprint'],'exports':exports,'final_inspection':checked['report'],'final_uv_audit':checked['uv_audit'],'expected_width_depth_height_cm':game_cm,'v2_width_depth_height_cm':previous,'max_dimension_change_from_v2_cm':drift,'surface_comparison_to_v2':compare,'clean_reopen_exact_geometry_uv_match':True,'source_files_preserved':assembly['source_files_preserved'],'textures_authored':False}
    write(DOC/(name+'_FINAL.json'),result);print(json.dumps({'id':name,'exported_formats':list(exports),'clean_blend':str(clean),'triangles':h['geometry']['triangles']}),flush=True)

def fbx(n):
    name=ASSETS[n]['id'];h=json.loads((DOC/(name+'_FINAL.json')).read_text(encoding='utf-8'));p=Path(h['exports']['fbx']);label=f'V3_{name}_FBX_QA'
    r=call('precision','blender_precision_inspect_fbx',{'job_id':label,'fbx_path':str(p),'expected_sha256':hashlib.sha256(p.read_bytes()).hexdigest()},label)
    assert r['uv_audit']['passed'] and r['report']['geometry']['triangles']==h['geometry']['triangles']
    for key in ('non_manifold_edges','degenerate_faces','invalid_face_normals','non_finite_vertices','loose_vertices','loose_edges','zero_length_edges'):assert not r['report']['geometry'][key],(name,key)
    dims=r['report']['object']['dimensions'];w,d,z=h['expected_width_depth_height_cm'];error=max(abs(a-b) for a,b in zip(dims,[d/100,w/100,z/100]));assert error<1e-5
    r.update(triangle_count_matches=True,dimensions_match=True,max_dimension_error_m=error);write(DOC/(name+'_FINAL_FBX_QA.json'),r);print(json.dumps({'id':name,'fbx_passed':True}),flush=True)

def assemble():
    hs=[json.loads((DOC/(ASSETS[n]['id']+'_FINAL.json')).read_text(encoding='utf-8')) for n in range(1,10)]
    specs=[{'blend_path':h['clean_blend'],'object_name':h['clean_object'],'expected_fingerprint':h['clean_fingerprint'],'display_name':h['clean_object']} for h in hs]
    label='V3_REVIEW_ASSEMBLY_001';dest=OUT/'Libregrove_Texturing_V3.blend'
    r=call('precision','blender_assemble_clean_review',{'job_id':label,'assets':specs,'output_blend':str(dest),'layout':'grid'},label);write(DOC/'clean_assembly.json',r)
    def check(pair):
        i,h=pair;label=f'V3_{h["id"]}_GRID_QA'
        r=call('precision','blender_precision_inspect',{'job_id':label,'blend_path':str(dest),'object_name':h['clean_object'],'include_uv_overlap':True},label)
        source=record(f'V3_{h["id"]}_DELIVERY_REOPEN')
        assert r['topology']['vertices']==source['topology']['vertices'] and r['topology']['faces']==source['topology']['faces']
        assert r['uv_audit']==source['uv_audit'] and r['report']['uv_layers']==source['report']['uv_layers']
        assert r['report']['object']['location']==[(i%3)*8.,(i//3)*8.,0.]
        return {'id':h['id'],'local_geometry_and_uv_match':True,'display_location':r['report']['object']['location']}
    with ThreadPoolExecutor(max_workers=2) as pool:checks=list(pool.map(check,enumerate(hs)))
    write(DOC/'clean_assembly_checks.json',checks);print(json.dumps({'clean_reopened_assets':len(checks)}),flush=True)

def previews(n):
    name=ASSETS[n]['id'];h=json.loads((DOC/(name+'_FINAL.json')).read_text(encoding='utf-8'))
    for view in ('front_right','back_left'):
        label=f'V3_{name}_FINAL_{view}';dest=OUT/'views'/(label+'.png')
        call('repair','blender_mesh_review_render',{'job_id':label,'blend_path':h['clean_blend'],'object_name':h['clean_object'],'output_path':str(dest),'view':view,'mode':'clay'},label)

if __name__=='__main__':
    if sys.argv[1]=='assemble':assemble()
    else:
        fn={'export':export,'fbx':fbx,'render':previews}[sys.argv[1]]
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(fn,[int(v) for v in sys.argv[2:]]))
