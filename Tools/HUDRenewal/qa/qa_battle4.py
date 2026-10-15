# -*- coding: utf-8 -*-
"""HUD QA: local battle against four foes (tag spacing on the foe row)."""
import unreal
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Battle.Fight 1004 1004 1004 1004', PC)
log('qa_battle4')
