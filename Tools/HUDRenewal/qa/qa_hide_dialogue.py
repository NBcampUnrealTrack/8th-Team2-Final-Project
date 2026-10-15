# -*- coding: utf-8 -*-
"""Proposal plates: hide the open dialogue widget in the play world only (render opacity 0), so mock dialogue UIs can be drawn
on the same frame. Nothing is saved."""
import unreal

cls = unreal.load_class(None, '/Script/ProjectM.NUDialogueWidget')
n = 0
for widget in unreal.ObjectIterator(cls):
    if widget.get_world() == GAME_WORLD:
        widget.set_render_opacity(0.0)
        n += 1
log('hid %d dialogue widgets' % n)
