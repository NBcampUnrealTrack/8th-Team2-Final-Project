# -*- coding: utf-8 -*-
"""Capture helper: on the creation screen as it stands (female by default), pick a hair colour. COLOUR via the file name
suffix is not possible, so this picks blue (7); game_female_hair_red.py picks red (3)."""
import unreal
COLOUR = 3
found = [w for w in unreal.ObjectIterator(unreal.NUCharacterCreateWidget) if not w.get_name().startswith('Default__') and w.get_world() == GAME_WORLD]
for widget in found:
    widget.call_method('HandleHairColorClicked', (COLOUR,))
log('female hair colour %d on %d widget(s)' % (COLOUR, len(found)))
