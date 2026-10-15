"""Save the reviewed compact town and leave its central square in the viewport."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004'
assert not unreal.WidgetService.is_pie_running()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Cindehearth'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
d=next(a for a in actors if isinstance(a,unreal.NUCindehearthDirector))
d.set_night(False);d.set_chapter_progress(0)
row=next(r for r in json.loads((root/'verification/review_captures.json').read_text()) if r['name']=='square')
c=row['capture']['camera'];r=c['rotation']
unreal.ViewportService.set_game_view(True)
unreal.ViewportService.set_camera_location(unreal.Vector(*c['location']))
unreal.ViewportService.set_camera_rotation(unreal.Rotator(pitch=r[0],yaw=r[1],roll=r[2]))
unreal.ViewportService.set_fov(c['fov'])
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
assert d.get_editor_property('completed_chapter_quest')==0 and not d.get_editor_property('night')
(root/'verification/final_saved.json').write_text(json.dumps(dict(saved=True,map=world.get_path_name(),actors=len(actors),chapter=0,night=False,pie=False,editor_view='square'),indent=2),encoding='utf-8')
print('SAVED COMPACT TOWN: Q0/day, PIE off, square viewport')
