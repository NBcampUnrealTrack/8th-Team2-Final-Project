# -*- coding: utf-8 -*-
"""HUD QA: close the open dialogue (like Esc / the leave row), so the field HUD should come back."""
import unreal

cls = unreal.load_class(None, '/Script/ProjectM.NUDialogueWidget')
for widget in unreal.ObjectIterator(cls):
    if widget.get_world() == GAME_WORLD and widget.is_dialogue_open():
        widget.close_dialogue(False)
        log('dialogue closed %s' % widget.get_name())
