import sys,json
import textured_handoff as h

op=sys.argv[1]
context=h.ue_call('get_editor_context',{},'V7_review_'+op+'_before')
assert context['play_session']=='stopped'
if op=='create':
    assert not context['level']['dirty'] and context['level']['path']=='/Game/Maps/L_VillageEntrance'
    backup=json.loads((h.DEST/'backup_manifest.json').read_text());assert h.hashlib.sha256(h.Path(backup['original_level']).read_bytes()).hexdigest()==backup['original_level_sha256']
else:assert context['level']['name']=='L_Libregrove_AssetReview_V7'
script=h.ROOT/'Tools/Libregrove/unreal_v7_review.py'
code='import unreal\nimport runpy\nrunpy.run_path('+repr(str(script))+',init_globals='+repr({'OP':op})+')'
print(json.dumps(h.ue_call('execute_python_code',{'code':code},'V7_review_'+op),indent=2))
print(json.dumps(h.ue_call('get_editor_context',{},'V7_review_'+op+'_after'),indent=2))
