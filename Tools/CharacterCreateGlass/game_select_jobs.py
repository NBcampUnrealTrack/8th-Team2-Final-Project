# -*- coding: utf-8 -*-
"""Capture helper: a roster with every job emblem (novice + five jobs) for the job-icon chips (10/14)."""
import unreal

for args in ('새벽 1 0 7001 1 1001 12', '하늘 1 1 0 0 1002 13', '모래 1 0 1 1 1003 14', '이슬 1 1 0 0 1004 11', '바람 1 0 1 1 1005 15', '초롱 1 1 0 0 0 3'):
    unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Char.Dummy ' + args, PC)
log('roster filled (jobs)')
