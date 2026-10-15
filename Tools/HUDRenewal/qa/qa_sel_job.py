# -*- coding: utf-8 -*-
"""HUD QA: open the first-job window through the real path (nu.HUD.JobSelect -> UI manager window). Runs inside pie_capture.py."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.JobSelect', PC)
log('qa_sel_job.py opened')
