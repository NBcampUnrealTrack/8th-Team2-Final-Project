import unreal
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in AS.get_all_level_actors():
    if a.get_actor_label() in ('CH_EveningResident_00','CH_Dorik','CH_Messenger'):
        print(a.get_actor_label(),a.get_actor_location(),a.get_actor_bounds(False),a.get_actor_scale3d(),a.get_actor_rotation(),a.get_editor_property('hidden'))
        c=a.get_component_by_class(unreal.PaperSpriteComponent)
        print(c.get_material(0),c.get_editor_property('visible'),c.get_editor_property('hidden_in_game'))
