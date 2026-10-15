import json
from blender_mcp_client import Client,EVIDENCE

client=Client()
try:
    names=[x['name'] for x in json.loads((EVIDENCE/'baseline_summary.json').read_text(encoding='utf-8'))]
    outputs=[]
    for name in names:
        before=client.call('blender_mesh_inspect',{'object_name':name},'pre_weld_'+name)['result']
        output=name+'_Weld01'
        client.call('blender_mesh_edit',{'source_object_name':name,'output_object_name':output,'operations':[{'type':'merge_by_distance','distance':0.00001}]},'weld_'+name)
        after=client.call('blender_mesh_inspect',{'object_name':output},'post_weld_'+name)['result']
        assert not after['geometry']['non_finite_vertices']
        outputs.append({'source':name,'output':output,'before':before['geometry'],'after':after['geometry']})
        print(json.dumps(outputs[-1]),flush=True)
    client.call('blender_save_copy',{'path':'Cindehearth/Libregrove_20260908/stages/Libregrove_Weld01.blend','overwrite':False},'006_weld_stage_save')
    (EVIDENCE/'weld_summary.json').write_text(json.dumps(outputs,indent=2),encoding='utf-8')
finally:client.close()
