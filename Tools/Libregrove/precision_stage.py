"""Stage-by-stage typed MCP operations, exact receipts and new output files."""
import json,sys
from concurrent.futures import ThreadPoolExecutor
from precision_workflow import BASE,OUT,DOC,EVIDENCE,record,call

def inspect(path,obj,label,uv=False):
    return call('blender_precision_inspect',{'job_id':label,'blend_path':str(path),'object_name':obj,'include_uv_overlap':uv},label)

def stage(name,operation,source_stage,options):
    if source_stage=='sized':
        h=json.loads((EVIDENCE/('review_handoff_'+name+'.json')).read_text(encoding='utf-8'))
        path=BASE/'Libregrove_ReviewWorkspace18.blend';obj=h['source_resized']
    else:
        h=json.loads((DOC/(name+'_'+source_stage+'.json')).read_text(encoding='utf-8'))
        path=h['output_blend'];obj=h['output_object']
    label=name+'_'+operation
    before=inspect(path,obj,label+'_before')
    args=options(before) if callable(options) else dict(options)
    out_name=name+'_'+operation;out_path=OUT/(out_name+'.blend')
    common={'job_id':label,'blend_path':str(path),'object_name':obj,'expected_fingerprint':before['report']['fingerprint'],'output_object_name':out_name,'output_blend':str(out_path)}
    tool=args.pop('tool')
    r=call(tool,{**common,**args},label)
    after=inspect(out_path,out_name,label+'_after',uv=operation.startswith('UV'))
    result={'id':name,'stage':operation,'source_blend':str(path),'source_object':obj,'output_blend':str(out_path),'output_object':out_name,'before':before['report'],'after':after['report'],'uv_audit':after['uv_audit'],'changes':r['changes'],'source_preserved':r['source_preserved']}
    dest=DOC/(label+'.json');assert not dest.exists();dest.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({'id':name,'stage':operation,'geometry':after['report']['geometry'],'uv':{k:v for k,v in after['uv_audit'].items() if k!='overlap_details'}}),flush=True)
    return result

def prepare(name):
    # Previous sizing copies predate UV fragmentation, except the workshop whose
    # source_resized points at its proven degenerate-free triangulated repair.
    stage(name,'Planar21','sized',lambda r:{'tool':'blender_planar_dissolve_copy','face_ids':list(range(len(r['topology']['faces']))),'angle_degrees':.2})
    def split(r):return {'tool':'blender_topology_split_copy','edge_ids':[e['id'] for e in r['topology']['edges'] if len(e['face_ids'])>2]}
    after=record(name+'_Planar21_after')
    if after['report']['geometry']['non_manifold_edges']:
        stage(name,'Split22','Planar21',split)
    else:
        h=json.loads((DOC/(name+'_Planar21.json')).read_text(encoding='utf-8'));(DOC/(name+'_Split22.json')).write_text(json.dumps(h,indent=2),encoding='utf-8')

if __name__=='__main__':
    names=[json.loads(p.read_text(encoding='utf-8'))['id'] for p in sorted(EVIDENCE.glob('review_handoff_*.json'))]
    if sys.argv[2:]:names=[n for n in names if int(n[3:5]) in [int(x) for x in sys.argv[2:]]]
    assert sys.argv[1]=='prepare'
    with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(prepare,names))
