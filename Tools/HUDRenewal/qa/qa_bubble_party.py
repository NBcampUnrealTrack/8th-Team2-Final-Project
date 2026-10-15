# -*- coding: utf-8 -*-
"""HUD QA capture helper: Party speech bubble over my pawn. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Bubble.SayParty 다들 준비됐으면 출발할게요', PC)
log('qa_bubble_party.py')
