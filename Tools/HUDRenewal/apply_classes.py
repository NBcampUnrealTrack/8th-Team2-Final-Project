# -*- coding: utf-8 -*-
"""Points the game at the new HUD widgets (run.ps1 -Steps apply), or back at the previous ones (run.ps1 -Steps revert).

Only class settings change; the old widgets stay where they are. Touched:
  BP_NUPlayerController (main game player controller) --- the HUD widget classes it creates
  Config/DefaultGame.ini [/Script/ProjectM.NUUISettings] --- location, notice, name plate classes (three lines edited in place)
The values before the first apply are kept in Saved/HUDRenewal/classes_original.json (revert uses them) and the
controller asset is copied to Saved/HUDRenewal/Backup.
"""
import importlib
import json
import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/HUDRenewal'))
import hud_kit
importlib.reload(hud_kit)
from hud_kit import ROOT, FIELD, BATTLE, B, E, log, backup

REVERT = os.environ.get('HUD_REVERT', '') == '1'
STATE = os.path.join(ROOT, 'Saved/HUDRenewal/classes_original.json')
PC = '/Game/Developers/dongh/PlayerController/BP_NUPlayerController'
INI = os.path.join(ROOT, 'Config/DefaultGame.ini')


def cls(folder, name):
    return '%s/%s.%s_C' % (folder, name, name)


# player controller property -> new widget class
PC_CLASSES = {
    'field_hud_widget_class': cls(FIELD, 'WBP_HUD_Field'),
    'chat_widget_class': cls(FIELD, 'WBP_HUD_Chat'),
    'party_prompt_widget_class': cls(FIELD, 'WBP_HUD_PartyPrompt'),
    'party_list_widget_class': cls(FIELD, 'WBP_HUD_PartyList'),
    'interaction_indicator_widget_class': cls(FIELD, 'WBP_HUD_Interact'),
    'quest_widget_class': cls(FIELD, 'WBP_HUD_Quest'),
}
if os.environ.get('HUD_BATTLE', '') == '1' or os.path.exists(os.path.join(ROOT, 'Content/Art/UI/HUD2026/Battle/WBP_HUD_Battle.uasset')):
    PC_CLASSES.update({
        'battle_hud_widget_class': cls(BATTLE, 'WBP_HUD_Battle'),
        'action_banner_widget_class': cls(BATTLE, 'WBP_HUD_ActionBanner'),
        'parry_cue_widget_class': cls(BATTLE, 'WBP_HUD_ParryCue'),
        'battle_command_widget_class': cls(BATTLE, 'WBP_HUD_BattleCommand'),
        'target_indicator_class': '',        # the new command panel draws the target cursor; the old indicator actor would double it
    })
# DefaultGame.ini key -> new class path (section NUUISettings). Keys the section does not have yet are added (and removed on revert)
INI_CLASSES = {
    'WorldLocationWidgetClass': cls(FIELD, 'WBP_HUD_Location'),
    'NoticeBannerClass': cls(FIELD, 'WBP_HUD_Notice'),
    'NameplateClass': cls(FIELD, 'WBP_HUD_Nameplate'),
    # the new location widget shows its own entry banner, so the generic 'entered X' toast would say it twice
    'bEnableLocationEntryNotice': 'False',
}
if 'battle_hud_widget_class' in PC_CLASSES:
    INI_CLASSES.update({'BattleAllyTagClass': cls(BATTLE, 'WBP_HUD_AllyTag'), 'BattleEnemyTagClass': cls(BATTLE, 'WBP_HUD_FoeTag')})
# NPC head quest marker (Docs/HUD_NpcMarker_Proposal_20261014, option A): replaces every NPC BP's WBP_WorldMarker through one setting.
# 160 px canvas drawn 80 cm wide (smooth edges), pivot low so the badge floats a little above the old bubble.
if os.path.exists(os.path.join(ROOT, 'Content/Art/UI/HUD2026/Field/WBP_HUD_HeadMarker.uasset')):
    INI_CLASSES.update({
        'HeadMarkerClass': cls(FIELD, 'WBP_HUD_HeadMarker'),
        'HeadMarkerMaterial': '/Game/Art/UI/HUD2026/Fx/M_HUD_WorldMarker.M_HUD_WorldMarker',
        'HeadMarkerDrawSize': '(X=160,Y=160)',
        'HeadMarkerWorldSize': '80.000000',
        'HeadMarkerPivot': '(X=0.500000,Y=0.820000)',      # the diamond's bottom corner
        'HeadMarkerGap': '10.000000',                       # cm above the NPC sprite's top (marker follows NPC size tuning)
    })


# NPC dialogue, subtitle style (Docs/HUD_Dialogue_Proposal_20261014, option C). The old zxcv1 WBP_DialogueWidget stays as it is.
if os.path.exists(os.path.join(ROOT, 'Content/Art/UI/HUD2026/Field/WBP_HUD_Dialogue.uasset')):
    INI_CLASSES['DialogueWidgetClass'] = cls(FIELD, 'WBP_HUD_Dialogue')


# job / starter Naia selection on a stage (Docs/HUD_Selection_Proposal_20261015, option A). The old zxcv1 WBP_JobSelection
# stays as it is; the starter Naia window had no WBP (the native class built its own grey cards), so its key is added.
SELECT = '/Game/Art/UI/HUD2026/Select'
if os.path.exists(os.path.join(ROOT, 'Content/Art/UI/HUD2026/Select/WBP_HUD_JobSelect.uasset')):
    INI_CLASSES['JobSelectionWidgetClass'] = cls(SELECT, 'WBP_HUD_JobSelect')
if os.path.exists(os.path.join(ROOT, 'Content/Art/UI/HUD2026/Select/WBP_HUD_NaiaSelect.uasset')):
    INI_CLASSES['StarterNaiaSelectionWidgetClass'] = cls(SELECT, 'WBP_HUD_NaiaSelect')


# battle result window + chat speech bubbles (Docs/HUD_ResultBubble_Proposal_20261015, option A). The old dongh WBP_BattleResult
# and dbals WBP_SpeechBubble(_Party) stay as they are; the world-chat bubble key is new (removed again on revert)
if os.path.exists(os.path.join(ROOT, 'Content/Art/UI/HUD2026/Battle/WBP_HUD_BattleResult.uasset')):
    PC_CLASSES['battle_result_widget_class'] = cls(BATTLE, 'WBP_HUD_BattleResult')
if os.path.exists(os.path.join(ROOT, 'Content/Art/UI/HUD2026/Field/WBP_HUD_SpeechBubble.uasset')):
    INI_CLASSES.update({'SpeechBubbleClass': cls(FIELD, 'WBP_HUD_SpeechBubble'),
                        'PartySpeechBubbleClass': cls(FIELD, 'WBP_HUD_SpeechBubble_Party'),
                        'WorldSpeechBubbleClass': cls(FIELD, 'WBP_HUD_SpeechBubble_World')})


# shop: glass side sheet (Docs/HUD_Shop_Proposal_20261014, option B). The old dbals WBP_ShopWindow stays as it is.
SHOP_INI = {}
if os.path.exists(os.path.join(ROOT, 'Content/Art/UI/HUD2026/Field/WBP_HUD_Shop.uasset')):
    SHOP_INI['ShopWindowClass'] = cls(FIELD, 'WBP_HUD_Shop')


def path_of(value):
    return value.get_path_name() if value else ''


def apply_pc(state):
    bp = unreal.load_asset(PC)
    defaults = unreal.get_default_object(bp.generated_class())
    original = state.setdefault('pc', {})
    changed = False
    for prop, new in PC_CLASSES.items():
        current = path_of(defaults.get_editor_property(prop))
        if prop not in original:
            original[prop] = current
        target = original[prop] if REVERT else new
        if current == target:
            continue
        value = unreal.load_class(None, target) if target else None
        assert value or not target, target
        bp.modify()
        changed = True
        defaults.set_editor_property(prop, value)
        log('PC %s: %s -> %s' % (prop, current.rsplit('/', 1)[-1], target.rsplit('/', 1)[-1]))
    if not changed:
        return          # nothing to write --- don't re-save (another editor may hold the asset)
    assert B.compile_blueprint(PC).success, PC
    assert E.save_asset(PC, False), PC


def apply_ini(state, section='[/Script/ProjectM.NUUISettings]', classes=None, key='ini'):
    classes = INI_CLASSES if classes is None else classes
    text = open(INI, encoding='utf-8-sig', newline='').read()      # keep the file's own line endings
    newline = '\r\n' if '\r\n' in text else '\n'
    lines = text.split(newline)
    original = state.setdefault(key, {})
    in_section = False
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith('['):
            in_section = stripped == section
            continue
        if not in_section or '=' not in line:
            continue
        name, value = line.split('=', 1)
        if name in classes:
            if name not in original:
                original[name] = value
            target = original[name] if REVERT else classes[name]
            if value != target:
                lines[i] = name + '=' + target
                log('ini %s: %s -> %s' % (name, value.rsplit('/', 1)[-1], target.rsplit('/', 1)[-1]))
    # keys the section did not have: add them after the section header (original value '' = remove on revert)
    present = {line.split('=', 1)[0] for line in lines if '=' in line}
    header = lines.index(section)
    for name in classes:
        if name in original:
            continue
        original[name] = ''
        if not REVERT:
            lines.insert(header + 1, name + '=' + classes[name])
            log('ini %s: added' % name)
    if REVERT:
        drop = [k for k, v in original.items() if v == '' and k in classes]
        lines = [line for line in lines if not (line.split('=', 1)[0] in drop and '=' in line)]
    updated = newline.join(lines)
    if updated == text:
        return              # nothing to change --- don't touch the file (another tool may hold it open)
    import time
    for attempt in range(10):      # another tool can hold the file for a moment (seen once: OSError 22 on open)
        try:
            with open(INI, 'w', encoding='utf-8', newline='') as f:
                f.write(updated)
            return
        except OSError:
            time.sleep(1.0)
    raise OSError('could not write ' + INI)


state = json.load(open(STATE, encoding='utf-8')) if os.path.exists(STATE) else {}
backup(PC)
if not os.path.exists(os.path.join(ROOT, 'Saved/HUDRenewal/Backup/DefaultGame.ini')):
    import shutil
    shutil.copy2(INI, os.path.join(ROOT, 'Saved/HUDRenewal/Backup/DefaultGame.ini'))
apply_pc(state)
os.makedirs(os.path.dirname(STATE), exist_ok=True)
json.dump(state, open(STATE, 'w', encoding='utf-8'), indent=1)      # keep the PC originals even if the ini step fails
apply_ini(state)
if SHOP_INI:
    apply_ini(state, '[/Script/ProjectM.NUShopSettings]', SHOP_INI, 'ini_shop')
os.makedirs(os.path.dirname(STATE), exist_ok=True)
json.dump(state, open(STATE, 'w', encoding='utf-8'), indent=1)
log('%s DONE' % ('reverted' if REVERT else 'applied'))
log('DONE')
