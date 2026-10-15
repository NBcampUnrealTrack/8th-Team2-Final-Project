# -*- coding: utf-8 -*-
"""NPC dialogue in the HUD's glass tone, subtitle style (Docs/HUD_Dialogue_Proposal_20261014, option C, user's pick).
Run headless through run.ps1 -Steps dialogue (rebuilt from nothing every time); run.ps1 -Steps apply points the game at it.

  WBP_HUD_Dialogue        child of UNUDialogueWidget (same C++ as the old zxcv1 WBP_DialogueWidget, so the logic stays)
  WBP_HUD_DialogueOption  child of UNUInteractionOptionEntryWidget (one choice row)

No box: the bottom of the screen darkens (T_HUD_DialogueScrim, clear at the top, ~.85 behind the lines --- the user asked the
transparency be kept readable), gold speaker name with a diamond and a fading rule, ivory lines with a strong shadow,
a gold chevron + ivory F key that bobs when the line is complete (the whole subtitle area is the next button).
Choices are glass rows on the right above the subtitle; quest rows carry the same diamond badges as the head marker
and the minimap instead of the "[진행 가능]" prefix, the chosen row has a gold bar and frame.
While a conversation is open the field HUD folds away (bHideFieldHUDWhileOpen).
Layout in 1080p units (shortest side 1080 = 1x).
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
NEW_KIT = ('T_HUD_DialogueScrim', 'T_HUD_OptionDot', 'T_HUD_OptionExit')

SCRIM_H = 340          # dark band at the bottom of the screen
COLUMN_W = 1080        # subtitle column, centred
TEXT_TOP = 118         # speaker name from the band's top (the band is still light there)
INDENT = 21            # lines start under the name, past the diamond
ROW_W = 420            # choice rows
CHOICE_RIGHT = 700     # choices' right edge, from the screen centre
CHOICE_BOTTOM = 262    # choices' bottom, from the screen bottom (a little into the band, above the name)


def wbp(name, parent):
    return widget_blueprint(D + name, '/Script/ProjectM.' + parent)


def import_new_kit():
    """Only the dialogue's own kit pictures (the rest of the kit is already imported and unchanged)."""
    tasks = []
    for name in NEW_KIT:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(KIT_SRC, name + '.png')
        task.destination_path = KIT; task.destination_name = name
        task.automated = True; task.replace_existing = True; task.save = False
        tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    for name in NEW_KIT:
        texture = unreal.load_asset(KIT + '/' + name)
        assert texture, name
        texture.set_editor_properties({
            'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI,
            'never_stream': True,
            'srgb': True,
            'filter': unreal.TextureFilter.TF_DEFAULT,
        })
        assert E.save_asset(KIT + '/' + name, False)
    log('dialogue kit: %d' % len(NEW_KIT))


def key_chip(tree, name, parent, label_text, size=28):
    """The ivory F key of the interaction cue, smaller."""
    cap = sized(tree, name, parent, size, size)
    face = box(overlay(tree, name + 'Face', name, VIS.HIT_TEST_INVISIBLE))
    box(image(tree, name + 'Rim', name + 'Face', rbox(('0E1A28', 1.0), 8)))
    box(image(tree, name + 'Fill', name + 'Face', rbox((IVORY, 1.0), 6.5)), pad=(2, 2, 2, 2))
    box(image(tree, name + 'Sheen', name + 'Face', rbox(('FFFFFF', 0.55), 5.5, None, 0.0, (12, 9))), pad=(3.5, 3, 3.5, 0), v='top')
    box(text(tree, name + 'Text', name + 'Face', label_text, size * 0.54, True, (NAVY, 1.0)), h='center', v='center')
    return cap


# ======================================================================= one choice row
def build_option():
    t = wbp('WBP_HUD_DialogueOption', 'NUInteractionOptionEntryWidget')
    root = t.add('SizeBox', 'RowSize', '')
    root.set_width_override(ROW_W)
    root.set_editor_property('min_desired_height', 48.0)
    overlay(t, 'Row', 'RowSize')
    normal = rbox(('0E1A28', 0.84), 12, (IVORY, 0.12), 1.0)
    hovered = rbox(('132538', 0.92), 12, (IVORY, 0.22), 1.0)
    pressed = rbox(('0A1420', 0.95), 12, (GOLD, 0.45), 1.0)
    disabled = rbox(('0E1A28', 0.64), 12, (IVORY, 0.08), 1.0)
    btn = box(button(t, 'OptionButton', 'Row', normal, hovered, pressed, disabled, pad=(14, 9, 18, 10)))
    content = box(hbox(t, 'RowContent', 'OptionButton'), v='center')
    box(image(t, 'GroupIcon', 'RowContent', tex('T_HUD_Badge_Offer', (26, 26))), pad=(0, 0, 12, 0), v='center')
    box(label(t, 'OptionText', 'RowContent', '대화하기', 19, True, wrap=True), v='center', fill=1.0)
    # chosen row: gold frame + a gold bar at the left edge (on top of the button, clicks pass through)
    hi = box(overlay(t, 'FocusHighlight', 'Row', VIS.HIT_TEST_INVISIBLE))
    box(image(t, 'FocusFrame', 'FocusHighlight', rbox(('F0D79D', 0.06), 12, (GOLD, 0.70), 1.2)))
    box(image(t, 'FocusBar', 'FocusHighlight', rbox((GOLD_HI, 1.0), 2, None, 0.0, (4, 16))), pad=(0, 10, 0, 10), h='left')
    hi.set_visibility(VIS.COLLAPSED)
    t.finish()

    d = cdo(t)
    d.set_editor_property('normal_text_color', lin(IVORY))
    d.set_editor_property('focused_text_color', lin(GOLD_HI))
    d.set_editor_property('disabled_text_color', lin('7E8A94'))
    d.set_editor_property('show_category_prefix', False)
    G = unreal.NUInteractionOptionGroup
    icons = {
        G.QUEST_TURN_IN: 'T_HUD_Badge_TurnIn',
        G.QUEST_OFFER: 'T_HUD_Badge_Offer',
        G.QUEST_TARGET: 'T_HUD_Badge_Target',
        G.QUEST_IN_PROGRESS: 'T_HUD_Badge_InProgress',
    }
    d.set_editor_property('group_icons', {k: unreal.load_asset(KIT + '/' + v) for k, v in icons.items()})
    d.set_editor_property('default_group_icon', unreal.load_asset(KIT + '/T_HUD_OptionDot'))
    d.set_editor_property('system_group_icon', unreal.load_asset(KIT + '/T_HUD_OptionExit'))     # leave (hidden enum value)
    t.finish()


# ======================================================================= the dialogue
def build_dialogue():
    t = wbp('WBP_HUD_Dialogue', 'NUDialogueWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)

    # --- subtitle band
    band = canvas(t.add('Border', 'DialoguePanel', 'Root', VIS.SELF_HIT_TEST_INVISIBLE), (0, 1, 1, 1), (0, -SCRIM_H, 0, SCRIM_H))
    band.set_brush(tex('T_HUD_DialogueScrim', (8, 256), nine=False))
    band.set_padding(unreal.Margin(0, 0, 0, 0))
    band.set_horizontal_alignment(HA['center']); band.set_vertical_alignment(VA['fill'])
    sized(t, 'SubtitleSize', 'DialoguePanel', COLUMN_W, None)
    overlay(t, 'SubtitleStack', 'SubtitleSize')

    # the whole column is the "next" button; its only content is the chevron + key at the bottom right
    nxt = box(button(t, 'NextButton', 'SubtitleStack', nodraw(), nodraw(), nodraw()))
    ind = box(hbox(t, 'NextIndicator', 'NextButton'), pad=(0, 0, 0, 30), h='right', v='bottom')
    ind.set_visibility(VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'NextChevron', 'NextIndicator', tex('T_HUD_Chevron', (16, 11))), pad=(0, 0, 9, 0), v='center')
    box(key_chip(t, 'NextKey', 'NextIndicator', 'F'), v='center')

    col = box(vbox(t, 'TextColumn', 'SubtitleStack'), pad=(0, TEXT_TOP, 0, 0), v='top')
    col.set_visibility(VIS.HIT_TEST_INVISIBLE)
    box(vbox(t, 'SpeakerNameRow', 'TextColumn'))
    box(hbox(t, 'NameLine', 'SpeakerNameRow'))
    box(image(t, 'NameDiamond', 'NameLine', tex('T_HUD_Diamond', (11, 11))), pad=(0, 1, 10, 0), v='center')
    name = box(label(t, 'SpeakerNameText', 'NameLine', '엘라던', 26, True, (GOLD_HI, 1.0)), v='center')
    name.set_shadow_offset(unreal.Vector2D(0, 2))
    name.set_shadow_color_and_opacity(lin('000000', 0.80))
    box(image(t, 'NameRule', 'SpeakerNameRow', tex('T_HUD_RuleR', (170, 2), ('FFFFFF', 0.75))), pad=(INDENT, 7, 0, 0), h='left')
    line = box(label(t, 'DialogueText', 'TextColumn',
                     '그대가 하는 일을 모두 숲의 정령들이 속삭여주었다. 그대의 손에 여섯 정령의 기운이 고였으니, 외지인이여.', 25, True, wrap=True),
               pad=(INDENT, 14, 64, 0))
    line.set_editor_property('line_height_percentage', 1.18)
    line.set_shadow_offset(unreal.Vector2D(0, 2))
    line.set_shadow_color_and_opacity(lin('000000', 0.85))

    # --- choices (right of centre, above the band)
    panel = canvas(t.add('Border', 'InteractionOptionPanel', 'Root', VIS.SELF_HIT_TEST_INVISIBLE),
                   (0.5, 1, 0.5, 1), (CHOICE_RIGHT, -CHOICE_BOTTOM, 0, 0), (1, 1), auto=True)
    panel.set_brush(nodraw())
    panel.set_padding(unreal.Margin(0, 0, 0, 0))
    holder = sized(t, 'OptionScrollSize', 'InteractionOptionPanel', ROW_W, None)
    holder.set_max_desired_height(6 * 56)
    scroll = t.add('ScrollBox', 'InteractionOptionScroll', 'OptionScrollSize')
    scroll.set_editor_property('scrollbar_thickness', unreal.Vector2D(3, 3))
    scroll.set_editor_property('scroll_bar_visibility', VIS.COLLAPSED)      # wheel and focus still scroll; no bar in the scene
    vbox(t, 'OptionList', 'InteractionOptionScroll')
    t.finish()

    old = unreal.get_default_object(unreal.load_asset('/Game/Developers/zxcv1/Dialogue/UI/WBP_DialogueWidget').generated_class())
    d = cdo(t)
    d.set_editor_property('option_entry_widget_class', unreal.load_class(None, D + 'WBP_HUD_DialogueOption.WBP_HUD_DialogueOption_C'))
    d.set_editor_property('text_characters_per_second', old.get_editor_property('text_characters_per_second'))
    d.set_editor_property('hide_field_hud_while_open', True)
    d.set_editor_property('play_open_motion', True)
    d.set_editor_property('collapse_dialogue_panel_without_lines', True)
    t.finish()


import_new_kit()
build_option()
log('built build_option')
build_dialogue()
log('built build_dialogue')
log('DONE')
