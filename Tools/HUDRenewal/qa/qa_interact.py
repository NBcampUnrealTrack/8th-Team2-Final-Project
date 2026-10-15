# -*- coding: utf-8 -*-
"""HUD QA: stand next to the nearest NPC that carries a quest marker so the interaction cue (F) shows.
Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

pawn = PC.get_controlled_pawn()
here = pawn.get_actor_location()
npc_class = unreal.load_class(None, '/Script/ProjectM.NUNPCBase')
best, best_d = None, 1e18
for actor in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, npc_class):
    marker = actor.get_component_by_class(unreal.load_class(None, '/Script/ProjectM.NUWorldMarkerComponent'))
    if marker is None or not marker.is_visible():
        continue
    d = (actor.get_actor_location() - here).length()
    if d < best_d:
        best, best_d = actor, d
if best:
    loc = best.get_actor_location()
    # stand to its right on screen (-X is screen right) and a little toward the camera (-Y), facing it
    pawn.set_actor_location(unreal.Vector(loc.x - 70.0, loc.y - 30.0, loc.z + 5.0), False, True)
    log('qa_interact.py next to %s (%.0f cm away before)' % (best.get_name(), best_d))
else:
    log('qa_interact.py: no NPC with a visible marker')
