# -*- coding: utf-8 -*-
"""HUD QA: attack -> target selection (cursor, ring, dimmed candidates)."""
import unreal
cc = PC.get_battle_command_component()
cc.select_root_action(unreal.NUBattleRootAction.ATTACK)
log('qa_attack')
