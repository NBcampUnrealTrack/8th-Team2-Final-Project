"""Read-only scene verification after the coordinated C++ build/restart."""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004/Runtime'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith('/Game/Maps/L_Cindehearth.'), world.get_path_name()
assert not unreal.WidgetService.is_pie_running()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
labels = {a.get_actor_label(): a for a in actors}
links = [a for a in actors if a.get_actor_label().startswith('CH_D4_Link_')]
assert len(links) == 106, len(links)
assert all(not a.get_actor_enable_collision() for a in links)
stair = labels['CH_Stairs_-30_-18_Stone']
stair_bounds = [list(v.to_tuple()) for v in stair.get_actor_bounds(False)]
assert abs(stair.get_actor_location().x-(-399)) < 0.1, 'Saved final west stair includes the coordinator\'s +60 cm clearance offset'
anchors = {}
for tag in ('CH.Archive.Entry', 'CH.Archive.Lower'):
    matches = [a for a in actors if a.actor_has_tag(tag)]
    assert len(matches) == 1, (tag, len(matches))
    anchors[tag] = list(matches[0].get_actor_location().to_tuple())
directors = [a for a in actors if isinstance(a, unreal.NUCindehearthDirector)]
assert len(directors) == 1
report = dict(passed=True, world=world.get_path_name(), pie_running=False, actor_count=len(actors),
              west_stair_location=list(stair.get_actor_location().to_tuple()), west_stair_bounds=stair_bounds,
              connection_dressing_count=len(links), archive_anchors=anchors,
              chapter=directors[0].get_editor_property('completed_chapter_quest'),
              night=directors[0].get_editor_property('night'),
              dirty_maps=[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
              dirty_content=[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
              scene_mutated=False, runtime_archive_qa_performed=False)
(out/'reopen_verified.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
