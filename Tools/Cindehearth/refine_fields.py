import unreal
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
idx={a.get_actor_label():a for a in AS.get_all_level_actors()}
for i in range(1,5):
    old=idx['CH_AshslopeBench_'+str(i)+'_Soil']
    old.set_actor_hidden_in_game(True);old.set_is_temporarily_hidden_in_editor(True);old.set_actor_enable_collision(False)
    cx=-94-i*25;z=3.5+i*2.5;cy=-8 if (i-1)%2 else 30
    # Cut a six-metre notch into the higher ledge so the slope reaches its top.
    pieces=[(cx-12.5,cx+5,-13,37),(cx+5,cx+12.5,-13,cy-3),(cx+5,cx+12.5,cy+3,37)]
    for j,(x0,x1,y0,y1) in enumerate(pieces):
        name=f'CH_AshslopeNotch_{i}_{j}'
        a=idx.get(name)
        if not a:
            a=AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
            a.set_actor_label(name);a.set_folder_path('Cindehearth/Ashslope/Terrain')
            a.tags=[unreal.Name('Cindehearth')]
            a.static_mesh_component.set_static_mesh(old.static_mesh_component.static_mesh)
        a.set_actor_location(unreal.Vector((x0+x1)*22.5,(y0+y1)*22.5,(z-.5)*45),False,False)
        a.set_actor_scale3d(unreal.Vector((x1-x0)*.45,(y1-y0)*.45,.45))
LS.save_current_level();print('ASHSLOPE NOTCHES SAVED')
