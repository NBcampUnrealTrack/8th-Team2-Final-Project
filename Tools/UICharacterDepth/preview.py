"""Start an in-editor runnable preview and record read-only transform samples.
Static by default. Enable the two nu.CharacterDepth toggles for the moving comparison.
Stop PIE to restore launch preferences and toggle values.
"""
import unreal,time,json,traceback
from pathlib import Path
OUT=Path(unreal.Paths.project_saved_dir())/'UICharacterDepth';OUT.mkdir(exist_ok=True)
(OUT/'ready.json').unlink(missing_ok=True);(OUT/'error.txt').unlink(missing_ok=True)
R=unreal.ObjectReflectionService
oldmode=R.get_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType');oldthrottle=R.get_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground')
oldidle=unreal.SystemLibrary.get_console_variable_int_value('nu.CharacterDepth.Idle');oldparallax=unreal.SystemLibrary.get_console_variable_int_value('nu.CharacterDepth.Parallax')
R.set_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground','False')
phase=0;due=time.monotonic()+6;started=time.monotonic();samples=[];shell=None
moving=['DepthFarTexture','CharacterIllustration','DepthRearOrbit','DepthFrontLine'];fixed=['CharacterPortrait','PortraitFloorLayer','MirrorFloorSurface','CharacterReflection','InfoContent'];baseline={}
def obj(w,n):return unreal.find_object(unreal.find_object(w,'WidgetTree_0'),n)
def state(w):
 t=w.get_editor_property('render_transform');return [t.translation.x,t.translation.y,t.angle,t.scale.x,t.scale.y]
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 R.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType',oldmode);R.set_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground',oldthrottle)
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 unreal.SystemLibrary.execute_console_command(world,'nu.CharacterDepth.Idle '+str(oldidle));unreal.SystemLibrary.execute_console_command(world,'nu.CharacterDepth.Parallax '+str(oldparallax))
 (OUT/'samples.json').write_text(json.dumps(samples,indent=2));print('DEPTH_PREVIEW_FINISHED')
def tick(dt):
 global phase,due,shell,baseline
 if time.monotonic()<due:return
 due=time.monotonic()+.10
 try:
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not world:
   if phase>=3:finish();return
   raise RuntimeError('PIE world unavailable')
  pc=unreal.GameplayStatics.get_player_controller(world,0)
  if phase==0:unreal.SystemLibrary.execute_console_command(world,'open /Game/Maps/L_TestLevel',pc);phase=1;due=time.monotonic()+12
  elif phase==1:
   for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True):
    if w.get_class().get_name()=='NUStorySlideshowWidget':w.on_skip_requested.broadcast()
   unreal.SystemLibrary.execute_console_command(world,'nu.CharacterDepth.Idle 0',pc);unreal.SystemLibrary.execute_console_command(world,'nu.CharacterDepth.Parallax 0',pc)
   unreal.SystemLibrary.execute_console_command(world,'nu.CharacterMenu.Preview Character',pc);phase=2;due=time.monotonic()+3
  elif phase==2:
   shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True) if w.get_class().get_name()=='WBP_CharacterMenuBase_C')
   page=obj(shell,'NamedSlot_PageContent').get_content();assert obj(page,'DepthExhibit')
   baseline={n:state(obj(page,n)) for n in fixed};baseline.update({n:state(obj(shell,n)) for n in ['NavCharacterButton','MajorMenuHeaderHeight','CloseButton']})
   unreal.SystemLibrary.execute_console_command(world,'shot showui filename="'+str(OUT/'static').replace('\\','/')+'"',pc)
   (OUT/'ready.json').write_text(json.dumps({'baseline':baseline,'page':page.get_path_name(),'viewport':'floating PIE','ready':True},indent=2));phase=3
  else:
   if not shell.is_in_viewport():samples.append({'time':time.monotonic()-started,'closed':True});return
   page=obj(shell,'NamedSlot_PageContent').get_content()
   if obj(page,'DepthExhibit'):
    snap={'time':time.monotonic()-started,'idle':unreal.SystemLibrary.get_console_variable_int_value('nu.CharacterDepth.Idle'),'parallax':unreal.SystemLibrary.get_console_variable_int_value('nu.CharacterDepth.Parallax'),'enabled':page.get_is_enabled(),'moving':{n:state(obj(page,n)) for n in moving},'fixed':{n:state(obj(page,n)) for n in fixed}}
    snap['fixed'].update({n:state(obj(shell,n)) for n in ['NavCharacterButton','MajorMenuHeaderHeight','CloseButton']});samples.append(snap)
   if len(samples)%20==0:(OUT/'samples.json').write_text(json.dumps(samples,indent=2))
  if time.monotonic()-started>240:unreal.WidgetService.stop_pie()
 except Exception:
  (OUT/'error.txt').write_text(traceback.format_exc());finish();unreal.WidgetService.stop_pie()
handle=unreal.register_slate_post_tick_callback(tick)
R.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');assert unreal.WidgetService.start_pie();print('DEPTH_PREVIEW_STARTING')
