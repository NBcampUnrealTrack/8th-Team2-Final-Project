# -*- coding: utf-8 -*-
"""Job selection and starter Naia selection on a stage, option A of Docs/HUD_Selection_Proposal_20261015 (user's pick).
Run headless through run.ps1 -Steps select (rebuilt from nothing every time); run.ps1 -Steps apply points the game at them.

  WBP_HUD_JobSelect     child of UNUJobSelectionWidget (same C++ as zxcv1 WBP_JobSelection --- request / confirm logic unchanged)
  WBP_HUD_JobCard       child of UNUJobSelectionCardWidget: the player in that job's clothes (combat idle when chosen), emblem, name
  WBP_HUD_JobSkillRow   one starting skill (kind chip, name, one-line description; the C++ fills it by name)
  WBP_HUD_SelChip       a small rounded chip (advancement branches)
  WBP_HUD_NaiaSelect    child of UNUStarterNaiaSelectionWidget (the native window had no WBP)
  WBP_HUD_NaiaCard      child of UNUStarterNaiaCardWidget: the Naia's idle flipbook, element icon, name

The scene behind is blurred and darkened, the chosen one stands in a light column of its colour, the rest step back dark;
a navy glass sheet at the bottom tells what the choice means (job: motto, 3 starting skills, Lv.20 branches; Naia: element,
innate passive, starting stats against the other five, evolution). Arrows / A D choose, F confirms (the C++ handles keys).
The new icons (element, reward) and job emblems are imported here into /Game/Art/UI/HUD2026/Icons.
Layout in 1080p units.
"""
import importlib
import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/HUDRenewal'))
import hud_kit
importlib.reload(hud_kit)
from hud_kit import *            # noqa: F401,F403

S = BASE + '/Select/'
ICONS = BASE + '/Icons'
ICON_SRC = os.path.join(ROOT, 'ArtSource/UI/HUD2026/reward_icons')
EMBLEM_SRC = os.path.join(ROOT, 'ArtSource/UI/HUD2026/select')
JOBS = ['NaiaKnight', 'Tracker', 'Elementalist', 'Gambler', 'Druid']
# job colours picked from each job's clothes (the code's GetJobAccent colours don't match the art)
JOB_ACCENT = {'NaiaKnight': '7E9BE0', 'Tracker': 'D9A74A', 'Elementalist': '7FC8EE', 'Gambler': 'D06A7E', 'Druid': '93B567'}
ELEMENTS = ['Fire', 'Water', 'Grass', 'Earth', 'Lightning', 'Wind']
DIALOG = '/Game/Art/UI/CharacterCreate/Glass/Dialog/WBP_CCG_ConfirmDialog.WBP_CCG_ConfirmDialog_C'
SHEET_W = 1620
SHEET_H = 284
DIMMED = unreal.LinearColor(0.10, 0.11, 0.14, 1.0)     # multiplies the figures that are not chosen


def wbp(name, parent):
    return widget_blueprint(S + name, parent if parent.startswith('/') else '/Script/ProjectM.' + parent)


def import_icons():
    """Element / reward icons (144 px, 48 dots x3, no mips like the item icons) and job emblems (256 px, mips)."""
    jobs = [(os.path.join(ICON_SRC, n + '.png'), n, False) for n in
            ['T_Reward_Exp', 'T_Reward_Gold'] + ['T_Element_' + e for e in ELEMENTS]]
    jobs += [(os.path.join(EMBLEM_SRC, 'T_HUD_JobEmblem_%s.png' % k), 'T_HUD_JobEmblem_' + k, True) for k in JOBS]
    tasks = []
    for path, name, _ in jobs:
        assert os.path.exists(path), path
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = ICONS; task.destination_name = name
        task.automated = True; task.replace_existing = True; task.save = False
        tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    for _, name, mips in jobs:
        texture = unreal.load_asset(ICONS + '/' + name)
        assert texture, name
        texture.set_editor_properties({
            'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE if mips else unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI,
            'never_stream': True,
            'srgb': True,
            'filter': unreal.TextureFilter.TF_DEFAULT,
        })
        assert E.save_asset(ICONS + '/' + name, False)
    log('select icons: %d' % len(jobs))


def icon(name, size, tint=('FFFFFF', 1.0)):
    asset = unreal.load_asset(ICONS + '/' + name)
    assert asset, name
    brush = unreal.SlateBrush()
    brush.set_editor_property('resource_object', asset)
    brush.image_size = vec2(size, size)
    brush.tint_color = slate(*tint)
    return brush


def key_chip(tree, name, parent, label_text, size=26):
    cap = sized(tree, name, parent, size if len(label_text) == 1 else None, size)
    face = box(overlay(tree, name + 'Face', name, VIS.HIT_TEST_INVISIBLE))
    box(image(tree, name + 'Rim', name + 'Face', rbox(('0E1A28', 1.0), 7)))
    box(image(tree, name + 'Fill', name + 'Face', rbox((IVORY, 1.0), 5.5)), pad=(2, 2, 2, 2))
    box(text(tree, name + 'Text', name + 'Face', label_text, size * 0.52, True, (NAVY, 1.0)), pad=(7, 0, 7, 0), h='center', v='center')
    return cap


def native(tree, kind, name, parent, hit=None):
    """A ProjectM C++ widget class (NUFlipbookImage ...) --- tried by short name, then by script path."""
    for k in (kind, '/Script/ProjectM.' + kind):
        result = W.add_component(tree.path, k, name, parent, True)
        if result.success:
            item = tree.get(name)
            if hit is not None:
                item.set_visibility(hit)
            return item
    raise AssertionError((kind, name, result.error_message))


def heading(tree, name, parent, caption, pad=(0, 0, 0, 0)):
    row = box(hbox(tree, name, parent), pad=pad)
    box(image(tree, name + 'Diamond', name, tex('T_HUD_Diamond', (8, 8))), pad=(0, 0, 8, 0), v='center')
    box(label(tree, name + 'Text', name, caption, 14, True, (GOLD_HI, 1.0), shadow=False), v='center')
    return row


def rule_v(tree, name, parent):
    return box(image(tree, name, parent, flat(IVORY, 0.13, (1, 32))), pad=(28, 6, 28, 6))


def hint_bar(tree, parent, items, y):
    row = canvas(hbox(tree, 'HintBar', parent), (0.5, 1, 0.5, 1), (0, y, 0, 0), (0.5, 0.5), auto=True)
    for i, (keys, caption) in enumerate(items):
        for j, k in enumerate(keys):
            box(key_chip(tree, 'HintKey%d_%d' % (i, j), 'HintBar', k), pad=(0 if (i == 0 and j == 0) else (6 if j else 26), 0, 0, 0), v='center')
        box(label(tree, 'HintText%d' % i, 'HintBar', caption, 15, True, (MUTED, 1.0), shadow=False), pad=(10 if keys else 26, 0, 0, 0), v='center')


def gold_button(tree, parent, caption):
    gold_n = rbox(('D6BE8C', 1.0), None, ('96763A', 1.0), 1.2)
    gold_h = rbox(('E6CF9E', 1.0), None, ('96763A', 1.0), 1.2)
    gold_p = rbox(('C2A874', 1.0), None, ('7A5F2C', 1.0), 1.2)
    gold_d = rbox(('6E6A62', 0.55), None, (IVORY, 0.12), 1.0)
    box(button(tree, 'ConfirmButton', parent, gold_n, gold_h, gold_p, gold_d))
    cr = box(hbox(tree, 'ConfirmRow', 'ConfirmButton'), h='center', v='center')
    cr.set_visibility(VIS.HIT_TEST_INVISIBLE)
    box(text(tree, 'ConfirmLabel', 'ConfirmRow', caption, 19, True, (NAVY, 1.0)), v='center')
    box(key_chip(tree, 'ConfirmKey', 'ConfirmRow', 'F', 24), pad=(12, 0, 0, 0), v='center')


def backdrop(tree, cap, title):
    """Blurred, darkened scene + the title block. Returns nothing; children go on 'Root'."""
    tree.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    blur = canvas(tree.add('BackgroundBlur', 'SceneBlur', 'Root', VIS.HIT_TEST_INVISIBLE), (0, 0, 1, 1), (0, 0, 0, 0))
    blur.set_editor_property('blur_strength', 7.0)
    canvas(image(tree, 'SceneVeil', 'Root', tex('T_HUD_SelVeil')), (0, 0, 1, 1), (0, 0, 0, 0))
    head = canvas(vbox(tree, 'TitleBlock', 'Root'), (0.5, 0, 0.5, 0), (0, 50, 0, 0), (0.5, 0), auto=True)
    box(label(tree, 'TitleCap', 'TitleBlock', cap, 15, True, (GOLD_HI, 1.0)), h='center')
    box(label(tree, 'StageTitle', 'TitleBlock', title, 38, True), pad=(0, 8, 0, 0), h='center')
    rule = box(hbox(tree, 'TitleRule', 'TitleBlock'), pad=(0, 14, 0, 0), h='center')
    box(image(tree, 'TitleRuleL', 'TitleRule', tex('T_HUD_RuleL', (160, 2))), v='center')
    box(image(tree, 'TitleRuleDiamond', 'TitleRule', tex('T_HUD_Diamond', (10, 10))), pad=(14, 0, 14, 0), v='center')
    box(image(tree, 'TitleRuleR', 'TitleRule', tex('T_HUD_RuleR', (160, 2))), v='center')


def sheet(tree, name):
    """The navy glass sheet at the bottom. Returns the HorizontalBox that holds its columns."""
    holder = canvas(sized(tree, name + 'Size', 'Root', SHEET_W, SHEET_H), (0.5, 1, 0.5, 1), (0, -64, SHEET_W, SHEET_H), (0.5, 1))
    content = glass(tree, name, name + 'Size', 20, strength=14.0, fill=('0E1A28', 0.88), line=(IVORY, 0.16))
    box(image(tree, 'AccentBar', content, rbox(('FFFFFF', 1.0), 2, None, 0.0, (4, 16))), pad=(1, 22, 0, 22), h='left')
    cols = box(hbox(tree, name + 'Columns', content), pad=(32, 28, 32, 28))
    return name + 'Columns'


# ======================================================================= small parts
def build_chip():
    t = wbp('WBP_HUD_SelChip', '/Script/UMG.UserWidget')
    pad = t.add('Border', 'ChipPad', '', VIS.HIT_TEST_INVISIBLE)
    pad.set_brush(nodraw()); pad.set_padding(unreal.Margin(0, 0, 10, 0))
    chip = box(t.add('Border', 'Chip', 'ChipPad', VIS.HIT_TEST_INVISIBLE))
    chip.set_brush(rbox((IVORY, 0.07), 15, (IVORY, 0.42), 1.0))
    chip.set_padding(unreal.Margin(14, 4, 14, 5))
    text(t, 'ChipText', 'Chip', '템페스트', 15, True, (IVORY, 1.0))
    t.finish()


def build_skill_row():
    t = wbp('WBP_HUD_JobSkillRow', '/Script/UMG.UserWidget')
    v = vbox(t, 'Row', '')
    top = box(hbox(t, 'Top', 'Row'))
    chip = box(t.add('Border', 'SkillKindChip', 'Top', VIS.HIT_TEST_INVISIBLE), v='center')
    chip.set_brush(rbox(('FFFFFF', 0.16), 11, ('FFFFFF', 0.8), 1.0))
    chip.set_padding(unreal.Margin(10, 2, 10, 3))
    text(t, 'SkillKind', 'SkillKindChip', '액티브', 12.5, True, (GOLD_HI, 1.0))
    box(label(t, 'SkillName', 'Top', '나이아 속사', 19, True, shadow=False), pad=(10, 0, 0, 0), v='center')
    desc = box(label(t, 'SkillDesc', 'Row', '공격력 135% 피해.', 14.5, False, (MUTED, 1.0), shadow=False), pad=(0, 5, 0, 10))
    desc.set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
    desc.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    t.finish()


def light(tree, parent, w, feet, beam_h, glow_w, figure_name):
    """Light column + ground light (SelectedLight), foot shadow, the figure anchored by its feet at x centre / y feet."""
    sel = canvas(overlay(tree, 'SelectedLight', parent, VIS.HIT_TEST_INVISIBLE), (0, 0, 0, 0), (0, 0, w, feet + 40))
    box(image(tree, 'LightBeam', 'SelectedLight', tex('T_HUD_SelBeam', (min(w + 40, 320), beam_h))), pad=(0, feet - beam_h, 0, 0), h='center', v='top')
    box(image(tree, 'LightGround', 'SelectedLight', tex('T_HUD_SelGlow', (glow_w, int(glow_w * 0.24)))), pad=(0, feet - int(glow_w * 0.12) + 4, 0, 0), h='center', v='top')
    canvas(image(tree, 'FootShadow', parent, tex('T_HUD_SelShadow', (int(glow_w * 0.55), int(glow_w * 0.13)))),
           (0.5, 0, 0.5, 0), (0, feet + 2, int(glow_w * 0.55), int(glow_w * 0.13)), (0.5, 0.5))


# ======================================================================= job
JOB_CARD_W, JOB_CARD_H, JOB_FEET = 270, 470, 380


def build_job_card():
    t = wbp('WBP_HUD_JobCard', 'NUJobSelectionCardWidget')
    sz = t.add('SizeBox', 'CardSize', '')
    sz.set_width_override(JOB_CARD_W); sz.set_height_override(JOB_CARD_H)
    t.add('CanvasPanel', 'Card', 'CardSize', VIS.SELF_HIT_TEST_INVISIBLE)
    light(t, 'Card', JOB_CARD_W, JOB_FEET, 300, 300, 'FigureOverlay')
    canvas(overlay(t, 'FigureOverlay', 'Card', VIS.HIT_TEST_INVISIBLE), (0.5, 0, 0.5, 0), (0, JOB_FEET + 6, 0, 0), (0.5, 1), auto=True)
    canvas(image(t, 'EmblemImage', 'Card', icon('T_HUD_JobEmblem_Tracker', 40)), (0.5, 0, 0.5, 0), (0, JOB_FEET + 44, 40, 40), (0.5, 0.5))
    name = canvas(label(t, 'NameText', 'Card', '트래커', 21, True), (0.5, 0, 0.5, 0), (0, JOB_FEET + 88, 0, 0), (0.5, 0.5), auto=True)
    name.set_editor_property('justification', unreal.TextJustify.CENTER)
    btn = canvas(button(t, 'CardButton', 'Card', nodraw(), rbox((IVORY, 0.035), 18), rbox((IVORY, 0.06), 18)), (0, 0, 1, 1), (0, 0, 0, 0))
    t.finish()
    cdo(t).set_editor_property('dimmed_figure_tint', DIMMED)      # the ones not chosen step back into the dark
    t.finish()


def build_job_select():
    t = wbp('WBP_HUD_JobSelect', 'NUJobSelectionWidget')
    backdrop(t, '첫 직업 · 엘라던의 단상', '어떤 힘으로 나아갈 것인가')
    # C++-required texts the stage doesn't show (the title above is the stage's own; the C++ writes the old one into TitleText)
    for name in ('TitleText', 'LevelStatusText', 'KeyHintText'):
        canvas(label(t, name, 'Root', '', 12, False, shadow=False), (0, 0, 0, 0), (0, 0, 0, 0), auto=True).set_visibility(VIS.COLLAPSED)
    canvas(hbox(t, 'JobListPanel', 'Root'), (0.5, 0, 0.5, 0), (0, 186, 0, 0), (0.5, 0), auto=True)

    cols = sheet(t, 'Sheet')
    # column 1: emblem + name, motto
    c1 = box(sized(t, 'Col1Size', cols, 520, None))
    v1 = vbox(t, 'Col1', 'Col1Size')
    top = box(hbox(t, 'NameRow', 'Col1'))
    box(image(t, 'DetailEmblem', 'NameRow', icon('T_HUD_JobEmblem_Tracker', 84)), v='center')
    box(label(t, 'SelectedJobText', 'NameRow', '트래커', 32, True), pad=(18, 0, 0, 0), v='center')
    box(label(t, 'DescriptionText', 'Col1', '“나는 누구보다 먼저 본다.”', 18, False, ('D6D2C8', 1.0), shadow=False, wrap=True), pad=(4, 20, 0, 0))
    rule_v(t, 'Rule1', cols)
    # column 2: starting skills
    c2 = box(vbox(t, 'Col2', cols), fill=1.0)
    heading(t, 'SkillHead', 'Col2', '시작 스킬 3종 · 전직하면 바로 배움')
    sl = box(vbox(t, 'SkillList', 'Col2'), pad=(0, 12, 0, 0))
    rule_v(t, 'Rule2', cols)
    # column 3: branches + confirm
    c3 = box(sized(t, 'Col3Size', cols, 330, None))
    v3 = vbox(t, 'Col3', 'Col3Size')
    heading(t, 'AdvHead', 'Col3', 'Lv.20 두 갈래 전직')
    box(t.add('WrapBox', 'AdvancementList', 'Col3', VIS.SELF_HIT_TEST_INVISIBLE), pad=(0, 14, 0, 0))
    box(label(t, 'AdvNote', 'Col3', '지금 고르는 건 첫 직업 하나뿐입니다', 14, False, (MUTED, 1.0), shadow=False), pad=(0, 12, 0, 0))
    box(sized(t, 'Col3Spacer', 'Col3', 10, 10), fill=1.0)
    box(sized(t, 'ConfirmSize', 'Col3', None, 56))
    gold_button(t, 'ConfirmSize', '트래커로 정하기')
    hint_bar(t, 'Root', [(['←', '→'], '고르기'), (['F'], '정하기'), ([], '정한 뒤에는 바꿀 수 없습니다')], -32)
    t.finish()

    d = cdo(t)
    d.set_editor_property('job_card_class', unreal.load_class(None, S + 'WBP_HUD_JobCard.WBP_HUD_JobCard_C'))
    d.set_editor_property('skill_row_class', unreal.load_class(None, S + 'WBP_HUD_JobSkillRow.WBP_HUD_JobSkillRow_C'))
    d.set_editor_property('advancement_chip_class', unreal.load_class(None, S + 'WBP_HUD_SelChip.WBP_HUD_SelChip_C'))
    d.set_editor_property('job_emblems', {k: unreal.load_asset(ICONS + '/T_HUD_JobEmblem_' + k) for k in JOBS})
    d.set_editor_property('job_accents', {k: lin(v) for k, v in JOB_ACCENT.items()})
    d.set_editor_property('select_first_on_open', True)
    d.set_editor_property('hide_field_hud_while_open', True)
    d.set_editor_property('confirm_dialog_class', unreal.load_class(None, DIALOG))
    t.finish()


# ======================================================================= starter Naia
NAIA_CARD_W, NAIA_CARD_H, NAIA_FEET = 230, 360, 262


def build_naia_card():
    t = wbp('WBP_HUD_NaiaCard', 'NUStarterNaiaCardWidget')
    sz = t.add('SizeBox', 'CardSize', '')
    sz.set_width_override(NAIA_CARD_W); sz.set_height_override(NAIA_CARD_H)
    t.add('CanvasPanel', 'Card', 'CardSize', VIS.SELF_HIT_TEST_INVISIBLE)
    light(t, 'Card', NAIA_CARD_W, NAIA_FEET, 240, 280, 'FigureImage')
    canvas(native(t, 'NUFlipbookImage', 'FigureImage', 'Card', VIS.HIT_TEST_INVISIBLE), (0.5, 0, 0.5, 0), (0, NAIA_FEET + 4, 0, 0), (0.5, 1), auto=True)
    canvas(image(t, 'ElementIcon', 'Card', icon('T_Element_Fire', 48)), (0.5, 0, 0.5, 0), (0, NAIA_FEET + 42, 48, 48), (0.5, 0.5))
    name = canvas(label(t, 'NameText', 'Card', '불코이', 21, True), (0.5, 0, 0.5, 0), (0, NAIA_FEET + 86, 0, 0), (0.5, 0.5), auto=True)
    name.set_editor_property('justification', unreal.TextJustify.CENTER)
    canvas(button(t, 'CardButton', 'Card', nodraw(), rbox((IVORY, 0.035), 18), rbox((IVORY, 0.06), 18)), (0, 0, 1, 1), (0, 0, 0, 0))
    t.finish()
    cdo(t).set_editor_property('dimmed_figure_tint', DIMMED)
    t.finish()


def stat_cell(tree, parent, index, caption):
    cell = box(vbox(tree, 'StatCell%d' % index, parent), pad=(0, 0, 36, 14))
    row = box(hbox(tree, 'StatHead%d' % index, 'StatCell%d' % index))
    box(label(tree, 'StatLabel%d' % index, 'StatHead%d' % index, caption, 15, False, (MUTED, 1.0), shadow=False), v='center', fill=1.0)
    box(label(tree, 'StatValue%d' % index, 'StatHead%d' % index, '0', 16, True, shadow=False), v='center')
    size = box(sized(tree, 'StatBarSize%d' % index, 'StatCell%d' % index, 250, 8), pad=(0, 8, 0, 0))
    bar = tree.add('ProgressBar', 'StatBar%d' % index, 'StatBarSize%d' % index, VIS.HIT_TEST_INVISIBLE)
    style = bar.get_editor_property('widget_style').copy()
    style.background_image = rbox(('FFFFFF', 0.10), 4, None, 0.0, (16, 8))
    style.fill_image = rbox(('FFFFFF', 1.0), 4, None, 0.0, (16, 8))
    style.marquee_image = nodraw()
    bar.set_editor_property('widget_style', style)
    bar.set_percent(0.7)


def build_naia_select():
    t = wbp('WBP_HUD_NaiaSelect', 'NUStarterNaiaSelectionWidget')
    backdrop(t, '시작 나이아 · 엘라던의 단상', '함께 걸을 나이아를 고르세요')
    canvas(hbox(t, 'NaiaListPanel', 'Root'), (0.5, 0, 0.5, 0), (0, 252, 0, 0), (0.5, 0), auto=True)

    cols = sheet(t, 'Sheet')
    c1 = box(sized(t, 'Col1Size', cols, 520, None))
    v1 = vbox(t, 'Col1', 'Col1Size')
    top = box(hbox(t, 'NameRow', 'Col1'))
    tile = box(sized(t, 'ElementTile', 'NameRow', 92, 92), v='center')
    overlay(t, 'ElementTileStack', 'ElementTile')
    box(image(t, 'ElementTileBack', 'ElementTileStack', rbox(('060C14', 1.0), 16, (IVORY, 0.16), 1.0)))
    box(image(t, 'DetailElementIcon', 'ElementTileStack', icon('T_Element_Fire', 72)), h='center', v='center')
    names = box(vbox(t, 'NameCol', 'NameRow'), pad=(20, 0, 0, 0), v='center')
    box(label(t, 'SelectedNaiaText', 'NameCol', '불코이', 32, True))
    chip = box(t.add('Border', 'ElementChip', 'NameCol', VIS.HIT_TEST_INVISIBLE), pad=(0, 10, 0, 0), h='left')
    chip.set_brush(rbox(('FFFFFF', 0.14), 13, ('FFFFFF', 0.75), 1.0))
    chip.set_padding(unreal.Margin(12, 3, 12, 4))
    text(t, 'ElementText', 'ElementChip', '불 속성', 14, True, (IVORY, 1.0))
    heading(t, 'PassiveHead', 'Col1', '타고난 힘', pad=(0, 22, 0, 0))
    box(label(t, 'PassiveNameText', 'Col1', '불꽃의 의지', 19, True, shadow=False), pad=(0, 8, 0, 0))
    box(label(t, 'DescriptionText', 'Col1', 'HP 40% 아래면 불 속성 스킬 위력 +30%.', 15, False, (MUTED, 1.0), shadow=False, wrap=True), pad=(0, 4, 0, 0))
    rule_v(t, 'Rule1', cols)

    c2 = box(vbox(t, 'Col2', cols), fill=1.0)
    heading(t, 'StatHeadTitle', 'Col2', '처음 능력치 · 여섯 나이아 중')
    box(hbox(t, 'StatGrid', 'Col2'), pad=(0, 16, 0, 0))
    box(vbox(t, 'StatColA', 'StatGrid'))
    box(vbox(t, 'StatColB', 'StatGrid'))
    for i, caption in enumerate(['체력', 'SP', '공격', '마법 공격', '방어', '속도']):
        stat_cell(t, 'StatColA' if i < 3 else 'StatColB', i, caption)
    rule_v(t, 'Rule2', cols)

    c3 = box(sized(t, 'Col3Size', cols, 330, None))
    v3 = vbox(t, 'Col3', 'Col3Size')
    heading(t, 'EvoHead', 'Col3', '진화')
    evo = box(hbox(t, 'EvoRow', 'Col3'), pad=(0, 12, 0, 0))
    etile = box(sized(t, 'EvoTile', 'EvoRow', 120, 96), v='center')
    overlay(t, 'EvoStack', 'EvoTile')
    box(image(t, 'EvoBack', 'EvoStack', rbox(('060C14', 1.0), 14, (IVORY, 0.16), 1.0)))
    clip(t.get('EvoStack'))
    box(native(t, 'NUFlipbookImage', 'EvolutionFigure', 'EvoStack', VIS.HIT_TEST_INVISIBLE), h='center', v='bottom').slot.set_padding(unreal.Margin(0, 0, 0, 6))
    box(label(t, 'EvoQuestion', 'EvoStack', '?', 30, True, (GOLD_HI, 1.0), shadow=False), h='center', v='center')
    names = box(vbox(t, 'EvoNames', 'EvoRow'), pad=(16, 0, 0, 0), v='center')
    box(label(t, 'EvolutionLevelText', 'EvoNames', 'Lv.22 에', 15, False, (MUTED, 1.0), shadow=False))
    box(label(t, 'EvolutionNameText', 'EvoNames', '블레이코', 21, True, shadow=False), pad=(0, 4, 0, 0))
    box(sized(t, 'Col3Spacer', 'Col3', 10, 10), fill=1.0)
    box(sized(t, 'ConfirmSize', 'Col3', None, 56))
    gold_button(t, 'ConfirmSize', '불코이와 함께하기')
    hint_bar(t, 'Root', [(['←', '→'], '고르기'), (['F'], '함께하기'), ([], '정한 뒤에는 바꿀 수 없습니다')], -32)
    t.finish()

    d = cdo(t)
    d.set_editor_property('silhouette_tint', lin('3A4C62'))     # lighter than the tile so the shape reads
    d.set_editor_property('naia_card_class', unreal.load_class(None, S + 'WBP_HUD_NaiaCard.WBP_HUD_NaiaCard_C'))
    d.set_editor_property('element_icons', {e: unreal.load_asset(ICONS + '/T_Element_' + e) for e in ELEMENTS})
    d.set_editor_property('select_first_on_open', True)
    d.set_editor_property('hide_field_hud_while_open', True)
    d.set_editor_property('confirm_dialog_class', unreal.load_class(None, DIALOG))
    t.finish()


import_icons()
for step in (build_chip, build_skill_row, build_job_card, build_job_select, build_naia_card, build_naia_select):
    step()
    log('built ' + step.__name__)
log('DONE')
