# -*- coding: utf-8 -*-
"""Character select screen, same glass sheet and inn-door stage as the creation screen.

Headless:  run.ps1 -Steps select   (removes the two widgets first, then builds them from nothing)
  /Game/Art/UI/CharacterCreate/Glass/Stage/T_CCG_InnDoor_Backdrop_NoFoot, T_CCG_FootShadow   stage picture without / and its foot shadow
  /Game/Art/UI/CharacterCreate/Glass/Select/WBP_CharacterSlot_Glass     one roster row (parent UNUCharacterSlotWidget)
  /Game/Art/UI/CharacterCreate/Glass/Select/WBP_CharacterSelect_Glass   the screen (parent UNUCharacterSelectWidget)
and points both select game modes at the new screen. The old WBP_CharacterSelect / WBP_CharacterSlot are not touched.

The stage is a picture inside the widget (the select level has no stage), and the picked character stands on it at 5 screen
pixels per art pixel, where the creation screen's stand is - moving between the two screens keeps the same place.
Mock: Docs/CharacterCreate_Proposal_20261013/select_mock.png.
"""
import importlib
import os
import shutil
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/CharacterCreateGlass'))
import ccg_kit
importlib.reload(ccg_kit)       # an editor session keeps modules between runs
from ccg_kit import *           # noqa: F401,F403  (paths, colours, brush and tree helpers)

ART = os.path.join(ROOT, 'ArtSource/UI/CharacterCreateGlass_20261013')
STAGE = BASE + '/Stage'
SELECT = BASE + '/Select'
SEL_MAIN = SELECT + '/WBP_CharacterSelect_Glass'
SEL_SLOT = SELECT + '/WBP_CharacterSlot_Glass'
OLD_SLOT = '/Game/Developers/dbals/UI/WBP_CharacterSlot'
OLD_SELECT = '/Game/Developers/dbals/UI/WBP_CharacterSelect'
SELECT_GAME_MODES = ['/Game/GameMode/BP_CharSelectGameMode', '/Game/Developers/dbals/BP_dbals_CharSelectGameMode']
FEET = (705, 930)           # where the creation stand's feet are, 1080p
HERO_SCALE = 5              # screen pixels per art pixel, as on the creation stage
CHIP = 56                   # roster face chip (corner radius 12: Slate clips the portrait square, so a small radius hides it)
ROW = 72                    # roster row height (plus an 8 px gap)


def import_stage():
    """The stage without the baked foot shadow, and the shadow on its own (bake_backdrop.py makes both)."""
    tasks = []
    for name in ('T_CCG_InnDoor_Backdrop_NoFoot', 'T_CCG_FootShadow'):
        task = unreal.AssetImportTask()
        task.filename = os.path.join(ART, name + '.png')
        task.destination_path = STAGE; task.destination_name = name
        task.automated = True; task.replace_existing = True; task.save = False
        tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    for name, compression in (('T_CCG_InnDoor_Backdrop_NoFoot', unreal.TextureCompressionSettings.TC_BC7),
                              ('T_CCG_FootShadow', unreal.TextureCompressionSettings.TC_EDITOR_ICON)):
        texture = unreal.load_asset(STAGE + '/' + name)
        assert texture, name
        texture.set_editor_properties({'compression_settings': compression, 'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                                       'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI, 'never_stream': True, 'srgb': True})
        assert E.save_asset(STAGE + '/' + name, False)
    log('stage textures imported')


JOB_KEYS = ['Novice', 'NaiaKnight', 'Tracker', 'Elementalist', 'Gambler', 'Druid']
JOB_ICON = 40               # job emblem in the roster chip (the 80 px pictures are drawn at half size)


def import_job_icons():
    """Roster chips show the job emblem instead of a live face (three flipbooks per row cost frames, 10/14).
    The pictures are the character menu's emblems (ArtSource/UI/JobEmblems_20260927) cut down to 80 px."""
    tasks = []
    for key in JOB_KEYS:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(ART, 'job_icons', 'T_CCG_Job_%s.png' % key)
        task.destination_path = SELECT; task.destination_name = 'T_CCG_Job_' + key
        task.automated = True; task.replace_existing = True; task.save = False
        tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    for key in JOB_KEYS:
        path = SELECT + '/T_CCG_Job_' + key
        texture = unreal.load_asset(path)
        assert texture, path
        texture.set_editor_properties({'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                                       'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE,
                                       'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI, 'never_stream': True, 'srgb': True})
        assert E.save_asset(path, False)
    table = unreal.load_asset('/Game/Developers/zxcv1/CharacterProgression/Data/DT_JobDefinition')
    if table:
        log('job keys in DT_JobDefinition: %s' % unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, 'JobKey'))
    log('job icons imported')


def stage_brush(name, size):
    brush = unreal.SlateBrush(); brush.draw_as = unreal.SlateBrushDrawType.IMAGE
    brush.set_editor_property('resource_object', unreal.load_asset(STAGE + '/' + name))
    brush.image_size = vec2(*size)
    return brush


def line(tree, name, parent, height, pad=(0, 0, 0, 0)):
    """A box as tall as the mock's CSS line box (see apply_ui.line)."""
    return box(sized(tree, name, parent, height=height), pad=pad)


def cap(tree, name, parent, value, pad):
    line(tree, name + 'Line', parent, 20, pad)
    return box(text(tree, name, name + 'Line', value, 15, True, (GOLD, 1.0), 160), h='left', v='center')


def clip(widget):
    widget.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    return widget


def build_slot():
    """One roster row: face chip, name, "Lv · job · race", and delete / pending on the right."""
    t = widget_blueprint(SEL_SLOT, '/Script/ProjectM.NUCharacterSlotWidget')
    t.add('SizeBox', 'SlotRoot', '')
    t.get('SlotRoot').set_height_override(ROW + 8)
    row = box(t.add('Overlay', 'Row', 'SlotRoot', VIS.SELF_HIT_TEST_INVISIBLE), pad=(0, 0, 0, 8))

    # the picked row: soft ivory fill and outline, and an ivory ring round the chip (CSS box-shadow 2.5 px outside the chip)
    frame = box(t.add('Overlay', 'SelectionFrame', row.get_name(), VIS.HIDDEN))
    box(image(t, 'SelectionFill', frame.get_name(), rbox((IVORY, 0.10), 18, (IVORY, 0.55), 1.0)))
    ring = box(sized(t, 'SelectionChipRingBox', frame.get_name(), CHIP + 5, CHIP + 5), pad=(12 - 2.5, 0, 0, 0), h='left', v='center')
    box(image(t, 'SelectionChipRing', ring.get_name(), rbox(('FFFFFF', 0.0), 14.5, (IVORY, 1.0), 2.5)))

    box(button(t, 'SlotButton', row.get_name(), nodraw(), rbox(('FFFFFF', 0.05), 18), rbox(('FFFFFF', 0.09), 18)))

    line_box = box(t.add('HorizontalBox', 'CharacterBox', row.get_name(), VIS.SELF_HIT_TEST_INVISIBLE), pad=(12, 0, 12, 0))
    chip = box(sized(t, 'ChipBox', line_box.get_name(), CHIP, CHIP), v='center')
    stack = clip(t.add('Overlay', 'Chip', chip.get_name(), VIS.HIT_TEST_INVISIBLE))
    box(image(t, 'ChipFill', stack.get_name(), rbox(('0C1722', 1.0), 12, ('FFFFFF', 0.16), 1.0)))
    # the chip shows the job emblem (C++ JobIconImage). PortraitOverlay stays only because the C++ binds it --- the live face
    # (three flipbooks per row, each on its own frame timer) is not drawn any more
    portrait = box(t.add('Overlay', 'PortraitOverlay', stack.get_name(), VIS.COLLAPSED))
    icon = unreal.SlateBrush(); icon.draw_as = unreal.SlateBrushDrawType.IMAGE
    icon.set_editor_property('resource_object', unreal.load_asset(SELECT + '/T_CCG_Job_Novice'))
    icon.image_size = vec2(JOB_ICON, JOB_ICON)
    icon.tint_color = slate(GOLD, 1.0)
    box(image(t, 'JobIconImage', stack.get_name(), icon), h='center', v='center')

    words = box(t.add('VerticalBox', 'TextColumn', line_box.get_name(), VIS.HIT_TEST_INVISIBLE), pad=(14, 0, 8, 0), v='center', fill=1.0)
    # the deletion badge sits next to the name, so the "Lv · job · race" line keeps its width
    box(t.add('HorizontalBox', 'NameRow', words.get_name(), VIS.HIT_TEST_INVISIBLE), h='left')
    box(text(t, 'NameText', 'NameRow', '새벽', 19, True), v='center')
    box(text(t, 'PendingText', 'NameRow', '', 13, True, ('F0A59C', 1.0)), pad=(10, 0, 0, 0), v='center')
    sub = box(text(t, 'SubText', words.get_name(), 'Lv.1 · 노비스 · 인간', 14, colour=(IVORY, 0.62)), pad=(0, 3, 0, 0), h='left')
    sub.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    delete = button(t, 'DeleteButton', line_box.get_name(), ghost(0.05), ghost(0.12, 0.34), ghost(0.18, 0.4), pad=(12, 0, 12, 0))
    box(delete, v='center')
    box(sized(t, 'DeleteBox', delete.get_name(), height=30))
    box(text(t, 'DeleteText', 'DeleteBox', '삭제', 14, colour=('DDE3E7', 1.0)), h='center', v='center')

    # required by the C++ but unused here: a vertical list holds only characters (the create row sits under the list)
    t.add('SizeBox', 'EmptyBox', row.get_name(), VIS.COLLAPSED)
    t.finish()

    old = unreal.get_default_object(unreal.load_asset(OLD_SLOT).generated_class())
    defaults = unreal.get_default_object(t.bp.generated_class())
    for name, value in (('portrait_pixel_scale', 2.0), ('portrait_align_top', True), ('walk_when_selected', False),
                        ('single_line_sub', True), ('delete_only_when_selected', True), ('portrait_height', 200.0),
                        ('default_idle_flipbook', old.get_editor_property('default_idle_flipbook')),
                        ('default_walk_flipbook', old.get_editor_property('default_walk_flipbook'))):
        defaults.set_editor_property(name, value)
    defaults.set_editor_property('job_icons', {unreal.Name(key): unreal.load_asset(SELECT + '/T_CCG_Job_' + key) for key in JOB_KEYS})
    log('slot job icons: %d' % len(defaults.get_editor_property('job_icons')))
    t.finish()


def build_screen():
    t = widget_blueprint(SEL_MAIN, '/Script/ProjectM.NUCharacterSelectWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)

    # ----- the stage: 2560 x 1080 art, height fills the screen (same framing as the creation camera); backdrop, ambience,
    # shadow and character move together. x below is stage units: the approved 16:9 frame starts at STAGE_SHIFT -----
    layer = stage_frame(t, 'Stage', 'Root').get_name()
    canvas(image(t, 'Backdrop', layer, stage_brush('T_CCG_InnDoor_Backdrop_NoFoot', (STAGE_W, STAGE_H))), (0, 0, 0, 0), (0, 0, STAGE_W, STAGE_H))
    ambience(t, 'Ambience', layer)
    feet_x = STAGE_SHIFT + FEET[0]
    canvas(image(t, 'HeroShadow', layer, stage_brush('T_CCG_FootShadow', (260, 60))), (0, 0, 0, 0), (feet_x - 130, FEET[1] - 31, 260, 60))
    hero_w, hero_h = 60 * HERO_SCALE, 100 * HERO_SCALE       # the character canvas is 60 x 100; its feet are on the bottom row
    canvas(t.add('Overlay', 'HeroPortrait', layer, VIS.HIT_TEST_INVISIBLE), (0, 0, 0, 0), (feet_x - hero_w / 2, FEET[1] - hero_h, hero_w, hero_h))
    canvas(image(t, 'TopFade', 'Root', tex('T_CCG_FadeDown', (8, 128), tint=('060A12', 0.6))), (0, 0, 1, 0), (0, 0, 0, 190))
    canvas(image(t, 'BottomFade', 'Root', tex('T_CCG_FadeUp', (8, 128), tint=('060A12', 0.5))), (0, 1, 1, 1), (0, 0, 0, 140), align=(0, 1))

    # ----- back to the title (disconnects; the C++ asks first) -----
    canvas(t.add('Overlay', 'TitlePanel', 'Root', VIS.SELF_HIT_TEST_INVISIBLE), (0, 0, 0, 0), (48, 40, 0, 0), auto=True, z=2)
    pill_blur = box(t.add('BackgroundBlur', 'TitleBlur', 'TitlePanel', VIS.HIT_TEST_INVISIBLE), pad=(8, 0, 8, 0))
    pill_blur.set_editor_property('blur_strength', 18.0)
    pill_blur.set_editor_property('apply_alpha_to_blur', True)
    back = button(t, 'TitleButton', 'TitlePanel', rbox(('0E141E', 0.55), None, ('FFFFFF', 0.16), 1.0), rbox(('18202C', 0.72), None, ('FFFFFF', 0.26), 1.0),
                  rbox(('0E141E', 0.86), None, ('FFFFFF', 0.3), 1.0), pad=(15, 0, 19, 0))
    box(back)
    box(sized(t, 'TitleBox', 'TitleButton', height=54))
    t.add('HorizontalBox', 'TitleRow', 'TitleBox', VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'TitleIcon', 'TitleRow', tex('T_CCG_Icon_Left', (18, 18), tint=(IVORY, 1.0))), h='center', v='center')
    box(text(t, 'TitleLabel', 'TitleRow', '타이틀로', 18), pad=(8, 0, 0, 0), v='center')

    # ----- the sheet -----
    form = t.add('Overlay', 'FormPanel', 'Root', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(form, (1, 0, 1, 1), (-40, 40, 492, 40), align=(1, 0), z=1)
    box(image(t, 'SheetShadow', 'FormPanel', tex('T_CCG_SheetShadow', (520, 520), (190 / 520, 160 / 520, 190 / 520, 220 / 520))), pad=(-160, -130, -160, -190))
    blur = box(t.add('BackgroundBlur', 'SheetBlur', 'FormPanel', VIS.HIT_TEST_INVISIBLE))
    blur.set_editor_property('blur_strength', 26.0)
    blur.set_editor_property('apply_alpha_to_blur', True)       # fades in with the sheet. No corner radius: see apply_ui
    box(image(t, 'SheetGlass', 'FormPanel', tex('T_CCG_Sheet', (128, 256), (0.25, 0.125, 0.25, 0.125)), VIS.VISIBLE))
    box(t.add('VerticalBox', 'SheetBody', 'FormPanel', VIS.SELF_HIT_TEST_INVISIBLE), pad=(33, 33, 33, 29))

    # header: title + "3 / 8"
    box(sized(t, 'TopRow', 'SheetBody', height=40), pad=(0, -1, 0, -1))
    t.add('HorizontalBox', 'TopLine', 'TopRow', VIS.SELF_HIT_TEST_INVISIBLE)
    box(text(t, 'TitleCap', 'TopLine', '캐릭터 선택', 15, True, (GOLD, 1.0), 160), pad=(0, 4, 0, 0), h='left', v='top', fill=1.0)
    # a pill drawn by an image under the text (a Border here drew its rounded outline at the wrong size)
    box(sized(t, 'CountPill', 'TopLine', height=40), h='right', v='center')
    t.add('Overlay', 'CountStack', 'CountPill', VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'CountBg', 'CountStack', ghost(0.05)))
    box(text(t, 'SlotCountText', 'CountStack', '3 / 8', 15, colour=('DDE3E7', 1.0)), pad=(14, 0, 14, 0), h='center', v='center')

    # the picked character
    line(t, 'NameLine', 'SheetBody', 46, (0, 20, 0, 0))
    box(text(t, 'SelectedNameText', 'NameLine', '새벽', 40, True), pad=(0, 0, 0, 7), h='left', v='center')
    line(t, 'SubLine', 'SheetBody', 24, (0, 6, 0, 0))
    box(text(t, 'SelectedSubText', 'SubLine', 'Lv.12 · 나이아 나이트 · 인간', 16, colour=(IVORY, 0.74)), h='left', v='center')
    box(image(t, 'Rule1', 'SheetBody', flat('FFFFFF', 0.10, (32, 1))), pad=(0, 20, 0, 20))

    # the roster: up to six rows, then it scrolls
    cap(t, 'ListCap', 'SheetBody', '내 캐릭터', (0, 0, 0, 10))
    holder = box(sized(t, 'ListHolder', 'SheetBody'))
    holder.set_max_desired_height(6 * (ROW + 8))
    scroll = t.add('ScrollBox', 'ListScroll', 'ListHolder')
    for name, value in (('scrollbar_thickness', unreal.Vector2D(4, 4)), ('scrollbar_padding', unreal.Margin(6, 0, 0, 0))):
        try:
            scroll.set_editor_property(name, value)
        except Exception as error:
            log('scroll.%s not set: %s' % (name, error))
    box(t.add('VerticalBox', 'CharacterListBox', scroll.get_name(), VIS.SELF_HIT_TEST_INVISIBLE))

    # new character: a dashed row right under the list
    dashed = lambda alpha: tex('T_CCG_DashedRow', (426, 64), tint=('FFFFFF', alpha))
    create = button(t, 'CreateButton', 'SheetBody', dashed(1.0), rbox(('FFFFFF', 0.05), 18, ('FFFFFF', 0.4), 1.0),
                    rbox(('FFFFFF', 0.09), 18, ('FFFFFF', 0.45), 1.0), dashed(0.4))
    box(create, pad=(0, 0, 0, 0))
    box(sized(t, 'CreateBox', 'CreateButton', height=64))
    box(t.add('HorizontalBox', 'CreateRow', 'CreateBox', VIS.HIT_TEST_INVISIBLE), h='center', v='center')
    box(image(t, 'CreateIcon', 'CreateRow', tex('T_CCG_Icon_Plus', (16, 16), tint=(IVORY, 0.8))), h='center', v='center')
    box(text(t, 'CreateLabel', 'CreateRow', '새 캐릭터 만들기', 17, colour=(IVORY, 0.8)), pad=(8, 0, 0, 0), v='center')

    box(t.add('Spacer', 'Grow', 'SheetBody', VIS.HIT_TEST_INVISIBLE), fill=1.0)
    line(t, 'ErrorLine', 'SheetBody', 20, (0, 0, 0, 12))
    box(text(t, 'ErrorText', 'ErrorLine', '', 15, colour=('F0A59C', 1.0)), h='left', v='center')

    # enter
    cta = lambda value, alpha=1.0: tex('T_CCG_Cta', (96, 64), (1 / 3.0, 0, 1 / 3.0, 0), ('%02X%02X%02X' % (value, value, value), alpha))
    box(t.add('Overlay', 'EnterStackOuter', 'SheetBody', VIS.SELF_HIT_TEST_INVISIBLE))
    box(image(t, 'EnterShadow', 'EnterStackOuter', tex('T_CCG_CtaShadow', (216, 164), (92 / 216.0, 0, 92 / 216.0, 0))), pad=(-60, -40, -60, -60))
    box(button(t, 'EnterButton', 'EnterStackOuter', cta(244), cta(255), cta(216), cta(255, 0.38)))
    box(sized(t, 'EnterBox', 'EnterButton', height=64))
    t.add('Overlay', 'EnterStack', 'EnterBox', VIS.HIT_TEST_INVISIBLE)
    shine(t, 'EnterShine', 'EnterStack')
    box(text(t, 'EnterLabel', 'EnterStack', '시작하기', 24, True, (NAVY, 1.0)), h='center', v='center')
    go = box(sized(t, 'EnterGo', 'EnterStack', 48, 48), pad=(0, 0, 8, 0), h='right', v='center')
    t.add('Overlay', 'EnterGoStack', go.get_name(), VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'EnterGoDisc', 'EnterGoStack', rbox((NAVY, 1.0))))
    box(image(t, 'EnterGoIcon', 'EnterGoStack', tex('T_CCG_Icon_Arrow', (22, 22), tint=(IVORY, 1.0))), h='center', v='center')
    intro_veil(t, 'Root')
    t.finish()

    old = unreal.get_default_object(unreal.load_asset(OLD_SELECT).generated_class())
    # deleting asks with the red glass dialog (run.ps1 -Steps dialog builds it); without it, the old dialog
    danger = unreal.load_class(None, BASE + '/Dialog/WBP_CCG_ConfirmDialog_Danger.WBP_CCG_ConfirmDialog_Danger_C')
    defaults = unreal.get_default_object(t.bp.generated_class())
    for name, value in (('slot_widget_class', unreal.load_class(None, SEL_SLOT + '.WBP_CharacterSlot_Glass_C')),
                        ('confirm_dialog_class', danger or old.get_editor_property('confirm_dialog_class')),
                        ('default_slot_count', old.get_editor_property('default_slot_count')),
                        ('auto_select_first', True), ('hero_pixel_scale', float(HERO_SCALE)), ('play_intro', True)):
        defaults.set_editor_property(name, value)
    t.finish()


def point_game_modes():
    target = unreal.load_class(None, SEL_MAIN + '.WBP_CharacterSelect_Glass_C')
    assert target, 'screen class missing'
    for path in SELECT_GAME_MODES:
        keep = os.path.join(BACKUP, path.strip('/').replace('/', '_') + '.uasset')
        if not os.path.exists(keep):
            os.makedirs(BACKUP, exist_ok=True)
            shutil.copy2(os.path.join(ROOT, path.replace('/Game/', 'Content/') + '.uasset'), keep)
        blueprint = unreal.load_asset(path)
        defaults = unreal.get_default_object(blueprint.generated_class())
        before = defaults.get_editor_property('character_select_widget_class')
        blueprint.modify()
        defaults.set_editor_property('character_select_widget_class', target)
        assert B.compile_blueprint(path).success, path
        assert E.save_asset(path, False), path
        log('%s: %s -> %s' % (path, before.get_name() if before else None, target.get_name()))


import_kit()
import_stage()
import_job_icons()
build_slot()
build_screen()
if os.environ.get('CC_SKIP_GAMEMODES', '') != '1':
    point_game_modes()
log('DONE')
