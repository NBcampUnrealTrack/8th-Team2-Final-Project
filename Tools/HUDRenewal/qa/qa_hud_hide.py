# -*- coding: utf-8 -*-
"""HUD QA: unhover, then hide the field HUD with the eye button (the eye keeps its own backdrop)."""
import unreal
for w in unreal.ObjectIterator(unreal.UserWidget):
    try:
        if w.get_world() != GAME_WORLD:
            continue
        name = w.get_class().get_name()
        if name.startswith('WBP_HUD_DockButton'):
            w.call_method('HandleUnhovered')
        elif name.startswith('WBP_HUD_Field'):
            w.call_method('HandleHUDVisibilityClicked')
            log('hud hidden via ' + w.get_name())
    except Exception as e:
        log('hide error %s' % e)
