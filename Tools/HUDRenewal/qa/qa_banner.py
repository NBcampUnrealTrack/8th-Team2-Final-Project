# -*- coding: utf-8 -*-
"""HUD QA capture helper: Area entry banner + notice toast. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.Location 펀브룩 수레길 진입로', PC)
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.HUD.Notice 뿌리 사이로 · 수레길 진입로에 도착했습니다', PC)
log('qa_banner.py')
