"""Paused-world entrance/exit, reopening and delayed action integration check."""
import unreal,time,json,traceback,importlib.util
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/UIHubMotion';OUT.mkdir(exist_ok=True)
s=importlib.util.spec_from_file_location('motion_rt',ROOT/'Tools/UILayoutContract/runtime.py');rt=importlib.util.module_from_spec(s);s.loader.exec_module(rt);rt.OUT=OUT
phase=0;due=time.monotonic()+8;opened=0;hub=None;samples=[];report={};shots=set()
old_throttle=unreal.ObjectReflectionService.get_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground')
unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground','False')
def finish():
 unreal.unregister_slate_post_tick_callback(handle);unreal.WidgetService.stop_pie()
 unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InNewProcess')
 unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground',old_throttle)
def find_hub(w):
 return next(o for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True) if o.get_class().get_name()=='WBP_GameMenu_C')
def state():
 data={}
 for n in ['HubLeftFit','HubFit','HubDeskFit','HubDepthForeground']:
  o=rt.obj(hub,n);v=o.get_editor_property('render_transform').translation;data[n]=[o.get_render_opacity(),v.x,v.y]
 rows=rt.obj(hub,'MainMenuTileView').get_displayed_entry_widgets();data['rows']=[o.get_render_opacity() for o in rows]
 return data
def tick(dt):
 global phase,due,hub,opened
 if time.monotonic()<due:return
 now=time.monotonic();due=now+.02
 try:
  w,pc=rt.context()
  if phase==0:unreal.SystemLibrary.execute_console_command(w,'open /Game/Maps/L_TestLevel',pc);due=now+15;phase=1
  elif phase==1:
   for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True):
    if o.get_class().get_name()=='NUStorySlideshowWidget':o.on_skip_requested.broadcast()
   phase=11;due=now+2
  elif phase==11:
   unreal.SystemLibrary.execute_console_command(w,'nu.Menu.Open',pc);hub=find_hub(w);phase=12;due=now+2
  elif phase==12:
   rt.obj(hub,'HubCloseButton').on_clicked.broadcast();phase=2;due=now+.4
  elif phase==2:
   unreal.GameplayStatics.set_game_paused(w,True);unreal.SystemLibrary.execute_console_command(w,'nu.Menu.Open',pc);hub=find_hub(w);opened=now
   report['initial']=state();assert report['initial']['HubLeftFit'][0]==0;phase=3
  elif phase==3:
   age=now-opened;snap=state();samples.append({'t':age,**snap})
   for threshold,label in [(.06,'enter_early'),(.20,'enter_middle'),(.5,'enter_complete')]:
    if age>=threshold and label not in shots:rt.shot(label);shots.add(label);break
   if age>.65:
    assert all(abs(snap[n][0]-1)<.001 and abs(snap[n][1])+abs(snap[n][2])<.001 for n in ['HubLeftFit','HubFit','HubDeskFit','HubDepthForeground'])
    assert len(snap['rows'])==9 and all(abs(v-1)<.001 for v in snap['rows'])
    assert any(len(v['rows'])==9 and v['rows'][0]>v['rows'][-1] for v in samples)
    for row in rt.obj(hub,'MainMenuTileView').get_displayed_entry_widgets():assert abs(rt.obj(row,'Btn_Common').get_editor_property('render_transform').scale.x-.75)<.001
    report['paused_entrance_stagger_rest']=True;phase=4;due=now+.1
  elif phase==4:
   rt.obj(hub,'HubCloseButton').on_clicked.broadcast();assert hub.is_in_viewport();report['exit_deferred']=True;phase=5;due=now+.06
  elif phase==5:report['exit_mid']=state();rt.shot('exit_middle');phase=6;due=now+.25
  elif phase==6:
   assert not hub.is_in_viewport();unreal.SystemLibrary.execute_console_command(w,'nu.Menu.Open',pc);hub=find_hub(w);assert hub.get_is_enabled();phase=7;due=now+.05
  elif phase==7:rt.obj(hub,'HubCloseButton').on_clicked.broadcast();phase=8;due=now+.25
  elif phase==8:
   assert not hub.is_in_viewport();report['interrupted_open_closes']=True
   unreal.SystemLibrary.execute_console_command(w,'nu.Menu.Open',pc);hub=find_hub(w);phase=9;due=now+.65
  elif phase==9:rt.obj(hub,'OptionsButton').on_common_clicked.broadcast();assert hub.is_in_viewport();phase=10;due=now+.3
  elif phase==10:
   assert not hub.is_in_viewport();options=next(o for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True) if o.get_class().get_name()=='WBP_OptionsWindow_C');rt.obj(options,'HeaderCloseButton').on_clicked.broadcast();report['delayed_options_action']=True
   unreal.GameplayStatics.set_game_paused(w,False);(OUT/'runtime.json').write_text(json.dumps({'report':report,'samples':samples},indent=2));finish()
 except Exception:
  (OUT/'error.txt').write_text(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');unreal.WidgetService.start_pie()
