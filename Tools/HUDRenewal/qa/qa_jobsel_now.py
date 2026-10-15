# -*- coding: utf-8 -*-
"""HUD QA: the job selection window as the game builds it today (zxcv1 WBP_JobSelection, cards made in C++), one job picked.
Opened straight into the viewport (the real path goes through the job NPC and the UI manager). Runs inside pie_capture.py."""
import unreal

cls = unreal.load_class(None, '/Game/Developers/zxcv1/CharacterProgression/UI/Job/WBP_JobSelection.WBP_JobSelection_C')
w = unreal.get_default_object(unreal.WidgetLibrary).call_method('Create', (GAME_WORLD, cls, PC))
w.add_to_viewport(60)
w.select_job(1002)
log('qa_jobsel_now.py opened %s, entries %d' % (w.get_name(), len(w.get_job_entries())))
