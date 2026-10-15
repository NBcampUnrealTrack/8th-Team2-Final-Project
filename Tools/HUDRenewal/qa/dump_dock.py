# -*- coding: utf-8 -*-
"""Read-only: list every widget in WBP_HUD_DockButton with its opacity, visibility and brush (draw type, tint, outline),
to find what draws the always-on frame around each dock icon. Prints lines starting with [DOCK]."""
import unreal

PATH = '/Game/Art/UI/HUD2026/Field/WBP_HUD_DockButton'


def brush_text(b):
    try:
        o = b.outline_settings
        return 'draw=%s tint=%s res=%s outline=(%s w=%.2f r=%s)' % (b.draw_as, b.tint_color.specified_color, b.resource_object,
                                                                    o.color.specified_color, o.width, o.corner_radii)
    except Exception as e:
        return 'brush? %s' % e


bp = unreal.load_asset(PATH)
tree = unreal.find_object(bp, 'WidgetTree')
for w in unreal.ObjectIterator(unreal.Widget):
    try:
        if w.get_outer() != tree:
            continue
    except Exception:
        continue
    line = '%s (%s) opacity=%.2f vis=%s' % (w.get_name(), w.get_class().get_name(), w.get_render_opacity(), w.get_visibility())
    if isinstance(w, unreal.Button):
        st = w.get_editor_property('widget_style')
        line += ' | normal ' + brush_text(st.normal) + ' | hovered ' + brush_text(st.hovered)
    elif isinstance(w, unreal.Image):
        line += ' | ' + brush_text(w.get_editor_property('brush'))
    unreal.log('[DOCK] ' + line)
cdo = unreal.get_default_object(bp.generated_class())
for prop in ('icon_only', 'b_icon_only', 'lift_px', 'button_margin'):
    try:
        unreal.log('[DOCK] cdo %s=%s' % (prop, cdo.get_editor_property(prop)))
    except Exception:
        pass
unreal.log('[DOCK] DONE')
