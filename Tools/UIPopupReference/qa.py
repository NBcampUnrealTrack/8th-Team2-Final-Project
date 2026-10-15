import unreal,time,json,traceback,importlib.util
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/UIPopupReference/QA';OUT.mkdir(parents=True,exist_ok=True)
(OUT/'error.txt').unlink(missing_ok=True)
spec=importlib.util.spec_from_file_location('popup_reference_rt',ROOT/'Tools/UILayoutContract/runtime.py');rt=importlib.util.module_from_spec(spec);spec.loader.exec_module(rt);rt.OUT=OUT
phase=0;due=time.monotonic()+8;popup=None;hub=None;report={}
old_mode=unreal.ObjectReflectionService.get_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType')
old_throttle=unreal.ObjectReflectionService.get_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground')
unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground','False')
def finish():
 global popup,hub
 unreal.unregister_slate_post_tick_callback(handle)
 popup=None;hub=None
 unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType',old_mode)
 unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground',old_throttle)
 unreal.WidgetService.stop_pie()
def find(cls):
 w,pc=rt.context();return next(o for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True) if o.get_class().get_name()==cls)
def tick(dt):
 global phase,due,popup,hub
 if time.monotonic()<due:return
 due=time.monotonic()+2
 try:
  w,pc=rt.context()
  if phase==0:unreal.SystemLibrary.execute_console_command(w,'open /Game/Maps/L_TestLevel',pc);due=time.monotonic()+12
  elif phase==1:
   for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True):
    if o.get_class().get_name()=='NUStorySlideshowWidget':o.on_skip_requested.broadcast()
  elif phase==2:unreal.SystemLibrary.execute_console_command(w,'nu.Menu.Open',pc)
  elif phase==3:hub=find('WBP_GameMenu_C');rt.obj(hub,'QuitGameButton').on_common_clicked.broadcast()
  elif phase==4:
   popup=find('WBP_ConfirmDialog_C');report['native_message']=str(rt.obj(popup,'MessageText').get_text());rt.shot('quit_normal')
   report['visible_widgets']=[o.get_class().get_name() for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True) if o.is_in_viewport()]
   report['geometry']={}
   for n in ['PanelBG','ConfirmButton','CancelButton','MessageText','ReferenceCloseButton']:
    v=rt.obj(popup,n).get_desired_size();report['geometry'][n]=[v.x,v.y]
  elif phase==5:
   rt.obj(popup,'CancelButton').on_clicked.broadcast();assert not popup.is_in_viewport();report['native_cancel']=True
   unreal.SystemLibrary.execute_console_command(w,'nu.Menu.Open',pc)
  elif phase==6:hub=find('WBP_GameMenu_C');rt.obj(hub,'QuitGameButton').on_common_clicked.broadcast()
  elif phase==7:popup=find('WBP_ConfirmDialog_C');rt.obj(popup,'ReferenceCloseButton').on_clicked.broadcast();assert not popup.is_in_viewport();report['header_x_cancel']=True
  elif phase==8:
   cls=unreal.load_class(None,'/Game/Developers/dbals/UI/WBP_ConfirmDialog.WBP_ConfirmDialog_C');popup=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(w,cls,pc));popup.set_editor_property('message',unreal.Text('게임을 종료할까요?'));popup.set_editor_property('confirm_label',unreal.Text('종료'));popup.add_to_viewport(500)
   # Transient layout area provides a second aspect ratio without changing settings.
   root=unreal.find_object(popup,'WidgetTree_0');rt.obj(popup,'PanelBG').slot.set_anchors(unreal.Anchors(unreal.Vector2D(.5,.5),unreal.Vector2D(.5,.5)))
   for n in ['ConfirmButton','CancelButton']:
    o=rt.obj(popup,n);st=o.get_editor_property('widget_style').copy();st.normal=st.hovered.copy();o.set_style(st)
  elif phase==9:rt.shot('hover_style');report['hover']='transient style preview; pointer hover not tested'
  elif phase==10:
   popup.set_desired_size_in_viewport(unreal.Vector2D(1280,960));rt.shot('area_4x3');report['second_ratio']='transient 1280x960 widget layout area, not viewport resolution'
  elif phase==11:
   popup.cancel();assert not popup.is_in_viewport();report['sample_cancel']=True
   cls=unreal.load_class(None,'/Game/Developers/dbals/UI/WBP_ConfirmDialog.WBP_ConfirmDialog_C')
   sample=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(w,cls,pc));sample.set_editor_property('message',unreal.Text('확인 동작 검증'));sample.add_to_viewport(500)
   rt.obj(sample,'ConfirmButton').on_clicked.broadcast();assert not sample.is_in_viewport();report['sample_confirm_without_callback']=True
   sample=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(w,cls,pc));sample.set_editor_property('message',unreal.Text('안내'))
   # Do not use the transactional reflection service for PIE objects: its
   # undo record retains the game instance and prevents PIE teardown GC.
   sample.set_editor_property('bNoticeOnly',True)
   sample.add_to_viewport(500);assert rt.obj(sample,'CancelButton').get_visibility()==unreal.SlateVisibility.COLLAPSED
   sample.cancel();report['notice_only_collapse']=True;sample=None;w=None;pc=None
   (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');finish();return
  phase+=1
 except Exception:
  (OUT/'error.txt').write_text(traceback.format_exc(),encoding='utf-8');finish()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');unreal.WidgetService.start_pie()
