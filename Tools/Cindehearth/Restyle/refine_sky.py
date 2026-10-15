import unreal
a={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
fog=a['CH_Smog'].get_component_by_class(unreal.ExponentialHeightFogComponent)
print('FOG BEFORE',fog.get_editor_property('fog_cutoff_distance'))
fog.set_editor_property('fog_cutoff_distance',25000.)
fog.set_editor_property('fog_density',.009)
fog.set_editor_property('fog_max_opacity',.65)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
p=unreal.Vector(0,-4900,600);rot=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(0,1600,1500))
print(unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Restyle_sky_cutoff',55,False,False))
