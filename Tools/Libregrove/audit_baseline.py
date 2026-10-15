import json
from blender_mcp_client import Client, EVIDENCE, ROOT

client=Client()
try:
    source=json.loads((EVIDENCE/'002_baseline_scene.json').read_text(encoding='utf-8'))[-1]
    scene=source.get('structuredContent') or json.loads(source['content'][0]['text'])
    names=[o['name'] for o in scene['result']['objects'] if o['type']=='MESH']
    summaries=[]
    for name in names:
        mesh=client.call('blender_mesh_inspect',{'object_name':name},'baseline_'+name+'_mesh')
        uv=client.call('blender_uv_inspect',{'object_name':name},'baseline_'+name+'_uv')
        summaries.append({'name':name,'mesh':mesh,'uv':uv})
        print(json.dumps({'name':name,'mesh':mesh},ensure_ascii=False),flush=True)
    client.call('blender_scene_measure',{'queries':[{'id':n,'type':'world_bounds','object_name':n} for n in names]},'003_baseline_bounds')
    (EVIDENCE/'baseline_summary.json').write_text(json.dumps(summaries,ensure_ascii=False,indent=2),encoding='utf-8')
finally:
    client.close()
