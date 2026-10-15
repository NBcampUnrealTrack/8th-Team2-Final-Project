import unreal,json
from pathlib import Path
OUT=Path(unreal.Paths.project_saved_dir())/'UISettingsReference'
options=None
keys=None

def context():
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world
    return world,unreal.GameplayStatics.get_player_controller(world,0)

def obj(w,name):
    tree=unreal.find_object(w,'WidgetTree_0')
    assert tree,(w.get_name(),'tree')
    result=unreal.find_object(tree,name)
    assert result,name
    return result

def shot(name):
    world,pc=context()
    unreal.SystemLibrary.execute_console_command(world,'shot showui filename="'+str(OUT/(name+'.png')).replace('\\','/')+'"',pc)

def show_options():
    global options
    world,pc=context()
    cls=unreal.load_class(None,'/Game/Developers/dbals/UI/WBP_OptionsWindow.WBP_OptionsWindow_C')
    options=unreal.get_default_object(unreal.WidgetLibrary).call_method('Create',(world,cls,pc))
    options.add_to_viewport(500)
    pc.set_editor_property('show_mouse_cursor',True)
    print('OPTIONS',options.get_path_name())

def tab(i):
    obj(options,['DisplayTabButton','GraphicsTabButton','SoundTabButton','ControlsTabButton'][i]).on_clicked.broadcast()
    assert obj(options,'TabSwitcher').get_active_widget_index()==i
    assert obj(options,'SettingsTabMark'+str(i)).get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE
    for j in range(4):
        assert (obj(options,'SettingsTabMark'+str(j)).get_visibility()==unreal.SlateVisibility.COLLAPSED)==(j!=i)

def open_keys():
    global keys
    obj(options,'KeyBindingsButton').on_clicked.broadcast()
    world,pc=context()
    keys=next(w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,True) if w.get_class().get_name()=='WBP_KeyBindingsWindow_C')
    print('KEY_ROWS',obj(keys,'RowBox').get_children_count())
