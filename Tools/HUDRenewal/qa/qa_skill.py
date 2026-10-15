# -*- coding: utf-8 -*-
"""HUD QA: open the skill list (detail panel) through the real command component."""
import unreal
cc = PC.get_battle_command_component()
cc.select_root_action(unreal.NUBattleRootAction.SKILL)
log('qa_skill phase=%s' % cc.get_selection_phase() if hasattr(cc, 'get_selection_phase') else 'qa_skill')
