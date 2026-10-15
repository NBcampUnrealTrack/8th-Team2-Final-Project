# -*- coding: utf-8 -*-
"""HUD QA: arrive somewhere and finish a quest in the same moment (a "reach X" objective): the area banner shows first,
the quest banner waits until it is gone. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.QuestFx done 뿌리 사이로', PC)
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.Location 리베르그로브 아랫마을 광장', PC)
log('qa_overlap.py')
