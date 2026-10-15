# -*- coding: utf-8 -*-
"""HUD QA capture helper: Short speech bubble over my pawn. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Bubble.Say 안녕하세요!', PC)
log('qa_bubble_short.py')
