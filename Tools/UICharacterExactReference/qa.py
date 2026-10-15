import unreal,time,json,traceback
from pathlib import Path
OUT=Path('D:/UnrealEngine/ProjectM/Saved/UICharacterExactReference/QA');OUT.mkdir(exist_ok=True);(OUT/'error.txt').unlink(missing_ok=True)
phase=0;due=time.monotonic()+6;shell=None;report={};pages=['Character','Equipment','Inventory','Stats','Traits','Naia'];motion_page=None
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
def particle(w,i):return obj(w,('ReferenceParticle_%02d'%i) if i<12 else ('ReferenceAirParticle_%02d'%(i-12)))
def tick(dt):
 global phase,due,shell,motion_page
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
    expected='ReferenceControls' if pages[i]=='Character' else 'ShellCanvas'
    assert obj(shell,'NavCharacterButton').get_parent().get_name()==expected
    assert obj(shell,'NamedSlot_PageContent').get_parent().get_name()==('ReferenceControls' if pages[i]=='Character' else 'PageDesignSize')
    assert (obj(shell,'ReferenceViewport').get_visibility()!=unreal.SlateVisibility.COLLAPSED)==(pages[i]=='Character')
    if pages[i]=='Character':
     assert obj(page,'ReferenceExactCanvas')
     assert obj(page,'RefCopyIdButton').get_path_name() in R.get_property(page.get_path_name(),'RefCopyIdButton')
     assert obj(page,'RefProfileName').get_text()==obj(page,'NameText').get_text()
     sz=obj(page,'CharacterPortrait').slot.get_size();assert abs(sz.x/sz.y-.75)<.0001
     geometry=unreal.WidgetLayoutLibrary.get_viewport_size(world)
     design_width=obj(shell,'ReferenceDesignSize').get_editor_property('width_override')
     assert abs(design_width-max(1448,geometry.x/geometry.y*1086))<.2
     extra=design_width-1448
     card_left=design_width-24-519*1.1
     exhibit_center=(236*1.1*1.2+card_left)*.5
     exhibit_shift=exhibit_center-595
     assert abs(obj(page,'NameText').slot.get_position().x-(card_left+(1120-904)*1.1))<.2
     portrait=obj(page,'CharacterPortrait');reflection=obj(page,'CharacterReflection')
     assert abs(sz.x-578.1)<.01 and abs(sz.y-770.8)<.01
     assert abs(portrait.slot.get_position().x-(305.95+exhibit_shift))<.2
     assert abs(portrait.slot.get_position().y+sz.y*((795+31.166)/940)-823)<.02
     assert abs(reflection.slot.get_position().y-823)<.01
     assert abs(reflection.slot.get_size().x-sz.x)<.01
     assert abs(obj(page,'ReferenceOuterRightFrame').slot.get_position().x-(1437+extra))<.2
     assert abs(obj(page,'ReferenceSource_Close').slot.get_position().x-obj(shell,'CloseButton').slot.get_position().x)<.2
     assert abs(obj(page,'ReferenceExhibitBase').slot.get_position().y-28)<.01
     for name in ['ReferenceResponsiveBackground','ReferenceWideAtmosphere','ReferenceExhibitBase','ReferenceOuterRightFrame']:
      assert obj(page,name).get_visibility()==unreal.SlateVisibility.COLLAPSED
     rail=obj(page,'ReferenceRailBase');assert abs(rail.slot.get_position().x)<.01
     assert abs(rail.slot.get_size().x-236*1.1*1.2)<.01
     for prefix in ['Character','Equipment','Inventory','Stats','Traits','Naia']:
      icon=obj(page,'ReferenceSource_'+prefix+'Icon');label=obj(page,'ReferenceSource_'+prefix+'Label')
      ip=icon.slot.get_position();lp=label.slot.get_position();isz=icon.slot.get_size();lsz=label.slot.get_size()
      assert abs((ip.x+lp.x+lsz.x*1.1)*.5-rail.slot.get_size().x*.5)<.05,prefix
      assert abs(lp.x-ip.x-isz.x*1.1-12*1.1)<.05,prefix
      assert abs(ip.y+isz.y*.55-lp.y-lsz.y*.55)<.05,prefix
     assert abs(obj(page,'ReferenceSource_ProfileEmblem').slot.get_position().x-(rail.slot.get_position().x+rail.slot.get_size().x+24))<.05
     card_right=obj(page,'ReferenceCardBase').slot.get_position().x+(1423-906)*1.1
     assert abs(design_width-card_right-24)<.2
     for name in ['ReferenceCardBase','NameText','RefCopyIdButton','ReferenceSource_Emblem']:
      scale=obj(page,name).get_editor_property('render_transform').scale;assert abs(scale.x-1.1)<.001 and abs(scale.y-1.1)<.001
     nav=obj(shell,'NavCharacterButton');arrow=obj(page,'ReferenceSource_SelectedArrow')
     assert abs(nav.slot.get_position().x-arrow.slot.get_position().x)<.01 and abs(nav.slot.get_position().y-arrow.slot.get_position().y)<.01
     assert abs(nav.get_editor_property('render_transform').scale.x-1.32)<.001
     bg=obj(page,'ReferenceUnifiedBackground');assert bg.get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE
     bp=bg.slot.get_position();bs=bg.slot.get_size();assert abs(bs.x/bs.y-3)<.0001
     assert abs(bp.x+879/2172*bs.x-exhibit_center)<.05 and abs(bp.y+535/724*bs.y-823)<.05
     assert bp.x<=0 and bp.x+bs.x>=design_width and bp.y<=0 and bp.y+bs.y>=1086
     for j in range(8):
      w=particle(page,j+12);position=w.slot.get_position();size=w.slot.get_size()
      assert 236 < position.x+size.x*.5 < design_width
      assert abs(size.x-size.y)<.01
     report['responsive_width']={'viewport':[geometry.x,geometry.y],'design_width':design_width,'uniform_portrait_aspect':sz.x/sz.y}
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
   page=obj(shell,'NamedSlot_PageContent').get_content()
   assert abs(obj(shell,'ReferenceDesignSize').get_editor_property('width_override')-1448)<.2
   assert abs(obj(page,'CharacterPortrait').slot.get_position().x-(305.95+(236*1.1*1.2+1448-24-519*1.1)*.5-595))<.2
   unreal.SystemLibrary.execute_console_command(world,'shot showui filename="'+str(OUT/'Character_4x3_area').replace('\\','/')+'"',pc)
   report['second_ratio']='Transient 1200x900 menu allocation inside original viewport; no OS viewport resize.'
  elif phase==20:
   obj(shell,'CloseButton').on_clicked.broadcast();due=time.monotonic()+1
  elif phase==21:
   assert not shell.is_in_viewport();assert obj(shell,'NamedSlot_PageContent').get_parent().get_name()=='PageDesignSize';report['header_close']=True;report['restoration_after_close']=True
  elif phase==22:
   unreal.SystemLibrary.execute_console_command(world,'nu.CharacterMenu.Preview Character',pc)
  elif phase==23:
   shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True) if w.get_class().get_name()=='WBP_CharacterMenuBase_C')
   motion_page=obj(shell,'NamedSlot_PageContent').get_content()
   assert abs(obj(motion_page,'NameText').get_editor_property('render_transform').scale.x-1.1)<.001
   assert abs(obj(shell,'NavCharacterButton').get_editor_property('render_transform').scale.x-1.32)<.001
   unreal.SystemLibrary.execute_console_command(world,'nu.CharacterReference.Particles 0',pc)
  elif phase==24:
   for i in range(20):
    t=particle(motion_page,i).get_editor_property('render_transform').translation
    assert t.x==0 and t.y==0
   report['particles_disabled_static']=True
   dust=obj(motion_page,'ReferenceAtmosphereDust').get_editor_property('render_transform').translation;assert dust.x==0 and dust.y==0
   report['fixed_positions']={n:[obj(motion_page,n).slot.get_position().x,obj(motion_page,n).slot.get_position().y] for n in ['CharacterPortrait','CharacterReflection','NameText','ExperienceBar']}
   unreal.SystemLibrary.execute_console_command(world,'nu.CharacterReference.Particles 1',pc);due=time.monotonic()+4
  elif phase==25:
   motions=[]
   for i in range(20):
    w=particle(motion_page,i);t=w.get_editor_property('render_transform').translation
    assert abs(t.x)<=1.651 and abs(t.y)<=3.001
    assert w.get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE
    assert .679<=w.get_render_opacity()<=1.001
    motions.append([t.x,t.y,w.get_render_opacity()])
   assert any(abs(x)+abs(y)>.1 for x,y,a in motions)
   for n,b in report['fixed_positions'].items():
    w=obj(motion_page,n);t=w.get_editor_property('render_transform').translation;p=w.slot.get_position();assert [p.x,p.y]==b and t.x==0 and t.y==0
   report['particles_active']=motions
   dust=obj(motion_page,'ReferenceAtmosphereDust').get_editor_property('render_transform').translation;assert abs(dust.x)<=.541 and abs(dust.y)<=.541
   unreal.SystemLibrary.execute_console_command(world,'shot showui filename="D:/UnrealEngine/ProjectM/Saved/UICharacterAspect/final"',pc)
  elif phase==26:
   obj(shell,'NavEquipmentButton').on_clicked.broadcast()
  elif phase==27:
   for i in range(20):
    w=particle(motion_page,i);t=w.get_editor_property('render_transform').translation
    assert t.x==0 and t.y==0 and w.get_render_opacity()==1
   report['particles_stop_reset_on_tab_exit']=True
   obj(shell,'CloseButton').on_clicked.broadcast()
  elif phase==28:
   assert not shell.is_in_viewport()
   (OUT/'runtime.json').write_text(json.dumps(report,indent=2));finish()
  phase+=1
 except Exception:
  (OUT/'error.txt').write_text(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
R.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InEditorFloating');assert unreal.WidgetService.start_pie()
print('QA_STARTED')



