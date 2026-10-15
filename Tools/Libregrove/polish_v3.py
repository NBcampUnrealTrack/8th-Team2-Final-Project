"""V3 controller: typed MCP only, immutable V2 inputs and fresh per-stage evidence."""
import hashlib,json,sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import blender_mcp_client as transport

ROOT=transport.ROOT
DOC=ROOT/'Docs/Libregrove/BlenderQA_V3_20260908'
EVIDENCE=DOC/'evidence'
OUT=Path('C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908/precision_v2/polish_v3')
PRIOR=ROOT/'Docs/Libregrove/BlenderQA_20260908/precision_v2'
DOC.mkdir(exist_ok=True);EVIDENCE.mkdir(exist_ok=True);OUT.mkdir(exist_ok=True)
transport.EVIDENCE=EVIDENCE
ASSETS={int(p.name[3:5]):json.loads(p.read_text(encoding='utf-8')) for p in PRIOR.glob('LG_*_FINAL.json')}

def call(kind,tool,args,label):
    c=transport.Client(kind)
    try:
        r=c.call(tool,args,label)
        return r.get('result',r)
    finally:c.close()

def record(label):
    r=json.loads((EVIDENCE/(label+'.json')).read_text(encoding='utf-8'))[-1]
    v=r.get('structuredContent')
    if v is None:v=json.loads(next(x['text'] for x in r['content'] if x['type']=='text'))
    assert not r.get('isError') and v.get('success',True) and v.get('state')=='finished',(label,v)
    return v['result']

def preflight():
    for kind in ('main','repair','precision'):
        c=transport.Client(kind)
        try:
            schema=c.rpc('tools/list',{});dest=DOC/(kind+'_tools.json');assert not dest.exists();dest.write_text(json.dumps(schema,indent=2),encoding='utf-8')
            if kind=='main':print(json.dumps(c.call('blender_bridge_status',{},'V3_bridge_status'),ensure_ascii=False))
        finally:c.close()
    checks=[]
    for n,h in sorted(ASSETS.items()):
        p=Path(h['output_blend']);sha=hashlib.sha256(p.read_bytes()).hexdigest();assert sha==h['surface_comparison']['input_sha256']
        checks.append({'id':h['id'],'path':str(p),'sha256':sha,'fingerprint':h['final_inspection']['fingerprint']})
    (DOC/'immutable_sources.json').write_text(json.dumps(checks,indent=2),encoding='utf-8');print(json.dumps({'source_hashes_match':len(checks)}))

def inspect(n,stage='BASE'):
    h=ASSETS[n] if stage=='BASE' else json.loads((DOC/(ASSETS[n]['id']+'_'+stage+'.json')).read_text(encoding='utf-8'))
    name=h['id'];label=f'V3_{name}_{stage}_INSPECT'
    r=call('precision','blender_precision_inspect',{'job_id':label,'blend_path':h['output_blend'],'object_name':h['output_object'],'include_uv_overlap':True},label)
    g=r['report']['geometry'];print(json.dumps({'id':name,'stage':stage,'geometry':g,'uv':r['uv_audit']['passed']}),flush=True)
    return r

def stage(n,source,output,tool,options,kind='precision'):
    h=ASSETS[n] if source=='BASE' else json.loads((DOC/(ASSETS[n]['id']+'_'+source+'.json')).read_text(encoding='utf-8'))
    name=h['id'];label=f'V3_{name}_{output}'
    before=call('precision','blender_precision_inspect',{'job_id':label+'_BEFORE','blend_path':h['output_blend'],'object_name':h['output_object'],'include_uv_overlap':False},label+'_BEFORE')
    args=options(before) if callable(options) else dict(options)
    dest=OUT/(name+'_'+output+'.blend');obj='V3_'+name+'_'+output
    r=call(kind,tool,{'job_id':label,'blend_path':h['output_blend'],'object_name':h['output_object'],'expected_fingerprint':before['report']['fingerprint'],'output_object_name':obj,'output_blend':str(dest),**args},label)
    after=call('precision','blender_precision_inspect',{'job_id':label+'_AFTER','blend_path':str(dest),'object_name':obj,'include_uv_overlap':True},label+'_AFTER')
    result={'id':name,'stage':output,'output_blend':str(dest),'output_object':obj,'source_blend':h['output_blend'],'source_object':h['output_object'],'before':before['report'],'after':after['report'],'uv_audit':after['uv_audit'],'uv_export_precision':after['uv_export_precision'],'changes':r['changes'],'source_preserved':r['source_preserved']}
    report=DOC/(name+'_'+output+'.json');assert not report.exists();report.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({'id':name,'stage':output,'geometry':after['report']['geometry'],'uv_passed':after['uv_audit']['passed']}),flush=True)
    return result

def render(n,source,view):
    h=ASSETS[n] if source=='BASE' else json.loads((DOC/(ASSETS[n]['id']+'_'+source+'.json')).read_text(encoding='utf-8'))
    label=f'V3_{h["id"]}_{source}_{view}';dest=OUT/'views'/(label+'.png');dest.parent.mkdir(exist_ok=True)
    return call('repair','blender_mesh_review_render',{'job_id':label,'blend_path':h['output_blend'],'object_name':h['output_object'],'output_path':str(dest),'view':view,'mode':'clay'},label)

if __name__=='__main__':
    if sys.argv[1]=='preflight':preflight()
    elif sys.argv[1]=='baseline':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(inspect,range(1,10)))
