import json
from blender_mcp_client import Client,EVIDENCE
from finish_assets import recorded
c=Client();name='LG_04_Workshop'
try:
    source=next(x['name'] for x in recorded('workshop17_import')['objects'] if x['type']=='MESH')
    mesh=c.call('blender_mesh_inspect',{'object_name':source,'requirements':{'require_applied_scale':True}},'workshop_blocked_mesh')['result']
    uv=c.call('blender_uv_inspect',{'object_name':source},'workshop_blocked_uv')['result']
    assert not uv['uv_layers'] and mesh['geometry']['degenerate_faces']==0
    paths={}
    for ext in ('glb','fbx'):
        c.call('blender_mesh_inspect',{'object_name':source},'workshop_blocked_pre_export_'+ext)
        c.call('blender_model_export',{'object_names':[source],'path':'Libregrove_20260908/review_exports/'+name+'_REVIEW_NO_UV.'+ext,'overwrite':False},'workshop_blocked_export_'+ext)
        paths[ext]=recorded('workshop_blocked_export_'+ext)['path']
    dims=mesh['object']['dimensions']
    h={'id':name,'output_object':source,'source_resized':source,'triangulated':True,'status':'UV_BLOCKED_OPERATOR_VALIDATION_FAILED','mesh_report':mesh,'uv_report':uv,'measured_width_depth_height_cm':[dims[1]*100,dims[0]*100,dims[2]*100],'exports':paths,'blocker':'Smart Project rejected two degenerate UV faces even after triangulation and selective tiny-face cleanup; rejected output rolled back. No valid UV layer on delivered workshop.'}
    (EVIDENCE/('review_handoff_'+name+'.json')).write_text(json.dumps(h,indent=2),encoding='utf-8')
finally:c.close()
