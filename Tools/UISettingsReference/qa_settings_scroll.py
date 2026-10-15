import unreal,time,json,traceback,importlib.util
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/UISettingsScroll';OUT.mkdir(exist_ok=True)
s=importlib.util.spec_from_file_location('rt',ROOT/'Saved/UISettingsReference/settings_runtime.py');rt=importlib.util.module_from_spec(s);s.loader.exec_module(rt);rt.OUT=OUT
phase=0;due=time.monotonic()+8;report={}
def finish():
 unreal.unregister_slate_post_tick_callback(handle);unreal.WidgetService.stop_pie()
 unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InNewProcess')
def tick(dt):
 global phase,due
 if time.monotonic()<due:return
 due=time.monotonic()+2
 try:
  if phase==0:
   rt.show_options();rt.options.remove_from_parent();rt.options.add_to_viewport(100000);rt.tab(1)
  elif phase==1:
   sc=rt.obj(rt.options,'SettingsGraphicsScroll')
   report['graphics_scroll_extent']=sc.get_scroll_offset_of_end();assert report['graphics_scroll_extent']>100
   assert rt.obj(rt.options,'TitleText').slot.get_position().x==54
   rt.shot('graphics_top')
  elif phase==2:rt.obj(rt.options,'SettingsGraphicsScroll').scroll_to_end()
  elif phase==3:
   sc=rt.obj(rt.options,'SettingsGraphicsScroll');report['bottom_offset']=sc.get_scroll_offset();assert report['bottom_offset']>100
   rt.shot('graphics_bottom')
  elif phase==4:rt.obj(rt.options,'SettingsGraphicsScroll').scroll_to_start()
  elif phase==5:
   assert rt.obj(rt.options,'SettingsGraphicsScroll').get_scroll_offset()<1
   rt.tab(0)
  elif phase==6:rt.shot('display')
  elif phase==7:rt.tab(2)
  elif phase==8:rt.shot('sound')
  elif phase==9:rt.tab(3)
  elif phase==10:rt.shot('controls')
  elif phase==11:rt.open_keys();rt.options.set_visibility(unreal.SlateVisibility.COLLAPSED);rt.keys.remove_from_parent();rt.keys.add_to_viewport(100001)
  elif phase==12:
   assert rt.obj(rt.keys,'TitleText').slot.get_position().x==82
   rt.shot('keys_title')
  elif phase==13:
   rt.obj(rt.keys,'FooterCloseButton').on_clicked.broadcast();assert not rt.keys.is_in_viewport()
   rt.options.set_visibility(unreal.SlateVisibility.VISIBLE);rt.obj(rt.options,'CloseButton').on_clicked.broadcast();assert not rt.options.is_in_viewport()
   report.update(four_tabs=True,scroll_top_bottom=True,title_offsets=True,key_launcher_and_close=True,options_close=True)
   (OUT/'runtime.json').write_text(json.dumps(report,indent=2));finish();return
  phase+=1
 except Exception:
  (OUT/'qa_error.txt').write_text(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');unreal.WidgetService.start_pie()
