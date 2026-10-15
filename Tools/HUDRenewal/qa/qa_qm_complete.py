# -*- coding: utf-8 -*-
"""HUD QA: turn in quest 10001 for real ("의뢰 완료" banner, then the reward window after the delay)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Quest.Complete 10001', PC)
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Quest.List', PC)
log('qa_qm_complete.py')
