# -*- coding: utf-8 -*-
"""HUD QA: which dialogue widget class is open, and the option rows it built (text, focus)."""
import unreal

cls = unreal.load_class(None, '/Script/ProjectM.NUDialogueWidget')
for widget in unreal.ObjectIterator(cls):
    if widget.get_world() == GAME_WORLD and widget.is_dialogue_open():
        log('open %s (%s)' % (widget.get_name(), widget.get_class().get_name()))
entry_cls = unreal.load_class(None, '/Script/ProjectM.NUInteractionOptionEntryWidget')
for entry in unreal.ObjectIterator(entry_cls):
    if entry.get_world() == GAME_WORLD and entry.is_in_viewport() is False:
        opt = entry.get_option()
        log('row %s group=%s text=%s' % (entry.get_class().get_name(), opt.get_editor_property('group'), opt.get_editor_property('display_text')))
