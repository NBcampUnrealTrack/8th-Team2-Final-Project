# -*- coding: utf-8 -*-
"""HUD QA: finish the objectives of the active quest 10001 for real (tracker -> ready, "목표 달성" banner)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Quest.Progress 10001 99', PC)
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Quest.List', PC)
log('qa_qm_progress.py')
