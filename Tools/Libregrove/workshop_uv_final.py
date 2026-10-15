import json
from blender_mcp_client import Client,EVIDENCE
from finish_assets import recorded
c=Client();name='LG_04_Workshop'
try:
    r=json.loads((EVIDENCE/'workshop_tri_repair.json').read_text(encoding='utf-8'))
    c.call('blender_scene_inspect',{},'workshop17_import_before')
    c.call('blender_model_import',{'path':r['export_path'],'name_prefix':'LGQATri04'},'workshop17_import')
    source=next(x['name'] for x in recorded('workshop17_import')['objects'] if x['type']=='MESH')
    before=c.call('blender_mesh_inspect',{'object_name':source},'workshop17_mesh_before')['result']
    assert before['geometry']['degenerate_faces']==0
    c.call('blender_uv_inspect',{'object_name':source},'workshop17_uv_before')
    output='SM_'+name+'_UV_QA17'
    c.call('blender_uv_edit',{'source_object_name':source,'output_object_name':output,'operation':'smart_project','uv_layer_name':'UV_Game','create_layer':True,'angle_limit_degrees':1,'island_margin':.002},'workshop17_uv_project')
    uv=c.call('blender_uv_inspect',{'object_name':output},'workshop17_uv_after')['result']
    mesh=c.call('blender_mesh_inspect',{'object_name':output,'requirements':{'require_uv':True,'require_applied_scale':True}},'workshop17_mesh_after')['result']
    exports={}
    for ext in ('glb','fbx'):
        c.call('blender_mesh_inspect',{'object_name':output},'workshop17_pre_export_'+ext)
        c.call('blender_model_export',{'object_names':[output],'path':'Libregrove_20260908/review_exports/'+name+'_REVIEW.'+ext,'overwrite':False},'workshop17_export_'+ext)
        exports[ext]=recorded('workshop17_export_'+ext)['path']
    dims=mesh['object']['dimensions']
    h={'id':name,'output_object':output,'source_resized':source,'triangulated':True,'status':'UV_UNWRAPPED_PENDING_EXACT_OVERLAP_QA','mesh_report':mesh,'uv_report':uv,'measured_width_depth_height_cm':[dims[1]*100,dims[0]*100,dims[2]*100],'exports':exports}
    (EVIDENCE/('review_handoff_'+name+'.json')).write_text(json.dumps(h,indent=2),encoding='utf-8')
    print(json.dumps({'name':name,'geometry':mesh['geometry'],'uv':uv}),flush=True)
finally:c.close()
