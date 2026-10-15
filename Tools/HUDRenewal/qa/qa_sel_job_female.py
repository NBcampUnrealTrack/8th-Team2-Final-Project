# -*- coding: utf-8 -*-
"""HUD QA: the job window for a female character (test character gender, nu.Char.TestGender 1), Druid picked."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Char.TestGender 1', PC)
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.JobSelect', PC)
cls = unreal.load_class(None, '/Script/ProjectM.NUJobSelectionWidget')
for w in unreal.ObjectIterator(cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        w.select_job(1005)
        log('qa_sel_job_female.py picked 1005 on %s' % w.get_name())
