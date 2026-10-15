import unreal,time,json,traceback,importlib.util
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/UIFeedbackPolish';OUT.mkdir(exist_ok=True)
s=importlib.util.spec_from_file_location('fbrt',ROOT/'Saved/UISettingsReference/settings_runtime.py');rt=importlib.util.module_from_spec(s);s.loader.exec_module(rt);rt.OUT=OUT
phase=0;due=time.monotonic()+8;report={};hub=None
def finish():
 unreal.unregister_slate_post_tick_callback(handle);unreal.WidgetService.stop_pie();unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InNewProcess')
def tick(dt):
 global phase,due,hub
 if time.monotonic()<due:return
 due=time.monotonic()+2
 try:
  if phase==0:rt.show_options();rt.options.remove_from_parent();rt.options.add_to_viewport(100000);rt.tab(0)
  elif phase==1:rt.shot('settings_display')
  elif phase==2:rt.tab(1)
  elif phase==3:
   sc=rt.obj(rt.options,'SettingsGraphicsScroll');report['graphics_extent']=sc.get_scroll_offset_of_end();assert report['graphics_extent']>100;rt.shot('settings_graphics_top')
  elif phase==4:rt.obj(rt.options,'SettingsGraphicsScroll').scroll_to_end()
  elif phase==5:rt.shot('settings_graphics_bottom');report['graphics_bottom']=rt.obj(rt.options,'SettingsGraphicsScroll').get_scroll_offset()
  elif phase==6:rt.tab(2)
  elif phase==7:rt.shot('settings_sound')
  elif phase==8:rt.tab(3)
  elif phase==9:rt.shot('settings_controls')
  elif phase==10:
   rt.obj(rt.options,'CloseButton').on_clicked.broadcast();assert not rt.options.is_in_viewport()
   w,pc=rt.context();unreal.SystemLibrary.execute_console_command(w,'open /Game/Maps/L_TestLevel',pc);due=time.monotonic()+15
  elif phase==11:
   w,pc=rt.context()
   for x in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True):
    if x.get_class().get_name()=='NUStorySlideshowWidget':x.on_skip_requested.broadcast()
  elif phase==12:
   w,pc=rt.context();unreal.SystemLibrary.execute_console_command(w,'nu.Menu.Open',pc)
  elif phase==13:
   w,pc=rt.context();hub=next(x for x in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True) if x.get_class().get_name()=='WBP_GameMenu_C')
   assert str(rt.obj(hub,'HubBannerProject').get_text())=='나이아 언바운드'
   rows=rt.obj(hub,'MainMenuTileView').get_displayed_entry_widgets();report['hub_rows']=len(rows);assert len(rows)==9
   report['hub_gap']=rt.obj(hub,'MainMenuTileView').get_editor_property('vertical_entry_spacing');assert report['hub_gap']==12
   pc.set_mouse_location(700,400)
  elif phase==14:rt.shot('hub_idle')
  elif phase==15:
   w,pc=rt.context();scale=pc.get_viewport_size()[1]/899;pc.set_mouse_location(round(200*scale),round(170*scale))
  elif phase==16:rt.shot('hub_hover')
  elif phase==17:
   rt.obj(hub,'HubCloseButton').on_clicked.broadcast();assert not hub.is_in_viewport();report['hub_close']=True
   (OUT/'runtime.json').write_text(json.dumps(report,indent=2));finish();return
  phase+=1
 except Exception:
  (OUT/'qa_error.txt').write_text(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');unreal.WidgetService.start_pie()
