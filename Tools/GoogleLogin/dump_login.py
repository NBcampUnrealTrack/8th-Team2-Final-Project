# -*- coding: utf-8 -*-
"""Read-only: prints the widget tree of the current login widget and the login game modes' widget classes.
Headless: UnrealEditor-Cmd ProjectM.uproject -run=pythonscript -script=Tools/GoogleLogin/dump_login.py"""
import unreal


def log(message):
    unreal.log('[LOGIN] ' + str(message))


def describe(widget, depth):
    line = '%s%s %s vis=%s' % ('  ' * depth, widget.get_class().get_name(), widget.get_name(), widget.get_visibility())
    slot = widget.slot
    if isinstance(slot, unreal.CanvasPanelSlot):
        layout = slot.get_layout()
        a = layout.anchors
        line += ' anchors=(%.2f,%.2f,%.2f,%.2f) offs=(%.0f,%.0f,%.0f,%.0f) align=(%.2f,%.2f) auto=%s z=%d' % (
            a.minimum.x, a.minimum.y, a.maximum.x, a.maximum.y, layout.offsets.left, layout.offsets.top, layout.offsets.right,
            layout.offsets.bottom, layout.alignment.x, layout.alignment.y, slot.get_auto_size(), slot.get_z_order())
    elif slot is not None:
        try:
            p = slot.get_editor_property('padding')
            line += ' pad=(%.0f,%.0f,%.0f,%.0f)' % (p.left, p.top, p.right, p.bottom)
        except Exception:
            pass
    if isinstance(widget, unreal.Image):
        brush = widget.get_editor_property('brush')
        res = brush.get_editor_property('resource_object')
        line += ' img=%s size=%s draw=%s' % (res.get_path_name() if res else None, brush.image_size, brush.draw_as)
    if isinstance(widget, unreal.TextBlock):
        line += ' text="%s"' % widget.get_text()
    if isinstance(widget, unreal.Button):
        style = widget.get_editor_property('widget_style')
        res = style.normal.get_editor_property('resource_object')
        line += ' normal=%s size=%s draw=%s' % (res.get_path_name() if res else None, style.normal.image_size, style.normal.draw_as)
    if isinstance(widget, unreal.Border):
        brush = widget.get_editor_property('background')
        res = brush.get_editor_property('resource_object')
        line += ' bg=%s draw=%s' % (res.get_path_name() if res else None, brush.draw_as)
    if isinstance(widget, unreal.SizeBox):
        line += ' w=%s h=%s' % (widget.get_editor_property('width_override') if widget.get_editor_property('override_width_override') else '-',
                                widget.get_editor_property('height_override') if widget.get_editor_property('override_height_override') else '-')
    log(line)
    if isinstance(widget, unreal.PanelWidget):
        for child in widget.get_all_children():
            describe(child, depth + 1)


for path in ('/Game/Developers/dongh/UI/Login/WBP_Login',):
    bp = unreal.load_asset(path)
    log('== %s' % path)
    root = unreal.WidgetService.get_root_widget(path)
    describe(unreal.find_object(unreal.find_object(bp, 'WidgetTree'), root), 0)

for path in ('/Game/GameMode/BP_LoginGameMode', '/Game/Developers/dbals/BP_dbals_LoginGameMode', '/Game/Developers/dongh/GameMode/BP_TestLoginGameMode'):
    bp = unreal.load_asset(path)
    if not bp:
        continue
    cdo = unreal.get_default_object(bp.generated_class())
    log('%s: widget=%s field=%s' % (path, cdo.get_editor_property('login_widget_class'),
                                             cdo.get_editor_property('field_level')))
world = unreal.load_asset('/Game/Maps/L_LoginLevel')
log('L_LoginLevel loaded=%s' % bool(world))
log('DONE')
