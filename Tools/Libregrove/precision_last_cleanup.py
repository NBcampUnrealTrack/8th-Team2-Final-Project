import json,sys
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from precision_workflow import DOC,OUT,call
from precision_stage import inspect,stage
from blender_mcp_client import Client

def test():
    c=Client('precision')
    try:(DOC/'live_precision_tools_v2_final.json').write_text(json.dumps(c.rpc('tools/list',{}),indent=2),encoding='utf-8')
    finally:c.close()
    r=call('blender_precision_self_test',{'job_id':'LG_PRECISION_SELFTEST_005','output_blend':str(OUT/'TestFixtures_005.blend')},'precision_selftest_005');print(json.dumps(r))

def clean(n):
    path=next(DOC.glob(f'LG_{n:02}_*_DELIVERY.json'));h=json.loads(path.read_text(encoding='utf-8'));name=h['id']
    source=h['stage']
    if n==4:source='UV34';h=json.loads((DOC/(name+'_'+source+'.json')).read_text(encoding='utf-8'))
    r=inspect(h['output_blend'],h['output_object'],name+'_FINAL_CLEAN_INSPECT',uv=True)
    if n==4:
        ids=[i['face_id'] for i in r['uv_export_precision']['risk_faces']]
        stage(name,'UV35',source,{'tool':'blender_uv_sliver_guard_copy','face_ids':ids});return
    groups=defaultdict(list);t=r['topology'];v=t['vertices']
    for i,f in enumerate(t['faces']):groups[tuple(sorted(tuple(v[j]) for j in f))].append(i)
    pairs=[{'keep_face':g[0],'remove_face':i} for g in groups.values() if len(g)>1 for i in g[1:]]
    if pairs:stage(name,'UV35',source,{'tool':'blender_duplicate_faces_copy','pairs':pairs})
    else:print(json.dumps({'id':name,'no_duplicates':True}),flush=True)

if __name__=='__main__':
    if sys.argv[1]=='test':test()
    else:
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(clean,[int(n) for n in sys.argv[1:]]))
