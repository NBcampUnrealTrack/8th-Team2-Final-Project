# -*- coding: utf-8 -*-
"""HUD QA capture helper: World-chat speech bubble over my pawn. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Bubble.SayWorld 대장간 앞에서 거래해요~', PC)
log('qa_bubble_world.py')
