# -*- coding: utf-8 -*-
"""HUD QA: log every dock button's runtime state (to find any stray frame), then hover the inventory button."""
import unreal
n = 0
for w in unreal.ObjectIterator(unreal.UserWidget):
    try:
        if not w.get_class().get_name().startswith('WBP_HUD_DockButton') or w.get_world() != GAME_WORLD:
            continue
        mark = w.get_editor_property('hover_mark')
        btn = w.get_editor_property('btn_common')
        style = btn.get_editor_property('widget_style') if btn else None
        log('dock %s enabled=%s mark=%s opacity=%s normal=%s disabled=%s' % (
            w.get_name(), w.get_is_enabled(), mark.get_name() if mark else None, mark.get_render_opacity() if mark else None,
            style.normal.draw_as if style else None, style.disabled.draw_as if style else None))
        if w.get_name().startswith('InventoryButton'):
            w.call_method('HandleHovered')
        n += 1
    except Exception as e:
        log('dock diag error %s' % e)
log('qa_dock_hover %d' % n)
