"""Read current visible mesh bounds along the recorded depot camera ray."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004'
capture = next(r for r in json.loads((root/'verification/player_captures_all.json').read_text()) if r['name'] == 'depot')
eye, target = capture['camera_position'], capture['pawn_position']
delta = [target[i]-eye[i] for i in range(3)]
rows = []
game = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
actors = (unreal.GameplayStatics.get_all_actors_of_class(game,unreal.StaticMeshActor) if game
          else unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
print('SOURCE', game.get_path_name() if game else 'editor', 'ACTORS',len(actors))
for actor in actors:
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    component = actor.static_mesh_component
    if not component.static_mesh or not component.is_visible() or actor.get_editor_property('hidden'):
        continue
    center, extent = [v.to_tuple() for v in actor.get_actor_bounds(False)]
    near, far = 0., 1.
    for i in range(3):
        lo, hi = center[i]-extent[i]-70, center[i]+extent[i]+70
        if abs(delta[i]) < 1.e-5:
            if not lo <= eye[i] <= hi:
                near, far = 1., 0.
                break
        else:
            a, b = (lo-eye[i])/delta[i], (hi-eye[i])/delta[i]
            near, far = max(near,min(a,b)), min(far,max(a,b))
    if near <= far:
        rows.append(dict(label=actor.get_actor_label(), ray_entry=near, ray_exit=far,
                         location=list(actor.get_actor_location().to_tuple()),
                         bounds_center=center, bounds_extent=extent,
                         mesh=component.static_mesh.get_path_name(),
                         collision=str(component.get_collision_profile_name()),
                         actor_collision=actor.get_actor_enable_collision()))
rows.sort(key=lambda r: r['ray_entry'])
(root/'Runtime/depot_view_candidates.json').write_text(json.dumps(dict(camera=eye,player=target,candidates=rows),indent=2),encoding='utf-8')
print(json.dumps(rows,indent=2))
