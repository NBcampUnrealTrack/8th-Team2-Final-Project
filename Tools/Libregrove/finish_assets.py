"""Resize, unwrap, pack and export inspected repaired assets through typed MCP."""
import json,sys
from blender_mcp_client import Client,EVIDENCE,ROOT

def recorded(name):
    raw=json.loads((EVIDENCE/(name+'.json')).read_text(encoding='utf-8'))[-1]
    value=raw.get('structuredContent') or json.loads(next(x['text'] for x in raw['content'] if x['type']=='text'))
    result=value['result']
    return result['result'] if 'job_id' in result else result

def finish(asset,c):
    name=asset['id'];resultfile=EVIDENCE/('final_handoff_'+name+'.json')
    if resultfile.exists():print('ALREADY PREPARED '+name,flush=True);return
    h=json.loads((EVIDENCE/('repair_handoff_'+name+'.json')).read_text(encoding='utf-8'))
    if (EVIDENCE/('import_'+name+'.json')).exists():imported=recorded('import_'+name)
    else:
        c.call('blender_scene_inspect',{},'pre_import_'+name)
        c.call('blender_model_import',{'path':h['export_path'],'name_prefix':'LGQA'+name[3:5]+'_'},'import_'+name)
        imported=recorded('import_'+name)
        c.call('blender_scene_inspect',{},'post_import_'+name)
    meshes=[x['name'] for x in imported['objects'] if x['type']=='MESH']
    assert len(meshes)==1
    source=meshes[0]
    before=c.call('blender_mesh_inspect',{'object_name':source},'pre_prepare_'+name)['result']
    assert before['geometry']['degenerate_faces']==0
    if before['geometry']['loose_vertices'] or before['geometry']['loose_edges']:
        clean=name+'_LooseClean08'
        c.call('blender_mesh_edit',{'source_object_name':source,'output_object_name':clean,'operations':[{'type':'delete_loose'}]},'delete_loose_final_'+name)
        source=clean
        before=c.call('blender_mesh_inspect',{'object_name':source},'post_delete_loose_'+name)['result']
    assert max(abs(v-1) for v in before['object']['scale'])<1e-5
    assert max(abs(v) for v in before['object']['rotation_degrees'])<.001
    lo=before['bounds']['local_min'];hi=before['bounds']['local_max']
    cx=(lo[0]+hi[0])/2;cy=(lo[1]+hi[1])/2
    target_height=asset['dimensions_ue_cm'][2]/100
    factor=target_height/(hi[2]-lo[2])
    sized=name+'_Sized09'
    # One coordinate normalization: source front -Y -> +X, right +X -> +Y,
    # uniform size, floor-center pivot, identity object transform.
    c.call('blender_mesh_edit',{'source_object_name':source,'output_object_name':sized,'operations':[{'type':'transform_vertices','scale':[factor]*3,'rotation_degrees':[0,0,90],'translation':[factor*cy,-factor*cx,-factor*lo[2]]},{'type':'transform_object','location':[0,0,0],'rotation_degrees':[0,0,0],'scale':[1,1,1]}]},'resize_'+name)
    sized_report=c.call('blender_mesh_inspect',{'object_name':sized,'requirements':{'require_applied_scale':True}},'post_resize_'+name)['result']
    assert abs(sized_report['object']['dimensions'][2]-target_height)<1e-5
    assert abs(sized_report['bounds']['local_min'][2])<1e-5
    assert sized_report['geometry']['degenerate_faces']==0
    c.call('blender_uv_inspect',{'object_name':sized},'pre_uv_'+name)
    uv=name+'_Unwrapped10'
    c.call('blender_uv_edit',{'source_object_name':sized,'output_object_name':uv,'operation':'smart_project','uv_layer_name':'UV_Game','create_layer':True,'angle_limit_degrees':66,'island_margin':.008},'unwrap_'+name)
    c.call('blender_uv_inspect',{'object_name':uv},'post_unwrap_'+name)
    packed='SM_'+name+'_UV11'
    c.call('blender_uv_edit',{'source_object_name':uv,'output_object_name':packed,'operation':'pack_islands','uv_layer_name':'UV_Game','create_layer':False,'island_margin':.008},'pack_'+name)
    uv_report=c.call('blender_uv_inspect',{'object_name':packed},'final_uv_'+name)['result']
    final=c.call('blender_mesh_inspect',{'object_name':packed,'requirements':{'require_applied_scale':True,'require_uv':True,'require_material':True}},'final_mesh_'+name)['result']
    assert final['quality']['passed'],final['quality']
    exports={}
    for extension in ('glb','fbx','obj'):
        # Re-inspection before each export proves the explicit exact selection.
        c.call('blender_mesh_inspect',{'object_name':packed},'pre_export_'+extension+'_'+name)
        c.call('blender_model_export',{'object_names':[packed],'path':'Libregrove_20260908/exports/'+name+'.'+extension,'overwrite':False,'apply_modifiers':True},'final_export_'+extension+'_'+name)
        exports[extension]=recorded('final_export_'+extension+'_'+name)['path']
    c.call('blender_mesh_inspect',{'object_name':meshes[0]},'preserved_import_'+name)
    result={'id':name,'source_original':name,'repaired_object':h['object_name'],'imported_object':meshes[0],'output_object':packed,'uniform_scale_factor':factor,'target_height_cm':target_height*100,'measured_width_depth_height_cm':[final['object']['dimensions'][1]*100,final['object']['dimensions'][0]*100,final['object']['dimensions'][2]*100],'mesh_report':final,'uv_report':uv_report,'exports':exports}
    resultfile.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'id':name,'vertices':final['geometry']['vertices'],'triangles':final['geometry']['triangles'],'dimensions_cm':result['measured_width_depth_height_cm'],'uv':uv_report}),flush=True)

if __name__=='__main__':
    assets=json.loads((ROOT/'Docs/Libregrove/model_budgets.json').read_text(encoding='utf-8'))['assets']
    wanted=set(sys.argv[1:]);c=Client()
    try:
        for asset in assets:
            if not wanted or asset['id'] in wanted:finish(asset,c)
        # New save path for each explicit invocation, preserving prior snapshots.
        suffix='_'.join(sorted(x[3:5] for x in wanted)) if wanted else 'ALL'
        c.call('blender_scene_inspect',{},'pre_save_prepared_'+suffix)
        c.call('blender_save_copy',{'path':'Cindehearth/Libregrove_20260908/stages/Libregrove_Prepared_'+suffix+'.blend','overwrite':False},'save_prepared_'+suffix)
    finally:c.close()
