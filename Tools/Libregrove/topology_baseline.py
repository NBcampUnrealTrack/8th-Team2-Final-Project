import json
import os
import sys
from concurrent.futures import ThreadPoolExecutor
from blender_mcp_client import Client, EVIDENCE

BASE=os.path.expandvars(r'%LOCALAPPDATA%/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908')
STAGE='reduced' if '--reduced' in sys.argv else 'base'
BLEND=BASE+('/stages/Libregrove_Reduced02.blend' if STAGE=='reduced' else '/source/Libregrove_SOURCE_before_QA.blend')

def inspect(name):
    client=Client('repair')
    try:
        result=client.call('blender_topology_inspect',{'job_id':'LG_'+STAGE+'_'+name,'blend_path':BLEND,'object_name':name},'topology_'+name)
        value=result['result']
        print(json.dumps({'name':name,'components':len(value['topology']['components']),'loops':len(value['topology']['boundary_loops'])}),flush=True)
    finally:client.close()

def render(name):
    client=Client('repair')
    try:
        args={'job_id':'LG_'+STAGE+'_render_'+name,'blend_path':BLEND,'object_name':name,'output_path':BASE+'/'+STAGE+'_views/'+name+'_front_right.png','view':'front_right','mode':'clay'}
        result=client.call('blender_mesh_review_render',args,'render_'+STAGE+'_'+name)
        print(json.dumps(result),flush=True)
    finally:client.close()

if __name__=='__main__':
    names=[x['name'] for x in json.loads((EVIDENCE/'baseline_summary.json').read_text(encoding='utf-8'))]
    if STAGE=='reduced':names=[n+'_Reduced02' for n in names]
    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(render if '--render' in sys.argv else inspect,names))
