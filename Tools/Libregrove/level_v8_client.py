"""Live MCP transport for the full Libregrove level, separate from V7 assets."""
import json,sys,re,subprocess,time
from pathlib import Path
import textured_handoff as h
ROOT=h.ROOT
DEST=ROOT/'Docs/Libregrove/Level_V8_20260908'
LEVEL='/Game/Libregrove/Level_V8/Maps/L_Libregrove'
h.DEST=DEST

def call(name,args,label):
    if h._UE_URL is None:
        for attempt in range(3):
            status_run=subprocess.run([str(ROOT/'Plugins/Revvy/revvy.cmd'),'status','--json'],capture_output=True,text=True,encoding='utf8')
            status=json.loads(status_run.stdout)
            if status.get('ue_running'):
                assert Path(status['active_project']['path']).resolve()==ROOT.resolve()
                h._UE_URL=status['proxy_url'];break
            time.sleep(1)
        assert h._UE_URL,'Editor readiness could not be verified; mutation not sent'
    save('evidence/'+label+'_request.json',{'tool':name,'arguments':args})
    result=h.ue_rpc('tools/call',{'name':name,'arguments':args})
    save('evidence/'+label+'_result.json',result)
    if result.get('isError'):raise RuntimeError(result)
    if result.get('structuredContent'):return result['structuredContent']
    body=next(c['text'] for c in result['content'] if c['type']=='text')
    parts=[];page=0
    while body.startswith('[revvy]'):
        header,fragment=body.split('\n\n',1);parts.append(fragment)
        match=re.search(r'revvy_result_page (\{[^\n]+\})',header)
        if not match:break
        page+=1
        next_result=h.ue_rpc('tools/call',{'name':'revvy_result_page','arguments':json.loads(match.group(1))})
        save('evidence/'+label+'_page'+str(page)+'.json',next_result)
        if next_result.get('isError'):raise RuntimeError(next_result)
        body=next(c['text'] for c in next_result['content'] if c['type']=='text')
    value=json.loads(''.join(parts) if parts else body)
    if isinstance(value,dict) and value.get('success') is False:raise RuntimeError(value)
    if parts:save('evidence/'+label+'_complete.json',value)
    return value
def save(name,value):return h.save(name,value)
def run(script,label,values=None):
    code='import unreal\nimport runpy\nrunpy.run_path('+repr(str(ROOT/'Tools/Libregrove'/script))+',init_globals='+repr(values or {})+')'
    result=call('execute_python_code',{'code':code},label)
    print(json.dumps(result,ensure_ascii=True,indent=2),flush=True)
    return result

if __name__=='__main__':
    schema=save('live_schema.json',h.ue_rpc('tools/list',{}))
    print(json.dumps(call('get_editor_context',{},'V8_context_000'),indent=2))
    for folder in ['/Game/Libregrove','/Game/Village','/Game/Cindehearth']:
        result=call('manage_asset',{'action':'list','path':folder},'V8_assets_'+folder.rsplit('/',1)[-1])
        print(folder,result.get('count'))
    print(json.dumps(call('discover_python_class',{'class_name':'unreal.LandscapeService,unreal.LevelEditorSubsystem','method_filter':'create_landscape|set_height_in_region|get_height_in_region|set_weights_in_region|add_layer|set_material|new_level_from_template|save_current_level','max_methods':30},'V8_discovery_terrain'),indent=2))
