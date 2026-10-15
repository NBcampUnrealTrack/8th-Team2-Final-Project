# -*- coding: utf-8 -*-
"""HUD QA: village start for the quest tracker / minimap pass + quest list in the log. Runs inside pie_capture.py."""
import unreal

exec(open(r'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/Tools/CharacterCreateGlass/game_hud_village.py', encoding='utf-8').read())
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Quest.List', PC)
log('qa_qm_idle.py')
