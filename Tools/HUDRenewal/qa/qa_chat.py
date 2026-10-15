# -*- coding: utf-8 -*-
"""HUD QA capture helper: Chat input open. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.ChatOpen', PC)
log('qa_chat.py')
