import json,os,sys
from concurrent.futures import ThreadPoolExecutor
from blender_mcp_client import Client,EVIDENCE
BASE=os.path.expandvars(r'%LOCALAPPDATA%/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908')

def render(args):
    c=Client('repair')
    try:
        result=c.call('blender_mesh_review_render',args,'review_'+args['job_id'])
        print(json.dumps(result),flush=True)
    finally:c.close()

if __name__=='__main__':
    first='--first' in sys.argv
    files=[EVIDENCE/'final_handoff_LG_01_Cottage_A.json'] if first else sorted(EVIDENCE.glob('final_handoff_*.json'))
    tasks=[]
    for file in files:
        h=json.loads(file.read_text(encoding='utf-8'));name=h['id']
        for view in (['front_right'] if first else ['front_left','back_right','top']):
            tasks.append({'job_id':name+'_UV11_'+view,'blend_path':BASE+'/stages/Libregrove_Prepared_'+('01' if first else 'ALL')+'.blend','object_name':h['output_object'],'output_path':BASE+'/final_views/'+name+'_'+view+'.png','view':view,'mode':'clay'})
    with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(render,tasks))
