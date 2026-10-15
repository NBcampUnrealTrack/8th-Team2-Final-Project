import json
from blender_mcp_client import Client,EVIDENCE
from finish_assets import recorded
name='LG_01_Cottage_A';c=Client()
try:
    source=name+'_Sized09';target='SM_'+name+'_UV_Strict12'
    c.call('blender_mesh_inspect',{'object_name':source},'strict_uv_pre_'+name)
    c.call('blender_uv_inspect',{'object_name':source},'strict_uv_before_'+name)
    c.call('blender_uv_edit',{'source_object_name':source,'output_object_name':target,'operation':'smart_project','uv_layer_name':'UV_Game','create_layer':True,'angle_limit_degrees':1,'island_margin':.002},'strict_uv_unwrap_'+name)
    c.call('blender_uv_inspect',{'object_name':target},'strict_uv_after_'+name)
    c.call('blender_mesh_inspect',{'object_name':target},'strict_uv_mesh_'+name)
    c.call('blender_model_export',{'object_names':[target],'path':'Libregrove_20260908/uv_trials/'+name+'_UV12.glb','overwrite':False},'strict_uv_export_'+name)
    h=json.loads((EVIDENCE/('final_handoff_'+name+'.json')).read_text(encoding='utf-8'));h['exports']['glb']=recorded('strict_uv_export_'+name)['path'];h['id']=name+'_UV12_trial'
    (EVIDENCE/'uv_trial_handoff.json').write_text(json.dumps(h,indent=2),encoding='utf-8')
finally:c.close()
