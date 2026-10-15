import json
from blender_mcp_client import Client,EVIDENCE
from finish_assets import recorded
name='LG_01_Cottage_A';c=Client()
try:
    source=name+'_Sized09';tri=name+'_Tri13';target='SM_'+name+'_UV_Tri14'
    c.call('blender_mesh_inspect',{'object_name':source},'tri_uv_pre_'+name)
    c.call('blender_modifier_apply',{'source_object_name':source,'output_object_name':tri,'modifier':{'type':'triangulate'}},'tri_uv_triangulate_'+name)
    c.call('blender_mesh_inspect',{'object_name':tri},'tri_uv_mesh_before_'+name)
    c.call('blender_uv_inspect',{'object_name':tri},'tri_uv_before_'+name)
    c.call('blender_uv_edit',{'source_object_name':tri,'output_object_name':target,'operation':'smart_project','uv_layer_name':'UV_Game','create_layer':True,'angle_limit_degrees':1,'island_margin':.002},'tri_uv_unwrap_'+name)
    c.call('blender_uv_inspect',{'object_name':target},'tri_uv_after_'+name)
    c.call('blender_mesh_inspect',{'object_name':target},'tri_uv_mesh_'+name)
    c.call('blender_model_export',{'object_names':[target],'path':'Libregrove_20260908/uv_trials/'+name+'_UV14.glb','overwrite':False},'tri_uv_export_'+name)
    h=json.loads((EVIDENCE/('final_handoff_'+name+'.json')).read_text(encoding='utf-8'));h['exports']['glb']=recorded('tri_uv_export_'+name)['path'];h['id']=name+'_UV14_trial'
    (EVIDENCE/'uv_tri_trial_handoff.json').write_text(json.dumps(h,indent=2),encoding='utf-8')
finally:c.close()
