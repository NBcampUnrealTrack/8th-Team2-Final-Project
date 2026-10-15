import json
from finish_assets import recorded
from blender_mcp_client import Client,EVIDENCE,ROOT
c=Client()
try:
    assets=json.loads((ROOT/'Docs/Libregrove/model_budgets.json').read_text(encoding='utf-8'))['assets']
    rows=[]
    for asset in assets:
        name=asset['id'];sized=name+'_Sized09'
        if (EVIDENCE/('post_resize_'+name+'.json')).exists():
            after=c.call('blender_mesh_inspect',{'object_name':sized},'resize_verified_'+name)['result']
            rows.append({'id':name,'object_name':sized,'report':after});continue
        h=json.loads((EVIDENCE/('repair_handoff_'+name+'.json')).read_text(encoding='utf-8'))
        c.call('blender_scene_inspect',{},'pre_import_'+name)
        c.call('blender_model_import',{'path':h['export_path'],'name_prefix':'LGQA'+name[3:5]+'_'},'import_'+name)
        imp=recorded('import_'+name);meshes=[x['name'] for x in imp['objects'] if x['type']=='MESH'];assert len(meshes)==1
        source=meshes[0];before=c.call('blender_mesh_inspect',{'object_name':source},'pre_resize_remaining_'+name)['result']
        if before['geometry']['loose_vertices'] or before['geometry']['loose_edges']:
            clean=name+'_LooseClean08'
            c.call('blender_mesh_edit',{'source_object_name':source,'output_object_name':clean,'operations':[{'type':'delete_loose'}]},'delete_loose_final_'+name)
            source=clean;before=c.call('blender_mesh_inspect',{'object_name':source},'post_delete_loose_'+name)['result']
        assert max(abs(v-1) for v in before['object']['scale'])<1e-5
        assert max(abs(v) for v in before['object']['rotation_degrees'])<.001
        lo=before['bounds']['local_min'];hi=before['bounds']['local_max'];cx=(lo[0]+hi[0])/2;cy=(lo[1]+hi[1])/2
        factor=(asset['dimensions_ue_cm'][2]/100)/(hi[2]-lo[2])
        c.call('blender_mesh_edit',{'source_object_name':source,'output_object_name':sized,'operations':[{'type':'transform_vertices','scale':[factor]*3,'rotation_degrees':[0,0,90],'translation':[factor*cy,-factor*cx,-factor*lo[2]]},{'type':'transform_object','location':[0,0,0],'rotation_degrees':[0,0,0],'scale':[1,1,1]}]},'resize_'+name)
        after=c.call('blender_mesh_inspect',{'object_name':sized},'post_resize_'+name)['result']
        rows.append({'id':name,'object_name':sized,'report':after})
        print(json.dumps({'id':name,'dimensions':after['object']['dimensions'],'geometry':after['geometry']}),flush=True)
    (EVIDENCE/'all_resized.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    c.call('blender_scene_inspect',{},'all_resized_scene')
    c.call('blender_save_copy',{'path':'Cindehearth/Libregrove_20260908/stages/Libregrove_All_Resized09.blend','overwrite':False},'all_resized_save')
finally:c.close()
