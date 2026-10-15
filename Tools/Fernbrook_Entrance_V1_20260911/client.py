"""Project-configured authenticated MCP; no credential logging or permission changes."""
import json,os,re,sys,uuid,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];DOC=ROOT/'Docs/Fernbrook_Entrance_V1_20260911'
def save(name,value):
    path=DOC/name;path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('x',encoding='utf-8') as out:json.dump(value,out,indent=2,ensure_ascii=False)
    return value
def rpc(method,params):
    server='Revvy' if os.environ.get('FERNBROOK_MCP_MODE')=='safe' else 'Revvy-Full'
    config=json.loads((ROOT/'.mcp.json').read_text(encoding='utf-8-sig'))['mcpServers'][server];url=config['url']
    assert url.startswith('http://127.0.0.1:')
    def expand(value):
        return re.sub(r'\$\{(?:env:)?([A-Za-z_][A-Za-z_0-9]*)\}',lambda m:os.environ[m[1]],value)
    headers={k:expand(v) for k,v in config.get('headers',{}).items()};headers.update({'Content-Type':'application/json','Accept':'application/json, text/event-stream'})
    req=urllib.request.Request(url,data=json.dumps({'jsonrpc':'2.0','id':uuid.uuid4().hex,'method':method,'params':params}).encode(),headers=headers)
    with urllib.request.urlopen(req,timeout=50) as response:raw=response.read().decode('utf-8')
    if raw.lstrip().startswith(('event:','data:')):raw='\n'.join(l[5:].strip() for l in raw.splitlines() if l.startswith('data:'))
    result=json.loads(raw)
    if 'error' in result:raise RuntimeError(result['error'])
    return result['result']
def call(name,args,label):
    save('evidence/'+label+'_request.json',{'tool':name,'arguments':args})
    result=rpc('tools/call',{'name':name,'arguments':args});save('evidence/'+label+'_response.json',result)
    if result.get('isError'):raise RuntimeError(result)
    if result.get('structuredContent') is not None:return result['structuredContent']
    body=next(c['text'] for c in result['content'] if c['type']=='text');parts=[];page=0
    while body.startswith('[revvy]'):
        header,fragment=body.split('\n\n',1);parts.append(fragment);match=re.search(r'revvy_result_page (\{[^\n]+\})',header)
        if not match:break
        page+=1;nxt=rpc('tools/call',{'name':'revvy_result_page','arguments':json.loads(match.group(1))});save('evidence/'+label+'_page'+str(page)+'.json',nxt)
        if nxt.get('isError'):raise RuntimeError(nxt)
        body=next(c['text'] for c in nxt['content'] if c['type']=='text')
    text=''.join(parts) if parts else body
    try:value=json.loads(text)
    except json.JSONDecodeError:value={'text':text}
    if parts:save('evidence/'+label+'_complete.json',value)
    if isinstance(value,dict) and value.get('success') is False:raise RuntimeError(value)
    return value
if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    if sys.argv[1]=='list':
        mode=os.environ.get('FERNBROOK_MCP_MODE','full');value=save('unreal_schema_'+mode+'_001.json',rpc('tools/list',{}));print(json.dumps([{'name':t['name'],'inputSchema':t['inputSchema']} for t in value['tools'] if t['name'] in ['manage_skills','get_editor_context','execute_python_code','discover_python_class','manage_asset','revvy_status']],ensure_ascii=False,indent=2))
    elif sys.argv[1]=='python':
        code=Path(sys.argv[2]).read_text(encoding='utf-8-sig')
        assert code.startswith('import unreal')
        if len(sys.argv)>4:code='import unreal\nASSET_ID='+repr(sys.argv[4])+'\n'+code
        print(json.dumps(call('execute_python_code',{'code':code},sys.argv[3]),ensure_ascii=False,indent=2))
    else:
        args=json.loads(Path(sys.argv[2]).read_text(encoding='utf-8-sig')) if sys.argv[2]!='-' else {}
        print(json.dumps(call(sys.argv[1],args,sys.argv[3]),ensure_ascii=False,indent=2))
