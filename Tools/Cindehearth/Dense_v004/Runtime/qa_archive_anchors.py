"""Run inside an already-started PIE session after the coordinator rebuilds C++."""
import json
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_dir()) / 'Tools/Cindehearth/Dense_v004/Runtime'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'Coordinator must start PIE after compiling the anchor implementation'
directors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NUCindehearthDirector)
assert len(directors) == 1
director = directors[0]
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
assert pawn
entries = unreal.GameplayStatics.get_all_actors_with_tag(world, 'CH.Archive.Entry')
lowers = unreal.GameplayStatics.get_all_actors_with_tag(world, 'CH.Archive.Lower')
assert len(entries) == len(lowers) == 1, 'One standing-position anchor per side is required'
entry, lower = entries[0].get_actor_location(), lowers[0].get_actor_location()
original_position = pawn.get_actor_location()
original_stage = director.get_editor_property('completed_chapter_quest')
original_auto = director.get_editor_property('auto_play_epilogue')
rows = []


def position(value):
    return [value.x, value.y, value.z]


def check(name, start, expected, stage):
    director.set_chapter_progress(stage)
    pawn.set_actor_location(start, False, True)
    director.use_archive_ladder()
    result = pawn.get_actor_location()
    error = (result - expected).length()
    row = dict(case=name, stage=stage, start=position(start), expected=position(expected),
               actual=position(result), error_cm=error, passed=error < 0.1)
    rows.append(row)
    assert row['passed'], row


try:
    director.set_editor_property('auto_play_epilogue', False)
    check('Q6 hatch remains gated', entry, entry, 6)
    check('Q7 enter at current upper anchor', entry, lower, 7)
    check('Q7 exit at current lower anchor', lower, entry, 7)
    far_xy = unreal.Vector(max(entry.x, lower.x) + 400, entry.y, entry.z)
    check('reject distant horizontal position', far_xy, far_xy, 7)
    far_z = unreal.Vector(entry.x, entry.y, max(entry.z, lower.z) + 200)
    check('reject vertically remote floor', far_z, far_z, 7)
finally:
    pawn.set_actor_location(original_position, False, True)
    director.set_chapter_progress(original_stage)
    director.set_editor_property('auto_play_epilogue', original_auto)
    (OUT / 'archive_anchor_qa.json').write_text(json.dumps(
        dict(passed=len(rows) == 5 and all(x['passed'] for x in rows),
             entry=position(entry), lower=position(lower), cases=rows,
             floor_physics_verified=False, cinematic_gate_verified=False), indent=2), encoding='utf-8')
print('ARCHIVE ANCHOR QA', json.dumps(rows))
