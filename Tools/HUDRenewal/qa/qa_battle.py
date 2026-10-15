# -*- coding: utf-8 -*-
"""HUD QA capture helper: Local battle without the server. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Battle.Fight 1004 1004', PC)
log('qa_battle.py')
