# -*- coding: utf-8 -*-
"""Capture helper: clicks the roster row named PICK (as a mouse click on the row would)."""
import unreal

PICK = '하늘'
clicked = 0
for label in unreal.ObjectIterator(unreal.TextBlock):
    if label.get_name() != 'NameText' or label.get_world() != GAME_WORLD or str(label.get_text()) != PICK:
        continue
    owner = label.get_outer()
    while owner is not None and not isinstance(owner, unreal.NUCharacterSlotWidget):
        owner = owner.get_outer()
    if owner is not None:
        owner.call_method('HandleSlotClicked')
        clicked += 1
log('clicked %d row(s) named %s' % (clicked, PICK))
