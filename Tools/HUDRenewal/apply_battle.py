# -*- coding: utf-8 -*-
"""Battle HUD widgets of the new in-game HUD (concept A, glass edge). Run headless through run.ps1 -Steps battle.

Same rule as the field widgets: new Widget Blueprints, children of the same C++ classes, nothing of the old look reused.
  WBP_HUD_Battle        turn badge (TURN 01 · 행동 선택) and the action order (names, slides in each action phase)
  WBP_HUD_TurnChip      one name in the action order (blue dot = ally, red dot = enemy; the C++ shows the right one)
  WBP_HUD_AllyTag       above each ally avatar: name, level, HP / SP, status icons, job gauges
  WBP_HUD_FoeTag        above each enemy avatar: name, level, HP, status icons
  WBP_HUD_StatusIcon / WBP_HUD_JobGauge / WBP_HUD_JobIcon   pieces the tags fill in
  WBP_HUD_ActionBanner  the skill name band at the top during an action
  WBP_HUD_ParryCue      the parry / dodge timing bar
The avatar tags are not HUD panels on purpose: server battles feed those avatar widgets directly (SetVitalsDirect ...).
The tag classes are picked by Project Settings NU UI > BattleAllyTagClass / BattleEnemyTagClass (apply step).
"""
import importlib
import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/HUDRenewal'))
import hud_kit
importlib.reload(hud_kit)
from hud_kit import *            # noqa: F401,F403

D = BATTLE + '/'


def wbp(name, parent):
    return widget_blueprint(D + name, '/Script/ProjectM.' + parent)


def dot(tree, name, parent, colour, size=8):
    return image(tree, name, parent, rbox((colour, 1.0), None, None, 0.0, (size, size)))


# ======================================================================= pieces the tags fill in
def build_pieces():
    t = wbp('WBP_HUD_StatusIcon', 'NU_StatusIconWidget')
    o = overlay(t, 'IconRoot', '', VIS.HIT_TEST_INVISIBLE)
    sized(t, 'IconSize', 'IconRoot', 16, 16)
    box(image(t, 'Img_Icon', 'IconSize', icon_brush('spark', 16)))
    badge = box(label(t, 'Txt_Badge', 'IconRoot', '2', 9, True, (IVORY, 1.0)), h='right', v='bottom')
    t.finish()

    t = wbp('WBP_HUD_JobGauge', 'NU_JobResourceGaugeWidget')
    hbox(t, 'GaugeRow', '')
    box(label(t, 'Txt_Name', 'GaugeRow', '자원', 9.5, True, (GOLD, 1.0), shadow=False), pad=(0, 0, 4, 0), v='center')
    s = box(sized(t, 'GaugeBarSize', 'GaugeRow', None, 3), v='center', fill=1.0)
    box(bar(t, 'Bar_Amount', 'GaugeBarSize', 'T_HUD_BarGold', True, 3))
    box(label(t, 'Txt_Value', 'GaugeRow', '2/3', 9.5, True, (GOLD_HI, 1.0), shadow=False), pad=(4, 0, 0, 0), v='center')
    t.finish()

    t = wbp('WBP_HUD_JobIcon', 'NU_JobResourceIconWidget')
    hbox(t, 'JobRow', '')
    box(image(t, 'Img_Icon', 'JobRow', tex('T_HUD_Pip', (11, 11))), v='center')
    box(label(t, 'Txt_Name', 'JobRow', '', 11, True, (GOLD, 1.0), shadow=False), pad=(4, 0, 0, 0), v='center')
    box(label(t, 'Txt_Badge', 'JobRow', '', 11, True, (GOLD_HI, 1.0), shadow=False), pad=(3, 0, 0, 0), v='center')
    t.finish()


def tag_defaults(t):
    d = cdo(t)
    d.set_editor_property('ally_health_color', unreal.LinearColor(1, 1, 1, 1))     # the bar texture carries the colour
    d.set_editor_property('enemy_health_color', unreal.LinearColor(1, 1, 1, 1))
    d.set_editor_property('status_icon_widget_class', unreal.load_class(None, D + 'WBP_HUD_StatusIcon.WBP_HUD_StatusIcon_C'))
    d.set_editor_property('job_resource_gauge_widget_class', unreal.load_class(None, D + 'WBP_HUD_JobGauge.WBP_HUD_JobGauge_C'))
    d.set_editor_property('job_resource_icon_widget_class', unreal.load_class(None, D + 'WBP_HUD_JobIcon.WBP_HUD_JobIcon_C'))
    t.finish()


def tag_back(t, name, parent, pad):
    """A small dark backing behind a tag --- reads over bright sets without a full glass panel."""
    item = t.add('Border', name, parent, VIS.SELF_HIT_TEST_INVISIBLE)
    item.set_brush(rbox(('0B1520', 0.62), 7, ('F1EDE6', 0.10), 1.0))
    item.set_padding(unreal.Margin(*pad))
    return item


def target_mark(t):
    """Gold outline around the tag while this combatant is the chosen target (C++ SetTargeted shows it, pulses its
    opacity and scales the tag up a little). The arrow above is the team's own ANUBattleTargetIndicator, not drawn here."""
    mark = box(image(t, 'TargetMark', 'TagStack', rbox(('F0D79D', 0.10), 9, ('F0D79D', 1.0), 1.5)), pad=(-3, -3, -3, -3))
    mark.set_visibility(VIS.COLLAPSED)

# ======================================================================= avatar tags
# Up to 4 players + 4 naia on the ally side and several foes: tags stay thin (about 124 x 34) so neighbours
# 150 cm apart on the formation axis never touch. Allies show bars + numbers (the C++ hides their identity row);
# foes show name + level + one thin bar. Status icons sit inline, not on an extra row.
def build_ally_tag():
    t = wbp('WBP_HUD_AllyTag', 'NU_CombatantStatusWidget')
    sized(t, 'TagWidth', '', 124, None)
    overlay(t, 'TagStack', 'TagWidth')
    box(tag_back(t, 'TagBack', 'TagStack', (7, 4, 7, 5)))
    target_mark(t)
    col = vbox(t, 'TagColumn', 'TagBack')
    ident = box(hbox(t, 'CombatantIdentity', 'TagColumn'), pad=(0, 0, 0, 2))
    box(label(t, 'CombatantNameText', 'CombatantIdentity', '새벽', 12, True, shadow=False), v='bottom', fill=1.0)
    box(label(t, 'CombatantLevelText', 'CombatantIdentity', 'Lv.12', 10, True, (MUTED, 1.0), shadow=False), pad=(4, 0, 0, 1), v='bottom')
    row = box(hbox(t, 'TagTopRow', 'TagColumn'))
    box(hbox(t, 'Panel_StatusIcons', 'TagTopRow'), v='center')
    box(label(t, 'Txt_StatusOverflow', 'TagTopRow', '+2', 10, True, (MUTED, 1.0), shadow=False), pad=(2, 0, 0, 0), v='center')
    hp = box(label(t, 'HealthText', 'TagTopRow', 'HP  100 / 100', 10.5, True, ('E7E2D9', 1.0), shadow=False), v='center', fill=1.0)
    hp.set_editor_property('justification', unreal.TextJustify.RIGHT)
    box(sized(t, 'HealthBarSize', 'TagColumn', None, 5), pad=(0, 3, 0, 0))
    box(bar(t, 'HealthBar', 'HealthBarSize', 'T_HUD_BarHP', True, 5))
    sp = box(t.add('SizeBox', 'SB_SP', 'TagColumn', VIS.SELF_HIT_TEST_INVISIBLE), pad=(0, 2, 0, 0))
    sp.set_height_override(3)
    box(bar(t, 'SPBar', 'SB_SP', 'T_HUD_BarSP', True, 3))
    box(vbox(t, 'Panel_JobResources', 'TagColumn'), pad=(0, 3, 0, 0))
    box(hbox(t, 'Panel_JobResourceIcons', 'TagColumn'), pad=(0, 2, 0, 0))
    t.finish()
    tag_defaults(t)


def build_foe_tag():
    t = wbp('WBP_HUD_FoeTag', 'NU_CombatantStatusWidget')
    sized(t, 'TagWidth', '', 116, None)
    overlay(t, 'TagStack', 'TagWidth')
    box(tag_back(t, 'TagBack', 'TagStack', (7, 3, 7, 5)))
    target_mark(t)
    col = vbox(t, 'TagColumn', 'TagBack')
    row = box(hbox(t, 'TagTopRow', 'TagColumn'))
    ident = box(hbox(t, 'CombatantIdentity', 'TagTopRow'), v='bottom', fill=1.0)
    box(label(t, 'CombatantNameText', 'CombatantIdentity', '푸루나', 12.5, True, shadow=False), v='bottom')
    box(label(t, 'CombatantLevelText', 'CombatantIdentity', 'Lv.4', 10, True, ('F1C8BD', 1.0), shadow=False), pad=(4, 0, 0, 1), v='bottom')
    box(hbox(t, 'Panel_StatusIcons', 'TagTopRow'), v='center')
    box(label(t, 'Txt_StatusOverflow', 'TagTopRow', '+2', 10, True, (MUTED, 1.0), shadow=False), pad=(2, 0, 0, 0), v='center')
    box(sized(t, 'HealthBarSize', 'TagColumn', None, 4), pad=(0, 3, 0, 0))
    box(bar(t, 'HealthBar', 'HealthBarSize', 'T_HUD_BarFoe', True, 4))
    t.finish()
    tag_defaults(t)


def build_turn_chip():
    t = wbp('WBP_HUD_TurnChip', 'NU_CombatantStatusWidget')
    pad = t.add('Border', 'ChipPad', '', VIS.SELF_HIT_TEST_INVISIBLE)
    pad.set_brush(nodraw()); pad.set_padding(unreal.Margin(0, 0, 6, 0))
    chip = t.add('Border', 'Chip', 'ChipPad', VIS.HIT_TEST_INVISIBLE)
    chip.set_brush(rbox(('0E1A28', 0.82), None, GLASS_LINE, 1.0)); chip.set_padding(unreal.Margin(10, 5, 13, 6))
    row = hbox(t, 'ChipRow', 'Chip')
    ally = box(t.add('SizeBox', 'SB_SP', 'ChipRow', VIS.HIT_TEST_INVISIBLE), pad=(0, 0, 7, 0), v='center')
    dot(t, 'AllyDot', 'SB_SP', PARTY)
    foe = box(hbox(t, 'CombatantIdentity', 'ChipRow'), pad=(0, 0, 7, 0), v='center')
    dot(t, 'FoeDot', 'CombatantIdentity', FOE)
    box(label(t, 'CombatantNameText', 'ChipRow', '새벽', 14, True, shadow=False), v='center')
    t.finish()
    d = cdo(t)
    d.set_editor_property('ally_health_color', unreal.LinearColor(1, 1, 1, 1))
    d.set_editor_property('enemy_health_color', unreal.LinearColor(1, 1, 1, 1))
    t.finish()


# ======================================================================= battle HUD
def build_battle():
    t = wbp('WBP_HUD_Battle', 'NU_BattleHUDWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    canvas(overlay(t, 'PhasePill', 'Root', VIS.HIT_TEST_INVISIBLE), (0.5, 0, 0.5, 0), (0, 30, 0, 0), (0.5, 0), auto=True)
    content = glass(t, 'PhaseGlass', 'PhasePill', 'pill')
    row = box(hbox(t, 'PhaseRow', content), pad=(24, 9, 26, 10))
    box(label(t, 'TurnCap', 'PhaseRow', 'TURN', 13, True, (GOLD, 1.0), 240, shadow=False), v='center')
    box(label(t, 'Txt_Turn', 'PhaseRow', '01', 28, True, shadow=False), pad=(12, 0, 12, 0), v='center')
    box(image(t, 'PhaseDivider', 'PhaseRow', flat(IVORY, 0.25, (1, 24))), pad=(0, 0, 14, 0), v='center')
    box(label(t, 'Txt_Phase', 'PhaseRow', '행동 선택', 18, True, shadow=False), v='center')
    canvas(hbox(t, 'Panel_TurnOrder', 'Root'), (0.5, 0, 0.5, 0), (0, 100, 0, 0), (0.5, 0), auto=True)
    t.finish()
    d = cdo(t)
    d.set_editor_property('turn_order_entry_class', unreal.load_class(None, D + 'WBP_HUD_TurnChip.WBP_HUD_TurnChip_C'))
    t.finish()


def build_action_banner():
    t = wbp('WBP_HUD_ActionBanner', 'NUActionBannerWidget')
    t.add('CanvasPanel', 'Root', '', VIS.HIT_TEST_INVISIBLE)
    canvas(overlay(t, 'BannerRoot', 'Root', VIS.HIT_TEST_INVISIBLE), (0.5, 0, 0.5, 0), (0, 150, 0, 0), (0.5, 0), auto=True)
    content = glass(t, 'BannerGlass', 'BannerRoot', 'pill', strength=10.0)
    row = box(hbox(t, 'BannerRow', content), pad=(10, 8, 28, 9))
    ic = box(overlay(t, 'BannerIcon', 'BannerRow', VIS.HIT_TEST_INVISIBLE), v='center')
    sized(t, 'BannerIconSize', 'BannerIcon', 42, 42)
    box(image(t, 'BannerIconBg', 'BannerIcon', rbox(('D6BE8C', 0.22), None, (GOLD, 0.7), 1.5)))
    box(image(t, 'BannerIconImg', 'BannerIcon', icon_brush('spark', 26)), h='center', v='center')
    box(label(t, 'BannerText', 'BannerRow', '몸통 박치기', 22, True, shadow=False), pad=(14, 0, 0, 0), v='center')
    line = box(image(t, 'BannerUnderline', 'BannerRoot', flat(GOLD, 0.85, (60, 2))), pad=(40, 0, 40, 4), v='bottom')
    t.finish()
    cdo(t).set_editor_property('lifetime', 1.4)
    t.finish()


def build_parry_cue():
    t = wbp('WBP_HUD_ParryCue', 'NUParryCueWidget')
    t.add('CanvasPanel', 'Root', '', VIS.HIT_TEST_INVISIBLE)
    canvas(overlay(t, 'CueRoot', 'Root', VIS.HIT_TEST_INVISIBLE), (0.5, 0.7, 0.5, 0.7), (0, 0, 0, 0), (0.5, 0.5), auto=True)
    content = glass(t, 'CueGlass', 'CueRoot', 16, strength=10.0)
    col = box(vbox(t, 'CueColumn', content), pad=(18, 14, 18, 12))
    bar_size = box(sized(t, 'CueBarSize', 'CueColumn', 300, 12), h='center')
    stack = overlay(t, 'CueBarStack', 'CueBarSize', VIS.HIT_TEST_INVISIBLE)
    box(image(t, 'CueTrack', 'CueBarStack', rbox(('FFFFFF', 0.10), 6)))
    box(image(t, 'CueSuccessGlow', 'CueBarStack', rbox(('E8C46E', 0.30), 6, ('F4D78F', 0.9), 1.5)), pad=(-3, -3, -3, -3)).set_render_opacity(0.0)
    cv = box(plain(t, 'CanvasPanel', 'CueBarCanvas', 'CueBarStack', VIS.HIT_TEST_INVISIBLE))
    zone = canvas(image(t, 'CueZone', 'CueBarCanvas', rbox(('E8C46E', 0.62), 4, ('F4D78F', 1.0), 1.0)), (0, 0, 0, 0), (180, -2, 48, 16))
    needle = canvas(image(t, 'CueNeedle', 'CueBarCanvas', rbox(('FFFFFF', 1.0), 1.5)), (0, 0, 0, 0), (150, -6, 3, 24), (0.5, 0))
    rows = box(hbox(t, 'CueKeys', 'CueColumn'), pad=(0, 10, 0, 0), h='center')
    for key, caption, cost in (('ParryRow', '패링', 'ParryCostText'), ('DodgeRow', '회피', 'DodgeCostText')):
        r = box(hbox(t, key, 'CueKeys'), pad=(6, 0, 6, 0), v='center')
        box(image(t, key + 'Diamond', key, tex('T_HUD_Diamond', (9, 9))), pad=(0, 0, 8, 0), v='center')
        box(label(t, key + 'Text', key, caption, 16, True, shadow=False), v='center')
        box(label(t, cost, key, 'SP 1', 13, True, (PARTY, 1.0), shadow=False), pad=(8, 0, 0, 0), v='center')
    t.finish()


# ======================================================================= command panel
ROOT_ACTIONS = [('Button_Attack', 'sword', '공격'), ('Button_Skill', 'spark', '스킬'), ('Button_Item', 'bag', '아이템'),
                ('Button_NaiaSwap', 'swap', '나이아 교체'), ('Button_Flee', 'flee', '도주')]


def build_command_button():
    t = wbp('WBP_HUD_CommandButton', 'NU_HudButtonWidget')
    sized(t, 'Size', '', None, 40)
    overlay(t, 'Stack', 'Size')
    box(flat_button(t, 'Btn_Common', 'Stack', nodraw(), nodraw(), rbox(('F1EDE6', 0.10), 10)))
    box(image(t, 'HoverMark', 'Stack', rbox(('F1EDE6', 0.07), 10)))
    lift = box(overlay(t, 'HoverLift', 'Stack', VIS.HIT_TEST_INVISIBLE))
    row = box(hbox(t, 'Row', 'HoverLift'), pad=(12, 0, 12, 0))
    box(image(t, 'Img_Icon', 'Row', icon_brush('sword', 26)), pad=(0, 0, 12, 0), v='center')
    box(label(t, 'Txt_Label', 'Row', '공격', 16.5, True, shadow=False), v='center', fill=1.0)
    t.finish()
    d = cdo(t)
    d.set_editor_property('lift_px', 0.0)          # rows don't hop --- the gold highlight slides instead
    t.finish()


def build_command():
    t = wbp('WBP_HUD_BattleCommand', 'NU_BattleCommandWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    # target selection: ground ring at the target's feet and a cursor above its tag (C++ places, pops and slides them)
    ring = canvas(image(t, 'TargetRing', 'Root', tex('T_HUD_TargetRing', (132, 42))), (0, 0, 0, 0), (0, 0, 132, 42), (0.5, 0.5))
    ring.set_visibility(VIS.COLLAPSED)
    cursor = canvas(image(t, 'TargetCursor', 'Root', tex('T_HUD_TargetCursor', (40, 32))), (0, 0, 0, 0), (0, 0, 40, 32), (0.5, 1.0))
    cursor.set_visibility(VIS.COLLAPSED)
    cursor.set_render_transform_pivot(unreal.Vector2D(0.5, 1.0))
    # tether first so the panel draws over its end
    canvas(image(t, 'TetherLine', 'Root', flat('D6BE8C', 0.55, (8, 1))), (0, 0, 0, 0), (0, 0, 30, 1), (0, 0))
    canvas(image(t, 'TetherDot', 'Root', tex('T_HUD_Diamond', (11, 11))), (0, 0, 0, 0), (0, 0, 11, 11), (0.5, 0.5))
    canvas(overlay(t, 'CommandRoot', 'Root'), (0, 0, 0, 0), (0, 0, 0, 0), (0, 0), auto=True)
    w = sized(t, 'CommandWidth', 'CommandRoot', 224, None)
    content = glass(t, 'CommandGlass', 'CommandWidth', 16)
    col = box(vbox(t, 'CommandColumn', content), pad=(8, 8, 8, 8))

    root = box(overlay(t, 'RootPanel', 'CommandColumn'))
    hl = box(overlay(t, 'RootHighlight', 'RootPanel', VIS.HIT_TEST_INVISIBLE), h='fill', v='top')
    box(sized(t, 'RootHighlightSize', 'RootHighlight', None, 40))
    box(image(t, 'RootHighlightFill', 'RootHighlightSize', rbox(('D6BE8C', 0.20), 10, ('F0D79D', 0.35), 1.0)))
    box(image(t, 'RootHighlightBar', 'RootHighlight', rbox(('F0D79D', 1.0), 1.5, None, 0.0, (3, 20))), pad=(0, 10, 0, 10), h='left', v='fill')
    rows = box(vbox(t, 'RootRows', 'RootPanel'))
    for name, icon_name, caption in ROOT_ACTIONS:
        item = box(t.add('WBP_HUD_CommandButton', name, 'RootRows'), pad=(0, 0, 0, 3))
        item.set_editor_property('icon_texture', unreal.load_asset(KIT + '/T_HUD_Icon_' + icon_name.capitalize()))
        item.set_editor_property('icon_size', unreal.Vector2D(26, 26))
        item.set_editor_property('label_text', caption)

    det = box(vbox(t, 'DetailPanel', 'CommandColumn'), pad=(6, 4, 6, 4))
    cap = box(hbox(t, 'DetailCapRow', 'DetailPanel'))
    box(label(t, 'Text_DetailCap', 'DetailCapRow', '스킬', 12, True, (GOLD, 1.0), 100, shadow=False), v='center', fill=1.0)
    box(label(t, 'Text_DetailPos', 'DetailCapRow', '1 / 4', 12, True, (MUTED, 1.0), shadow=False), v='center')
    namerow = box(hbox(t, 'DetailNameRow', 'DetailPanel'), pad=(0, 6, 0, 0))
    box(label(t, 'DetailPrev', 'DetailNameRow', '\u2039', 19, True, (MUTED, 1.0), shadow=False), pad=(0, 0, 8, 0), v='center')
    box(label(t, 'Text_DetailName', 'DetailNameRow', '스킬 이름', 19, True, shadow=False), v='center', fill=1.0)
    box(label(t, 'DetailNext', 'DetailNameRow', '\u203a', 19, True, (MUTED, 1.0), shadow=False), pad=(8, 0, 0, 0), v='center')
    box(label(t, 'Text_DetailBody', 'DetailPanel', '스킬 설명', 13.5, False, ('DFE3E6', 1.0), shadow=False, wrap=True), pad=(0, 6, 0, 0))
    sp = box(hbox(t, 'SPRow', 'DetailPanel'), pad=(0, 10, 0, 0))
    box(label(t, 'SPCap', 'SPRow', 'SP', 12.5, True, (GOLD, 1.0), shadow=False), pad=(0, 0, 8, 0), v='center')
    box(hbox(t, 'Panel_SPPips', 'SPRow'), v='center', fill=1.0)
    box(label(t, 'Text_SPHits', 'SPRow', '1타 · SP 0', 12, True, (PARTY, 1.0), shadow=False), v='center')

    wait = box(vbox(t, 'WaitPanel', 'CommandColumn'), pad=(8, 8, 8, 8))
    box(label(t, 'Text_Wait', 'WaitPanel', '명령을 보내는 중\u2026', 14, True, ('DFE3E6', 1.0), shadow=False, wrap=True))
    box(label(t, 'Text_Notice', 'CommandColumn', '지금은 쓸 수 없는 행동입니다', 12.5, True, ('F39A85', 1.0), shadow=False, wrap=True), pad=(6, 6, 6, 2))
    t.finish()
    d = cdo(t)
    d.set_editor_property('row_pitch', 43.0)
    d.set_editor_property('root_width', 224.0)
    d.set_editor_property('detail_width', 300.0)
    d.set_editor_property('pip_on_brush', tex('T_HUD_Pip', (11, 11)))
    d.set_editor_property('pip_off_brush', tex('T_HUD_PipEmpty', (11, 11)))
    t.finish()


STEPS = [build_pieces, build_ally_tag, build_foe_tag, build_turn_chip, build_battle, build_action_banner, build_parry_cue,
         build_command_button, build_command]
only = os.environ.get('HUD_ONLY', '')
for step in STEPS:
    if only and step.__name__ not in only.split(','):
        continue
    step()
    log('built ' + step.__name__)
log('DONE')
