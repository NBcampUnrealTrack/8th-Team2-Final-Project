# -*- coding: utf-8 -*-
"""HUD QA capture helper: the CURRENT battle result window (dongh WBP_BattleResult) with sample data, over the battle.
There is no win cheat and ShowResult is not callable from Python, so this fills the window's bound widgets by hand
the same way UNUBattleResultWidget::ShowResult / SetMember / SetEntry do. Runs inside pie_capture.py (GAME_WORLD, PC, log)."""
import unreal

CLS = '/Game/Developers/dongh/UI/Battle/WBP_BattleResult.WBP_BattleResult_C'
MEMBER = '/Game/Developers/dongh/UI/Battle/WBP_BattleResultMember.WBP_BattleResultMember_C'
ENTRY = '/Game/Developers/dongh/UI/Battle/WBP_BattleResultEntry.WBP_BattleResultEntry_C'
lib = unreal.get_default_object(unreal.WidgetLibrary)


def make(path):
    return lib.call_method('Create', (GAME_WORLD, unreal.load_class(None, path), PC))


def prop(w, name):
    try:
        return w.get_editor_property(name)
    except Exception:
        return None


w = make(CLS)
w.add_to_viewport(60)
prop(w, 'ExperienceText').set_text('184')
prop(w, 'GoldText').set_text('62')

# members: me (level up) + three party Naia (no experience: empty bar, remaining hidden)
box = prop(w, 'MemberColumnBox')
box.clear_children()
col = None
rows = (('리아', 6, 0.18, 412, True, True), ('푸루나', 5, 0, 0, False, False), ('볼또', 4, 0, 0, False, False), ('아쿠퐁', 4, 0, 0, False, False))
for i, (name, lv, ratio, remain, has_exp, up) in enumerate(rows):
    if col is None or i % 3 == 0:
        col = unreal.new_object(unreal.VerticalBox, outer=w)
        slot = box.add_child(col)
        slot.set_padding(unreal.Margin(0, 0, 12, 0))
    m = make(MEMBER)
    prop(m, 'NameText').set_text(name)
    prop(m, 'LevelText').set_text(str(lv))
    prop(m, 'ExperienceBar').set_percent(ratio)
    r = prop(m, 'RemainingExperienceText')
    if r:
        r.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE if has_exp else unreal.SlateVisibility.HIDDEN)
        r.set_text(str(remain))
    u = prop(m, 'LevelUpRoot')
    if u:
        u.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE if up else unreal.SlateVisibility.COLLAPSED)
    s = col.add_child(m)
    s.set_padding(unreal.Margin(0, 0, 0, 8))


def fill(box_name, panel_name, entries):
    panel, ebox = prop(w, panel_name), prop(w, box_name)
    if panel:
        panel.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    if not ebox:
        return
    ebox.clear_children()
    for name, count, icon in entries:
        e = make(ENTRY)
        prop(e, 'NameText').set_text(name)
        prop(e, 'CountText').set_text(str(count))
        img = prop(e, 'IconImage')
        tex = unreal.load_asset(icon) if icon else None
        if img and tex:
            img.set_brush_from_texture(tex, True)
        elif img:
            img.set_visibility(unreal.SlateVisibility.COLLAPSED)
        ebox.add_child(e)


I = '/Game/Developers/dbals/UI/ItemIcons/'
fill('ItemBox', 'ItemPanel', (('가죽 재료', 3, I + 'T_Item_M_HIDE_01'), ('하급 체력 포션', 1, I + 'T_Item_C_VIT_01'),
                             ('갱목 가공 재료', 2, I + 'T_Item_M_WOOD_01')))
# Naia definitions have no IconTexture yet (UNUNaiaRosterWindowWidget notes it), so the real window hides the icon too
fill('NaiaBox', 'NaiaPanel', (('푸루나', 1, None),))
# the battle is still in command selection here (no win cheat); at the real result the command menu is gone
for o in unreal.ObjectIterator(unreal.UserWidget):
    try:
        if o.get_world() == GAME_WORLD and 'BattleCommand' in o.get_class().get_name() and o.is_in_viewport():
            o.set_visibility(unreal.SlateVisibility.HIDDEN)
            log('hid ' + o.get_class().get_name())
    except Exception:
        pass
w.set_keyboard_focus()
log('qa_result_now: shown')
