# -*- coding: utf-8 -*-
"""HUD QA capture helper: Chat closed again (Escape path: end focus). Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

pc = PC
chat = pc.get_chat_widget() if hasattr(pc, 'get_chat_widget') else None
chat.end_focus() if chat else log('no chat widget')
log('qa_chat_close.py')
