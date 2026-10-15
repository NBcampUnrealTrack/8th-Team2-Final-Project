import unreal
import json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Cindehearth',world.get_path_name()
assert not unreal.WidgetService.is_pie_running()
data=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if 'Gorm' in a.get_actor_label():
        c=a.get_component_by_class(unreal.StaticMeshComponent)
        data.append(dict(label=a.get_actor_label(),path=a.get_path_name(),location=list(a.get_actor_location().to_tuple()),rotation=str(a.get_actor_rotation()),scale=list(a.get_actor_scale3d().to_tuple()),mesh=c.static_mesh.get_path_name() if c and c.static_mesh else None,hidden=a.get_editor_property('hidden'),editor_hidden=a.is_temporarily_hidden_in_editor(),collision=a.get_actor_enable_collision()))
print(json.dumps(data))
(Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Pipeline/verification/gorm_before.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
