# -*- coding: utf-8 -*-
"""HUD QA: click the '대화하기' choice row (its button's OnClicked, the same path as a mouse click)."""
import unreal

entry_cls = unreal.load_class(None, '/Script/ProjectM.NUInteractionOptionEntryWidget')
for entry in unreal.ObjectIterator(entry_cls):
    if entry.get_world() != GAME_WORLD:
        continue
    text = str(entry.get_option().get_editor_property('display_text'))
    if text == '대화하기':
        entry.get_editor_property('option_button').on_clicked.broadcast()
        log('picked %s' % text)
        break
