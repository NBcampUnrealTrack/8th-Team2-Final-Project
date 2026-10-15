# -*- coding: utf-8 -*-
"""HUD QA: move the player 20 m to map-left (+X) so quest NPCs east of the start fall outside the minimap and the
edge pins show. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

pawn = PC.get_controlled_pawn()
loc = pawn.get_actor_location()
pawn.set_actor_location(unreal.Vector(loc.x + 2000.0, loc.y, loc.z + 60.0), False, True)
log('qa_qm_edge.py %s -> %s' % (loc, pawn.get_actor_location()))
