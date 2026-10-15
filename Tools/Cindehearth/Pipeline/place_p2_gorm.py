import unreal
import json
from pathlib import Path

def place():
    base='/Game/Cindehearth/Tripo/GormHouse_P2_v001'
    mesh=unreal.load_asset(base+'/CH_GormP2_Game_v001/StaticMeshes/CH_GormP2_Game_v001')
    mat=unreal.load_asset(base+'/MI_CH_GormP2_Pixel')
    diag=unreal.MaterialNodeService.get_material_diagnostics(base+'/M_CH_P2_Pixel')
    assert diag.success and diag.is_compiled_ok,list(diag.compile_errors)
    assert len(diag.referenced_texture_paths)==2,list(diag.referenced_texture_paths)
    assert mesh and mat and mesh.get_num_triangles(0)==3363
    assert not unreal.WidgetService.is_pie_running()
    assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    current=actors.get_all_level_actors()
    assert not any(a.get_actor_label()=='CH_GormHouse_P2' for a in current)
    shell=['CH_GormHouse_'+s for s in ['Stone','Timber','Iron','Brass','StoneLight','RoofPatch','Roof','Window','Dark','ToolSign_Timber','ToolSign_Iron','ToolSign_Brass']]
    old=[a for a in current if a.get_actor_label() in shell]
    assert len(old)==len(shell)
    backup=[]
    for a in old:
        c=a.static_mesh_component
        backup.append(dict(label=a.get_actor_label(),hidden=a.get_editor_property('hidden'),editor_hidden=a.is_temporarily_hidden_in_editor(),visible=c.get_editor_property('visible'),component_hidden=c.get_editor_property('hidden_in_game'),collision=a.get_actor_enable_collision()))
    evidence=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Pipeline/verification'
    assert not (evidence/'p2_replaced_shell.json').exists()
    (evidence/'p2_replaced_shell.json').write_text(json.dumps(backup,indent=2),encoding='utf-8')
    with unreal.ScopedEditorTransaction('Cindehearth Gorm P2 sample'):
        body=mesh.get_editor_property('body_setup')
        assert body
        body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        mesh.set_material(0,mat)
        a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-1125,-1620,0),unreal.Rotator())
        a.set_actor_label('CH_GormHouse_P2')
        a.set_folder_path('Cindehearth/Lower/GormP2')
        assert a.static_mesh_component.set_static_mesh(mesh)
        a.static_mesh_component.set_collision_profile_name('BlockAll')
        a.tags=['CH.P2SmartMesh','CH.GormHouse']
        for original in old:
            original.modify()
            original.static_mesh_component.set_visibility(False,True)
            original.static_mesh_component.set_hidden_in_game(True,True)
            original.set_actor_hidden_in_game(True)
            original.set_is_temporarily_hidden_in_editor(True)
            original.set_actor_enable_collision(False)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    print('P2 PLACED',a.get_path_name(),'bounds',a.get_actor_bounds(False),'triangles',mesh.get_num_triangles(0))

place()
