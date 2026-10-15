# -*- coding: utf-8 -*-
"""Character creation screen, glass-sheet UI.

Headless:  UnrealEditor-Cmd.exe ProjectM.uproject -run=pythonscript -script=Tools/CharacterCreateGlass/apply_ui.py
Builds everything from scratch and can be run again (it empties and refills its own widget trees):
  /Game/Art/UI/CharacterCreate/Glass/Kit/T_CCG_*      textures rendered by make_ui_kit.py
  /Game/Art/UI/CharacterCreate/Glass/WBP_CCG_SegButton, WBP_CCG_RaceChip, WBP_CCG_Swatch   buttons for this screen (same C++ parent as the shared button)
  /Game/Art/UI/CharacterCreate/Glass/WBP_CharacterCreate_Glass             the screen (parent UNUCharacterCreateWidget)
and points both creation game modes at the new screen. The old WBP_CharacterCreate is not touched.

All sizes are 1080p units taken from the approved mock (Docs/CharacterCreate_Proposal_20261013/charcreate_ui_concepts.html).
Font sizes there are CSS px; Slate wants points, so px * 0.75.
"""
import importlib
import json
import os
import shutil
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/CharacterCreateGlass'))
import ccg_kit
importlib.reload(ccg_kit)       # an editor session keeps modules between runs
from ccg_kit import *           # noqa: F401,F403  (paths, colours, brush and tree helpers)


def build_seg_button():
    """Gender segment: icon + label on a transparent pill. The selected look is an overlay drawn by the screen."""
    t = widget_blueprint(SEG, '/Script/ProjectM.NU_CommonButtonWidget')
    button(t, 'Btn_Common', '', nodraw(), rbox(('FFFFFF', 0.06)), rbox(('FFFFFF', 0.11)), nodraw())
    row = box(t.add('HorizontalBox', 'Row', 'Btn_Common', VIS.HIT_TEST_INVISIBLE), h='center', v='center')
    icon = box(t.add('Image', 'Img_Icon', 'Row', VIS.HIT_TEST_INVISIBLE), pad=(0, 0, 8, 0), h='center', v='center')
    icon.set_brush(tex('T_CCG_Icon_Female', (18, 18)))
    icon.set_color_and_opacity(lin(IVORY, 0.72))
    label = text(t, 'Txt_Label', 'Row', '여성', 19, colour=(IVORY, 0.72))
    box(label, h='center', v='center')
    t.finish()


def build_race_chip():
    """Race chip: just the emblem. Ring, selected ring and lock badge are drawn by the screen around it."""
    t = widget_blueprint(CHIP, '/Script/ProjectM.NU_CommonButtonWidget')
    button(t, 'Btn_Common', '', nodraw(), rbox(('FFFFFF', 0.10)), rbox(('FFFFFF', 0.16)), nodraw())
    icon = box(t.add('Image', 'Img_Icon', 'Btn_Common', VIS.HIT_TEST_INVISIBLE))
    brush = unreal.SlateBrush(); brush.draw_as = unreal.SlateBrushDrawType.IMAGE
    brush.set_editor_property('resource_object', unreal.load_asset(EMBLEMS + 'Human'))   # preview: the C++ swaps the texture and keeps this tint
    brush.image_size = vec2(56, 56); brush.tint_color = slate('FFFFFF', 1.0)
    icon.set_brush(brush)
    t.finish()


def line(tree, name, parent, height, pad=(0, 0, 0, 0), h='fill'):
    """A box as tall as the mock's CSS line box. Text goes inside, centred: the stack then ignores the font's own (taller) line height."""
    return box(sized(tree, name, parent, height=height), pad=pad, h=h)


def build_swatch():
    """Hair-colour swatch: only the click area. The screen draws the colour, the rim and the selected ring around it."""
    t = widget_blueprint(SWATCH, '/Script/ProjectM.NU_CommonButtonWidget')
    button(t, 'Btn_Common', '', nodraw(), rbox(('FFFFFF', 0.16)), rbox(('FFFFFF', 0.26)), nodraw())
    t.finish()


def cap(tree, name, parent, value, pad):
    line(tree, name + 'Line', parent, 20, pad)
    return box(text(tree, name, name + 'Line', value, 15, True, (GOLD, 1.0), 160), h='left', v='center')


def option_row(t, prefix, label, parent, thumbs=3):
    """헤어 스타일 / 외모 / 피부색: label, then  < thumbs >  n / m.  With nothing to pick the C++ shows <prefix>Empty instead."""
    sized(t, prefix + 'Row', parent, height=60)
    box(t.get(prefix + 'Row'))
    line = t.add('HorizontalBox', prefix + 'Line', prefix + 'Row')
    box(text(t, prefix + 'Label', prefix + 'Line', label, 18), h='left', v='center', fill=1.0)

    options = box(t.add('HorizontalBox', prefix + 'Options', prefix + 'Line', VIS.SELF_HIT_TEST_INVISIBLE), h='right', v='center')
    for side, icon_name in (('Prev', 'T_CCG_Icon_Left'), ('Next', 'T_CCG_Icon_Right')):
        if side == 'Next':
            for index in range(thumbs):
                name = '%sThumb%d' % (prefix, index)
                wrap = box(sized(t, name + 'Cell', prefix + 'Options', 48, 48), pad=(10, 0, 0, 0), v='center')   # "<thumb>Cell": the C++ folds it with an unused thumb
                wrap.set_visibility(VIS.SELF_HIT_TEST_INVISIBLE)
                t.add('Overlay', name + 'Stack', wrap.get_name(), VIS.SELF_HIT_TEST_INVISIBLE)
                box(image(t, name + 'Idle', name + 'Stack', tex('T_CCG_ThumbRingIdle', (50, 50))), pad=(-1, -1, -1, -1))
                thumb = box(t.add('Border', name, name + 'Stack', VIS.HIT_TEST_INVISIBLE))      # the C++ puts the picture in here and clips it to this box
                thumb.set_brush(tex('T_CCG_ThumbFill', (48, 48)))
                thumb.set_padding(unreal.Margin(0, 0, 0, 0))
                ring = box(image(t, name + 'Selected', name + 'Stack', tex('T_CCG_ThumbRing', (62, 62))), pad=(-7, -7, -7, -7))
                ring.set_visibility(VIS.HIT_TEST_INVISIBLE if index == 0 else VIS.COLLAPSED)
        circle = button(t, prefix + side + 'Button', prefix + 'Options', ghost(0.05, 0.24), ghost(0.12, 0.34), ghost(0.18, 0.4), ghost(0.02, 0.1))
        box(circle, pad=(10 if side == 'Next' else 0, 0, 0, 0), v='center')
        frame = box(sized(t, prefix + side + 'Box', circle.get_name(), 38, 38))
        glyph = image(t, prefix + side + 'Icon', frame.get_name(), tex(icon_name, (14, 14), tint=(IVORY, 1.0)))
        box(glyph, h='center', v='center')
    count = box(sized(t, prefix + 'IndexBox', prefix + 'Options', width=40), pad=(10, 0, 0, 0), v='center')
    index_text = text(t, prefix + 'IndexText', count.get_name(), '1 / 2', 15, colour=(IVORY, 0.6))
    index_text.set_editor_property('justification', unreal.TextJustify.RIGHT)
    box(index_text, h='right', v='center')

    empty = box(t.add('Overlay', prefix + 'Empty', prefix + 'Line', VIS.COLLAPSED), h='right', v='center')
    box(image(t, prefix + 'EmptyPill', prefix + 'Empty', tex('T_CCG_SoonPill', (71, 27))), h='center', v='center')
    box(text(t, prefix + 'EmptyText', prefix + 'Empty', '준비 중', 14, colour=(IVORY, 0.6)), h='center', v='center')


def build_screen():
    t = widget_blueprint(MAIN, '/Script/ProjectM.NUCharacterCreateWidget')
    old = unreal.find_object(unreal.load_asset('/Game/Developers/dbals/UI/WBP_CharacterCreate'), 'WidgetTree')

    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    # over the 3D stage (the camera frames it the same way): lantern breath and embers, and the flash when the look changes
    fx = stage_frame(t, 'StageFx', 'Root')
    ambience(t, 'Ambience', fx.get_name())
    # twinkles round the head and shoulders when the look changes (the C++ pops them one after another); on the 5 px grid
    feet_x = STAGE_SHIFT + 705
    for index, (dx, y, small) in enumerate(((-95, 470, True), (85, 445, False), (115, 545, True), (-130, 555, False),
                                           (-75, 690, True), (90, 705, False), (10, 410, True))):
        size = 25 if small else 35
        sparkle = canvas(image(t, 'LookSparkle%d' % index, fx.get_name(), tex('T_CCG_SparkleSmall' if small else 'T_CCG_Sparkle', (size, size))),
                         (0, 0, 0, 0), (feet_x + dx - size // 2 // 5 * 5, y, size, size))
        sparkle.set_render_opacity(0.0)
    canvas(image(t, 'TopFade', 'Root', tex('T_CCG_FadeDown', (8, 128), tint=('060A12', 0.6))), (0, 0, 1, 0), (0, 0, 0, 190))
    canvas(image(t, 'BottomFade', 'Root', tex('T_CCG_FadeUp', (8, 128), tint=('060A12', 0.5))), (0, 1, 1, 1), (0, 0, 0, 140), align=(0, 1))

    # ----- back to character select (the cancel button) -----
    canvas(t.add('Overlay', 'CancelPanel', 'Root', VIS.SELF_HIT_TEST_INVISIBLE), (0, 0, 0, 0), (48, 40, 0, 0), auto=True, z=2)
    pill_blur = box(t.add('BackgroundBlur', 'CancelBlur', 'CancelPanel', VIS.HIT_TEST_INVISIBLE), pad=(8, 0, 8, 0))   # square blur kept inside the pill's round ends
    pill_blur.set_editor_property('blur_strength', 18.0)
    pill_blur.set_editor_property('apply_alpha_to_blur', True)
    back = button(t, 'CancelButton', 'CancelPanel', rbox(('0E141E', 0.55), None, ('FFFFFF', 0.16), 1.0), rbox(('18202C', 0.72), None, ('FFFFFF', 0.26), 1.0),
                  rbox(('0E141E', 0.86), None, ('FFFFFF', 0.3), 1.0), pad=(15, 0, 19, 0))
    box(back)
    box(sized(t, 'CancelBox', 'CancelButton', height=54))
    t.add('HorizontalBox', 'CancelRow', 'CancelBox', VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'CancelIcon', 'CancelRow', tex('T_CCG_Icon_Left', (18, 18), tint=(IVORY, 1.0))), h='center', v='center')
    box(text(t, 'CancelLabel', 'CancelRow', '캐릭터 선택', 18), pad=(8, 0, 0, 0), v='center')
    key = t.add('Border', 'CancelKey', 'CancelRow', VIS.HIT_TEST_INVISIBLE)
    key.set_brush(rbox(('E8E4DC', 1.0), 6))
    key.set_padding(unreal.Margin(8, 4, 8, 4))
    box(key, pad=(16, 0, 0, 0), h='center', v='center')
    text(t, 'CancelKeyText', 'CancelKey', 'ESC', 13, True, (NAVY, 1.0))

    # ----- the sheet -----
    form = t.add('Overlay', 'FormPanel', 'Root', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(form, (1, 0, 1, 1), (-40, 40, 492, 40), align=(1, 0), z=1)
    box(image(t, 'SheetShadow', 'FormPanel', tex('T_CCG_SheetShadow', (520, 520), (190 / 520, 160 / 520, 190 / 520, 220 / 520))), pad=(-160, -130, -160, -190))
    blur = box(t.add('BackgroundBlur', 'SheetBlur', 'FormPanel', VIS.HIT_TEST_INVISIBLE))
    blur.set_editor_property('blur_strength', 26.0)
    blur.set_editor_property('apply_alpha_to_blur', True)     # fades in with the sheet (at rest the alpha is 1)
    # No corner radius: with one, UE 5.8 blends the blurred copy by the back buffer's alpha, which is 0 over the 3D scene,
    # so nothing is drawn at all (measured). Square corners leave a blurred sliver of at most 12 px outside the round glass corners.
    box(image(t, 'SheetGlass', 'FormPanel', tex('T_CCG_Sheet', (128, 256), (0.25, 0.125, 0.25, 0.125)), VIS.VISIBLE))   # Visible: swallows clicks meant for the stage
    body = box(t.add('VerticalBox', 'SheetBody', 'FormPanel', VIS.SELF_HIT_TEST_INVISIBLE), pad=(33, 33, 33, 29))

    # header
    top = box(sized(t, 'TopRow', 'SheetBody', height=40), pad=(0, -1, 0, -1))
    t.add('HorizontalBox', 'TopLine', 'TopRow', VIS.SELF_HIT_TEST_INVISIBLE)
    box(text(t, 'TitleCap', 'TopLine', '새 캐릭터', 15, True, (GOLD, 1.0), 160), pad=(0, 4, 0, 0), h='left', v='top', fill=1.0)
    random = button(t, 'RandomButton', 'TopLine', ghost(0.05), ghost(0.12, 0.34), ghost(0.18, 0.4), pad=(14, 0, 14, 0))
    box(random, h='right', v='center')
    box(sized(t, 'RandomBox', 'RandomButton', height=40))
    t.add('HorizontalBox', 'RandomRow', 'RandomBox', VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'RandomIcon', 'RandomRow', tex('T_CCG_Icon_Dice', (18, 18), tint=('DDE3E7', 1.0))), h='center', v='center')
    box(text(t, 'RandomLabel', 'RandomRow', '무작위', 15, colour=('DDE3E7', 1.0)), pad=(8, 0, 0, 0), v='center')

    # race chips
    box(t.add('HorizontalBox', 'RaceRow', 'SheetBody', VIS.SELF_HIT_TEST_INVISIBLE), pad=(0, 20, 0, 0), h='left')
    for index, emblem in enumerate(('Human', 'Elf', 'Dwarf', 'Beast')):
        cell = box(sized(t, 'RaceBox%d' % index, 'RaceRow', 56, 56), pad=(0, 0, 12, 0))
        t.add('Overlay', 'RaceCell%d' % index, cell.get_name(), VIS.SELF_HIT_TEST_INVISIBLE)
        box(image(t, 'RaceIdle%d' % index, 'RaceCell%d' % index, tex('T_CCG_RaceRingIdle', (72, 72))), pad=(-8, -8, -8, -8))
        chip = box(t.add('WBP_CCG_RaceChip', 'RaceButton%d' % index, 'RaceCell%d' % index))
        chip.set_editor_property('icon_size', unreal.Vector2D(56, 56))
        chip.set_editor_property('tag', index)
        preview = unreal.load_asset(EMBLEMS + ('Beastfolk' if emblem == 'Beast' else emblem))
        if preview:
            chip.set_editor_property('icon_texture', preview)     # designer preview only: the C++ fills it from the race table
        ring = box(image(t, 'RaceSelected%d' % index, 'RaceCell%d' % index, tex('T_CCG_RaceRing', (72, 72))), pad=(-8, -8, -8, -8))
        ring.set_visibility(VIS.HIT_TEST_INVISIBLE if index == 0 else VIS.COLLAPSED)
        lock = box(t.add('Overlay', 'RaceLock%d' % index, 'RaceCell%d' % index, VIS.COLLAPSED if index == 0 else VIS.HIT_TEST_INVISIBLE))
        box(image(t, 'RaceLockDim%d' % index, lock.get_name(), rbox(('0B111B', 0.62))))
        box(image(t, 'RaceLockBadge%d' % index, lock.get_name(), tex('T_CCG_LockBadge', (22, 22))), pad=(0, 0, -4, -4), h='right', v='bottom')
    line(t, 'RaceNameLine', 'SheetBody', 46, (0, 16, 0, 0))
    box(text(t, 'RaceNameText', 'RaceNameLine', '인간', 40, True), pad=(0, 0, 0, 7), h='left', v='center')
    line(t, 'RaceDescLine', 'SheetBody', 48, (0, 6, 0, 0))                    # two lines of 24
    desc = box(text(t, 'RaceDescText', 'RaceDescLine', '결핍이 낳은, 가장 다양한 얼굴들.\n마법과 기계를 융합해 살아남은 종족입니다.', 16, colour=(IVORY, 0.74), wrap=True), v='center')
    desc.set_editor_property('line_height_percentage', 24.0 / 22.0)
    emblem_slot = t.add('Border', 'RaceEmblem', 'SheetBody', VIS.COLLAPSED)     # required by the C++; the chips already show the emblem
    emblem_slot.set_brush(nodraw())
    box(image(t, 'Rule1', 'SheetBody', flat('FFFFFF', 0.10, (32, 1))), pad=(0, 20, 0, 20))

    # gender
    cap(t, 'GenderCap', 'SheetBody', '성별', (0, 0, 0, 10))
    box(sized(t, 'GenderBox', 'SheetBody', height=52))
    t.add('Overlay', 'GenderSeg', 'GenderBox', VIS.SELF_HIT_TEST_INVISIBLE)
    box(image(t, 'GenderTrack', 'GenderSeg', rbox(('FFFFFF', 0.07), None, ('FFFFFF', 0.14), 1.0)))
    box(t.add('HorizontalBox', 'GenderRow', 'GenderSeg', VIS.SELF_HIT_TEST_INVISIBLE), pad=(5, 5, 5, 5))
    for key_name, label, icon_name in (('Female', '여성', 'T_CCG_Icon_Female'), ('Male', '남성', 'T_CCG_Icon_Male')):
        cell = box(t.add('Overlay', 'Gender%sCell' % key_name, 'GenderRow', VIS.SELF_HIT_TEST_INVISIBLE), fill=1.0)
        seg = box(t.add('WBP_CCG_SegButton', 'Gender%sButton' % key_name, cell.get_name()))
        seg.set_editor_property('label_text', label)
        seg.set_editor_property('label_color', lin(IVORY, 0.72))
        seg.set_editor_property('icon_texture', unreal.load_asset(KIT + '/' + icon_name))
        seg.set_editor_property('icon_size', unreal.Vector2D(18, 18))
        mark = box(t.add('Overlay', 'Gender%sSelected' % key_name, cell.get_name(), VIS.HIT_TEST_INVISIBLE if key_name == 'Male' else VIS.COLLAPSED))
        box(image(t, 'Gender%sPill' % key_name, mark.get_name(), rbox((IVORY, 1.0))))
        row = box(t.add('HorizontalBox', 'Gender%sOn' % key_name, mark.get_name(), VIS.HIT_TEST_INVISIBLE), h='center', v='center')
        box(image(t, 'Gender%sOnIcon' % key_name, row.get_name(), tex(icon_name, (18, 18), tint=(NAVY, 1.0))), pad=(0, 0, 8, 0), h='center', v='center')
        box(text(t, 'Gender%sOnText' % key_name, row.get_name(), label, 19, True, (NAVY, 1.0)), v='center')

    # appearance
    cap(t, 'LookCap', 'SheetBody', '외형', (0, 22, 0, 10))
    box(t.add('VerticalBox', 'LookRows', 'SheetBody', VIS.SELF_HIT_TEST_INVISIBLE))
    option_row(t, 'Hair', '헤어 스타일', 'LookRows')
    # hair colour: the C++ folds the whole row (rule included) for a body that has no hair part to colour
    box(t.add('VerticalBox', 'HairColorRow', 'LookRows', VIS.SELF_HIT_TEST_INVISIBLE))
    box(image(t, 'RuleColor', 'HairColorRow', flat('FFFFFF', 0.08, (32, 1))))
    box(sized(t, 'HairColorBox', 'HairColorRow', height=60))
    t.add('HorizontalBox', 'HairColorLine', 'HairColorBox', VIS.SELF_HIT_TEST_INVISIBLE)
    box(text(t, 'HairColorLabel', 'HairColorLine', '머리 색', 18), h='left', v='center', fill=1.0)
    box(t.add('HorizontalBox', 'HairColorOptions', 'HairColorLine', VIS.SELF_HIT_TEST_INVISIBLE), h='right', v='center')
    for index in range(HAIR_COLOURS):
        cell = box(sized(t, 'HairColorCell%d' % index, 'HairColorOptions', 22, 22), pad=(8 if index else 0, 0, 0, 0), v='center')
        cell.set_visibility(VIS.SELF_HIT_TEST_INVISIBLE)
        stack = t.add('Overlay', 'HairColorStack%d' % index, cell.get_name(), VIS.SELF_HIT_TEST_INVISIBLE)
        box(image(t, 'HairColorFill%d' % index, stack.get_name(), rbox(('FFFFFF', 1.0))))            # the C++ sets its colour from the palette
        box(image(t, 'HairColorRim%d' % index, stack.get_name(), tex('T_CCG_SwatchRim', (22, 22))))
        ring = box(image(t, 'HairColorSelected%d' % index, stack.get_name(), tex('T_CCG_SwatchRing', (34, 34))), pad=(-6, -6, -6, -6))
        ring.set_visibility(VIS.HIT_TEST_INVISIBLE if index == 0 else VIS.COLLAPSED)
        swatch = box(t.add('WBP_CCG_Swatch', 'HairColorButton%d' % index, stack.get_name()), pad=(-4, -4, -4, -4))   # click area 30 px
        swatch.set_editor_property('tag', index)
    box(image(t, 'Rule2', 'LookRows', flat('FFFFFF', 0.08, (32, 1))))
    option_row(t, 'Face', '외모', 'LookRows')
    box(image(t, 'Rule3', 'LookRows', flat('FFFFFF', 0.08, (32, 1))))
    option_row(t, 'Skin', '피부색', 'LookRows')
    note = text(t, 'LookEmptyNote', 'SheetBody', '여성 외형은 준비 중입니다. 기본 모습으로 만들어집니다.', 16, colour=(IVORY, 0.6))
    box(note, pad=(0, 19, 0, 19), h='left')        # a 60 px row like the others; the C++ shows it instead of LookRows and writes the sentence
    note.set_visibility(VIS.COLLAPSED)

    # name
    cap(t, 'NameCap', 'SheetBody', '이름', (0, 22, 0, 10))
    box(sized(t, 'NameField', 'SheetBody', height=56))
    t.add('Overlay', 'NameFieldStack', 'NameField', VIS.SELF_HIT_TEST_INVISIBLE)
    box(image(t, 'NameFieldBg', 'NameFieldStack', rbox(('FFFFFF', 0.07), 16, ('FFFFFF', 0.22), 1.0)))
    box(t.add('HorizontalBox', 'NameFieldRow', 'NameFieldStack', VIS.SELF_HIT_TEST_INVISIBLE), pad=(17, 0, 10, 0))
    name_box = box(t.add('EditableTextBox', 'NameTextBox', 'NameFieldRow'), v='center', fill=1.0)
    style = unreal.find_object(old, 'NameTextBox').get_editor_property('widget_style').copy()
    for state in ('background_image_normal', 'background_image_hovered', 'background_image_focused', 'background_image_read_only'):
        style.set_editor_property(state, nodraw())
    info = unreal.SlateFontInfo()
    info.font_object, info.typeface_font_name = typeface(FONT_BOLD)
    info.size = 22 * 0.75
    text_style = style.get_editor_property('text_style')
    text_style.set_editor_property('font', info)
    text_style.set_editor_property('color_and_opacity', slate(IVORY))
    style.set_editor_property('text_style', text_style)
    for field, value in (('foreground_color', slate(IVORY)), ('focused_foreground_color', slate(IVORY)), ('padding', unreal.Margin(0, 0, 8, 0))):
        try:
            style.set_editor_property(field, value)
        except Exception as error:
            log('text box style.%s not set: %s' % (field, error))
    name_box.set_editor_property('widget_style', style)
    name_box.set_hint_text('이름을 입력하세요')
    check = button(t, 'NameCheckButton', 'NameFieldRow', ghost(0.05), ghost(0.12, 0.34), ghost(0.18, 0.4), ghost(0.02, 0.1), pad=(14, 0, 14, 0))
    box(check, h='right', v='center')
    box(sized(t, 'NameCheckBox', 'NameCheckButton', height=38))
    box(text(t, 'NameCheckLabel', 'NameCheckBox', '중복확인', 15, colour=('DDE3E7', 1.0)), h='center', v='center')
    line(t, 'NameMsgLine', 'SheetBody', 20, (0, 10, 0, 0))
    box(t.add('HorizontalBox', 'NameMsgRow', 'NameMsgLine', VIS.HIT_TEST_INVISIBLE), h='left', v='center')
    box(image(t, 'NameOkMark', 'NameMsgRow', tex('T_CCG_Icon_Check', (15, 15), tint=('9FE0A0', 1.0))), pad=(0, 0, 6, 0), h='center', v='center')
    box(text(t, 'NameErrorText', 'NameMsgRow', '사용 가능한 이름입니다.', 15, colour=('9FE0A0', 1.0)), v='center')

    box(t.add('Spacer', 'Grow', 'SheetBody', VIS.HIT_TEST_INVISIBLE), fill=1.0)

    # create
    cta = lambda value, alpha=1.0: tex('T_CCG_Cta', (96, 64), (1 / 3.0, 0, 1 / 3.0, 0), ('%02X%02X%02X' % (value, value, value), alpha))
    box(t.add('Overlay', 'CreateStackOuter', 'SheetBody', VIS.SELF_HIT_TEST_INVISIBLE))
    box(image(t, 'CreateShadow', 'CreateStackOuter', tex('T_CCG_CtaShadow', (216, 164), (92 / 216.0, 0, 92 / 216.0, 0), ('FFFFFF', 1.0))), pad=(-60, -40, -60, -60))
    create = button(t, 'CreateButton', 'CreateStackOuter', cta(244), cta(255), cta(216), cta(255, 0.38))
    box(create)
    box(sized(t, 'CreateBox', 'CreateButton', height=64))
    t.add('Overlay', 'CreateStack', 'CreateBox', VIS.HIT_TEST_INVISIBLE)
    shine(t, 'CreateShine', 'CreateStack')
    box(text(t, 'CreateLabel', 'CreateStack', '생성하기', 24, True, (NAVY, 1.0)), h='center', v='center')
    go = box(sized(t, 'CreateGo', 'CreateStack', 48, 48), pad=(0, 0, 8, 0), h='right', v='center')
    t.add('Overlay', 'CreateGoStack', 'CreateGo', VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'CreateGoDisc', 'CreateGoStack', rbox((NAVY, 1.0))))
    box(image(t, 'CreateGoIcon', 'CreateGoStack', tex('T_CCG_Icon_Arrow', (22, 22), tint=(IVORY, 1.0))), h='center', v='center')

    # footer
    line(t, 'FootLine', 'SheetBody', 20, (0, 14, 0, 0))
    box(t.add('HorizontalBox', 'FootRow', 'FootLine', VIS.SELF_HIT_TEST_INVISIBLE), v='center')
    reset = button(t, 'ResetButton', 'FootRow', nodraw(), rbox(('FFFFFF', 0.08)), rbox(('FFFFFF', 0.14)), pad=(8, 2, 8, 2))
    box(reset, pad=(-8, -2, 0, -2), h='left', v='center')
    box(t.add('HorizontalBox', 'ResetRow', 'ResetButton', VIS.HIT_TEST_INVISIBLE))
    box(image(t, 'ResetIcon', 'ResetRow', tex('T_CCG_Icon_Reset', (15, 15), tint=(IVORY, 0.7))), h='center', v='center')
    box(text(t, 'ResetLabel', 'ResetRow', '초기화', 15, colour=(IVORY, 0.7)), pad=(7, 0, 0, 0), v='center')
    box(text(t, 'NameRule', 'FootRow', '2~14자, 한글 또는 영문', 15, colour=(IVORY, 0.7)), h='right', v='center', fill=1.0)
    t.get('NameRule').set_editor_property('justification', unreal.TextJustify.RIGHT)

    intro_veil(t, 'Root')
    t.finish()

    # class defaults: start on the first open race, fold unused thumbnails, colours for the dark sheet
    defaults = unreal.get_default_object(t.bp.generated_class())
    for name, value in (('auto_pick_first_race', True), ('collapse_unused_thumbs', True), ('play_intro', True),
                        ('thumb_box_size', 48.0),
                        ('thumb_selected_color', unreal.LinearColor(1, 1, 1, 1)), ('thumb_normal_color', unreal.LinearColor(1, 1, 1, 0.7)),
                        ('thumb_icon_dim_color', unreal.LinearColor(1, 1, 1, 0.7)),
                        ('name_error_color', lin('F0A59C')), ('name_message_color', lin('9FE0A0'))):
        defaults.set_editor_property(name, value)
    t.finish()


def point_game_modes():
    target = unreal.load_class(None, MAIN + '.WBP_CharacterCreate_Glass_C')
    assert target, 'screen class missing'
    for path in GAME_MODES:
        keep = os.path.join(BACKUP, path.strip('/').replace('/', '_') + '.uasset')
        if not os.path.exists(keep):
            os.makedirs(BACKUP, exist_ok=True)
            shutil.copy2(os.path.join(ROOT, path.replace('/Game/', 'Content/') + '.uasset'), keep)
        blueprint = unreal.load_asset(path)
        defaults = unreal.get_default_object(blueprint.generated_class())
        before = defaults.get_editor_property('character_create_widget_class')
        blueprint.modify()
        defaults.set_editor_property('character_create_widget_class', target)
        assert B.compile_blueprint(path).success, path
        assert E.save_asset(path, False), path
        log('%s: %s -> %s' % (path, before.get_name() if before else None, target.get_name()))


import_kit()
build_seg_button()
build_race_chip()
build_swatch()
build_screen()
if os.environ.get('CC_SKIP_GAMEMODES', '') != '1':
    point_game_modes()
log('DONE')
