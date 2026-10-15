# -*- coding: utf-8 -*-
"""HUD QA: advance the open dialogue by one line (finishes the text reveal first, like the key)."""
import unreal

cls = unreal.load_class(None, '/Script/ProjectM.NUDialogueWidget')
for widget in unreal.ObjectIterator(cls):
    if widget.get_world() == GAME_WORLD and widget.is_dialogue_open():
        widget.advance_dialogue()
        log('dialogue advanced %s' % widget.get_name())
