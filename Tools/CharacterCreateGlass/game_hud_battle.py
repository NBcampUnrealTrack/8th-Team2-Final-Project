# -*- coding: utf-8 -*-
"""HUD mock-up backdrop helper (battle): a local battle without the server (editor module command)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Battle.Fight 1004 1004', PC)
log('local battle requested')
