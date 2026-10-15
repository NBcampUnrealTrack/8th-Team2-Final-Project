# -*- coding: utf-8 -*-
"""HUD QA: close the job window (its own close request -> UI manager) and open the starter Naia window (nu.HUD.NaiaSelect)."""
import unreal

cls = unreal.load_class(None, '/Script/ProjectM.NUJobSelectionWidget')
for w in unreal.ObjectIterator(cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        w.request_close()
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.NaiaSelect', PC)
log('qa_sel_naia.py opened')
