# -*- coding: utf-8 -*-
"""Capture helper (runs in the play world): male, a typed name, and the name check answered "available" - the state the mock shows.
The server is not there in an editor capture, so the answer (200) is fed straight into the widget."""
import unreal

NAME = '새벽'
found = [w for w in unreal.ObjectIterator(unreal.NUCharacterCreateWidget) if not w.get_name().startswith('Default__') and w.get_world() == GAME_WORLD]
fields = [f for f in unreal.ObjectIterator(unreal.EditableTextBox) if f.get_name() == 'NameTextBox' and f.get_world() == GAME_WORLD]
for widget in found:
    widget.call_method('HandleGenderMaleClicked')
for field in fields:
    field.set_text(NAME)
for widget in found:
    widget.call_method('HandleNameChanged', (unreal.Text(NAME),))   # SetText does not fire the change event
    widget.call_method('HandleNameCheckClicked')
    widget.call_method('HandleNameCheckResult', (200,))
log('male + checked name on %d widget(s)' % len(found))
