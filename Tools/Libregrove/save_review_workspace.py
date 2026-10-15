import json
from blender_mcp_client import Client,EVIDENCE,ROOT

c=Client()
try:
    hs=[json.loads(p.read_text(encoding='utf-8')) for p in sorted(EVIDENCE.glob('review_handoff_*.json'))]
    assert len(hs)==9
    assets={a['id']:a for a in json.loads((ROOT/'Docs/Libregrove/model_budgets.json').read_text(encoding='utf-8'))['assets']}
    queries=[];assertions=[]
    for h in hs:
        name=h['id'];queries.append({'id':name,'type':'world_bounds','object_name':h['output_object']})
        for field,value in [('size.z',assets[name]['dimensions_ue_cm'][2]/100),('minimum.z',0),('center.x',0),('center.y',0)]:
            assertions.append({'query_id':name,'field':field,'operator':'near','expected':value,'tolerance':.00001})
    measured=c.call('blender_scene_measure',{'queries':queries,'assertions':assertions},'review_dimensions_assertions')['result']
    assert measured['passed']
    c.call('blender_scene_inspect',{'include_hidden':True},'review_collection_before')
    collection='LG_REVIEW_20260908'
    c.call('blender_collection_create',{'collection_name':collection},'review_collection_create')
    c.call('blender_scene_inspect',{'include_hidden':True},'review_collection_created')
    previews=[]
    for i,h in enumerate(hs):
        source=h['output_object'];name='REVIEW_'+h['id']
        before=c.call('blender_mesh_inspect',{'object_name':source},'preview_source_'+h['id'])['result']
        c.call('blender_object_duplicate',{'source_object_name':source,'output_object_name':name,'collection_name':collection,'copy_data':True,'location':[(i%3)*8,15+(i//3)*8,0]},'preview_duplicate_'+h['id'])
        after=c.call('blender_mesh_inspect',{'object_name':name},'preview_inspect_'+h['id'])['result']
        assert after['object']['data_name']!=before['object']['data_name']
        srcafter=c.call('blender_mesh_inspect',{'object_name':source},'preview_preserved_'+h['id'])['result']
        assert srcafter['fingerprint']==before['fingerprint']
        previews.append({'name':name,'source':source,'data_name':after['object']['data_name'],'independent_data':True})
    baseline=json.loads((EVIDENCE/'baseline_summary.json').read_text(encoding='utf-8'));preserved=[]
    for item in baseline:
        actual=c.call('blender_mesh_inspect',{'object_name':item['name']},'final_original_'+item['name'])['result']
        same=actual['fingerprint']==item['mesh']['result']['fingerprint'];assert same
        preserved.append({'name':item['name'],'fingerprint':actual['fingerprint'],'unchanged':same})
    c.call('blender_bridge_status',{},'review_final_bridge_status')
    c.call('blender_scene_inspect',{'include_hidden':True},'review_final_scene')
    c.call('blender_save_copy',{'path':'Cindehearth/Libregrove_20260908/Libregrove_ReviewWorkspace18.blend','overwrite':False},'review_workspace_save')
    (EVIDENCE/'workspace_handoff.json').write_text(json.dumps({'status':'GAME_SIGNOFF_WITHHELD','collection':collection,'previews':previews,'originals':preserved},indent=2),encoding='utf-8')
    print('9 original fingerprints unchanged; 36 dimension assertions passed; review workspace saved.',flush=True)
finally:c.close()
