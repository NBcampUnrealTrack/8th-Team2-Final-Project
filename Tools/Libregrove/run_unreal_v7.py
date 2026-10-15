"""Serial model imports with persisted request/result reconciliation."""
import json,sys,time
import textured_handoff as h

def run(script,ident,evidence):
    code='import unreal\nimport runpy\nrunpy.run_path('+repr(str(h.ROOT/'Tools/Libregrove'/script))+',init_globals='+repr({'ASSET_ID':ident})+')'
    result=h.ue_call('execute_python_code',{'code':code},evidence)
    print(json.dumps(result,ensure_ascii=True),flush=True)
    return result

def import_rest(start=1):
    rows=json.loads((h.DEST/'export_manifest.json').read_text(encoding='utf-8'))
    for row in rows[start:]:
        ident=row['id'];listed=h.ue_call('manage_asset',{'action':'list','path':row['ue_destination']},'V7_before_import_'+ident);assert listed['count']==0
        run('unreal_v7_import.py',ident,'V7_import_'+ident)
        result=h.DEST/'unreal_jobs'/ident/'result.json';started=time.monotonic()
        while not result.exists():
            assert time.monotonic()-started<180,'Import status uncertain; inspect without resubmitting'
            time.sleep(1)
        assert json.loads(result.read_text())['status']=='finished'
        run('unreal_v7_qa.py',ident,'V7_qa_'+ident)
        paths=json.loads(result.read_text())['objects']
        for n,asset in enumerate(paths):h.ue_call('manage_asset',{'action':'save','asset_path':asset['path']},f'V7_save_{ident}_{n:02d}')
        h.ue_call('manage_asset',{'action':'list','path':row['ue_destination']},'V7_after_import_'+ident)
        print('IMPORTED_AND_VERIFIED '+ident,flush=True)

if __name__=='__main__':
    if sys.argv[1]=='rest':import_rest(int(sys.argv[2]) if len(sys.argv)>2 else 1)
    else:run('unreal_v7_qa.py','LG_01_Cottage_A','V7_qa_LG_01_Cottage_A')
