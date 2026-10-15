"""Run existing real movement and chapter checks sequentially; retain every receipt."""
import json,sys,time
from pathlib import Path
root=Path(__file__).resolve().parent;sys.path.insert(0,str(root.parent))
from call import call
out=root/'verification'
def execute(code,name):
 r=call('execute_python_code',{'code':code});(out/(name+'.json')).write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8')
 assert not isinstance(r,dict) or not r.get('error_message'),r
 if isinstance(r,dict):print(name,r.get('output',''),flush=True)
 return r
def file(name,stage,receipt):return execute('STAGE='+repr(stage)+'\n'+(root/name).read_text(encoding='utf-8'),receipt)
def wait_receipt(name,timeout=100):
 p=out/name;start=time.monotonic()
 while not p.exists():
  assert time.monotonic()-start<timeout,name+' timed out';time.sleep(2)
 d=json.loads(p.read_text());assert d['passed'],d;print(name,'PASS',flush=True);return d
execute('import unreal; assert not unreal.WidgetService.is_pie_running(); assert unreal.WidgetService.start_pie()','qa_pie_start')
time.sleep(3)
file('qa_runtime.py','states','qa_states_execution')
routes=[]
for mode in ['rail','cemetery','ashslope','deepvein','pinehollow']:
 name='walk_'+mode+'.json';assert not (out/name).exists()
 file('qa_runtime.py','walk:'+mode,'qa_start_'+mode);routes.append(wait_receipt(name))
assert not (out/'gorm_walk.json').exists();file('walk_gorm.py','execute','qa_start_gorm');routes.append(wait_receipt('gorm_walk.json',60))
file('qa_runtime.py','archive_enter','qa_archive_enter');time.sleep(2);file('qa_runtime.py','archive_exit','qa_archive_exit')
file('qa_runtime.py','epilogue','qa_epilogue_start');time.sleep(21);file('qa_runtime.py','epilogue_verify','qa_epilogue_verify')
file('qa_camera.py','prepare','qa_camera_prepare');time.sleep(2);file('qa_camera.py','verify','qa_camera_verify')
(out/'runtime_suite.json').write_text(json.dumps(dict(passed=True,routes=[dict(route=x.get('route','gorm'),passed=x['passed']) for x in routes],archive=True,epilogue=True,camera=True),indent=2),encoding='utf-8')
print('ALL GAMEPLAY CHECKS PASS; PIE remains available for visual evidence',flush=True)
