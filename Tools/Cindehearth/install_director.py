import unreal
import math
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert w.get_name()=='L_Cindehearth'
directors=[a for a in AS.get_all_level_actors() if isinstance(a,unreal.NUCindehearthDirector)]
assert len(directors)<2
d=directors[0] if directors else AS.spawn_actor_from_class(unreal.NUCindehearthDirector,unreal.Vector())
d.set_actor_label('CH_LevelDirector');d.set_folder_path('Cindehearth/Gameplay')
d.tags=[unreal.Name('Cindehearth')]
d.set_editor_property('day_sky',unreal.load_asset('/Game/Cindehearth/Materials/MI_CH_Sky'))
d.set_editor_property('night_sky',unreal.load_asset('/Game/Cindehearth/Materials/MI_CH_Sky_Night'))
route=[]
for target,distance,pitch in [((-6,-4,3.8),32,-28),((-18,-30,3),14,-24),((-25,-39.2,1.7),5,-7),((-23.8,-39.64,1.3),2.4,-3)]:
    rotation=unreal.Rotator(yaw=90,pitch=pitch)
    location=unreal.Vector(*[x*45 for x in target])-rotation.get_forward_vector()*(distance*45)
    route.append(unreal.Transform(location=location,rotation=rotation))
d.set_editor_property('epilogue_camera_route',route)
d.set_editor_property('epilogue_duration',18)
d.set_editor_property('auto_play_epilogue',True)
d.set_chapter_progress(0);d.set_night(False)
AS.select_nothing();LS.save_current_level()
print('DIRECTOR',d.get_path_name(),'PHASE',d.completed_chapter_quest,'MOOD',d.get_square_mood(),'ROUTE',len(d.get_editor_property('epilogue_camera_route')))
