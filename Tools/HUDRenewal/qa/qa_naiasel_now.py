# -*- coding: utf-8 -*-
"""HUD QA: close the job window and open the starter Naia window as the game builds it today (native class, no WBP set).
Without the roster component a card cannot be picked, so this is the unpicked state. Runs inside pie_capture.py."""
import unreal

job_cls = unreal.load_class(None, '/Script/ProjectM.NUJobSelectionWidget')
for w in unreal.ObjectIterator(job_cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        w.remove_from_parent()
w = unreal.get_default_object(unreal.WidgetLibrary).call_method('Create', (GAME_WORLD, unreal.NUStarterNaiaSelectionWidget, PC))
w.add_to_viewport(60)
log('qa_naiasel_now.py opened %s, entries %d' % (w.get_name(), len(w.get_naia_entries())))
