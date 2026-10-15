# -*- coding: utf-8 -*-
"""Battle result window and chat speech bubbles, option A of Docs/HUD_ResultBubble_Proposal_20261015 (user's pick).
Run headless through run.ps1 -Steps result (rebuilt from nothing every time); run.ps1 -Steps apply points the game at them.

  Battle/WBP_HUD_BattleResult   child of UNUBattleResultWidget (same C++ as dongh WBP_BattleResult --- dismiss / stage teardown unchanged)
                                "승리" over the stage + "레벨 업!" above my avatar, a glass settlement sheet on the right:
                                rewards (experience / gold pictures, numbers count up) -> party (faces, level, bar filling,
                                level-up chip) -> spoils (item pictures, caught Naia). First key skips the reveal, the next one closes.
  Battle/WBP_HUD_ResultMember   child of UNUBattleResultMemberWidget: one party row
  Battle/WBP_HUD_ResultEntry    child of UNUBattleResultEntryWidget: one spoil slot
  Field/WBP_HUD_SpeechBubble(_Party, _World)   children of UNUNoticeBannerWidget: navy glass like the name plate, ivory text,
                                a channel chip for party (sky) / world (gold); the C++ splits the lines evenly (sentence end first)

The old widgets stay where they are (apply / revert switch the classes). Layout in 1080p units.
"""
import importlib
import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/HUDRenewal'))
import hud_kit
importlib.reload(hud_kit)
from hud_kit import *            # noqa: F401,F403

ICONS = BASE + '/Icons'
ELEMENTS = ['Fire', 'Water', 'Grass', 'Earth', 'Lightning', 'Wind']
SKY = '7EBEEC'
SHEET_W, SHEET_H = 632, 760
SHEET_FILL = ('0E1A28', 0.86)
OLD_BUBBLE = '/Game/Developers/dbals/UI/WBP_SpeechBubble'


def import_kit_part(name):
    """One new kit texture (the rest of the kit is already imported --- re-importing all of it rewrites every .uasset)."""
    task = unreal.AssetImportTask()
    task.filename = os.path.join(KIT_SRC, name + '.png')
    task.destination_path = KIT; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    TOOLS.import_asset_tasks([task])
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
    log('kit part ' + name)


def wbp(folder, name, parent):
    return widget_blueprint(folder + '/' + name, parent if parent.startswith('/') else '/Script/ProjectM.' + parent)


def icon(name, size, tint=('FFFFFF', 1.0)):
    asset = unreal.load_asset(ICONS + '/' + name)
    assert asset, name + ' missing: run.ps1 -Steps select first'
    brush = unreal.SlateBrush()
    brush.set_editor_property('resource_object', asset)
    brush.image_size = vec2(size, size)
    brush.tint_color = slate(*tint)
    return brush


def native(tree, kind, name, parent, hit=None):
    """A ProjectM C++ widget class (NUFlipbookImage) --- tried by short name, then by script path."""
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


def key_chip(tree, name, parent, label_text, size=26):
    cap = sized(tree, name, parent, size, size)
    face = box(overlay(tree, name + 'Face', name, VIS.HIT_TEST_INVISIBLE))
    box(image(tree, name + 'Rim', name + 'Face', rbox(('0E1A28', 1.0), 7)))
    box(image(tree, name + 'Fill', name + 'Face', rbox((IVORY, 1.0), 5.5)), pad=(2, 2, 2, 2))
    box(text(tree, name + 'Text', name + 'Face', label_text, size * 0.52, True, (NAVY, 1.0)), h='center', v='center')
    return cap


def gold_pill(tree, name, parent, caption, px=13, pad=(10, 1, 10, 2), text_name=None):
    """A gold chip with navy lettering (level up, new companion). Returns the TextBlock."""
    chip = tree.add('Border', name, parent, VIS.HIT_TEST_INVISIBLE)
    chip.set_brush(rbox(('D6BE8C', 1.0), None))      # no outline: a ROUNDED_BOX outline ignores the reveal's fade
    chip.set_padding(unreal.Margin(*pad))
    chip.set_horizontal_alignment(HA['center']); chip.set_vertical_alignment(VA['center'])
    return text(tree, text_name or name + 'Text', name, caption, px, True, (NAVY, 1.0))


# ======================================================================= battle result
def outline(tree, name, parent, picture, colour, pad=(0, 0, 0, 0)):
    """A 9-slice outline picture filling its slot (ROUNDED_BOX outlines don't fade with the widget in UE 5.8)."""
    return box(image(tree, name, parent, tex(picture, None, colour)), pad=pad)


def figure_canvas(tree, name, parent, figure, bottom):
    """A canvas the flipbook can overflow in (an overlay / size box would squeeze the 256 px frame into the slot);
    the figure's bottom centre sits `bottom` px above the slot's bottom. The parent clips."""
    box(tree.add('CanvasPanel', name, parent, VIS.HIT_TEST_INVISIBLE))
    canvas(native(tree, 'NUFlipbookImage', figure, name, VIS.HIT_TEST_INVISIBLE), (0.5, 1, 0.5, 1), (0, -bottom, 0, 0), (0.5, 1), auto=True)


def build_member():
    t = wbp(BATTLE, 'WBP_HUD_ResultMember', 'NUBattleResultMemberWidget')
    v = vbox(t, 'MemberRoot', '')
    row = box(hbox(t, 'RowBox', 'MemberRoot'), pad=(0, 9, 0, 9))
    # face: my character (layered portrait, top-aligned 2x) or the Naia's preview flipbook (1x, feet near the bottom)
    face = box(sized(t, 'Face', 'RowBox', 52, 52), v='center')
    overlay(t, 'FaceStack', 'Face', VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'FaceBg', 'FaceStack', rbox(('26394E', 1.0), None)))
    clipbox = box(clip(sized(t, 'FaceClip', 'FaceStack', 52, 52)))
    stack = overlay(t, 'FaceClipStack', 'FaceClip', VIS.HIT_TEST_INVISIBLE)
    box(overlay(t, 'PortraitBox', 'FaceClipStack', VIS.HIT_TEST_INVISIBLE), pad=(-34, -44, -34, -104))
    figure_canvas(t, 'NaiaCanvas', 'FaceClipStack', 'NaiaFigure', 3)
    box(image(t, 'FaceMask', 'FaceStack', tex('T_HUD_PortraitMask72', (52, 52), ('FFFFFF', 1.0))))      # the mask is drawn navy #0E1926, ~the colour the sheet glass ends up (#101B26 measured)
    box(image(t, 'FaceRing', 'FaceStack', tex('T_HUD_CircleLine52', (52, 52), (GOLD, 0.95))))

    info = box(vbox(t, 'Info', 'RowBox'), pad=(16, 0, 0, 0), v='center', fill=1.0)
    top = box(hbox(t, 'Top', 'Info'))
    box(label(t, 'NameText', 'Top', '리아', 19, True, shadow=False), v='center')
    box(label(t, 'LevelPrefix', 'Top', 'Lv.', 14.5, False, (GOLD_HI, 1.0), shadow=False), pad=(12, 2, 1, 0), v='center')
    box(label(t, 'LevelText', 'Top', '6', 15, True, (GOLD_HI, 1.0), shadow=False), pad=(0, 1, 0, 0), v='center')
    box(sized(t, 'TopSpacer', 'Top', 8, 8), fill=1.0)
    box(image(t, 'ElementImage', 'Top', icon('T_Element_Grass', 28)), pad=(0, 0, 2, 0), v='center')
    lv = box(gold_pill(t, 'LevelUpRoot', 'Top', '레벨 업  5 → 6', text_name='LevelUpText').get_parent(), v='center')
    exp = box(vbox(t, 'ExperienceRoot', 'Info'), pad=(0, 8, 0, 0))
    hold = box(sized(t, 'BarSize', 'ExperienceRoot', None, 7))
    box(bar(t, 'ExperienceBar', 'BarSize', 'T_HUD_BarGold', True, 7))
    tail = box(hbox(t, 'RemainRow', 'ExperienceRoot'), pad=(0, 5, 0, 0), h='right')
    box(label(t, 'RemainCaption', 'RemainRow', '다음 레벨까지 ', 12.5, False, (MUTED, 1.0), shadow=False), v='center')
    box(label(t, 'RemainingExperienceText', 'RemainRow', '412', 12.5, False, (MUTED, 1.0), shadow=False), v='center')
    box(image(t, 'Divider', 'MemberRoot', flat(IVORY, 0.08, (8, 1))))
    t.finish()
    d = cdo(t)
    d.set_editor_property('portrait_pixel_scale', 2.0)
    d.set_editor_property('naia_pixel_scale', 1.0)       # a Naia is ~46 px tall in its 128 px frame: fits the 52 px face at 1x
    d.set_editor_property('element_icons', {e: unreal.load_asset(ICONS + '/T_Element_' + e) for e in ELEMENTS})
    d.set_editor_property('bar_fill_seconds', 0.7)
    t.finish()


def build_entry():
    t = wbp(BATTLE, 'WBP_HUD_ResultEntry', 'NUBattleResultEntryWidget')
    cell = t.add('SizeBox', 'Cell', '', VIS.SELF_HIT_TEST_INVISIBLE)
    cell.set_width_override(98)
    col = vbox(t, 'CellColumn', 'Cell')
    tile = box(sized(t, 'Tile', 'CellColumn', 76, 76), pad=(0, 10, 0, 0), h='center')
    overlay(t, 'TileStack', 'Tile', VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'TileBack', 'TileStack', rbox(('060C14', 1.0), 14)))
    outline(t, 'TileLine', 'TileStack', 'T_HUD_Line14', (IVORY, 0.16))
    box(image(t, 'TileGlow', 'TileStack', tex('T_HUD_Glow', (64, 64), (IVORY, 0.20))), h='center', v='center')
    box(image(t, 'IconImage', 'TileStack', icon('T_Reward_Gold', 60)), pad=(0, 0, 0, 2), h='center', v='center')
    clipped = box(clip(sized(t, 'FigureClip', 'TileStack', 76, 76)))
    figure_canvas(t, 'FigureCanvas', 'FigureClip', 'NaiaFigure', 6)
    # caught Naia: gold frame + "새 동료" on the top edge
    new = box(overlay(t, 'NewNaiaRoot', 'TileStack', VIS.HIT_TEST_INVISIBLE), pad=(0, -10, 0, 0))
    outline(t, 'NewFrame', 'NewNaiaRoot', 'T_HUD_Line14', (GOLD, 1.0), pad=(0, 10, 0, 0))
    box(gold_pill(t, 'NewTag', 'NewNaiaRoot', '새 동료', 11.5, (8, 0, 8, 1)).get_parent(), h='center', v='top')
    badge = box(overlay(t, 'CountRoot', 'TileStack', VIS.HIT_TEST_INVISIBLE), pad=(0, 0, 4, 4), h='right', v='bottom')
    box(image(t, 'CountBg', 'CountRoot', rbox(('0E1A28', 0.96), 8)))
    outline(t, 'CountLine', 'CountRoot', 'T_HUD_Line8', (IVORY, 0.28))
    box(text(t, 'CountText', 'CountRoot', '3', 13, True, (IVORY, 1.0)), pad=(6, 0, 6, 1), h='center', v='center')
    name = box(label(t, 'NameText', 'CellColumn', '가죽 재료', 13.5, False, ('E7E2D9', 1.0), shadow=False), pad=(0, 8, 0, 0), h='center')
    name.set_editor_property('justification', unreal.TextJustify.CENTER)
    name.set_editor_property('text_overflow_policy', unreal.TextOverflowPolicy.ELLIPSIS)
    name.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    t.finish()
    cdo(t).set_editor_property('naia_pixel_scale', 1.0)
    t.finish()


def reward_tile(t, name, parent, picture, caption, value_name):
    tile = box(overlay(t, name, parent, VIS.HIT_TEST_INVISIBLE), fill=1.0)
    box(image(t, name + 'Fill', name, rbox(('060C14', 0.80), 14)))
    outline(t, name + 'Line', name, 'T_HUD_Line14', (IVORY, 0.13))
    row = box(hbox(t, name + 'Row', name), pad=(14, 12, 16, 12))
    box(image(t, name + 'Icon', name + 'Row', icon(picture, 56)), v='center')
    col = box(vbox(t, name + 'Col', name + 'Row'), pad=(14, 0, 0, 0), v='center')
    box(label(t, name + 'Caption', name + 'Col', caption, 14.5, False, (MUTED, 1.0), shadow=False))
    box(label(t, value_name, name + 'Col', '+184', 28, True, shadow=False), pad=(0, 2, 0, 0))
    return tile


def build_result():
    t = wbp(BATTLE, 'WBP_HUD_BattleResult', 'NUBattleResultWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(image(t, 'ScrimRight', 'Root', tex('T_HUD_ScrimR', (64, 64))), (0.45, 0, 1, 1), (0, 0, 0, 0))

    # title over the stage (left of the sheet)
    title = canvas(overlay(t, 'TitleRoot', 'Root', VIS.HIT_TEST_INVISIBLE), (0.3125, 0, 0.3125, 0), (0, 96, 0, 0), (0.5, 0), auto=True)
    box(image(t, 'TitlePool', 'TitleRoot', tex('T_HUD_BannerGlow', (680, 280))), pad=(-160, -60, -160, -60), h='center', v='center')
    tb = box(vbox(t, 'TitleBlock', 'TitleRoot'), h='center', v='center')
    box(label(t, 'TitleCap', 'TitleBlock', '전투 결과', 15, True, (GOLD_HI, 1.0)), h='center')
    rt = box(label(t, 'ResultTitleText', 'TitleBlock', '승리', 64, True), pad=(0, 6, 0, 0), h='center')
    rule = box(hbox(t, 'TitleRule', 'TitleBlock'), pad=(0, 8, 0, 0), h='center')
    box(image(t, 'TitleRuleL', 'TitleRule', tex('T_HUD_RuleL', (168, 2))), v='center')
    box(image(t, 'TitleRuleDiamond', 'TitleRule', tex('T_HUD_Diamond', (11, 11))), pad=(14, 0, 14, 0), v='center')
    box(image(t, 'TitleRuleR', 'TitleRule', tex('T_HUD_RuleR', (168, 2))), v='center')

    # "레벨 업!" above my avatar (the C++ moves it every frame; bottom centre = the avatar's head)
    call = canvas(overlay(t, 'LevelUpCallout', 'Root', VIS.HIT_TEST_INVISIBLE), (0, 0, 0, 0), (600, 700, 0, 0), (0.5, 1), auto=True)
    box(image(t, 'CalloutGlow', 'LevelUpCallout', tex('T_HUD_Glow', (220, 110), ('F0D79D', 0.55))), pad=(-40, -30, -40, -30), h='center', v='center')
    cr = box(hbox(t, 'CalloutRow', 'LevelUpCallout'), h='center', v='center')
    box(image(t, 'CalloutDiamondL', 'CalloutRow', tex('T_HUD_Diamond', (9, 9))), pad=(0, 0, 10, 0), v='center')
    box(label(t, 'CalloutText', 'CalloutRow', '레벨 업!', 25, True, (GOLD_HI, 1.0)), v='center')
    box(image(t, 'CalloutDiamondR', 'CalloutRow', tex('T_HUD_Diamond', (9, 9))), pad=(10, 0, 0, 0), v='center')

    # the settlement sheet
    holder = canvas(sized(t, 'SheetRoot', 'Root', SHEET_W, SHEET_H), (1, 0.5, 1, 0.5), (-88, 16, SHEET_W, SHEET_H), (1, 0.5))
    content = glass(t, 'Sheet', 'SheetRoot', 22, strength=14.0, fill=SHEET_FILL, line=(IVORY, 0.0))
    outline(t, 'SheetLine', content, 'T_HUD_Line22', (IVORY, 0.16))
    body = box(vbox(t, 'SheetBody', content), pad=(32, 30, 32, 24))
    heading(t, 'RewardHead', 'SheetBody', '보상')
    rewards = box(hbox(t, 'Rewards', 'SheetBody'), pad=(0, 14, 0, 0))
    reward_tile(t, 'ExpTile', 'Rewards', 'T_Reward_Exp', '경험치', 'ExperienceText')
    box(sized(t, 'RewardGap', 'Rewards', 16, 8))
    reward_tile(t, 'GoldTile', 'Rewards', 'T_Reward_Gold', '골드', 'GoldText')
    heading(t, 'PartyHead', 'SheetBody', '파티', pad=(0, 26, 0, 0))
    box(hbox(t, 'MemberColumnBox', 'SheetBody'), pad=(0, 6, 0, 0))
    spoils = box(hbox(t, 'Spoils', 'SheetBody'), pad=(0, 20, 0, 0))
    box(vbox(t, 'ItemPanel', 'Spoils'), pad=(0, 0, 18, 0))
    heading(t, 'ItemHead', 'ItemPanel', '획득')
    box(hbox(t, 'ItemBox', 'ItemPanel'), pad=(0, 6, 0, 0))
    box(vbox(t, 'NaiaPanel', 'Spoils'))
    heading(t, 'NaiaHead', 'NaiaPanel', '포획')
    box(hbox(t, 'NaiaBox', 'NaiaPanel'), pad=(0, 6, 0, 0))
    box(sized(t, 'BodySpacer', 'SheetBody', 8, 8), fill=1.0)
    box(image(t, 'FooterRule', 'SheetBody', flat(IVORY, 0.12, (8, 1))), pad=(0, 0, 0, 14))
    foot = box(hbox(t, 'Footer', 'SheetBody'))
    box(label(t, 'FooterHint', 'Footer', '아무 키나 눌러도 넘어갑니다', 13.5, False, (MUTED, 1.0), shadow=False), v='center', fill=1.0)
    btn = box(button(t, 'ContinueButton', 'Footer', nodraw(), rbox((IVORY, 0.07), 10), rbox((IVORY, 0.12), 10)), v='center')
    cr = box(hbox(t, 'ContinueRow', 'ContinueButton'), pad=(8, 4, 8, 4))
    cr.set_visibility(VIS.HIT_TEST_INVISIBLE)
    box(key_chip(t, 'ContinueKey', 'ContinueRow', 'F', 26), v='center')
    box(label(t, 'ContinueText', 'ContinueRow', '계속', 16, True, shadow=False), pad=(10, 0, 0, 0), v='center')
    t.finish()

    d = cdo(t)
    d.set_editor_property('member_widget_class', unreal.load_class(None, BATTLE + '/WBP_HUD_ResultMember.WBP_HUD_ResultMember_C'))
    d.set_editor_property('entry_widget_class', unreal.load_class(None, BATTLE + '/WBP_HUD_ResultEntry.WBP_HUD_ResultEntry_C'))
    d.set_editor_property('members_per_column', 4)
    d.set_editor_property('fill_single_column', True)
    d.set_editor_property('column_padding', unreal.Margin(0, 0, 0, 0))
    d.set_editor_property('member_padding', unreal.Margin(0, 0, 0, 0))
    d.set_editor_property('entry_padding', unreal.Margin(0, 0, 8, 0))
    d.set_editor_property('experience_format', '+{0}')
    d.set_editor_property('gold_format', '+{0}')
    d.set_editor_property('reveal_stagger', 0.08)
    d.set_editor_property('count_up_seconds', 0.9)
    d.set_editor_property('callout_offset', unreal.Vector2D(0, -8))
    t.finish()


# ======================================================================= speech bubbles
BUBBLE_FILL = ('132233', 0.93)


def old_gap():
    """The old bubble's gap under the tail (keeps the bubble where it sat above the name plate)."""
    try:
        bp = unreal.load_asset(OLD_BUBBLE)
        tree = unreal.find_object(bp, 'WidgetTree')
        spacer = unreal.find_object(tree, 'NameplateGap')
        return spacer.get_editor_property('size').y
    except Exception as error:
        log('old gap ? %s' % error)
        return 4.0


def build_bubble(name, chip=None):
    t = wbp(FIELD, name, 'NUNoticeBannerWidget')
    root = vbox(t, 'BubbleRoot', '')
    note = box(overlay(t, 'NoticeRoot', 'BubbleRoot', VIS.HIT_TEST_INVISIBLE), h='center')
    shape = box(vbox(t, 'Shape', 'NoticeRoot'), h='center')
    stack = box(overlay(t, 'BodyStack', 'Shape', VIS.HIT_TEST_INVISIBLE), h='center')
    box(image(t, 'BodyFill', 'BodyStack', rbox(BUBBLE_FILL, 14)))
    outline(t, 'BodyLine', 'BodyStack', 'T_HUD_Line14', (IVORY, 0.24))
    body = box(t.add('Border', 'BubbleBody', 'BodyStack', VIS.HIT_TEST_INVISIBLE))
    body.set_brush(nodraw())
    body.set_padding(unreal.Margin(16, 9, 16, 10))
    row = hbox(t, 'BodyRow', 'BubbleBody')
    if chip:
        caption, colour = chip
        c = box(overlay(t, 'ChannelChip', 'BodyRow', VIS.HIT_TEST_INVISIBLE), pad=(0, 2, 8, 0), v='top')
        box(image(t, 'ChannelFill', 'ChannelChip', rbox((colour, 0.16), 10)))
        outline(t, 'ChannelLine', 'ChannelChip', 'T_HUD_Line10', (colour, 0.80))
        box(text(t, 'ChannelText', 'ChannelChip', caption, 12, True, (colour, 1.0)), pad=(8, 1, 8, 2), h='center', v='center')
    msg = box(label(t, 'MessageText', 'BodyRow', '안녕하세요!', 17, False, (IVORY, 1.0), shadow=False), v='center')
    msg.set_editor_property('auto_wrap_text', True)
    msg.set_editor_property('wrap_text_at', 372.0)       # safety for one long word; the C++ splits the lines itself
    box(image(t, 'Tail', 'Shape', tex('T_HUD_BubbleTail', (16, 8), BUBBLE_FILL)), pad=(0, -1, 0, 0), h='center')
    gap = t.add('Spacer', 'NameplateGap', 'BubbleRoot')
    gap.set_editor_property('size', unreal.Vector2D(1, old_gap()))
    t.finish()
    d = cdo(t)
    d.set_editor_property('lifetime', 5.0)
    d.set_editor_property('balanced_wrap_width', 360.0)
    d.set_editor_property('max_lines', 3)
    d.set_editor_property('enter_offset', unreal.Vector2D(0, 8))
    d.set_editor_property('enter_from_scale', 0.9)
    d.set_editor_property('enter_seconds', 0.22)
    t.finish()


for part in ('T_HUD_BubbleTail', 'T_HUD_Line8', 'T_HUD_Line10', 'T_HUD_Line14', 'T_HUD_Line22', 'T_HUD_CircleLine52'):
    import_kit_part(part)
STEPS = [build_member, build_entry, build_result,
         lambda: build_bubble('WBP_HUD_SpeechBubble'),
         lambda: build_bubble('WBP_HUD_SpeechBubble_Party', ('파티', SKY)),
         lambda: build_bubble('WBP_HUD_SpeechBubble_World', ('전체', GOLD_HI))]
for i, step in enumerate(STEPS):
    step()
    log('built step %d' % i)
log('DONE')
