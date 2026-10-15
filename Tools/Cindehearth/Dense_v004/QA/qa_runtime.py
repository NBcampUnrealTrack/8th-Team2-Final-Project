"""Execute one v004 QA stage inside PIE. Coordinator supplies STAGE and QA_OUT."""
import json
import time
import traceback
from pathlib import Path
import unreal

assert 'QA_OUT' in globals(), 'A fresh per-run QA_OUT is required; use QA/run.py'
OUT = Path(QA_OUT)
OUT.mkdir(parents=True, exist_ok=True)
QA_ROOT = Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004/QA'
namespace = {}
exec((QA_ROOT/'routes.py').read_text(encoding='utf-8'), namespace)
ROUTES = namespace['ROUTES']
TOLERANCE_CM = namespace['TOLERANCE_CM']
TIMEOUT_SECONDS = namespace['TIMEOUT_SECONDS']
STAGE = globals().get('STAGE', '')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'PIE must be running after the v004 layout and rebuilt C++ are ready'
directors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NUCindehearthDirector)
assert len(directors) == 1
director = directors[0]
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
controller = unreal.GameplayStatics.get_player_controller(world, 0)
assert pawn and controller


def vector(value):
    return list(value.to_tuple())


def write(name, value):
    path = OUT/(name+'.json')
    assert not path.exists(), 'Refusing to overwrite a QA receipt: '+str(path)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')


def shown(actor):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    return bool(component and component.is_visible() and not component.get_editor_property('hidden_in_game')
                and not actor.get_editor_property('hidden'))


if STAGE == 'states':
    rows = []
    passed = False
    error = None
    auto = director.get_editor_property('auto_play_epilogue')
    old_stage = director.get_editor_property('completed_chapter_quest')
    old_night = director.get_editor_property('night')
    try:
        director.set_editor_property('auto_play_epilogue', False)
        for chapter in (0,7,8,9,11,12):
            director.set_night(True)
            director.set_chapter_progress(chapter)
            actors = unreal.GameplayStatics.get_all_actors_with_tag(world, 'Cindehearth')
            tagged = lambda tag: [a for a in actors if a.actor_has_tag(tag)]
            row = dict(chapter=chapter, mood=str(director.get_square_mood()),
                       crowd=sum(not a.get_editor_property('hidden') for a in tagged('CH.Crowd')),
                       gate_collision=sum(a.get_actor_enable_collision() for a in tagged('CH.Q8.Blocker')),
                       hatch_collision=sum(a.get_actor_enable_collision() for a in tagged('CH.Q7.Blocker')),
                       gate_visible=sum(shown(a) for a in tagged('CH.Q8.Blocker')),
                       cocoon_visible=sum(shown(a) for a in tagged('CH.Q11.Cocoon')),
                       freed=sum(not a.get_editor_property('hidden') for a in tagged('CH.Q11.Freed')))
            rows.append(row)
            assert row['crowd'] == 24, row
            assert bool(row['gate_collision']) == (chapter < 8), row
            assert bool(row['hatch_collision']) == (chapter < 7), row
            assert bool(row['gate_visible']) == (chapter < 8), row
            assert bool(row['cocoon_visible']) == (chapter < 11), row
            assert bool(row['freed']) == (chapter >= 11), row
            assert row['mood'] == ('Recovered' if chapter >= 12 else 'Hushed' if chapter >= 9 else 'Lively'), row
            for resident in tagged('CH.Resident'):
                component = resident.get_component_by_class(unreal.PaperSpriteComponent)
                assert component and component.get_sprite() and component.get_material(0), resident.get_name()
                assert resident.get_actor_bounds(False)[1].z > 30, resident.get_name()
        # Keep documentary revelations in the compact assay cellar only.
        clues = [a for a in actors if a.actor_has_tag('Shadowvale.Export') or a.actor_has_tag('Doren.DoubleRing')]
        assert clues, 'Q7 documentary clues are missing'
        for actor in clues:
            point = actor.get_actor_location()
            assert abs(point.x-1172.7) < 240 and abs(point.y-263.475) < 180 and point.z < 0, vector(point)
        passed = True
    except Exception:
        error = traceback.format_exc()
        raise
    finally:
        director.set_chapter_progress(old_stage)
        director.set_night(old_night)
        director.set_editor_property('auto_play_epilogue', auto)
        write('state_qa', dict(passed=passed, states=rows, error=error))
    print('V004 PRESENTATION STATES PASS')

elif STAGE.startswith('walk:'):
    mode = STAGE.split(':', 1)[1]
    assert mode in ROUTES, mode
    director.set_editor_property('auto_play_epilogue', False)
    director.set_chapter_progress(8 if mode == 'deepvein' else 0)
    director.set_night(False)

    class CindehearthDenseWalk:
        def __init__(self, actor, name):
            self.pawn = actor
            self.name = name
            self.route = ROUTES[name]
            self.index = 0
            self.samples = []
            self.reached = []
            self.start = time.monotonic()
            self.next_sample = 0
            self.best = float('inf')
            self.progress_at = self.start
            first = self.route[0]
            self.pawn.character_movement.stop_movement_immediately()
            self.pawn.set_actor_location(unreal.Vector(first[0], first[1], first[2]+52), False, True)
            self.handle = unreal.register_slate_post_tick_callback(self.tick)

        def tick(self, delta_seconds):
            try:
                now = time.monotonic()
                elapsed = now-self.start
                if elapsed < .8:
                    return
                position = self.pawn.get_actor_location()
                target = self.route[self.index]
                delta = unreal.Vector(target[0]-position.x, target[1]-position.y, 0)
                distance = delta.length()
                grounded = bool(self.pawn.character_movement.current_floor.walkable_floor)
                ground_error = position.z-48-target[2]
                sample = dict(seconds=round(elapsed,2), waypoint=self.index, position=vector(position),
                              distance=round(distance,2), floor_error_cm=round(ground_error,2),
                              walkable_floor=grounded, movement_mode=str(self.pawn.character_movement.movement_mode))
                if elapsed >= self.next_sample:
                    self.samples.append(sample)
                    self.next_sample = elapsed+.5
                if distance < self.best-8:
                    self.best = distance
                    self.progress_at = now
                if distance < TOLERANCE_CM[self.name] and grounded and abs(ground_error) < 40:
                    self.reached.append(sample)
                    self.index += 1
                    self.best = float('inf')
                    self.progress_at = now
                    if self.index == len(self.route):
                        self.finish(True)
                    return
                if elapsed > TIMEOUT_SECONDS[self.name]:
                    self.finish(False, 'route_timeout')
                    return
                if now-self.progress_at > 15:
                    self.finish(False, 'no_progress_or_wrong_floor_for_15_seconds')
                    return
                if position.z < -1000:
                    self.finish(False, 'fell_below_expected_play_surfaces')
                    return
                if distance > 1:
                    amount = min(1, distance/max(self.pawn.character_movement.max_walk_speed*delta_seconds*1.5, 1))
                    self.pawn.add_movement_input(delta/distance, amount, False)
            except Exception:
                self.finish(False, traceback.format_exc())

        def finish(self, passed, reason=None):
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.pawn.character_movement.stop_movement_immediately()
            write('walk_'+self.name, dict(passed=passed, route=self.name, waypoints_reached=self.index,
                  total_waypoints=len(self.route), route_floor_points=self.route, reached=self.reached,
                  final_position=vector(self.pawn.get_actor_location()), reason=reason, samples=self.samples,
                  movement='add_movement_input; only initial placement teleports',
                  horizontal_tolerance_cm=TOLERANCE_CM[self.name], floor_tolerance_cm=40))
            unreal.log('V004 WALK '+self.name+' '+str(passed))

    dense_walk_test = CindehearthDenseWalk(pawn, mode)
    print('V004 WALK STARTED', mode)

elif STAGE == 'archive_enter':
    director.set_editor_property('auto_play_epilogue', False)
    director.set_chapter_progress(7)
    entries = unreal.GameplayStatics.get_all_actors_with_tag(world, 'CH.Archive.Entry')
    lowers = unreal.GameplayStatics.get_all_actors_with_tag(world, 'CH.Archive.Lower')
    assert len(entries) == len(lowers) == 1
    entry = entries[0].get_actor_location()
    lower = lowers[0].get_actor_location()
    assert (entry-unreal.Vector(*namespace['EXPECTED_ANCHORS']['entry'])).length() < .2
    assert (lower-unreal.Vector(*namespace['EXPECTED_ANCHORS']['lower'])).length() < .2
    pawn.character_movement.stop_movement_immediately()
    pawn.set_actor_location(entry, False, True)
    director.use_archive_ladder()
    assert (pawn.get_actor_location()-lower).length() < .1
    write('archive_enter', dict(passed=True, entry=vector(entry), lower=vector(lower), actual=vector(pawn.get_actor_location()), floor_settled=False))

elif STAGE == 'archive_lower_floor_exit':
    lower = unreal.GameplayStatics.get_all_actors_with_tag(world, 'CH.Archive.Lower')[0].get_actor_location()
    entry = unreal.GameplayStatics.get_all_actors_with_tag(world, 'CH.Archive.Entry')[0].get_actor_location()
    position = pawn.get_actor_location()
    assert pawn.character_movement.current_floor.walkable_floor, vector(position)
    assert (unreal.Vector(position.x, position.y, lower.z)-lower).length() < 30
    assert abs(position.z-lower.z) < 25, vector(position)
    director.use_archive_ladder()
    assert (pawn.get_actor_location()-entry).length() < .1
    write('archive_lower_floor_exit', dict(passed=True, settled_lower=vector(position), upper=vector(entry), lower_floor_walkable=True))

elif STAGE == 'archive_upper_floor':
    entry = unreal.GameplayStatics.get_all_actors_with_tag(world, 'CH.Archive.Entry')[0].get_actor_location()
    position = pawn.get_actor_location()
    assert pawn.character_movement.current_floor.walkable_floor, vector(position)
    assert abs(position.z-entry.z) < 25 and (unreal.Vector(position.x,position.y,entry.z)-entry).length() < 30
    write('archive_upper_floor', dict(passed=True, settled_upper=vector(position), upper_floor_walkable=True))

elif STAGE == 'epilogue':
    director.set_editor_property('auto_play_epilogue', False)
    director.set_chapter_progress(12)
    assert director.play_epilogue()
    assert controller.is_move_input_ignored() and controller.is_look_input_ignored()
    assert controller.get_view_target() != pawn
    # The ladder must also be disabled throughout the mandatory cinematic.
    entries = unreal.GameplayStatics.get_all_actors_with_tag(world, 'CH.Archive.Entry')
    saved_position = pawn.get_actor_location()
    pawn.set_actor_location(entries[0].get_actor_location(), False, True)
    before = pawn.get_actor_location()
    director.use_archive_ladder()
    assert (pawn.get_actor_location()-before).length() < .1
    pawn.set_actor_location(saved_position, False, True)
    write('epilogue_start', dict(passed=True, input_ignored=True, cinematic_archive_gate=True,
          duration=director.get_editor_property('epilogue_duration'), route_count=len(director.get_editor_property('epilogue_camera_route'))))

elif STAGE == 'epilogue_verify':
    assert director.get_editor_property('epilogue_played')
    assert not controller.is_move_input_ignored() and not controller.is_look_input_ignored()
    assert controller.get_view_target() == pawn
    write('epilogue_end', dict(passed=True, input_restored=True, pawn_view_target_restored=True))
else:
    raise AssertionError('Unknown v004 QA stage: '+STAGE)
