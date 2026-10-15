# -*- coding: utf-8 -*-
"""HUD QA capture helper: the battle after a win, for result-window mock-ups --- enemy avatars hidden (they fall at the real
result), taken with "noui". Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

n = 0
for a in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.NUBattleEnemyAvatar):
    a.set_actor_hidden_in_game(True)
    n += 1
log('qa_result_plate: hid %d enemies' % n)
