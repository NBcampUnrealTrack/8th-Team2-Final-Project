import unreal
a=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label()=='CH_GormHouse_P2')
assert abs(a.get_actor_rotation().yaw)<.1
assert a.set_actor_rotation(unreal.Rotator(yaw=180),False)
print(a.get_actor_rotation())
