# -*- coding: utf-8 -*-
"""Proposal plates: hide the interaction cue (F) in the play world only (render opacity 0, after its pop-in has finished),
so mock cues can be drawn on a clean frame. The head markers stay. Nothing is saved."""
import unreal

cls = unreal.load_class(None, '/Script/ProjectM.NU_InteractPromptWidget')
n = 0
for widget in unreal.ObjectIterator(cls):
    if widget.get_world() == GAME_WORLD:
        widget.set_render_opacity(0.0)
        n += 1
log('hid %d interaction cues' % n)
