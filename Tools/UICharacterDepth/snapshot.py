import unreal,json,math
from pathlib import Path
OUT=Path(unreal.Paths.project_saved_dir())/'UICharacterDepth'
def capture(tag):
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert world
 shell=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True) if w.get_class().get_name()=='WBP_CharacterMenuBase_C')
 def obj(w,n):return unreal.find_object(unreal.find_object(w,'WidgetTree_0'),n)
 page=obj(shell,'NamedSlot_PageContent').get_content();exhibit=obj(page,'DepthExhibit');assert exhibit
 unreal.SystemLibrary.execute_console_command(world,'nu.CharacterDepth.Status')
 native=json.loads((OUT/'native_state.json').read_text(encoding='utf-8-sig'));assert native['state_count']==1,native
 geom=native['pages'][0];scale=geom['scale'];assert scale>0
 rect=[geom['origin_x'],geom['origin_y'],geom['width'],geom['height']]
 def state(w):
  t=w.get_editor_property('render_transform')
  return {'transform':[t.translation.x,t.translation.y,t.angle,t.scale.x,t.scale.y]}
 d={'tag':tag,'scale':scale,'native':native,'exhibit':rect,'idle':unreal.SystemLibrary.get_console_variable_int_value('nu.CharacterDepth.Idle'),'parallax':unreal.SystemLibrary.get_console_variable_int_value('nu.CharacterDepth.Parallax'),'enabled':page.get_is_enabled(),'moving':{n:state(obj(page,n)) for n in ['DepthFarTexture','CharacterIllustration','DepthRearOrbit','DepthFrontLine']},'fixed':{n:state(obj(page,n)) for n in ['CharacterPortrait','PortraitFloorLayer','MirrorFloorSurface','CharacterReflection','InfoContent']}}
 d['fixed'].update({n:state(obj(shell,n)) for n in ['NavCharacterButton','MajorMenuHeaderHeight','CloseButton']})
 (OUT/(tag+'.json')).write_text(json.dumps(d,indent=2))
 unreal.SystemLibrary.execute_console_command(world,'shot showui filename="'+str(OUT/tag).replace('\\','/')+'"',unreal.GameplayStatics.get_player_controller(world,0))
 print(json.dumps({'tag':tag,'scale':scale,'exhibit':d['exhibit'],'moving':{n:x['transform'][:3] for n,x in d['moving'].items()}},indent=2))
