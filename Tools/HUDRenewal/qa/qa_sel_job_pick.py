# -*- coding: utf-8 -*-
"""HUD QA: pick a job on the open job window (SelectJob, same as clicking its card). QA_JOB env picks which (default 1002 Tracker)."""
import os
import unreal

job = int(os.environ.get('QA_JOB', '1002'))
cls = unreal.load_class(None, '/Script/ProjectM.NUJobSelectionWidget')
for w in unreal.ObjectIterator(cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        w.select_job(job)
        log('qa_sel_job_pick.py picked %d on %s' % (job, w.get_name()))
