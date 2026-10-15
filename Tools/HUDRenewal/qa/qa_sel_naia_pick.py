# -*- coding: utf-8 -*-
"""HUD QA: pick a starter Naia on the open window (SelectNaia, same as clicking its card). QA_NAIA env (default 1007 Bulcoi)."""
import os
import unreal

naia = int(os.environ.get('QA_NAIA', '1007'))
cls = unreal.load_class(None, '/Script/ProjectM.NUStarterNaiaSelectionWidget')
for w in unreal.ObjectIterator(cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        w.select_naia(naia)
        log('qa_sel_naia_pick.py picked %d on %s' % (naia, w.get_name()))
