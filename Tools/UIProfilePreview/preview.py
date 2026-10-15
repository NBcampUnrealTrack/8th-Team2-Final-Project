"""Transient design preview. Leaves PIE open for the user; no server requests."""
import unreal,time
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
def create(path):
 cls=unreal.load_class(None,path+'.'+path.split('/')[-1]+'_C')
 o=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(world,cls,pc));o.add_to_viewport(510);return o
def obj(w,n):return unreal.find_object(unreal.find_object(w,'WidgetTree_0'),n)
for cls in [unreal.NUCharacterProfileWidget,unreal.NUPlayerContextMenu]:
 for o in unreal.WidgetLibrary.get_all_widgets_of_class(world,cls,True):o.remove_from_parent()
profile=create('/Game/Developers/dbals/UI/WBP_CharacterProfile')
profile.call_method('HandleInventoryChanged')
menu=create('/Game/Developers/dongh/UI/WBP_PlayerContextMenu')
target=unreal.NUPlayerContextTarget();target.set_editor_property('display_name',unreal.Text('미리보기 캐릭터'));target.set_editor_property('action_mask',159)
menu.set_editor_property('target',target);menu.set_editor_property('target_display_name',unreal.Text('미리보기 캐릭터'));menu.on_target_changed()
obj(menu,'Text_PlayerName').set_text('미리보기 캐릭터')
for n in ['PartyKickButton','PartyLeaveButton']:obj(menu,n).set_visibility(unreal.SlateVisibility.COLLAPSED)
menu.set_alignment_in_viewport(unreal.Vector2D(0,0));menu.set_position_in_viewport(unreal.Vector2D(1040,180));menu.set_visibility(unreal.SlateVisibility.VISIBLE)
pc.set_editor_property('show_mouse_cursor',True);unreal.WidgetLibrary.set_input_mode_ui_only_ex(pc,profile)
due=time.monotonic()+3
def finish(dt):
 if time.monotonic()<due:return
 unreal.unregister_slate_post_tick_callback(handle)
 for n,v in [('NameText','미리보기 캐릭터'),('LevelText','Lv. 30'),('JobText','모험가')]:obj(profile,n).set_text(v)
 obj(profile,'EquipListBox').set_visibility(unreal.SlateVisibility.VISIBLE)
 profile.force_layout_prepass();menu.force_layout_prepass()
 print('PROFILE EQUIP CHILDREN',obj(profile,'EquipListBox').get_children_count())
 out=Path(unreal.Paths.project_saved_dir())/'UIProfilePreview';out.mkdir(exist_ok=True)
 unreal.SystemLibrary.execute_console_command(world,'shot showui filename="'+str(out/'styled_field.png').replace('\\','/')+'"',pc)
 unreal.ObjectReflectionService.set_property('/Script/UnrealEd.Default__LevelEditorPlaySettings','LastExecutedPlayModeType','PlayMode_InNewProcess')
handle=unreal.register_slate_post_tick_callback(finish)
