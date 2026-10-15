# -*- coding: utf-8 -*-
"""HUD QA: give the player a push up-right on screen (-X = screen right, +Y = into the screen) so the minimap arrow
turns to the movement. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

pawn = PC.get_controlled_pawn()
pawn.launch_character(unreal.Vector(-520.0, 300.0, 0.0), True, False)
log('qa_qm_move.py launched %s' % pawn.get_name())
