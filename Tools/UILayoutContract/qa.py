import unreal,time,json,traceback,importlib.util
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/UILayoutContract'
s=importlib.util.spec_from_file_location('contract_runtime',ROOT/'Tools/UILayoutContract/runtime.py');rt=importlib.util.module_from_spec(s);s.loader.exec_module(rt);rt.OUT=OUT
phase=0;due=time.monotonic()+8;report={};hub=None;hover_style=None
def finish():
 unreal.unregister_slate_post_tick_callback(handle);unreal.WidgetService.stop_pie();unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InNewProcess')
def tick(dt):
 global phase,due,hub,hover_style
 if time.monotonic()<due:return
 due=time.monotonic()+2
 try:
  if phase==0:rt.show_options();rt.options.remove_from_parent();rt.options.add_to_viewport(100000);rt.tab(1)
  elif phase==1:rt.shot('options');report['options_buttons']=[rt.obj(rt.options,n).slot.get_size().x for n in ['DefaultsButton','ApplyButton','CloseButton']];assert report['options_buttons']==[222,150,150]
  elif phase==2:rt.obj(rt.options,'SettingsGraphicsScroll').scroll_to_end()
  elif phase==3:rt.shot('options_bottom');assert rt.obj(rt.options,'SettingsGraphicsScroll').get_scroll_offset()>100
  elif phase==4:rt.tab(3);rt.open_keys();rt.options.set_visibility(unreal.SlateVisibility.COLLAPSED);rt.keys.remove_from_parent();rt.keys.add_to_viewport(100001)
  elif phase==5:
   rt.shot('keys');report['key_buttons']=[rt.obj(rt.keys,n).slot.get_size().x for n in ['DefaultsButtonBox','ApplyButtonBox','FooterCloseButton']];assert report['key_buttons']==[222,150,150]
   for n in ['DefaultsButton','ApplyButton']:assert 'WBP_SettingsFooter' in rt.obj(rt.keys,n).get_class().get_name()
  elif phase==6:rt.obj(rt.keys,'RowScroll').scroll_to_end()
  elif phase==7:rt.shot('keys_bottom')
  elif phase==8:
   rt.obj(rt.keys,'FooterCloseButton').on_clicked.broadcast();assert not rt.keys.is_in_viewport();rt.options.set_visibility(unreal.SlateVisibility.VISIBLE);rt.obj(rt.options,'CloseButton').on_clicked.broadcast()
   w,pc=rt.context();unreal.SystemLibrary.execute_console_command(w,'open /Game/Maps/L_TestLevel',pc);due=time.monotonic()+15
  elif phase==9:
   w,pc=rt.context()
   for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True):
    if o.get_class().get_name()=='NUStorySlideshowWidget':o.on_skip_requested.broadcast()
  elif phase==10:
   w,pc=rt.context();unreal.SystemLibrary.execute_console_command(w,'nu.Menu.Open',pc)
  elif phase==11:
   w,pc=rt.context();hub=next(o for o in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,True) if o.get_class().get_name()=='WBP_GameMenu_C');rows=rt.obj(hub,'MainMenuTileView').get_displayed_entry_widgets();assert len(rows)==9;report['hub_rows']=len(rows)
   pc.set_mouse_location(700,400)
  elif phase==12:rt.shot('hub')
  elif phase==13:
   rows=rt.obj(hub,'MainMenuTileView').get_displayed_entry_widgets();b=rt.obj(rows[0],'Btn_Common');hover_style=b.get_editor_property('widget_style').copy();st=hover_style.copy();st.normal=st.hovered;b.set_style(st)
   rt.obj(rows[0],'Txt_Label').set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(.018,.03,.045,1)))
  elif phase==14:rt.shot('hub_hover_material_preview')
  elif phase==15:
   rt.obj(hub,'HubCloseButton').on_clicked.broadcast();assert not hub.is_in_viewport();report['close']=True;report['hover_material_preview_only']=True;(OUT/'runtime.json').write_text(json.dumps(report,indent=2));finish();return
  phase+=1
 except Exception:
  (OUT/'qa_error.txt').write_text(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');unreal.WidgetService.start_pie()
