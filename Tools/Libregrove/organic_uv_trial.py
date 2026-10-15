import json
from blender_mcp_client import Client,EVIDENCE
from finish_assets import recorded
from uv_export_qa import audit
c=Client()
try:
    for name in ['LG_06_NaiaShelter','LG_08_BroadleafTree']:
        source=name+'_Sized09';output='SM_'+name+'_UV_Organic18'
        c.call('blender_mesh_inspect',{'object_name':source},'organic_before_'+name)
        c.call('blender_uv_inspect',{'object_name':source},'organic_uv_before_'+name)
        c.call('blender_uv_edit',{'source_object_name':source,'output_object_name':output,'operation':'smart_project','uv_layer_name':'UV_Game','create_layer':True,'angle_limit_degrees':30,'island_margin':.004},'organic_unwrap_'+name)
        uv=c.call('blender_uv_inspect',{'object_name':output},'organic_uv_after_'+name)['result']
        mesh=c.call('blender_mesh_inspect',{'object_name':output,'requirements':{'require_uv':True,'require_applied_scale':True}},'organic_mesh_after_'+name)['result']
        c.call('blender_model_export',{'object_names':[output],'path':'Libregrove_20260908/uv_trials/'+name+'_Organic18.glb','overwrite':False},'organic_export_'+name)
        h=json.loads((EVIDENCE/('review_handoff_'+name+'.json')).read_text(encoding='utf-8'));h.update(output_object=output,uv_report=uv,mesh_report=mesh,uv_angle_limit=30,uv_margin=.004)
        h['exports']['glb']=recorded('organic_export_'+name)['path'];oldid=h['id'];h['id']=name+'_ORGANIC18'
        audit(h);h['id']=oldid
        (EVIDENCE/('organic_trial_'+name+'.json')).write_text(json.dumps(h,indent=2),encoding='utf-8')
finally:c.close()
