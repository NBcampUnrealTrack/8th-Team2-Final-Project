import json
from blender_mcp_client import Client,EVIDENCE,ROOT

client=Client()
try:
    assets=json.loads((ROOT/'Docs/Libregrove/model_budgets.json').read_text(encoding='utf-8'))['assets']
    outputs=[]
    for asset in assets:
        name=asset['id']; source=name+'_Weld01'; output=name+'_Reduced02'
        before=client.call('blender_mesh_inspect',{'object_name':source},'pre_reduce_'+name)['result']
        tris=before['geometry']['triangles']; ratio=min(1,asset['triangle_ceiling']*.95/tris)
        if ratio<1:
            args={'source_object_name':source,'output_object_name':output,'modifier':{'type':'decimate','ratio':ratio}}
            client.call('blender_modifier_apply',args,'reduce_'+name)
        else:
            client.call('blender_mesh_edit',{'source_object_name':source,'output_object_name':output,'operations':[{'type':'delete_loose'}]},'reduce_'+name)
        after=client.call('blender_mesh_inspect',{'object_name':output},'post_reduce_'+name)['result']
        outputs.append({'source':source,'output':output,'ratio':ratio,'before':before['geometry'],'after':after['geometry']})
        print(json.dumps(outputs[-1]),flush=True)
    client.call('blender_save_copy',{'path':'Cindehearth/Libregrove_20260908/stages/Libregrove_Reduced02.blend','overwrite':False},'007_reduced_stage_save')
    (EVIDENCE/'reduce_summary.json').write_text(json.dumps(outputs,indent=2),encoding='utf-8')
finally:client.close()
