# -*- coding: utf-8 -*-
"""HUD QA: talk to the quest NPC next to the player (run after qa_interact.py) --- the same call the F key makes."""
import unreal

pawn = PC.get_controlled_pawn()
here = pawn.get_actor_location()
npc_class = unreal.load_class(None, '/Script/ProjectM.NUNPCBase')
best, best_d = None, 1e18
for actor in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, npc_class):
    d = (actor.get_actor_location() - here).length()
    if d < best_d:
        best, best_d = actor, d
if best:
    ok = best.call_method('Interact', (pawn,))
    log('talk to %s (%d cm): %s' % (best.get_name(), best_d, ok))
