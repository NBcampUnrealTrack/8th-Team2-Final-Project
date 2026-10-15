import unreal,time,json,traceback,importlib.util
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/UIHubShadows'
s=importlib.util.spec_from_file_location('shadow_runtime',ROOT/'Tools/UILayoutContract/runtime.py');rt=importlib.util.module_from_spec(s);s.loader.exec_module(rt);rt.OUT=OUT
phase=0;due=time.monotonic()+8;hub=None;report={}
def finish():
 unreal.unregister_slate_post_tick_callback(handle);unreal.WidgetService.stop_pie();unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InNewProcess')
def tick(dt):
 global phase,due,hub
 if time.monotonic()<due:return
 due=time.monotonic()+2
 try:
  w,pc=rt.context()
  if phase==0:unreal.SystemLibrary.execute_console_command(w,'open /Game/Maps/L_TestLevel',pc);due=time.monotonic()+15
  elif phase==1:
   for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True):
    if o.get_class().get_name()=='NUStorySlideshowWidget':o.on_skip_requested.broadcast()
  elif phase==2:unreal.SystemLibrary.execute_console_command(w,'nu.Menu.Open',pc)
  elif phase==3:
   hub=next(o for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True) if o.get_class().get_name()=='WBP_GameMenu_C');assert len(rt.obj(hub,'MainMenuTileView').get_displayed_entry_widgets())==9
   for n in ['HubProfileContactShadow','HubProfileMovingShade','HubDeskContactShadow','HubDeskMovingShade','HubDepthForeground']:assert rt.obj(hub,n).get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE
   unreal.GameplayStatics.set_game_paused(w,True);report['paused_world']=True
  elif phase==4:rt.shot('shadow_a');due=time.monotonic()+2
  elif phase==5:rt.shot('shadow_b')
  elif phase==6:
   unreal.GameplayStatics.set_game_paused(w,False);rt.obj(hub,'HubCloseButton').on_clicked.broadcast()
  elif phase==7:
   assert not hub.is_in_viewport();report['close']=True;report['nine_rows_and_shadow_hit_test_invisible']=True;(OUT/'runtime.json').write_text(json.dumps(report,indent=2));finish();return
  phase+=1
 except Exception:
  (OUT/'qa_error.txt').write_text(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');unreal.WidgetService.start_pie()
