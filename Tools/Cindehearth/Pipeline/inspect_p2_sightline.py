import unreal
import json
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows=[]
for a in actors:
    label=a.get_actor_label()
    if label.startswith(('CH_MinersHome_06','CH_MinersHome_07','CH_GormHouse','CH_RetainingWalls','CH_Stairs_-30_-18','CH_Terrace1')):
        c=a.get_component_by_class(unreal.StaticMeshComponent)
        center,extent=a.get_actor_bounds(False)
        rows.append(dict(label=label,location=list(a.get_actor_location().to_tuple()),rotation=str(a.get_actor_rotation()),bounds_center=list(center.to_tuple()),bounds_extent=list(extent.to_tuple()),visible=c.get_editor_property('visible') if c else None,mesh=c.static_mesh.get_path_name() if c and c.static_mesh else None,material=c.get_material(0).get_path_name() if c and c.get_material(0) else None))
print(json.dumps(rows,indent=2))
