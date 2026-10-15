import json,os
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from blender_mcp_client import Client,EVIDENCE
BASE=os.path.expandvars(r'%LOCALAPPDATA%/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908')

def review(task):
    c=Client('repair')
    try:
        result=c.call('blender_mesh_review_render',task,'latest_'+task['job_id'])
        print(json.dumps(result),flush=True)
    finally:c.close()

if __name__=='__main__':
    hs=[json.loads(p.read_text(encoding='utf-8')) for p in sorted(EVIDENCE.glob('review_handoff_*.json'))]
    tasks=[]
    (Path(BASE)/'review_views').mkdir(parents=True,exist_ok=True)
    for h in hs:
        for view in ('front_right','back_left'):
            output=BASE+'/review_views/'+h['id']+'_'+view+'.png'
            if Path(output).exists():continue
            tasks.append({'job_id':h['id']+'_QA18_'+view+'_R2','blend_path':BASE+'/Libregrove_ReviewWorkspace18.blend','object_name':h['output_object'],'output_path':output,'view':view,'mode':'clay'})
    with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(review,tasks))
