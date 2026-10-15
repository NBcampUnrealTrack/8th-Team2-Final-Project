# -*- coding: utf-8 -*-
"""HUD QA: click the shop row of the open conversation (text with 상점/구매/거래, else the first service-looking row)."""
import unreal

entry_cls = unreal.load_class(None, '/Script/ProjectM.NUInteractionOptionEntryWidget')
rows = [e for e in unreal.ObjectIterator(entry_cls) if e.get_world() == GAME_WORLD]
pick = None
for e in rows:
    text = str(e.get_option().get_editor_property('display_text'))
    if any(k in text for k in ('상품', '상점', '구매', '거래', '물건')):
        pick = e
        break
if pick:
    pick.get_editor_property('option_button').on_clicked.broadcast()
    log('picked %s' % pick.get_option().get_editor_property('display_text'))
else:
    log('no shop row among %s' % [str(e.get_option().get_editor_property('display_text')) for e in rows])
