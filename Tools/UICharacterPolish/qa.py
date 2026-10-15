import unreal,time,json,traceback
from pathlib import Path
OUT=Path('D:/UnrealEngine/ProjectM/Saved/UICharacterPolish/QA');OUT.mkdir(exist_ok=True);(OUT/'error.txt').unlink(missing_ok=True)
phase=0;due=time.monotonic()+6;shell=None;report={};pages=['Character','Equipment','Inventory','Stats','Traits','Naia']
R=unreal.ObjectReflectionService
oldmode=R.get_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType');oldthrottle=R.get_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground')
R.set_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground','False')
def context():
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert world
 return world,unreal.GameplayStatics.get_player_controller(world,0)
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 R.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType',oldmode);R.set_property('/Script/UnrealEd.Default__EditorPerformanceSettings','bThrottleCPUWhenNotForeground',oldthrottle)
 unreal.WidgetService.stop_pie()
def obj(w,n):return unreal.find_object(unreal.find_object(w,'WidgetTree_0'),n)
def tick(dt):
 global phase,due,shell
 if time.monotonic()<due:return
 due=time.monotonic()+1.5
 try:
  world,pc=context()
  if phase==0:unreal.SystemLibrary.execute_console_command(world,'open /Game/Maps/L_TestLevel',pc);due=time.monotonic()+12
  elif phase==1:
   for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True):
    if w.get_class().get_name()=='NUStorySlideshowWidget':w.on_skip_requested.broadcast()
  elif phase==2:
   unreal.SystemLibrary.execute_console_command(world,'nu.CharacterMenu.Preview Character',pc)
  elif 3<=phase<=14:
   i=(phase-3)//2
   if phase%2==1:
    shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True) if w.get_class().get_name()=='WBP_CharacterMenuBase_C')
    obj(shell,'Nav'+pages[i]+'Button').on_clicked.broadcast()
   else:
    assert shell.is_in_viewport()
    page=obj(shell,'NamedSlot_PageContent').get_content();assert page
    geom={}
    for n in ['MajorMenuHeaderHeight','CloseButton','PageContentPadding']:
     v=obj(shell,n).get_desired_size();t=obj(shell,n).get_editor_property('render_transform').translation;geom[n]=[t.x,t.y,v.x,v.y]
    report[pages[i]]={'page':page.get_class().get_name(),'shared_geometry':geom}
    unreal.SystemLibrary.execute_console_command(world,'shot showui filename="'+str(OUT/pages[i]).replace('\\','/')+'"',pc)
  elif phase==15:
   obj(shell,'NavInventoryButton').on_clicked.broadcast()
  elif phase==16:
   page=obj(shell,'NamedSlot_PageContent').get_content();grid=obj(page,'InventoryGrid')
   assert grid and grid.get_children_count()>0
   cell=grid.get_child_at(0);button=obj(cell,'CellButton');assert button
   button.on_clicked.broadcast();report['inventory_selection']=str(obj(page,'SelectedItemName').get_text())
   assert report['inventory_selection']!='선택한 아이템 없음'
  elif phase==17:
   unreal.SystemLibrary.execute_console_command(world,'shot showui filename="'+str(OUT/'InventorySelected').replace('\\','/')+'"',pc)
  elif phase==18:
   obj(shell,'NavCharacterButton').on_clicked.broadcast()
   shell.set_anchors_in_viewport(unreal.Anchors(unreal.Vector2D(),unreal.Vector2D()))
   shell.set_alignment_in_viewport(unreal.Vector2D())
   shell.set_desired_size_in_viewport(unreal.Vector2D(1200,900))
  elif phase==19:
   unreal.SystemLibrary.execute_console_command(world,'shot showui filename="'+str(OUT/'Character_4x3_area').replace('\\','/')+'"',pc)
   report['second_ratio']='Transient 1200x900 menu allocation inside original viewport; no OS viewport resize.'
  elif phase==20:
   obj(shell,'CloseButton').on_clicked.broadcast();due=time.monotonic()+1
  elif phase==21:
   assert not shell.is_in_viewport();report['header_close']=True
   (OUT/'runtime.json').write_text(json.dumps(report,indent=2));finish()
  phase+=1
 except Exception:
  (OUT/'error.txt').write_text(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
R.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');assert unreal.WidgetService.start_pie()
print('QA_STARTED')
