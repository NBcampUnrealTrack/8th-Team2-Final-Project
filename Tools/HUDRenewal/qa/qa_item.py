# -*- coding: utf-8 -*-
"""HUD QA: back to the root menu, then open the item list through the real command component."""
import unreal
cc = PC.get_battle_command_component()
cc.cancel_selection()
cc.select_root_action(unreal.NUBattleRootAction.ITEM)
log('qa_item')
