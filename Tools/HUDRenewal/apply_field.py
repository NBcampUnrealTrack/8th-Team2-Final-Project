# -*- coding: utf-8 -*-
"""Field HUD widgets of the new in-game HUD (concept A, glass edge). Run headless through run.ps1 -Steps field.

Every widget is made from nothing in /Game/Art/UI/HUD2026/Field (run.ps1 deletes the old copies first) as a child of the
SAME C++ class the old one used, so the game logic stays; only the look is new. Nothing of the old HUD is used.
Layout is in 1080p units (the project's DPI rule: shortest side 1080 = 1x), corners 40 / 36 px in from the screen edge.
"""
import importlib
import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/HUDRenewal'))
import hud_kit
importlib.reload(hud_kit)
from hud_kit import *            # noqa: F401,F403

D = FIELD + '/'
EDGE_X, EDGE_Y = 40, 36


def wbp(name, parent):
    return widget_blueprint(D + name, '/Script/ProjectM.' + parent if not parent.startswith('/') else parent)


# ======================================================================= dock button
def build_dock_button():
    t = wbp('WBP_HUD_DockButton', 'NU_HudButtonWidget')
    sized(t, 'Size', '', 52, 52)
    overlay(t, 'Stack', 'Size')
    box(flat_button(t, 'Btn_Common', 'Stack'))
    lift = box(overlay(t, 'HoverLift', 'Stack', VIS.HIT_TEST_INVISIBLE))
    mark = box(overlay(t, 'HoverMark', 'HoverLift', VIS.HIT_TEST_INVISIBLE), pad=(2, 2, 2, 2))
    mark.set_render_opacity(0.0)                     # C++ fades it in on hover
    box(image(t, 'HoverFrame', 'HoverMark', tex('T_HUD_HoverFrame')))
    box(image(t, 'HoverLine', 'HoverMark', rbox(('E8C46E', 1.0), 1, None, 0.0, (16, 2))), pad=(0, 0, 0, 4), h='center', v='bottom')
    box(image(t, 'Img_Icon', 'HoverLift', icon_brush('bag', 26)), h='center', v='center')
    anchor = box(overlay(t, 'TooltipAnchor', 'Stack', VIS.HIT_TEST_INVISIBLE), pad=(-90, -50, -90, 60))
    tip = box(t.add('Border', 'Tooltip', 'TooltipAnchor', VIS.HIT_TEST_INVISIBLE), h='center', v='center')
    tip.set_brush(rbox(('0C1622', 0.95), None, ('F1EDE6', 0.14), 1.0))
    tip.set_padding(unreal.Margin(14, 6, 8, 6))
    row = hbox(t, 'TooltipRow', 'Tooltip')
    box(label(t, 'Txt_Label', 'TooltipRow', '소지품', 14.5, True, shadow=False), v='center')
    cap = box(t.add('Border', 'KeyCap', 'TooltipRow', VIS.HIT_TEST_INVISIBLE), pad=(9, 0, 0, 0), v='center')
    cap.set_brush(rbox(('F1EDE6', 0.08), 5, ('F1EDE6', 0.40), 1.0))
    cap.set_padding(unreal.Margin(6, 0, 6, 1))
    text(t, 'Txt_Key', 'KeyCap', 'I', 12, True, (IVORY, 1.0))
    tip.set_visibility(VIS.COLLAPSED)
    t.finish()


# ======================================================================= status plate (my character)
def build_player_status():
    t = wbp('WBP_HUD_PlayerStatus', 'NU_PlayerVitalWidget')
    sized(t, 'Size', '', 384, 104)
    content = glass(t, 'Plate', 'Size', 16)
    body = box(t.add('CanvasPanel', 'Body', content, VIS.SELF_HIT_TEST_INVISIBLE))

    face = canvas(overlay(t, 'Face', 'Body', VIS.HIT_TEST_INVISIBLE), (0, 0, 0, 0), (16, 16, 72, 72))
    box(image(t, 'FaceBg', 'Face', rbox(('26394E', 1.0), None)))
    clipbox = box(clip(sized(t, 'FaceClip', 'Face', 72, 72)))
    box(overlay(t, 'PortraitBox', 'FaceClip', VIS.HIT_TEST_INVISIBLE), pad=(-24, -34, -24, -94))
    box(image(t, 'FaceMask', 'Face', tex('T_HUD_PortraitMask72', (72, 72), ('FFFFFF', 0.92))))
    box(image(t, 'FaceRing', 'Face', rbox(('FFFFFF', 0.0), None, (GOLD, 1.0), 2.0)))

    badge = canvas(t.add('Border', 'LevelBadge', 'Body', VIS.HIT_TEST_INVISIBLE), (0, 0, 0, 0), (64, 66, 0, 0), auto=True)
    badge.set_brush(rbox(('D6BE8C', 1.0), None, ('0E1A28', 0.9), 2.0))
    badge.set_padding(unreal.Margin(8, 1, 8, 2))
    badge.set_horizontal_alignment(HA['center'])
    text(t, 'PlayerLevelText', 'LevelBadge', '12', 13, True, (NAVY, 1.0))

    row = canvas(hbox(t, 'NameRow', 'Body'), (0, 0, 0, 0), (104, 12, 0, 0), auto=True)
    box(label(t, 'PlayerNameText', 'NameRow', '새벽', 21, True, shadow=False), v='bottom')
    box(label(t, 'PlayerJobText', 'NameRow', '노비스', 13.5, False, (MUTED, 1.0), shadow=False), pad=(9, 0, 0, 3), v='bottom')

    for i, (key, fill, value) in enumerate((('Health', 'T_HUD_BarHP', '812'), ('SP', 'T_HUD_BarSP', '60'))):
        rname = 'HPRow' if key == 'Health' else 'SPRow'
        r = canvas(hbox(t, rname, 'Body'), (0, 0, 0, 0), (104, 52 + i * 22, 264, 18))
        box(label(t, rname + 'Label', rname, 'HP' if key == 'Health' else 'SP', 11.5, True, (GOLD, 1.0), shadow=False), pad=(0, 0, 8, 0), v='center')
        stack = box(overlay(t, rname + 'Bars', rname, VIS.HIT_TEST_INVISIBLE), v='center', fill=1.0)
        hold = box(sized(t, rname + 'BarSize', rname + 'Bars', None, 8))
        bars = box(overlay(t, rname + 'BarStack', rname + 'BarSize', VIS.HIT_TEST_INVISIBLE))
        if key == 'Health':
            box(bar(t, 'HealthGhostBar', rname + 'BarStack', 'T_HUD_BarFoe', True))
            box(bar(t, 'HealthBar', rname + 'BarStack', fill, False))
        else:
            box(bar(t, 'SPBar', rname + 'BarStack', fill, True))
        v = box(label(t, key + 'ValueText', rname, value, 13, True, ('E7E2D9', 1.0), shadow=False), pad=(8, 0, 0, 0), v='center')
        v.set_editor_property('justification', unreal.TextJustify.RIGHT)
        v.set_editor_property('min_desired_width', 34.0)
    t.finish()
    d = cdo(t)
    d.set_editor_property('portrait_pixel_scale', 2.0)
    t.finish()


# ======================================================================= minimap
# quest / portal badges by marker tag (Docs/HUD_QuestMap_Proposal_20261014): one colour per state
BADGES = {'NU.Marker.Quest.Offer': 'T_HUD_Badge_Offer', 'NU.Marker.Quest.TurnIn': 'T_HUD_Badge_TurnIn',
          'NU.Marker.Quest.InProgress': 'T_HUD_Badge_InProgress', 'NU.Marker.Quest.Locked': 'T_HUD_Badge_Locked',
          'NU.Marker.Quest.Target': 'T_HUD_Badge_Target', 'NU.Marker.Map.Portal': 'T_HUD_Badge_Portal'}
MARKER_PX = 26


def build_map_marker():
    t = wbp('WBP_HUD_MapMarker', 'NUWorldMarkerWidget')
    overlay(t, 'MarkerRoot', '', VIS.HIT_TEST_INVISIBLE)
    # a ring that keeps growing out of "go here" badges (TurnIn / Target, C++ PulseTags); bigger than the 26 px slot
    pulse = box(image(t, 'MarkerPulse', 'MarkerRoot', material_brush(FX + '/M_HUD_Pulse', (52, 52))), pad=(-13, -13, -13, -13))
    pulse.set_visibility(VIS.COLLAPSED)
    box(image(t, 'MarkerImage', 'MarkerRoot', tex('T_HUD_Badge_Offer', (MARKER_PX, MARKER_PX))))
    # the edge pointer: the C++ turns and pushes it out when the badge is held at the minimap rim
    ptr = box(image(t, 'MarkerEdgePointer', 'MarkerRoot', tex('T_HUD_EdgePointer', (14, 10))), h='center', v='center')
    ptr.set_visibility(VIS.COLLAPSED)
    t.finish()
    old = unreal.get_default_object(unreal.load_asset('/Game/Developers/dbals/UI/WBP_MapMarker').generated_class())
    textures = old.get_editor_property('marker_textures')
    for key in list(textures.keys()):
        tag = str(key.get_editor_property('tag_name'))
        new = BADGES.get(tag)
        if new:
            textures[key] = unreal.load_asset(KIT + '/' + new)
        else:
            log('marker texture kept: %s %s' % (tag, textures[key].get_name() if textures[key] else ''))
    tints = old.get_editor_property('marker_tints')
    tints.clear()                         # the badges carry their own colours (the old grey tint dimmed "locked")
    d = cdo(t)
    d.set_editor_property('marker_textures', textures)
    d.set_editor_property('marker_tints', tints)
    d.set_editor_property('edge_pointer_distance', 18.0)
    t.finish()
    log('marker map: %d tags' % len(textures))


# NPC head quest marker (Docs/HUD_NpcMarker_Proposal_20261014, option A): the map badges, drawn big for the world.
# Every NPC BP points at zxcv1 WBP_WorldMarker; the C++ swaps in this one through NU UI > HeadMarkerClass (apply step), with
# M_HUD_WorldMarker (soft edges), a 160 px canvas drawn 80 cm wide and the pivot low so the badge sits just above the head.
HEAD_BADGES = {'NU.Marker.Quest.Offer': 'T_HUD_Head_Offer', 'NU.Marker.Quest.TurnIn': 'T_HUD_Head_TurnIn',
               'NU.Marker.Quest.Target': 'T_HUD_Head_Target', 'NU.Marker.Quest.Locked': 'T_HUD_Head_Locked'}
HEAD_CANVAS, HEAD_BADGE = 160, 124


def build_head_marker():
    t = wbp('WBP_HUD_HeadMarker', 'NUWorldMarkerWidget')
    overlay(t, 'MarkerRoot', '', VIS.HIT_TEST_INVISIBLE)
    box(sized(t, 'MarkerSize', 'MarkerRoot', HEAD_CANVAS, HEAD_CANVAS))
    # everything floats together (C++ MarkerFloat: slow bob, pop when the marker appears or changes)
    box(overlay(t, 'MarkerFloat', 'MarkerRoot', VIS.HIT_TEST_INVISIBLE))
    box(image(t, 'MarkerShade', 'MarkerFloat', tex('T_HUD_MapPlayerHalo', (136, 136), ('FFFFFF', 0.95))), h='center', v='center')
    pulse = box(image(t, 'MarkerPulse', 'MarkerFloat', material_brush(FX + '/MI_HUD_HeadRing', (HEAD_CANVAS - 4, HEAD_CANVAS - 4))),
                h='center', v='center')
    pulse.set_visibility(VIS.COLLAPSED)
    box(image(t, 'MarkerImage', 'MarkerFloat', tex('T_HUD_Head_Offer', (HEAD_BADGE, HEAD_BADGE))), h='center', v='center')
    t.finish()
    old = unreal.get_default_object(unreal.load_asset('/Game/Developers/zxcv1/UI/WBP_WorldMarker').generated_class())
    textures = old.get_editor_property('marker_textures')
    for key in list(textures.keys()):
        tag = str(key.get_editor_property('tag_name'))
        new = HEAD_BADGES.get(tag)
        if new:
            textures[key] = unreal.load_asset(KIT + '/' + new)
        else:
            log('head marker texture kept: %s %s' % (tag, textures[key].get_name() if textures[key] else ''))
    tints = old.get_editor_property('marker_tints')
    tints.clear()                         # the badges carry their own colours
    d = cdo(t)
    d.set_editor_property('marker_textures', textures)
    d.set_editor_property('marker_tints', tints)
    d.set_editor_property('float_amplitude', 5.0)
    d.set_editor_property('float_period', 2.4)
    d.set_editor_property('pulse_on_offer', True)   # the ring also on a new quest (map markers ring only for report / target)
    log('head marker: %d tags' % len(textures))
    t.finish()


def build_map_view():
    t = wbp('WBP_HUD_MapView', 'NU_MapImageViewWidget')
    overlay(t, 'Root', '')
    # plain white brush: SetBrushFromTexture keeps the brush tint, so a coloured placeholder darkens the map (it was
    # multiplied by #2B4636 --- ~7x darker). With no map the C++ collapses the whole view, so white never shows.
    box(image(t, 'Img_Map', 'Root', flat('FFFFFF', 1.0)))
    box(t.add('CanvasPanel', 'Canvas_Markers', 'Root', VIS.SELF_HIT_TEST_INVISIBLE))
    # player: facing cone + halo + pulse + arrow, turned as a whole by the C++ (bRotatePlayerMarker) about its centre
    player = canvas(overlay(t, 'Marker_Player', 'Canvas_Markers', VIS.HIT_TEST_INVISIBLE), (0.5, 0.5, 0.5, 0.5), (0, 0, 96, 96), (0.5, 0.5), auto=True, z=5)
    box(image(t, 'PlayerCone', 'Marker_Player', tex('T_HUD_MapPlayerCone', (96, 96))), h='center', v='center')
    box(image(t, 'PlayerHalo', 'Marker_Player', tex('T_HUD_MapPlayerHalo', (38, 38))), h='center', v='center')
    box(image(t, 'PlayerPulse', 'Marker_Player', material_brush(FX + '/M_HUD_Pulse', (44, 44))), h='center', v='center')
    box(image(t, 'PlayerArrow', 'Marker_Player', tex('T_HUD_MapPlayer', (30, 30))), h='center', v='center')
    t.finish()
    d = cdo(t)
    for prop, value in (('icon_marker_class', unreal.load_class(None, D + 'WBP_HUD_MapMarker.WBP_HUD_MapMarker_C')),
                        ('icon_marker_size', unreal.Vector2D(MARKER_PX, MARKER_PX)), ('center_on_player', True), ('center_zoom', 3.0),
                        ('rotate_player_marker', True), ('edge_inset', 26.0)):
        try:
            d.set_editor_property(prop, value)
        except Exception as error:
            log('map view %s: %s' % (prop, error))
    t.finish()


def build_minimap():
    t = wbp('WBP_HUD_Minimap', 'NU_MinimapWidget')
    sized(t, 'Size', '', 252, 252)
    frame = overlay(t, 'Frame', 'Size')
    content = glass(t, 'MapGlass', 'Frame', 24)
    clipped = box(clip(overlay(t, 'MapClip', content, VIS.SELF_HIT_TEST_INVISIBLE)), pad=(8, 8, 8, 8))
    box(t.add('WBP_HUD_MapView', 'MapView', 'MapClip'))
    box(image(t, 'MapVignette', 'MapClip', tex('T_HUD_MapVignette')))
    box(image(t, 'MapCorners', 'MapClip', tex('T_HUD_MapCorners', None, ('FFFFFF', 0.95))))
    chip = box(t.add('Border', 'NorthChip', 'Frame', VIS.HIT_TEST_INVISIBLE), pad=(0, -11, 0, 0), h='center', v='top')
    chip.set_brush(rbox(('16293D', 1.0), None, (GOLD, 0.8), 1.0))
    chip.set_padding(unreal.Margin(9, 1, 9, 2))
    text(t, 'NorthText', 'NorthChip', 'N', 12, True, (GOLD, 1.0))
    t.finish()


# ======================================================================= field HUD frame
DOCK_PAD = 6
DOCK = [('QuestJournalButton', 'quest', '퀘스트 일지'), ('CharacterInfoButton', 'character', '캐릭터'), ('StatsButton', 'stats', '능력치'),
        ('GrowthTreeButton', 'growth', '성장'), ('InventoryButton', 'bag', '소지품'), ('NaiaRosterButton', 'naia', '나이아')]
SHORTCUTS = {'QuestJournalButton': '/Game/Input/Field/IA_ToggleQuestJournal', 'CharacterInfoButton': '/Game/Developers/dbals/Input/IA_ToggleCharacterInfo',
             'StatsButton': '/Game/Input/UI/IA_UI_ToggleCharacterStats', 'GrowthTreeButton': '/Game/Input/UI/IA_UI_ToggleProgressionGraph',
             'InventoryButton': '/Game/Developers/dbals/Input/IA_ToggleInventory', 'NaiaRosterButton': '/Game/Input/Inventory/IA_ToggleNaiaRoster'}


def set_dock_button(item, icon_name, text_label, action=None):
    item.set_editor_property('icon_texture', unreal.load_asset(KIT + '/T_HUD_Icon_' + icon_name.capitalize()))
    item.set_editor_property('icon_size', unreal.Vector2D(26, 26))
    item.set_editor_property('label_text', text_label)
    if action:
        item.set_editor_property('shortcut_action', unreal.load_asset(action))


def build_field():
    t = wbp('WBP_HUD_Field', 'NU_FieldHUDWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(t.add('CanvasPanel', 'FieldHUDContent', 'Root', VIS.SELF_HIT_TEST_INVISIBLE), (0, 0, 1, 1), (0, 0, 0, 0))

    canvas(overlay(t, 'HudTopLeft', 'FieldHUDContent'), (0, 0, 0, 0), (EDGE_X, EDGE_Y, 0, 0), auto=True)
    box(t.add('WBP_HUD_PlayerStatus', 'PlayerStatus', 'HudTopLeft'))

    canvas(overlay(t, 'HudTopRight', 'FieldHUDContent'), (1, 0, 1, 0), (-EDGE_X, EDGE_Y, 0, 0), (1, 0), auto=True)
    box(t.add('WBP_HUD_Minimap', 'Minimap', 'HudTopRight'))

    canvas(overlay(t, 'HudBottomRight', 'FieldHUDContent'), (1, 1, 1, 1), (-EDGE_X, -EDGE_Y, 0, 0), (1, 1), auto=True)
    content = glass(t, 'Dock', 'HudBottomRight', 'pill')
    row = box(hbox(t, 'DockRow', content), pad=(DOCK_PAD, DOCK_PAD, DOCK_PAD, DOCK_PAD))
    for name, icon_name, text_label in DOCK:
        item = box(t.add('WBP_HUD_DockButton', name, 'DockRow'), pad=(0, 0, 2, 0))
        set_dock_button(item, icon_name, text_label, SHORTCUTS.get(name))
    box(image(t, 'DockDivider', 'DockRow', flat(IVORY, 0.18, (1, 26))), pad=(8, 0, 10, 0), v='center')
    menu = box(t.add('WBP_HUD_DockButton', 'GameMenuButton', 'DockRow'), pad=(0, 0, 2, 0))
    set_dock_button(menu, 'menu', '메뉴')
    sized(t, 'EyeSlot', 'DockRow', 52, 52)          # the eye button sits over this

    # the HUD toggle stays outside FieldHUDContent so a hidden HUD can be brought back
    canvas(overlay(t, 'EyeWrap', 'Root'), (1, 1, 1, 1), (-EDGE_X - DOCK_PAD, -EDGE_Y - DOCK_PAD, 0, 0), (1, 1), auto=True)
    backdrop = glass(t, 'EyeBackdrop', 'EyeWrap', 'pill')
    t.get('EyeBackdrop').slot.set_padding(unreal.Margin(-DOCK_PAD, -DOCK_PAD, -DOCK_PAD, -DOCK_PAD))
    t.get('EyeBackdrop').set_visibility(VIS.COLLAPSED)          # C++ shows it only while the HUD is hidden
    box(sized(t, 'EyeSize', 'EyeWrap', 52, 52))
    hover = tex('T_HUD_HoverFrame')
    btn = flat_button(t, 'HUDVisibilityButton', 'EyeSize', nodraw(), hover, tex('T_HUD_HoverFrame', None, ('FFFFFF', 1.0)))
    switch = t.add('WidgetSwitcher', 'HUDVisibilityIcons', 'HUDVisibilityButton', VIS.HIT_TEST_INVISIBLE)
    box(switch, h='center', v='center')
    image(t, 'EyeOpen', 'HUDVisibilityIcons', icon_brush('eye', 26))
    image(t, 'EyeClosed', 'HUDVisibilityIcons', icon_brush('eyeoff', 26))
    t.finish()


# ======================================================================= location line + area banner
def build_location():
    t = wbp('WBP_HUD_Location', 'NUWorldLocationWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    line = canvas(hbox(t, 'LocationLine', 'Root'), (1, 0, 1, 0), (-EDGE_X - 2, 300, 0, 0), (1, 0), auto=True)
    box(image(t, 'LocDiamond', 'LocationLine', tex('T_HUD_Diamond', (9, 9))), pad=(0, 0, 9, 0), v='center')
    box(label(t, 'AreaText', 'LocationLine', '리베르그로브', 21, True), v='center')
    box(label(t, 'LocDot', 'LocationLine', '·', 16, True, (GOLD, 1.0)), pad=(8, 0, 8, 0), v='center')
    box(label(t, 'LocationText', 'LocationLine', '공방 구역', 15.5, False, ('D5DADE', 1.0)), pad=(0, 2, 0, 0), v='center')

    root = canvas(overlay(t, 'BannerRoot', 'Root', VIS.HIT_TEST_INVISIBLE), (0.5, 0, 0.5, 0), (0, 178, 0, 0), (0.5, 0), auto=True)
    box(image(t, 'BannerGlow', 'BannerRoot', tex('T_HUD_BannerGlow', (900, 230))), h='center', v='center')
    stack = box(vbox(t, 'BannerStack', 'BannerRoot'), h='center', v='center')
    box(label(t, 'BannerAreaText', 'BannerStack', '펀브룩', 17, True, (GOLD_HI, 1.0), 480), h='center')
    row = box(hbox(t, 'BannerTitleRow', 'BannerStack'), pad=(0, 6, 0, 0), h='center')
    box(image(t, 'BannerRuleLeft', 'BannerTitleRow', tex('T_HUD_RuleL', (150, 2))), v='center')
    title = box(label(t, 'BannerLocationText', 'BannerTitleRow', '수레길 진입로', 52, True, spacing=40), pad=(22, 0, 22, 0), v='center')
    title.set_shadow_offset(unreal.Vector2D(0, 2))
    box(image(t, 'BannerRuleRight', 'BannerTitleRow', tex('T_HUD_RuleR', (150, 2))), v='center')
    root.set_visibility(VIS.COLLAPSED)
    t.finish()


# ======================================================================= notice toast
def build_notice():
    t = wbp('WBP_HUD_Notice', 'NUNoticeBannerWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(overlay(t, 'NoticeRoot', 'Root', VIS.HIT_TEST_INVISIBLE), (0.5, 0, 0.5, 0), (0, 32, 0, 0), (0.5, 0), auto=True)
    content = glass(t, 'NoticeGlass', 'NoticeRoot', 20)
    row = box(hbox(t, 'NoticeRow', content), pad=(12, 11, 24, 11))
    ic = box(overlay(t, 'NoticeIcon', 'NoticeRow', VIS.HIT_TEST_INVISIBLE), v='center')
    sized(t, 'NoticeIconSize', 'NoticeIcon', 40, 40)
    box(image(t, 'NoticeIconBg', 'NoticeIcon', rbox(('D6BE8C', 0.20), 12)))
    box(image(t, 'NoticeIconImg', 'NoticeIcon', icon_brush('spark', 26)), h='center', v='center')
    msg = box(label(t, 'MessageText', 'NoticeRow', '알림 문구', 16.5, True, shadow=False), pad=(14, 0, 0, 0), v='center')
    t.finish()


# ======================================================================= interaction prompt
def build_interact():
    """Interaction cue (Docs/HUD_Interact_Proposal_20261014, option A): an ivory keycap docked to the head marker's lower right
    corner (the player controller places it there, InteractionIndicatorMarkerAnchor), or above the target when it has no marker.
    Neutral ivory against the gold quest marks, and no rings or glow: while it stays the key is pressed and released
    (C++ KeyCap moves down, KeyLip --- the key's lower edge --- fades), so it reads as "press" rather than as a quest effect.
    Rims are filled boxes, not outlines: a ROUNDED_BOX outline ignores render opacity and would hang on during the pop."""
    t = wbp('WBP_HUD_Interact', 'NU_InteractPromptWidget')
    overlay(t, 'PromptRoot', '', VIS.HIT_TEST_INVISIBLE)
    box(sized(t, 'PromptSize', 'PromptRoot', 48, 50))
    box(image(t, 'KeyShade', 'PromptRoot', tex('T_HUD_MapPlayerHalo', (56, 56), ('FFFFFF', 0.85))), h='center', v='center', pad=(0, 6, 0, 0))
    lip = box(sized(t, 'KeyLipSize', 'PromptRoot', 36, 36), h='center', v='center', pad=(0, 8, 0, 0))
    box(image(t, 'KeyLip', 'KeyLipSize', rbox(('0E1A28', 1.0), 10)))
    cap = box(sized(t, 'KeyCap', 'PromptRoot', 36, 36), h='center', v='center', pad=(0, 0, 0, 2))
    face = box(overlay(t, 'KeyFace', 'KeyCap', VIS.HIT_TEST_INVISIBLE))
    box(image(t, 'KeyRim', 'KeyFace', rbox(('0E1A28', 1.0), 10)))
    box(image(t, 'KeyFill', 'KeyFace', rbox(('F1EDE6', 1.0), 8)), pad=(2.5, 2.5, 2.5, 2.5))
    box(image(t, 'KeySheen', 'KeyFace', rbox(('FFFFFF', 0.55), 7, None, 0.0, (16, 12))), pad=(4.5, 4, 4.5, 0), v='top')
    box(text(t, 'KeyText', 'KeyFace', 'F', 19, True, (NAVY, 1.0)), h='center', v='center')
    t.finish()


# ======================================================================= name plate
def build_nameplate():
    t = wbp('WBP_HUD_Nameplate', 'NUNameplateWidget')
    plate = t.add('Border', 'Plate', '', VIS.HIT_TEST_INVISIBLE)
    plate.set_brush(rbox(('080E16', 0.42), None))
    plate.set_padding(unreal.Margin(12, 2, 12, 3))
    plate.set_horizontal_alignment(HA['center'])
    label(t, 'NameText', 'Plate', '닉네임', 16, True)
    t.finish()


# ======================================================================= quest tracker + offer
# tracker text sits on a scrim straight over the scene (10/15 rollback): a thin dark outline keeps it readable over bright roofs
TRACKER_OUTLINE = 1


def outlined(item, size=TRACKER_OUTLINE, colour=('09121C', 0.85)):
    """A thin dark outline around a text (font outline settings), for text drawn over the scene."""
    if not size:
        return item
    info = item.get_editor_property('font')
    outline = unreal.FontOutlineSettings()
    outline.set_editor_property('outline_size', size)
    outline.set_editor_property('outline_color', lin(*colour))
    info.set_editor_property('outline_settings', outline)
    item.set_font(info)
    return item


def build_quest_entry():
    """One tracked quest inside the glass tracker. Ready-to-turn-in: gold title + "보고" chip + a gold wash (C++)."""
    t = wbp('WBP_HUD_QuestEntry', 'NUQuestTrackerEntryWidget')
    overlay(t, 'Entry', '')
    back = box(image(t, 'EntryReadyBack', 'Entry', tex('T_HUD_RuleR', (256, 4), ('FFFFFF', 0.20))), pad=(-8, -4, -8, -4))   # gold fading right
    back.set_visibility(VIS.COLLAPSED)
    flash = box(image(t, 'EntryFlash', 'Entry', rbox(('F4D78F', 0.34), 10)), pad=(-8, -4, -8, -4))
    flash.set_render_opacity(0.0)
    body = box(vbox(t, 'EntryBody', 'Entry'))
    row = box(hbox(t, 'TitleRow', 'EntryBody'))
    box(image(t, 'EntryDiamond', 'TitleRow', tex('T_HUD_Diamond', (9, 9))), pad=(0, 0, 10, 0), v='center')
    box(outlined(label(t, 'EntryTitleText', 'TitleRow', '뿌리 사이로', 17, True, shadow=False, wrap=True)), v='center', fill=1.0)
    badge = box(t.add('Border', 'EntryTurnInBadge', 'TitleRow', VIS.HIT_TEST_INVISIBLE), pad=(8, 0, 0, 0), v='center')
    badge.set_brush(rbox(('F4D78F', 1.0), None))
    badge.set_padding(unreal.Margin(9, 1, 9, 2))
    text(t, 'EntryTurnInText', 'EntryTurnInBadge', '보고', 12, True, (NAVY, 1.0))
    badge.set_visibility(VIS.COLLAPSED)
    box(outlined(label(t, 'EntryObjectiveText', 'EntryBody', '리베르그로브 아랫마을에 도착하기', 14.5, False, ('DCE1E5', 1.0), shadow=False, wrap=True)), pad=(19, 2, 0, 0))
    t.finish()
    d = cdo(t)
    d.set_editor_property('apply_ready_style', True)
    t.finish()


def reward_icon(name, size):
    """Reward / element pictures imported by apply_select into /Game/Art/UI/HUD2026/Icons (item-icon tone, 144 px)."""
    asset = unreal.load_asset(BASE + '/Icons/' + name)
    assert asset, name + ' missing: run.ps1 -Steps select first'
    brush = unreal.SlateBrush()
    brush.set_editor_property('resource_object', asset)
    brush.image_size = vec2(size, size)
    return brush


def build_quest_rows():
    t = wbp('WBP_HUD_QuestObjRow', '/Script/UMG.UserWidget')
    hbox(t, 'Row', '')
    check = box(t.add('CheckBox', 'ObjectiveCheck', 'Row', VIS.HIT_TEST_INVISIBLE), pad=(0, 4, 10, 0), v='top')
    style = check.get_editor_property('widget_style').copy()
    for state in ('unchecked_image', 'unchecked_hovered_image', 'unchecked_pressed_image'):
        style.set_editor_property(state, tex('T_HUD_DiamondHollow', (12, 12)))
    for state in ('checked_image', 'checked_hovered_image', 'checked_pressed_image'):
        style.set_editor_property(state, tex('T_HUD_Diamond', (12, 12)))
    check.set_editor_property('widget_style', style)
    box(label(t, 'ObjectiveLabel', 'Row', '목표', 15.5, False, ('E7E2D9', 1.0), shadow=False, wrap=True), fill=1.0)
    box(label(t, 'ObjectiveCount', 'Row', '0/1', 14, True, (GOLD_HI, 1.0), shadow=False), pad=(8, 0, 0, 0))
    t.finish()

    # reward slot (Docs/HUD_Selection_Proposal_20261015, offer A): a picture in a dark tile --- experience crystal, gold coins,
    # or the item's own icon (the C++ sets RewardItem's brush) --- the amount in a badge, the name under it
    t = wbp('WBP_HUD_QuestRewardRow', '/Script/UMG.UserWidget')
    cell = t.add('SizeBox', 'Cell', '', VIS.SELF_HIT_TEST_INVISIBLE)
    cell.set_width_override(100)
    col = vbox(t, 'CellColumn', 'Cell')
    tile = box(sized(t, 'Tile', 'CellColumn', 80, 80), h='center')
    overlay(t, 'TileStack', 'Tile')
    box(image(t, 'TileBack', 'TileStack', rbox(('060C14', 1.0), 14, (IVORY, 0.18), 1.0)))
    box(image(t, 'TileGlow', 'TileStack', tex('T_HUD_Glow', (70, 70), (IVORY, 0.22))), h='center', v='center')
    sw = box(t.add('WidgetSwitcher', 'RewardIconSwitcher', 'TileStack', VIS.HIT_TEST_INVISIBLE), pad=(0, 0, 0, 4), h='center', v='center')
    image(t, 'RewardExp', 'RewardIconSwitcher', reward_icon('T_Reward_Exp', 72))
    image(t, 'RewardGold', 'RewardIconSwitcher', reward_icon('T_Reward_Gold', 72))
    image(t, 'RewardItem', 'RewardIconSwitcher', reward_icon('T_Reward_Gold', 72))      # the C++ puts the item's icon here
    badge = box(t.add('Border', 'AmountBadge', 'TileStack', VIS.HIT_TEST_INVISIBLE), pad=(0, 0, 4, 4), h='right', v='bottom')
    badge.set_brush(rbox(('0E1A28', 0.96), 8, (IVORY, 0.28), 1.0))
    badge.set_padding(unreal.Margin(6, 0, 6, 1))
    text(t, 'RewardAmount', 'AmountBadge', '120', 13, True, (IVORY, 1.0))
    name = box(label(t, 'RewardLabel', 'CellColumn', '경험치', 14, True, ('E7E2D9', 1.0), shadow=False), pad=(0, 7, 0, 0), h='center')
    name.set_editor_property('justification', unreal.TextJustify.CENTER)
    t.finish()

    # item reward tooltip (the C++ fills TipName / TipKind / TipDesc and hangs it on the item's slot)
    t = wbp('WBP_HUD_RewardTip', '/Script/UMG.UserWidget')
    size = t.add('SizeBox', 'TipSize', '', VIS.HIT_TEST_INVISIBLE)
    size.set_width_override(300)
    tip = box(t.add('Border', 'TipCard', 'TipSize', VIS.HIT_TEST_INVISIBLE))
    tip.set_brush(rbox(('16293D', 0.98), 12, (GOLD, 0.6), 1.0))
    tip.set_padding(unreal.Margin(16, 12, 16, 14))
    vbox(t, 'TipBody', 'TipCard')
    box(label(t, 'TipName', 'TipBody', '마을 약도', 16.5, True, shadow=False))
    kind = box(t.add('Border', 'TipKindChip', 'TipBody', VIS.HIT_TEST_INVISIBLE), pad=(0, 6, 0, 0), h='left')
    kind.set_brush(rbox(('D6BE8C', 0.16), 10, (GOLD, 0.75), 1.0))
    kind.set_padding(unreal.Margin(9, 1, 9, 2))
    text(t, 'TipKind', 'TipKindChip', '중요 물품', 12, True, (GOLD_HI, 1.0))
    box(label(t, 'TipDesc', 'TipBody', '리베르그로브 전역이 손그림으로 그려진 약도.', 13.5, False, (MUTED, 1.0), shadow=False, wrap=True), pad=(0, 8, 0, 0))
    t.finish()


# where the quest banner sparks fly to (x, y from the banner centre); the first four are used for accept / ready
SPARKS = [(-250, -30), (240, -40), (-180, 58), (200, 52), (-90, -92), (110, -88), (-300, 14), (290, 20)]


def build_quest_fx(t):
    """Quest accepted / ready / completed banner (C++ FNUQuestFx drives it by these names). Upper third of the screen,
    over the character's head; the same gold rules and lettering as the area banner, plus rays / ring / sparks for
    the completion."""
    # 0.32: under the area entry banner (top 178) so the two never share lines; the C++ also waits for that banner to finish
    root = canvas(overlay(t, 'QuestFxRoot', 'Root', VIS.HIT_TEST_INVISIBLE), (0.5, 0.32, 0.5, 0.32), (0, 0, 0, 0), (0.5, 0.5), auto=True, z=20)
    box(image(t, 'QuestFxGlow', 'QuestFxRoot', tex('T_HUD_QuestGlow', (1000, 340), ('FFFFFF', 1.0))), h='center', v='center')
    rays = box(image(t, 'QuestFxRays', 'QuestFxRoot', tex('T_HUD_Rays', (560, 560))), h='center', v='center')
    rays.set_render_opacity(0.0)
    ring = box(image(t, 'QuestFxRing', 'QuestFxRoot', tex('T_HUD_Ring', (320, 320))), h='center', v='center')
    ring.set_render_opacity(0.0)
    stack = box(vbox(t, 'QuestFxStack', 'QuestFxRoot'), h='center', v='center')
    cap = box(hbox(t, 'QuestFxCapRow', 'QuestFxStack'), h='center')
    box(image(t, 'QuestFxCapDiamondL', 'QuestFxCapRow', tex('T_HUD_Diamond', (8, 8), ('FFFFFF', 1.0))), pad=(0, 0, 12, 0), v='center')
    box(label(t, 'QuestFxCap', 'QuestFxCapRow', '새 의뢰', 18, True, (GOLD_HI, 1.0), 300), v='center')
    box(image(t, 'QuestFxCapDiamondR', 'QuestFxCapRow', tex('T_HUD_Diamond', (8, 8), ('FFFFFF', 1.0))), pad=(12, 0, 0, 0), v='center')
    row = box(hbox(t, 'QuestFxTitleRow', 'QuestFxStack'), pad=(0, 6, 0, 0), h='center')
    box(image(t, 'QuestFxRuleL', 'QuestFxTitleRow', tex('T_HUD_RuleL', (130, 2))), v='center')
    title = box(label(t, 'QuestFxTitle', 'QuestFxTitleRow', '뿌리 사이로', 40, True, spacing=30), pad=(22, 0, 22, 0), v='center')
    title.set_shadow_offset(unreal.Vector2D(0, 2.5))
    title.set_shadow_color_and_opacity(lin('000000', 0.8))
    box(image(t, 'QuestFxRuleR', 'QuestFxTitleRow', tex('T_HUD_RuleR', (130, 2))), v='center')
    box(label(t, 'QuestFxSub', 'QuestFxStack', '리베르그로브 아랫마을에 도착하기', 17, False, ('E3E7EA', 1.0)), pad=(0, 6, 0, 0), h='center')
    sparks = box(t.add('CanvasPanel', 'QuestFxSparks', 'QuestFxRoot', VIS.HIT_TEST_INVISIBLE), h='center', v='center')
    for index, (x, y) in enumerate(SPARKS):
        slot = canvas(sized(t, 'QuestFxSpark%d' % index, 'QuestFxSparks', 30, 30), (0.5, 0.5, 0.5, 0.5), (x, y, 30, 30), (0.5, 0.5))
        img = box(image(t, 'QuestFxSpark%dImg' % index, 'QuestFxSpark%d' % index, tex('T_HUD_Sparkle', (30, 30))))
        img.set_render_opacity(0.0)
    root.set_visibility(VIS.COLLAPSED)


def build_quest():
    t = wbp('WBP_HUD_Quest', 'NUQuestWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)

    # tracker: text on a soft scrim along the right edge, under the minimap. 10/15 the user preferred this to the glass card
    # (the card drew the eye too much) --- rolled back to the first version with a darker scrim (T_HUD_ScrimR, make_hud_kit.py)
    panel = canvas(t.add('Border', 'TrackerPanel', 'Root', VIS.SELF_HIT_TEST_INVISIBLE), (1, 0, 1, 0), (-EDGE_X, 340, 0, 0), (1, 0), auto=True)
    panel.set_brush(tex('T_HUD_ScrimR', (256, 64)))
    panel.set_padding(unreal.Margin(40, 12, 16, 16))
    sized(t, 'TrackerWidth', 'TrackerPanel', 330, None)
    vbox(t, 'TrackerStack', 'TrackerWidth')
    head = box(hbox(t, 'TrackerHeader', 'TrackerStack'))
    box(outlined(label(t, 'TrackerTitleText', 'TrackerHeader', '퀘스트', 13.5, True, (GOLD, 1.0), 180)), v='center', fill=1.0)
    collapse = box(flat_button(t, 'TrackerCollapseButton', 'TrackerHeader', nodraw(), rbox(('F1EDE6', 0.10), 6)), v='center')
    box(outlined(label(t, 'TrackerCollapseGlyph', 'TrackerCollapseButton', '▾', 14, True, (IVORY, 0.85), shadow=False)), h='center', v='center')
    close = box(flat_button(t, 'TrackerCloseButton', 'TrackerHeader', nodraw(), rbox(('F1EDE6', 0.10), 6)), pad=(4, 0, 0, 0), v='center')
    box(outlined(label(t, 'TrackerCloseGlyph', 'TrackerCloseButton', '×', 15, True, (IVORY, 0.85), shadow=False)), h='center', v='center')
    content = box(t.add('Border', 'TrackerContent', 'TrackerStack', VIS.SELF_HIT_TEST_INVISIBLE), pad=(0, 8, 0, 0))
    content.set_brush(nodraw())
    t.add('VerticalBox', 'TrackerEntryBox', 'TrackerContent', VIS.SELF_HIT_TEST_INVISIBLE)

    normal, hovered, pressed = pill_button_brushes(False)
    reopen = canvas(button(t, 'TrackerReopenButton', 'Root', rbox(('0E1A28', 0.80), None, GLASS_LINE, 1.0), rbox(('16293D', 0.92), None, (GOLD, 0.6), 1.0)),
                    (1, 0, 1, 0), (-EDGE_X, 340, 0, 0), (1, 0), auto=True)
    rr = hbox(t, 'ReopenRow', 'TrackerReopenButton')
    box(image(t, 'ReopenDiamond', 'ReopenRow', tex('T_HUD_Diamond', (8, 8))), pad=(14, 0, 8, 0), v='center')
    box(label(t, 'ReopenText', 'ReopenRow', '퀘스트', 14, True, shadow=False), pad=(0, 7, 14, 8), v='center')
    reopen.set_visibility(VIS.COLLAPSED)

    build_quest_fx(t)

    # offer: a glass card in the middle, like the confirm dialogs
    # offer (Docs/HUD_Selection_Proposal_20261015, offer A): right of the conversation, so the NPC and the player stay
    # visible; the field HUD folds while it is up (bHideFieldHUDDuringOffer); the giver's face and name on top
    offer = canvas(t.add('Border', 'OfferPanel', 'Root'), (1, 0.5, 1, 0.5), (-150, -40, 0, 0), (1, 0.5), auto=True)
    offer.set_brush(nodraw()); offer.set_padding(unreal.Margin(0, 0, 0, 0))
    sized(t, 'OfferWidth', 'OfferPanel', 640, None)
    card = glass(t, 'OfferCard', 'OfferWidth', 22, strength=16.0, fill=('0E1A28', 0.88))
    body = box(vbox(t, 'OfferBody', card), pad=(32, 26, 32, 26))
    head = box(hbox(t, 'QO_HeadRow', 'OfferBody'))
    medal = box(sized(t, 'QO_GiverMedal', 'QO_HeadRow', 64, 64), pad=(0, 0, 16, 0), v='center')
    overlay(t, 'QO_GiverStack', 'QO_GiverMedal')
    box(image(t, 'QO_GiverBack', 'QO_GiverStack', rbox(('16293D', 1.0), 32)))
    box(image(t, 'QO_GiverPortrait', 'QO_GiverStack', flat(IVORY, 1.0, (58, 58))), h='center', v='center')
    box(image(t, 'QO_GiverMask', 'QO_GiverStack', tex('T_HUD_PortraitMask72', (64, 64), ('0E1A28', 1.0))))
    box(image(t, 'QO_GiverRing', 'QO_GiverStack', rbox(('000000', 0.0), 32, (GOLD, 0.9), 2.0)))
    medal.set_visibility(VIS.COLLAPSED)
    names = box(vbox(t, 'QO_HeadNames', 'QO_HeadRow'), v='center', fill=1.0)
    box(label(t, 'OfferCap', 'QO_HeadNames', '새 의뢰', 13.5, True, (GOLD_HI, 1.0), 160, shadow=False))
    box(label(t, 'OfferTitleText', 'QO_HeadNames', '뿌리 사이로', 27, True, shadow=False), pad=(0, 6, 0, 0))
    loc = box(hbox(t, 'QO_LocationRow', 'OfferBody'), pad=(0, 6, 0, 0))
    box(image(t, 'QO_LocDiamond', 'QO_LocationRow', tex('T_HUD_Diamond', (8, 8))), pad=(0, 0, 8, 0), v='center')
    box(label(t, 'QO_LocationText', 'QO_LocationRow', '리베르그로브', 14, False, (MUTED, 1.0), shadow=False), v='center')
    box(image(t, 'OfferRuleTop', 'OfferBody', flat(IVORY, 0.10, (32, 1))), pad=(0, 16, 0, 14))
    scroll_size = box(sized(t, 'OfferScrollSize', 'OfferBody'))
    scroll_size.set_max_desired_height(360)
    scroll = t.add('ScrollBox', 'OfferBodyScroll', 'OfferScrollSize')
    scroll.set_editor_property('scrollbar_thickness', unreal.Vector2D(4, 4))
    vbox(t, 'OfferScrollBody', 'OfferBodyScroll')
    box(label(t, 'OfferSummaryText', 'OfferScrollBody', '의뢰 내용', 16, False, ('E7E2D9', 1.0), shadow=False, wrap=True))
    box(label(t, 'QO_ObjHeading', 'OfferScrollBody', '목표', 13, True, (GOLD, 1.0), 160, shadow=False), pad=(0, 18, 0, 0))
    box(vbox(t, 'QO_ObjectiveRows', 'OfferScrollBody'), pad=(0, 8, 0, 0))
    box(label(t, 'OfferObjectiveText', 'OfferScrollBody', '목표', 15, False, ('E7E2D9', 1.0), shadow=False, wrap=True), pad=(0, 8, 0, 0))
    box(label(t, 'QO_RewardHeading', 'OfferScrollBody', '보상', 13, True, (GOLD, 1.0), 160, shadow=False), pad=(0, 18, 0, 0))
    wrap = box(t.add('WrapBox', 'QO_RewardRows', 'OfferScrollBody', VIS.SELF_HIT_TEST_INVISIBLE), pad=(0, 10, 0, 0))
    wrap.set_editor_property('inner_slot_padding', unreal.Vector2D(6, 8))
    box(label(t, 'OfferRewardText', 'OfferScrollBody', '보상', 15, False, ('E7E2D9', 1.0), shadow=False, wrap=True), pad=(0, 8, 0, 0))
    box(image(t, 'OfferRuleBottom', 'OfferBody', flat(IVORY, 0.10, (32, 1))), pad=(0, 18, 0, 18))
    buttons = box(hbox(t, 'OfferButtons', 'OfferBody'))
    for name, gold, caption, pad in (('DeclineButton', False, '거절', (0, 0, 6, 0)), ('AcceptButton', True, '수락', (6, 0, 0, 0))):
        cell = box(sized(t, name + 'Size', 'OfferButtons', None, 56), pad=pad, fill=1.0)
        n, h, p = pill_button_brushes(gold)
        box(button(t, name, name + 'Size', n, h, p))
        row = box(hbox(t, name + 'Row', name), h='center', v='center')
        row.set_visibility(VIS.HIT_TEST_INVISIBLE)
        box(label(t, name + 'Label', name + 'Row', caption, 19, True, (NAVY if gold else IVORY, 1.0), shadow=False), v='center')
        if gold:      # F accepts (the interact key); the chip says so
            chip = box(sized(t, 'AcceptKey', name + 'Row', 24, 24), pad=(12, 0, 0, 0), v='center')
            face = box(overlay(t, 'AcceptKeyFace', 'AcceptKey', VIS.HIT_TEST_INVISIBLE))
            box(image(t, 'AcceptKeyRim', 'AcceptKeyFace', rbox(('0E1A28', 1.0), 7)))
            box(image(t, 'AcceptKeyFill', 'AcceptKeyFace', rbox((IVORY, 1.0), 5.5)), pad=(2, 2, 2, 2))
            box(text(t, 'AcceptKeyText', 'AcceptKeyFace', 'F', 12.5, True, (NAVY, 1.0)), h='center', v='center')
    # row templates: the C++ makes rows of their classes; the placed copies stay hidden
    tmpl = box(t.add('WBP_HUD_QuestObjRow', 'QO_ObjectiveTemplate', 'OfferBody'))
    tmpl.set_visibility(VIS.COLLAPSED)
    tmpl = box(t.add('WBP_HUD_QuestRewardRow', 'QO_RewardTemplate', 'OfferBody'))
    tmpl.set_visibility(VIS.COLLAPSED)
    offer.set_visibility(VIS.COLLAPSED)
    t.finish()
    old = unreal.get_default_object(unreal.load_asset('/Game/Developers/zxcv1/Quest/UI/WBP_QuestWidget').generated_class())
    d = cdo(t)
    d.set_editor_property('tracker_entry_widget_class', unreal.load_class(None, D + 'WBP_HUD_QuestEntry.WBP_HUD_QuestEntry_C'))
    d.set_editor_property('tracker_entry_padding', unreal.Margin(0, 0, 0, 7))
    d.set_editor_property('reward_popup_class', old.get_editor_property('reward_popup_class'))   # the reward window is a separate screen, kept
    d.set_editor_property('reward_tooltip_class', unreal.load_class(None, D + 'WBP_HUD_RewardTip.WBP_HUD_RewardTip_C'))
    d.set_editor_property('hide_field_hud_during_offer', True)
    t.finish()


# ======================================================================= party
def build_party():
    t = wbp('WBP_HUD_PartyRow', 'NUPartyMemberRow')
    pad = t.add('Border', 'RowPad', '', VIS.SELF_HIT_TEST_INVISIBLE)
    pad.set_brush(nodraw()); pad.set_padding(unreal.Margin(0, 0, 0, 8))
    sized(t, 'RowSize', 'RowPad', 300, 46)
    overlay(t, 'RowBody', 'RowSize')
    content = glass(t, 'RowGlass', 'RowBody', 'pill', blur=False, fill=('0E1A28', 0.84), shadow='T_HUD_ShadowSoft', shadow_pad=24)
    # The rounded fill is the row's only hit surface: a right click reaches UNUPartyMemberRow (party context menu),
    # a left click bubbles up to WBP_HUD_PartyList (window drag). Every other layer is hit-test invisible.
    t.get('RowGlass_Fill').set_visibility(VIS.VISIBLE)
    body = box(t.add('CanvasPanel', 'RowCanvas', content, VIS.SELF_HIT_TEST_INVISIBLE))
    face = canvas(overlay(t, 'RowFace', 'RowCanvas', VIS.HIT_TEST_INVISIBLE), (0, 0, 0, 0), (6, 6, 34, 34))
    box(image(t, 'RowFaceBg', 'RowFace', rbox(('26394E', 1.0), None, (PARTY, 0.8), 1.5)))
    box(image(t, 'RowFaceIcon', 'RowFace', icon_brush('character', 22)), h='center', v='center')
    name_row = canvas(hbox(t, 'RowNameLine', 'RowCanvas'), (0, 0, 0, 0), (52, 5, 0, 0), auto=True)
    box(label(t, 'RowNameText', 'RowNameLine', '하늘', 15.5, True, (PARTY, 1.0), shadow=False), v='center')
    lead = box(image(t, 'RowLeaderMark', 'RowNameLine', tex('T_HUD_Diamond', (9, 9))), pad=(8, 0, 0, 0), v='center')
    canvas(bar(t, 'RowHealthBar', 'RowCanvas', 'T_HUD_BarHP', True, 5), (0, 0, 0, 0), (52, 30, 150, 5))
    t.finish()

    t = wbp('WBP_HUD_PartyList', 'NUPartyListWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(plain(t, 'Overlay', 'PartyFrame', 'Root', VIS.SELF_HIT_TEST_INVISIBLE), (0, 0, 0, 0), (EDGE_X, 152, 0, 0), auto=True)
    box(t.add('VerticalBox', 'PartyMemberBox', 'PartyFrame', VIS.SELF_HIT_TEST_INVISIBLE))
    t.finish()
    cdo(t).set_editor_property('member_row_class', unreal.load_class(None, D + 'WBP_HUD_PartyRow.WBP_HUD_PartyRow_C'))
    t.finish()

    t = wbp('WBP_HUD_PartyPrompt', 'NUPartyPromptWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(overlay(t, 'PromptRoot', 'Root'), (0, 0, 0, 0), (EDGE_X, 270, 0, 0), auto=True)
    sized(t, 'PromptSize', 'PromptRoot', 384, 144)
    content = glass(t, 'PromptGlass', 'PromptSize', 16)
    c = box(t.add('CanvasPanel', 'PromptCanvas', content, VIS.SELF_HIT_TEST_INVISIBLE))
    face = canvas(overlay(t, 'PromptFace', 'PromptCanvas', VIS.HIT_TEST_INVISIBLE), (0, 0, 0, 0), (18, 18, 48, 48))
    box(image(t, 'PromptFaceBg', 'PromptFace', rbox(('26394E', 1.0), None, (PARTY, 0.8), 1.5)))
    box(image(t, 'PromptFaceIcon', 'PromptFace', icon_brush('party', 30)), h='center', v='center')
    canvas(label(t, 'PromptCap', 'PromptCanvas', '파티', 12, True, (GOLD_HI, 1.0), 120, shadow=False), (0, 0, 0, 0), (80, 16, 0, 0), auto=True)
    msg = canvas(label(t, 'PromptMessageLine', 'PromptCanvas', '하늘님이 파티에 초대했습니다.', 16.5, True, shadow=False, wrap=True), (0, 0, 0, 0), (80, 36, 288, 44))
    for i, (name, gold, caption) in enumerate((('PromptAccept', True, '수락'), ('PromptDecline', False, '거절'))):
        n, h, p = pill_button_brushes(gold)
        b = canvas(button(t, name, 'PromptCanvas', n, h, p), (0, 0, 0, 0), (80 + i * 112, 82, 104, 38))
        box(label(t, name + 'Label', name, caption, 15, True, (NAVY if gold else IVORY, 1.0), shadow=False), h='center', v='center')
    canvas(bar(t, 'PromptTimeBar', 'PromptCanvas', 'T_HUD_BarGold', True, 3), (0, 0, 0, 0), (20, 132, 344, 3))
    t.finish()


# ======================================================================= chat
def build_chat_parts():
    t = wbp('WBP_HUD_ChatLine', 'NUChatLineWidget')
    hbox(t, 'Line', '')
    chip = box(t.add('Border', 'ChannelChip', 'Line', VIS.HIT_TEST_INVISIBLE), pad=(0, 2, 8, 0), v='top')
    chip.set_brush(rbox(('FFFFFF', 0.14), 5)); chip.set_padding(unreal.Margin(6, 1, 6, 2))
    text(t, 'ChannelText', 'ChannelChip', '일반', 11.5, True)
    box(label(t, 'SenderText', 'Line', '이름', 15.5, True), pad=(0, 0, 7, 0), v='top')
    box(label(t, 'BodyText', 'Line', '대화 내용', 15.5, False, ('ECE8E0', 1.0), wrap=True), v='top', fill=1.0)
    t.finish()

    t = wbp('WBP_HUD_ChatTab', 'NUChatTabButtonWidget')
    o = overlay(t, 'TabRoot', '')
    box(flat_button(t, 'TabButton', 'TabRoot', nodraw(), rbox(('F1EDE6', 0.08), 8)))
    box(label(t, 'LabelText', 'TabRoot', '전체', 14.5, True, (MUTED, 1.0), shadow=False), pad=(12, 6, 12, 9), h='center', v='center').set_visibility(VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'SelectedFrame', 'TabRoot', flat(GOLD, 1.0, (24, 2))), h='fill', v='bottom')
    t.finish()
    d = cdo(t)
    d.set_editor_property('selected_label_color', lin(IVORY))
    d.set_editor_property('idle_label_color', lin(MUTED))
    t.finish()

    t = wbp('WBP_HUD_ChatChannel', 'NUChatChannelButtonWidget')
    sized(t, 'RootSizeBox', '', 96, 34)
    btn = button(t, 'ChannelButton', 'RootSizeBox', rbox(('0C1622', 0.94), 8, GLASS_LINE, 1.0), rbox(('16293D', 0.96), 8, (GOLD, 0.6), 1.0))
    box(label(t, 'LabelText', 'ChannelButton', '일반', 13.5, True, shadow=False), h='center', v='center')
    t.finish()


def build_chat():
    t = wbp('WBP_HUD_Chat', 'NUChatWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(overlay(t, 'ChatRoot', 'Root'), (0, 1, 0, 1), (EDGE_X, -EDGE_Y, 0, 0), (0, 1), auto=True)
    sized(t, 'ChatWidth', 'ChatRoot', 600, None)
    stack = overlay(t, 'ChatStack', 'ChatWidth')
    panel = box(overlay(t, 'FocusOnlyPanel', 'ChatStack'))
    glass(t, 'ChatGlass', 'FocusOnlyPanel', 18)
    col = box(vbox(t, 'ChatColumn', 'ChatStack'), pad=(14, 10, 14, 12))

    tabs = box(hbox(t, 'FocusOnlyTabs', 'ChatColumn'))
    for name, glyph in (('ChatTabPrevButton', '‹'),):
        b = box(flat_button(t, name, 'FocusOnlyTabs', nodraw(), rbox(('F1EDE6', 0.08), 8)), v='center')
        box(label(t, name + 'Glyph', name, glyph, 17, True, (MUTED, 1.0), shadow=False), pad=(8, 2, 8, 4), h='center', v='center')
    box(plain(t, 'HorizontalBox', 'ChatTabBox', 'FocusOnlyTabs', VIS.SELF_HIT_TEST_INVISIBLE), v='center', fill=1.0)
    for name, glyph in (('ChatTabNextButton', '›'), ('ChatAddTabButton', '+')):
        b = box(flat_button(t, name, 'FocusOnlyTabs', nodraw(), rbox(('F1EDE6', 0.08), 8)), v='center')
        box(label(t, name + 'Glyph', name, glyph, 17, True, (GOLD if glyph == '+' else MUTED, 1.0), shadow=False), pad=(8, 2, 8, 4), h='center', v='center')
    box(image(t, 'FocusOnlyRule', 'ChatColumn', flat(IVORY, 0.12, (32, 1))), pad=(0, 4, 0, 8))

    log_size = box(sized(t, 'ChatLogSize', 'ChatColumn', None, 92))
    log = overlay(t, 'ChatLog', 'ChatLogSize')
    box(image(t, 'IdleOnlyScrim', 'ChatLog', tex('T_HUD_ScrimL', (256, 64))), pad=(-14, -8, 0, -6))
    scroll = box(plain(t, 'ScrollBox', 'ChatScrollBox', 'ChatLog'), pad=(0, 0, 0, 0))
    scroll.set_editor_property('scrollbar_thickness', unreal.Vector2D(4, 4))

    inp = box(t.add('Border', 'FocusOnlyInput', 'ChatColumn', VIS.SELF_HIT_TEST_INVISIBLE), pad=(0, 10, 0, 0))
    inp.set_brush(rbox(('04090F', 0.45), 12, (GOLD, 0.55), 1.0))
    inp.set_padding(unreal.Margin(8, 4, 8, 4))
    irow = hbox(t, 'InputRow', 'FocusOnlyInput')
    chan_size = box(sized(t, 'ChatSendChannelButtonSize', 'InputRow', None, 30), v='center')
    cb = button(t, 'ChatSendChannelButton', 'ChatSendChannelButtonSize', rbox(('78C8EB', 0.20), 7), rbox(('78C8EB', 0.30), 7))
    box(label(t, 'ChatSendChannelText', 'ChatSendChannelButton', '일반 ▾', 13, True, (PARTY, 1.0), shadow=False), pad=(9, 0, 9, 1), h='center', v='center')
    edit = box(t.add('EditableTextBox', 'ChatInputBox', 'InputRow'), pad=(10, 0, 8, 0), v='center', fill=1.0)
    try:
        style = edit.get_editor_property('widget_style').copy()
        for state in ('background_image_normal', 'background_image_hovered', 'background_image_focused', 'background_image_read_only'):
            style.set_editor_property(state, nodraw())
        text_style = style.get_editor_property('text_style')
        info = unreal.SlateFontInfo()
        info.font_object, info.typeface_font_name = hud_kit.ccg_kit.typeface(hud_kit.ccg_kit.FONT_SEMI)
        info.size = 15.5 * 0.75
        text_style.set_editor_property('font', info)
        text_style.set_editor_property('color_and_opacity', slate(IVORY))
        style.set_editor_property('text_style', text_style)
        style.set_editor_property('foreground_color', slate(IVORY))
        edit.set_editor_property('widget_style', style)
    except Exception as error:
        log('chat input style: %s' % error)
    edit.set_editor_property('hint_text', '대화 입력')
    cap = box(t.add('Border', 'EnterCap', 'InputRow', VIS.HIT_TEST_INVISIBLE), v='center')
    cap.set_brush(rbox(('F1EDE6', 0.09), 6, ('F1EDE6', 0.38), 1.0)); cap.set_padding(unreal.Margin(7, 2, 7, 3))
    text(t, 'EnterCapText', 'EnterCap', 'Enter', 12, True)

    hint = box(t.add('Border', 'IdleOnlyHint', 'ChatColumn', VIS.HIT_TEST_INVISIBLE), pad=(0, 10, 0, 0), h='left')
    hint.set_brush(rbox(('0E1A28', 0.80), None, GLASS_LINE, 1.0)); hint.set_padding(unreal.Margin(10, 7, 16, 8))
    hr = hbox(t, 'HintRow', 'IdleOnlyHint')
    box(image(t, 'HintIcon', 'HintRow', icon_brush('chat', 26)), pad=(0, 0, 9, 0), v='center')
    hcap = box(t.add('Border', 'HintCap', 'HintRow', VIS.HIT_TEST_INVISIBLE), v='center')
    hcap.set_brush(rbox(('F1EDE6', 0.09), 6, ('F1EDE6', 0.38), 1.0)); hcap.set_padding(unreal.Margin(7, 2, 7, 3))
    text(t, 'HintCapText', 'HintCap', 'Enter', 12, True)
    box(label(t, 'HintText', 'HintRow', '대화하기', 15, True, ('C8D0D6', 1.0), shadow=False), pad=(9, 0, 0, 0), v='center')

    menu = box(t.add('VerticalBox', 'ChatSendChannelMenu', 'ChatStack', VIS.SELF_HIT_TEST_INVISIBLE), pad=(14, 0, 0, 64), h='left', v='bottom')

    # custom tab window (opened by +, handled in C++)
    panel = box(t.add('Border', 'ChatCustomTabPanel', 'ChatStack'), h='center', v='center')
    panel.set_brush(rbox(('0C1622', 0.97), 16, (GOLD, 0.5), 1.0)); panel.set_padding(unreal.Margin(20, 16, 20, 16))
    cv = vbox(t, 'CustomBody', 'ChatCustomTabPanel')
    box(label(t, 'CustomTitle', 'CustomBody', '새 탭', 17, True, shadow=False))
    name = box(t.add('EditableTextBox', 'ChatCustomTabName', 'CustomBody'), pad=(0, 10, 0, 10))
    name.set_editor_property('hint_text', '탭 이름')
    for key, caption in (('Local', '지역'), ('World', '월드'), ('Party', '파티'), ('Whisper', '귓속말'), ('System', '알림')):
        r = box(hbox(t, 'CustomRow_' + key, 'CustomBody'), pad=(0, 2, 0, 2))
        box(t.add('CheckBox', 'ChatCustomCheck_' + key, 'CustomRow_' + key), v='center')
        box(label(t, 'CustomText_' + key, 'CustomRow_' + key, caption, 14.5, True, shadow=False), pad=(8, 0, 0, 0), v='center')
    br = box(hbox(t, 'CustomButtons', 'CustomBody'), pad=(0, 12, 0, 0))
    for nm, gold, caption, pad in (('ChatCustomTabCancel', False, '취소', (0, 0, 4, 0)), ('ChatCustomTabApply', True, '만들기', (4, 0, 0, 0))):
        cell = box(sized(t, nm + 'Size', 'CustomButtons', 110, 38), pad=pad)
        n, h, p = pill_button_brushes(gold)
        box(button(t, nm, nm + 'Size', n, h, p))
        box(label(t, nm + 'Label', nm, caption, 14.5, True, (NAVY if gold else IVORY, 1.0), shadow=False), h='center', v='center')
    panel.set_visibility(VIS.COLLAPSED)
    t.finish()
    old = unreal.get_default_object(unreal.load_asset('/Game/Developers/dongh/UI/Chat/WBP_Chat').generated_class())
    d = cdo(t)
    d.set_editor_property('tab_button_class', unreal.load_class(None, D + 'WBP_HUD_ChatTab.WBP_HUD_ChatTab_C'))
    d.set_editor_property('channel_button_class', unreal.load_class(None, D + 'WBP_HUD_ChatChannel.WBP_HUD_ChatChannel_C'))
    d.set_editor_property('line_widget_class', unreal.load_class(None, D + 'WBP_HUD_ChatLine.WBP_HUD_ChatLine_C'))
    d.set_editor_property('max_visible_tabs', old.get_editor_property('max_visible_tabs'))
    t.finish()


STEPS = [build_dock_button, build_player_status, build_map_marker, build_head_marker, build_map_view, build_minimap, build_field, build_location,
         build_notice, build_interact, build_nameplate, build_quest_entry, build_quest_rows, build_quest, build_party,
         build_chat_parts, build_chat]
only = os.environ.get('HUD_ONLY', '')
for step in STEPS:
    if only and step.__name__ not in only.split(','):
        continue
    step()
    log('built ' + step.__name__)
log('DONE')
