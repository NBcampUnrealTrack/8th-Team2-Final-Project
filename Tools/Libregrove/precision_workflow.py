"""MCP-only precision workflow and durable evidence. No Blender code execution."""
import json,sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from blender_mcp_client import Client,EVIDENCE,ROOT

BASE=Path('C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908')
OUT=BASE/'precision_v2';DOC=ROOT/'Docs/Libregrove/BlenderQA_20260908/precision_v2'
OUT.mkdir(exist_ok=True);DOC.mkdir(exist_ok=True)

def record(name):
    history=json.loads((EVIDENCE/(name+'.json')).read_text(encoding='utf-8'))
    r=history[-1];v=r.get('structuredContent')
    if v is None:v=json.loads(next(x['text'] for x in r['content'] if x['type']=='text'))
    assert not r.get('isError') and v.get('success',True) and v.get('state')=='finished',(name,v)
    return v['result']

def call(tool,args,label):
    c=Client('precision')
    try:return c.call(tool,args,label)['result']
    finally:c.close()

def preflight():
    from precision_mcp import ids_checked,finite_range,identifier,path_checked
    bad=[lambda:ids_checked([1,1]),lambda:ids_checked([-1]),lambda:finite_range(float('nan'),0,1),lambda:identifier('../bad'),lambda:path_checked(str(BASE.parent/'outside.blend'))]
    rejected=0
    for test in bad:
        try:test()
        except (ValueError,AssertionError):rejected+=1
    assert rejected==len(bad)
    c=Client('precision')
    try:
        schema=c.rpc('tools/list',{});dest=DOC/'live_precision_tools_v2.json'
        assert not dest.exists();dest.write_text(json.dumps(schema,indent=2),encoding='utf-8')
        result=c.call('blender_precision_self_test',{'job_id':'LG_PRECISION_SELFTEST_001','output_blend':str(OUT/'TestFixtures_001.blend')},'precision_selftest_001')
        print(json.dumps({'validation_negative_tests_passed':rejected,'self_test':result},ensure_ascii=False))
    finally:c.close()
    c=Client()
    try:
        tools=c.rpc('tools/list',{});(DOC/'live_main_tools_v2.json').write_text(json.dumps(tools,indent=2),encoding='utf-8')
        print(json.dumps(c.call('blender_bridge_status',{},'precision_main_status_v2'),ensure_ascii=False))
    finally:c.close()

def baseline_one(h):
    name=h['id'];args={'job_id':name+'_PRECISION_BASE20','blend_path':str(BASE/'Libregrove_ReviewWorkspace18.blend'),'object_name':h['output_object'],'include_uv_overlap':False}
    r=call('blender_precision_inspect',args,'precision_base_'+name)
    print(json.dumps({'id':name,'vertices':r['report']['geometry']['vertices'],'faces':r['report']['geometry']['polygons'],'nonmanifold':r['report']['geometry']['non_manifold_edges'],'components':len(r['topology']['components'])}),flush=True)

if __name__=='__main__':
    if sys.argv[1]=='preflight':preflight()
    elif sys.argv[1]=='baseline':
        hs=[json.loads(p.read_text(encoding='utf-8')) for p in sorted(EVIDENCE.glob('review_handoff_*.json'))]
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(baseline_one,hs))
