import unreal,time,json,traceback,importlib.util
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/UIPopupPalette/QA';OUT.mkdir(parents=True,exist_ok=True)
(OUT/'error.txt').unlink(missing_ok=True)
s=importlib.util.spec_from_file_location('palette_rt',ROOT/'Tools/UILayoutContract/runtime.py');rt=importlib.util.module_from_spec(s);s.loader.exec_module(rt);rt.OUT=OUT
paths=['/Game/Developers/dbals/UI/WBP_ConfirmDialog','/Game/Developers/dbals/UI/WBP_CountDialog','/Game/Developers/zxcv1/Quest/UI/WBP_RewardPopup']
phase=0;due=time.monotonic()+8;popup=None;report={}
def finish():
 unreal.unregister_slate_post_tick_callback(handle);unreal.WidgetService.stop_pie();unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InNewProcess')
def create(i):
 w,pc=rt.context();cls=unreal.load_class(None,paths[i]+'.'+paths[i].split('/')[-1]+'_C');o=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(w,cls,pc))
 if i==0:o.set_editor_property('message',unreal.Text('선택한 아이템을 버리시겠습니까?'));o.set_editor_property('confirm_label',unreal.Text('버리기'));o.set_editor_property('cancel_label',unreal.Text('취소'))
 if i==2:o.set_editor_property('title',unreal.Text('보상 획득'))
 o.add_to_viewport(500)
 if i==1:
  # Native Show supplies this non-reflected field; direct preview creation does not.
  rt.obj(o,'MessageText').set_text('이동할 수량을 입력하세요.');rt.obj(o,'CountTextBox').set_text('1');rt.obj(o,'ConfirmButton').set_label_text('확인')
 if i==2:rt.obj(o,'EntryText').set_text('모험 경험치  +120\n골드  +500\n회복 물약  ×3')
 return o
def tick(dt):
 global phase,due,popup
 if time.monotonic()<due:return
 due=time.monotonic()+2
 try:
  if phase==0:popup=create(0)
  elif phase==1:rt.shot('confirm')
  elif phase==2:popup.cancel();assert not popup.is_in_viewport();report['confirm_cancel']=True;popup=create(0)
  elif phase==3:popup.confirm();assert not popup.is_in_viewport();report['confirm_accept_no_callback']=True;popup=create(1)
  elif phase==4:rt.shot('count')
  elif phase==5:popup.cancel();assert not popup.is_in_viewport();report['count_cancel']=True;popup=create(2)
  elif phase==6:rt.shot('reward')
  elif phase==7:
   # Native Close requires registration by NotificationUIController. This
   # direct preview only validates visuals, not the controller lifecycle.
   report['reward_controller_queue']='not tested: direct preview';popup.remove_from_parent()
   (OUT/'runtime.json').write_text(json.dumps(report,indent=2));finish();return
  phase+=1
 except Exception:(OUT/'error.txt').write_text(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');unreal.WidgetService.start_pie()
