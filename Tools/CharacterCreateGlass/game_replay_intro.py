# -*- coding: utf-8 -*-
"""Capture helper: takes the creation / select screen off the screen and puts it back, which plays its entrance again."""
import unreal

count = 0
for cls in (unreal.NUCharacterCreateWidget, unreal.NUCharacterSelectWidget):
    for widget in unreal.ObjectIterator(cls):
        if widget.get_name().startswith('Default__') or widget.get_world() != GAME_WORLD or not widget.is_in_viewport():
            continue
        widget.remove_from_parent()
        widget.add_to_viewport()
        count += 1
log('intro replayed on %d widget(s)' % count)
