# -*- coding: utf-8 -*-
"""Capture helper (creation screen, run after game_male_checked.py): presses the create button, which opens the confirm dialog."""
import unreal

found = [w for w in unreal.ObjectIterator(unreal.NUCharacterCreateWidget) if not w.get_name().startswith('Default__') and w.get_world() == GAME_WORLD]
for widget in found:
    widget.call_method('HandleCreateClicked')
log('create pressed on %d widget(s)' % len(found))
