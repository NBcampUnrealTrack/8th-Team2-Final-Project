import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not isinstance(a,unreal.StaticMeshActor):continue
    p=a.get_actor_location()
    if abs(p.x+270)<100 and abs(p.y)<180:
        c=a.static_mesh_component;o,e=a.get_actor_bounds(False)
        rows.append(dict(label=a.get_actor_label(),visible=c.get_editor_property('visible'),hidden=a.get_editor_property('hidden'),component_hidden=c.get_editor_property('hidden_in_game'),bounds=[list(o.to_tuple()),list(e.to_tuple())],materials=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())]))
print(json.dumps(rows))
print(unreal.MaterialNodeService.export_material_graph('/Game/Cindehearth/HD2D_v002/Materials/M_CH_PixelFire')[:3400])
