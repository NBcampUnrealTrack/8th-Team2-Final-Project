"""Dedicated non-gameplay inspection level; existing maps stay untouched."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();dest=root/'Docs/Libregrove/Unreal_Textured_V7_20260908'
levelpath='/Game/Libregrove/Textured_V7/Maps/L_Libregrove_AssetReview_V7'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def describe():
    records=[]
    for actor in actors.get_all_level_actors():
        c,e=actor.get_actor_bounds(False)
        records.append({'name':actor.get_name(),'label':actor.get_actor_label(),'class':actor.get_class().get_name(),'location':actor.get_actor_location().to_tuple(),'scale':actor.get_actor_scale3d().to_tuple(),'bounds_center':c.to_tuple(),'bounds_extent':e.to_tuple()})
    return records

if OP=='create':
    assert not unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/Libregrove/Textured_V7/Maps',True)
    assert levels.new_level_from_template(levelpath,'/Engine/Maps/Templates/Template_Default')
    result=describe()
    p=dest/'review_template.json'
    with p.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
    print(json.dumps(result))
elif OP=='populate':
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Libregrove_AssetReview_V7'
    baseline=json.loads((dest/'review_template.json').read_text(encoding='utf-8'))
    assert {a.get_name() for a in actors.get_all_level_actors()}=={a['name'] for a in baseline}
    floor=next(a for a in baseline if a['name']=='Floor_0');ground_z=floor['bounds_center'][2]+floor['bounds_extent'][2]
    rows=json.loads((dest/'export_manifest.json').read_text(encoding='utf-8'))
    for i,row in enumerate(rows):
        qa=json.loads((dest/'unreal_qa'/(row['id']+'.json')).read_text(encoding='utf-8'));assert qa['passed']
        mesh=unreal.load_asset(qa['path']);assert mesh
        actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector((i%3-1)*850,(i//3-1)*850,ground_z),unreal.Rotator())
        actor.set_actor_label('LGV7_'+row['id']);actor.set_folder_path('Libregrove/ImportedModels')
        assert actor.static_mesh_component.set_static_mesh(mesh)
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        _,extent=actor.get_actor_bounds(False);assert extent.x>0 and extent.y>0 and extent.z>0
    actors.select_nothing();levels.editor_invalidate_viewports()
    result=describe()
    with (dest/'review_actors.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
    print(json.dumps(result))
