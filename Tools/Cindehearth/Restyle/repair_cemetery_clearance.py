import unreal,json
from pathlib import Path
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
changes=[]
# The new, broader rocks reached into the existing rising cemetery trail.
# Recess scenery to the north; the existing path keeps its tested collision.
for a in sub.get_all_level_actors():
    n=a.get_actor_label()
    if not (n.endswith('_HD2D') and ('SideCliff_1_' in n or 'RidgeMass' in n)):continue
    p=a.get_actor_location()
    if 3100<p.x<4400 and 2100<p.y<3300:
        a.modify();before=list(p.to_tuple());p.y+=650
        a.set_actor_location(p,False,False)
        a.static_mesh_component.set_collision_profile_name('NoCollision')
        a.set_actor_enable_collision(False)
        changes.append(dict(label=n,before=before,after=list(p.to_tuple())))
assert changes
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification/cemetery_clearance.json').write_text(json.dumps(changes,indent=2),encoding='utf-8')
print('RECESSED',len(changes),'rocks from cemetery route')
