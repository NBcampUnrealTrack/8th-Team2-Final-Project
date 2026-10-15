# -*- coding: utf-8 -*-
"""HUD QA capture helper: Long speech bubble over my pawn. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Bubble.Say 펀브룩 초입에서 푸루나 잡으실 분 구해요. 포획 도와드릴게요!', PC)
log('qa_bubble_long.py')
