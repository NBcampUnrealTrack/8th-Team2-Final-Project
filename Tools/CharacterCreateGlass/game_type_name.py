# -*- coding: utf-8 -*-
"""Capture helper (runs in the play world): types a name into the creation screen so the name message is visible."""
import unreal

NAME = '새벽'
found = [w for w in unreal.ObjectIterator(unreal.NUCharacterCreateWidget) if not w.get_name().startswith('Default__') and w.get_world() == GAME_WORLD]
fields = [f for f in unreal.ObjectIterator(unreal.EditableTextBox) if f.get_name() == 'NameTextBox' and f.get_world() == GAME_WORLD]
for field in fields:
    field.set_text(NAME)
for widget in found:
    widget.call_method('HandleNameChanged', (unreal.Text(NAME),))   # SetText does not fire the change event
log('typed name into %d widget(s)' % len(found))
