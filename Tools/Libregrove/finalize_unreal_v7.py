"""Explicitly save only new V7 content and preserve immutable evidence."""
import json,hashlib,shutil
import textured_handoff as h
context=h.ue_call('get_editor_context',{},'V7_final_before')
assert context['play_session']=='stopped' and context['level']['name']=='L_Libregrove_AssetReview_V7'
rows=json.loads((h.DEST/'export_manifest.json').read_text(encoding='utf-8'))
saved=[]
for row in rows:
    receipt=json.loads((h.DEST/'unreal_jobs'/row['id']/'result.json').read_text())
    assert receipt['status']=='finished' and len(receipt['objects'])==3
    for i,obj in enumerate(receipt['objects']):
        result=h.ue_call('manage_asset',{'action':'save','asset_path':obj['path']},f'V7_FINAL_SAVE_{row["id"]}_{i:02d}')
        saved.append({'object':obj,'result':result})
    print('SAVED '+row['id'],flush=True)
level=context['level']['path']
h.ue_call('manage_asset',{'action':'find','asset_path':level},'V7_FINAL_FIND_MAP')
h.ue_call('manage_asset',{'action':'save','asset_path':level},'V7_FINAL_SAVE_MAP')
context=h.ue_call('get_editor_context',{},'V7_final_after_save')
assert not context['level']['dirty'] and context['level']['path']==level
code='import unreal\nimport runpy\nrunpy.run_path('+repr(str(h.ROOT/'Tools/Libregrove/unreal_v7_final_audit.py'))+',init_globals={\'STAGE\':\'saved\'})'
print(json.dumps(h.ue_call('execute_python_code',{'code':code},'V7_FINAL_ENGINE_READBACK'),indent=2),flush=True)
h.ue_call('manage_asset',{'action':'list','path':'/Game/Libregrove/Textured_V7'},'V7_final_asset_list')
h.ue_call('read_logs',{'action':'filter','file':'main','pattern':'(?i)(Libregrove|SM_LG_).*(warning|error)|(warning|error).*Libregrove','max_matches':150,'context_lines':0},'V7_final_import_log_review')
baseline=json.loads((h.DEST/'blender_scene.json').read_text())
c=h.blender.Client('main',quiet=True)
try:
    status=h.unwrap(c.call('blender_bridge_status',{},'V7_final_blender_status'))
    assert status['blender']['runtime_supported'] and status['mcp']['bridge_compatibility']['compatible']
    scene=h.unwrap(c.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},'V7_final_blender_scene'))
    lookup={o['name']:o for o in scene['objects']}
    differences={o['name']:{k:[v,lookup[o['name']][k]] for k,v in o.items() if v!=lookup[o['name']][k]} for o in baseline['objects']}
    assert all(set(diff)<= {'hidden','selected'} for diff in differences.values())
    h.save('final_source_preservation.json',{'original_geometry_transform_fingerprints_preserved':len(baseline['objects']),'live_mesh_objects':len(scene['objects']),'ui_differences':differences,'active_object':[baseline['active_object'],scene['active_object']],'passed':True})
finally:c.close()
backup=json.loads((h.DEST/'backup_manifest.json').read_text())
assert hashlib.sha256(h.Path(backup['original_level']).read_bytes()).hexdigest()==backup['original_level_sha256']
working=h.EXCHANGE/'Libregrove_Textured_V7_WorkingCopy.blend'
local=h.DEST/'Libregrove_Textured_V7_WorkingCopy.blend';assert not local.exists()
shutil.copy2(working,local)
files=[]
for obj in [s['object'] for s in saved]+[{'path':level,'class':'World'}]:
    package=obj['path'].split('.')[0]
    path=h.ROOT/'Content'/(package.removeprefix('/Game/')+('.umap' if obj['class']=='World' else '.uasset'))
    assert path.is_file() and path.stat().st_size>0
    files.append({'asset_path':obj['path'],'class':obj['class'],'file':str(path),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
h.save('saved_content_manifest.json',{'files':files,'original_level_unchanged':True,'source_backup_sha256':backup['blend_copy_sha256'],'working_blend_sha256':hashlib.sha256(local.read_bytes()).hexdigest(),'passed':True})
print('V7_SAVED_AND_READBACK_VERIFIED',flush=True)
