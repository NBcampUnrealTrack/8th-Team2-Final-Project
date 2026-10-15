# -*- coding: utf-8 -*-
"""HUD QA: back to the root menu, slow UI motion 10x and replay the command panel's open (hide + show)."""
import unreal
cc = PC.get_battle_command_component()
cc.cancel_selection()
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.UI.MotionScale 0.1', PC)
found = 0
for w in unreal.ObjectIterator(unreal.UserWidget):
    try:
        if w.get_class().get_name().startswith('WBP_HUD_BattleCommand') and w.get_world() == GAME_WORLD:
            rest = w.get_visibility()
            w.set_visibility(unreal.SlateVisibility.COLLAPSED)
            w.set_visibility(rest)
            found += 1
    except Exception as e:
        log('reopen error %s' % e)
log('qa_reopen widgets=%d' % found)
