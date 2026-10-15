import json
from blender_mcp_client import Client,EVIDENCE
from finish_assets import recorded

c=Client();results=[]
try:
    rows=json.loads((EVIDENCE/'all_resized.json').read_text(encoding='utf-8'))
    for row in rows:
        name=row['id'];source=row['object_name'];dest=EVIDENCE/('review_handoff_'+name+'.json')
        if dest.exists():results.append(json.loads(dest.read_text(encoding='utf-8')));continue
        if name=='LG_04_Workshop':
            repair=json.loads((EVIDENCE/'workshop_sized_repair.json').read_text(encoding='utf-8'))
            c.call('blender_scene_inspect',{},'sized_reimport_before_'+name)
            c.call('blender_model_import',{'path':repair['export_path'],'name_prefix':'LGQASized04'},'sized_reimport_'+name)
            imported=recorded('sized_reimport_'+name);source=next(x['name'] for x in imported['objects'] if x['type']=='MESH')
        before=c.call('blender_mesh_inspect',{'object_name':source},'uv16_before_'+name)['result']
        c.call('blender_uv_inspect',{'object_name':source},'uv16_layers_before_'+name)
        assert before['geometry']['degenerate_faces']==0
        output='SM_'+name+'_UV_QA16';triangulated=False
        try:
            c.call('blender_uv_edit',{'source_object_name':source,'output_object_name':output,'operation':'smart_project','uv_layer_name':'UV_Game','create_layer':True,'angle_limit_degrees':1,'island_margin':.002},'uv16_project_'+name)
        except RuntimeError as error:
            failure=json.loads(str(error))
            if failure.get('state')!='finished' or failure.get('error',{}).get('code')!='uv_validation_failed':raise
            # The typed bridge removes a rejected UV output. Verify before new work.
            scene=c.call('blender_scene_inspect',{'include_hidden':True},'uv16_failed_scene_'+name)['result']
            assert output not in [o['name'] for o in scene['objects']]
            c.call('blender_mesh_inspect',{'object_name':source},'uv16_tri_pre_'+name)
            tri=name+'_UVTri16'
            c.call('blender_modifier_apply',{'source_object_name':source,'output_object_name':tri,'modifier':{'type':'triangulate'}},'uv16_tri_'+name)
            trireport=c.call('blender_mesh_inspect',{'object_name':tri},'uv16_tri_post_'+name)['result']
            if trireport['geometry']['degenerate_faces']:
                results.append({'id':name,'output_object':source,'status':'GEOMETRY_REPAIR_REQUIRED','report':trireport});continue
            output='SM_'+name+'_UV_QA16_Tri';triangulated=True
            c.call('blender_uv_inspect',{'object_name':tri},'uv16_tri_uv_pre_'+name)
            c.call('blender_uv_edit',{'source_object_name':tri,'output_object_name':output,'operation':'smart_project','uv_layer_name':'UV_Game','create_layer':True,'angle_limit_degrees':1,'island_margin':.002},'uv16_tri_project_'+name)
        uv=c.call('blender_uv_inspect',{'object_name':output},'uv16_final_'+name)['result']
        mesh=c.call('blender_mesh_inspect',{'object_name':output,'requirements':{'require_uv':True,'require_applied_scale':True,'require_material':True}},'uv16_mesh_'+name)['result']
        assert mesh['quality']['passed']
        paths={}
        for extension in ('glb','fbx'):
            c.call('blender_mesh_inspect',{'object_name':output},'uv16_pre_export_'+extension+'_'+name)
            c.call('blender_model_export',{'object_names':[output],'path':'Libregrove_20260908/review_exports/'+name+'_REVIEW.'+extension,'overwrite':False},'uv16_export_'+extension+'_'+name)
            paths[extension]=recorded('uv16_export_'+extension+'_'+name)['path']
        item={'id':name,'output_object':output,'source_resized':source,'triangulated':triangulated,'status':'UV_UNWRAPPED_PENDING_EXACT_OVERLAP_QA','mesh_report':mesh,'uv_report':uv,'measured_width_depth_height_cm':[mesh['object']['dimensions'][1]*100,mesh['object']['dimensions'][0]*100,mesh['object']['dimensions'][2]*100],'exports':paths}
        dest.write_text(json.dumps(item,indent=2),encoding='utf-8');results.append(item)
        print(json.dumps({'id':name,'status':item['status'],'vertices':mesh['geometry']['vertices'],'triangles':mesh['geometry']['triangles']}),flush=True)
    (EVIDENCE/'reviewed_all.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    c.call('blender_scene_inspect',{},'review_master_pre_save')
    c.call('blender_save_copy',{'path':'Cindehearth/Libregrove_20260908/stages/Libregrove_UV_Review16.blend','overwrite':False},'review_master_save')
finally:c.close()
