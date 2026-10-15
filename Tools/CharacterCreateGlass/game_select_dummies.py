# -*- coding: utf-8 -*-
"""Capture helper (runs in the play world, select screen, no server): fills the roster with three characters
through the debug command  nu.Char.Dummy [name] [race] [gender] [hair] [face] [job] [level] [hours until deletion].
hair = hair style + colour x 1000: 7001 = style 1 in blue."""
import unreal

for args in ('새벽 1 0 7001 1 1001 12', '하늘 1 1 0 0 0 3', '모래 1 0 1 1 0 1 18'):
    unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Char.Dummy ' + args, PC)
log('roster filled')
