# -*- coding: utf-8 -*-
"""HUD QA: put the pawn on the village PlayerStart (the offline save can start in Fernbrook), no fake players.
Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

pawn = unreal.GameplayStatics.get_player_pawn(GAME_WORLD, 0)
starts = [a for a in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.PlayerStart)
          if abs(a.get_actor_location().x) < 20000 and abs(a.get_actor_location().y) < 20000]
if pawn and starts:
    spot = starts[0].get_actor_location() + unreal.Vector(0, 0, 60)
    pawn.set_actor_location(spot, False, True)
    log('qa_village_solo.py pawn moved to %s' % spot)
else:
    log('qa_village_solo.py: no village start (pawn %s, starts %d)' % (pawn, len(starts)))
