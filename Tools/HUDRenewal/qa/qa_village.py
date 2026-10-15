# -*- coding: utf-8 -*-
"""HUD QA capture helper: Village start + two fake players + sample chat lines. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

exec(open(r'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/Tools/CharacterCreateGlass/game_hud_village.py', encoding='utf-8').read())
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.ChatSample', PC)
log('qa_village.py')
