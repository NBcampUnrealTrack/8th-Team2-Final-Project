# -*- coding: utf-8 -*-
"""HUD QA: back to the root menu, then attack -> target selection."""
import unreal
cc = PC.get_battle_command_component()
cc.cancel_selection()
cc.select_root_action(unreal.NUBattleRootAction.ATTACK)
log('qa_target')
