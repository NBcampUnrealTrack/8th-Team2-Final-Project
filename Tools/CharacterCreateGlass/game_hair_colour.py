# -*- coding: utf-8 -*-
"""Capture helper (runs in the play world): picks a hair colour on the screen as it stands (run it after game_male_checked.py)."""
import unreal

COLOUR = 7      # index in the palette (project settings > NU Appearance Data > Hair Colors): 7 = blue
found = [w for w in unreal.ObjectIterator(unreal.NUCharacterCreateWidget) if not w.get_name().startswith('Default__') and w.get_world() == GAME_WORLD]
for widget in found:
    widget.call_method('HandleHairNextClicked')                 # second hair style, to see the colour on both thumbnails
    widget.call_method('HandleHairColorClicked', (COLOUR,))
log('hair colour %d on %d widget(s)' % (COLOUR, len(found)))
