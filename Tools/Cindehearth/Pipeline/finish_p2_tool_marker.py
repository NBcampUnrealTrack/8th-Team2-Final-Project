import unreal
import json
from pathlib import Path
assert not unreal.WidgetService.is_pie_running()
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=sub.get_all_level_actors()
by_label={a.get_actor_label():a for a in actors}
out=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Pipeline/verification/p2_supervisor_tool.json'
if STAGE=='apply':
    assert not out.exists()
    sources=[by_label['CH_GormHouse_ToolSign_'+part] for part in ['Timber','Iron','Brass']]
    assert not any(a.get_actor_label().startswith('CH_GormP2_SupervisorTool') for a in actors)
    records=[]
    for source in sources:
        assert not source.static_mesh_component.get_editor_property('visible')
        a=sub.duplicate_actor(source)
        assert a
        label='CH_GormP2_SupervisorTool_'+source.get_actor_label().rsplit('_',1)[-1]
        a.set_actor_label(label)
        a.set_actor_location(unreal.Vector(-1200,-1746,8),False,False)
        a.set_actor_scale3d(unreal.Vector(.88,.88,.88))
        a.set_actor_hidden_in_game(False);a.set_is_temporarily_hidden_in_editor(False)
        a.set_actor_enable_collision(False)
        a.static_mesh_component.set_visibility(True)
        a.static_mesh_component.set_hidden_in_game(False)
        a.set_editor_property('tags',['Cindehearth','CH.GormHouse','CH.P2Surroundings'])
        records.append(dict(source=source.get_actor_label(),output=label,location=[-1200,-1746,8],scale=[.88,.88,.88]))
        out.write_text(json.dumps(records,indent=2),encoding='utf-8')
    print('SUPERVISOR TOOL COPIED',len(records))
elif STAGE=='fit':
    records=json.loads(out.read_text(encoding='utf-8'))
    for row in records:
        a=by_label[row['output']]
        print('BEFORE',row['output'],a.get_actor_scale3d(),a.get_actor_bounds(False)[1])
        row['previous_scale']=list(a.get_actor_scale3d().to_tuple())
        row['previous_location']=list(a.get_actor_location().to_tuple())
        row['scale']=[.24,.24,.24];row['location']=[-1204,-1770,40]
        a.set_actor_scale3d(unreal.Vector(*row['scale']))
        a.set_actor_location(unreal.Vector(*row['location']),False,False)
    out.write_text(json.dumps(records,indent=2),encoding='utf-8')
elif STAGE=='verify':
    for row in json.loads(out.read_text(encoding='utf-8')):
        a=by_label[row['output']]
        assert a.static_mesh_component.get_editor_property('visible')
        assert not a.static_mesh_component.get_editor_property('hidden_in_game')
        assert not by_label[row['source']].static_mesh_component.get_editor_property('visible')
        assert all(abs(x-y)<.01 for x,y in zip(a.get_actor_location().to_tuple(),row['location']))
        assert all(abs(x-y)<.01 for x,y in zip(a.get_actor_scale3d().to_tuple(),row['scale']))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    sub.select_nothing()
    print('SUPERVISOR TOOL VERIFIED')
