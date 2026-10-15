# -*- coding: utf-8 -*-
"""Read-only: prints the settings of the current HUD widgets that the new ones must keep (saves nothing)."""
import unreal

def log(m): unreal.log('[HUD] ' + str(m))

def cdo(path):
    bp = unreal.load_asset(path)
    return unreal.get_default_object(bp.generated_class()) if bp else None

def props(path, names):
    d = cdo(path)
    if not d:
        log('MISSING ' + path); return
    for n in names:
        try:
            log('%s.%s = %s' % (path.rsplit('/', 1)[1], n, d.get_editor_property(n)))
        except Exception as e:
            log('%s.%s ERR %s' % (path.rsplit('/', 1)[1], n, e))

def tree(path):
    bp = unreal.load_asset(path)
    wt = unreal.find_object(bp, 'WidgetTree')
    names = []
    for w in unreal.WidgetService.list_widgets(path) if hasattr(unreal.WidgetService, 'list_widgets') else []:
        names.append(str(w))
    log('tree %s %s' % (path, names[:80]))

props('/Game/Developers/dbals/UI/WBP_MapMarker', ['marker_textures', 'marker_tints'])
props('/Game/Developers/dbals/UI/WBP_MapImageView', ['b_enable_zoom', 'max_zoom', 'b_center_on_player', 'center_zoom', 'icon_marker_class', 'icon_marker_size'])
props('/Game/Developers/zxcv1/Quest/UI/WBP_QuestWidget', ['tracker_entry_widget_class', 'tracker_entry_padding', 'reward_popup_class'])
props('/Game/Developers/dongh/UI/Chat/WBP_Chat', ['tab_button_class', 'b_stretch_tabs_to_fill', 'tab_slot_padding', 'max_visible_tabs', 'channel_button_class', 'b_match_channel_menu_to_send_button', 'line_widget_class'])
props('/Game/Developers/dongh/UI/WBP_PartyList', ['member_row_class'])
props('/Game/Developers/dongh/UI/WBP_ActionBanner', ['lifetime'])
props('/Game/Developers/dbals/UI/WBP_NoticeBanner', ['lifetime'])
props('/Game/Developers/dongh/UI/Chat/WBP_ChatLine', ['b_apply_channel_color', 'b_auto_wrap_lines'])
props('/Game/Developers/MinkiCho/Art/FieldHUD/Chat_New/Widgets/WBP_ChatRootTab', ['default_label', 'selected_label_color', 'idle_label_color'])
for p in ('/Game/Developers/zxcv1/UI/WBP_InteractIndicator', '/Game/Developers/dongh/UI/WBP_PartyPrompt', '/Game/Developers/dongh/UI/WBP_PartyMemberRow',
          '/Game/Developers/dbals/UI/WBP_Nameplate', '/Game/Developers/dbals/UI/WBP_Minimap', '/Game/Developers/dongh/UI/Chat/WBP_Chat'):
    bp = unreal.load_asset(p)
    wt = unreal.find_object(bp, 'WidgetTree') if bp else None
    found = []
    if wt:
        for w in unreal.ObjectIterator(unreal.Widget):
            if w.get_outer() == wt:
                t = ''
                if isinstance(w, unreal.TextBlock): t = ' "%s"' % w.get_text()
                found.append(w.get_class().get_name() + ':' + w.get_name() + t)
    log('tree %s -> %s' % (p.rsplit('/', 1)[1], ', '.join(found)[:1500]))
log('DONE')
