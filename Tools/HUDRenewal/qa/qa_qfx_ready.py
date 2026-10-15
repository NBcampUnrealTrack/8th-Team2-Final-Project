# -*- coding: utf-8 -*-
"""HUD QA: quest banner 'ready' (nu.HUD.QuestFx). Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.QuestFx ready 뿌리 사이로', PC)
log('qa_qfx_ready.py')
