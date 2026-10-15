"""Local, reversible blockout corrections found from the actual player camera."""
import unreal
import json
from pathlib import Path

sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
actors=sub.get_all_level_actors()
journal=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Pipeline/verification/p2_sightline_changes.json'
if STAGE=='apply':
    assert not journal.exists(), 'Inspect journal instead of repeating the edit'
    moved=[a for a in actors if a.get_actor_label().startswith('CH_MinersHome_06_')]
    assert len(moved)==15, len(moved)
    assert next(a for a in moved if a.get_actor_label()=='CH_MinersHome_06_Stone').get_actor_location().x==-1305
    assert not any(a.get_actor_label().startswith('CH_GormStairInfill') for a in actors)
    cube=unreal.load_asset('/Engine/BasicShapes/Cube')
    stone=unreal.load_asset('/Game/Cindehearth/Materials/MI_CH_Stone')
    cap=unreal.load_asset('/Game/Cindehearth/Materials/MI_CH_StoneLight')
    assert cube and stone and cap
    data=dict(status='started',move_delta=[-157.5,0,0],before=[dict(label=a.get_actor_label(),location=list(a.get_actor_location().to_tuple())) for a in moved],created=[])
    journal.write_text(json.dumps(data,indent=2),encoding='utf-8')
    for a in moved:
        p=a.get_actor_location();p.x-=157.5
        a.set_actor_location(p,False,False)
    for side,x in [('West',-945),('East',-675)]:
        for part,z,dimensions,mat in [('Wall',76.5,(90,38.25,153),stone),('Cap',156.6,(90,44.1,8.1),cap)]:
            a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,-1350,z),unreal.Rotator())
            label='CH_GormStairInfill_'+side+'_'+part
            a.set_actor_label(label)
            a.set_editor_property('tags',['Cindehearth','CH.Terrain','CH.P2Surroundings'])
            a.set_folder_path('Cindehearth/Terrain')
            c=a.static_mesh_component;c.set_static_mesh(cube);c.set_material(0,mat)
            c.set_collision_profile_name('BlockAll')
            a.set_actor_scale3d(unreal.Vector(*(v/100 for v in dimensions)))
            data['created'].append(dict(label=label,location=list(a.get_actor_location().to_tuple()),dimensions=dimensions))
            journal.write_text(json.dumps(data,indent=2),encoding='utf-8')
    data['status']='applied'
    journal.write_text(json.dumps(data,indent=2),encoding='utf-8')
    print('MOVED',len(moved),'ADDED',len(data['created']))
elif STAGE=='verify':
    data=json.loads(journal.read_text(encoding='utf-8'))
    by_label={a.get_actor_label():a for a in actors}
    for row in data['before']:
        p=by_label[row['label']].get_actor_location()
        assert abs(p.x-(row['location'][0]-157.5))<.01
        assert abs(p.y-row['location'][1])<.01
    for row in data['created']:
        a=by_label[row['label']]
        _,extent=a.get_actor_bounds(False)
        assert all(abs(v*2-d)<.02 for v,d in zip(extent.to_tuple(),row['dimensions']))
    data['readback_passed']=True
    journal.write_text(json.dumps(data,indent=2),encoding='utf-8')
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    sub.select_nothing()
    print('SIGHTLINE READBACK PASSED')
elif STAGE=='capture':
    # Pose measured from the running game's PlayerCameraManager, not a new camera setup.
    pos=unreal.Vector(-1125,-3365.4021031123584,318.9096501049541)
    rot=unreal.Rotator(pitch=-8,yaw=90)
    unreal.ViewportService.set_camera_location(pos);unreal.ViewportService.set_camera_rotation(rot)
    unreal.ViewportService.set_game_view(True)
    print(unreal.ScreenshotService.capture_from_pose(pos,rot,'CH_Gorm_P2_clear_player_camera',30,False,False))
