# -*- coding: utf-8 -*-
"""HUD QA capture helper: Party invite. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.PartyInvite 하늘', PC)
log('qa_invite.py')
