# -*- coding: utf-8 -*-
"""HUD QA: move the target cursor once and raise SP so the mark and the pips change."""
import unreal
cc = PC.get_battle_command_component()
cc.select_next_target()
cc.adjust_sp_spend(1)
log('qa_target2')
