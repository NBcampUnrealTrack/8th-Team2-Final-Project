# -*- coding: utf-8 -*-
"""HUD QA capture helper: the new battle result window, level up + caught Naia (nu.HUD.BattleResult 1 1), through the real PlayerController path, over a
won battle: enemy avatars hidden and the battle HUD folded the way the Ending phase folds it.
Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

for a in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.NUBattleEnemyAvatar):
    a.set_actor_hidden_in_game(True)
for o in unreal.ObjectIterator(unreal.UserWidget):
    try:
        name = o.get_class().get_name()
        if o.get_world() == GAME_WORLD and o.is_in_viewport() and ('BattleCommand' in name or name.startswith('WBP_HUD_Battle_') or name == 'WBP_HUD_Battle_C'):
            o.set_visibility(unreal.SlateVisibility.COLLAPSED)
            log('folded ' + name)
    except Exception:
        pass
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.BattleResult 1 1', PC)
log('qa_result_new')
