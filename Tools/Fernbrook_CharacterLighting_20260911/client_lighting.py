"""Use the existing official authenticated project MCP transport; unique character-lighting receipts."""
import sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/Fernbrook_Entrance_V1_20260911'))
import client as c
c.DOC=ROOT/'Docs/Fernbrook_CharacterLighting_20260911'
sys.stdout.reconfigure(encoding='utf-8')
if __name__=='__main__':
 mode,label=sys.argv[1:3]
 if mode=='skill':
  result=c.call('manage_skills',{'action':'load','skill_name':sys.argv[3]},label)
  print(result['content'])
 elif mode=='class':
  result=c.call('discover_python_class',{'class_name':sys.argv[3],'method_filter':sys.argv[4],'max_methods':45,'include_inherited':len(sys.argv)>5},label)
  if sys.argv[4]!='__NONE__':
   result={'classes':[{k:v for k,v in x.items() if k not in ['doc_string','base_classes']} for x in result.get('classes',[result])]}
  print(json.dumps(result,ensure_ascii=False,indent=2))
 elif mode=='python':
  code=Path(sys.argv[3]).read_text(encoding='utf-8-sig');assert code.startswith('import unreal')
  if len(sys.argv)>4:code='import unreal\nSTAGE='+repr(sys.argv[4])+'\n'+code
  print(json.dumps(c.call('execute_python_code',{'code':code},label),ensure_ascii=False,indent=2))
 else:
  args=json.loads(Path(sys.argv[3]).read_text(encoding='utf-8-sig')) if len(sys.argv)>3 and sys.argv[3]!='-' else {}
  print(json.dumps(c.call(mode,args,label),ensure_ascii=False,indent=2))
