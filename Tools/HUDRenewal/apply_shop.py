# -*- coding: utf-8 -*-
"""Shop window in the HUD's glass tone: the side sheet of Docs/HUD_Shop_Proposal_20261014 (option B, user's pick).
Run headless through run.ps1 -Steps shop (rebuilt from nothing every time); run.ps1 -Steps apply points the game at it.

  WBP_HUD_Shop            child of UNUShopWindowWidget (same C++ as dbals WBP_ShopWindow --- buy/sell logic unchanged)
  WBP_HUD_ShopEntry       child of UNUShopEntryWidget (one row: icon, name, owned / description, price or level lock)
  WBP_HUD_ShopRowButton   child of UNU_CommonButtonWidget (the row's invisible full-size button --- the row selects its item)

A navy glass sheet on the right (40 px in, like the HUD corners), the scene and the merchant stay visible on the left.
Title + purse + close, buy / sell tabs (gold underline), the list, and the chosen item's panel: icon, name, owned / stock,
description, quantity (- / + / max, arrows), total and a gold confirm button (F). Selling happens in the same sheet,
so the old inventory window no longer opens with the shop (bOpenInventoryWithShop false); the field HUD folds and the
character stays put while it is open, like a conversation.
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

D = FIELD + '/'
NEW_KIT = ('T_HUD_Coin', 'T_HUD_Close')
SHEET_W = 640
EDGE = 40
ROW_H = 64
LIST_ROWS = 6


def wbp(name, parent):
    return widget_blueprint(D + name, parent if parent.startswith('/') else '/Script/ProjectM.' + parent)


def import_new_kit():
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
            'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE,
            'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI,
            'never_stream': True,
            'srgb': True,
            'filter': unreal.TextureFilter.TF_DEFAULT,
        })
        assert E.save_asset(KIT + '/' + name, False)
    log('shop kit: %d' % len(NEW_KIT))


def key_chip(tree, name, parent, label_text, size=26):
    cap = sized(tree, name, parent, size, size)
    face = box(overlay(tree, name + 'Face', name, VIS.HIT_TEST_INVISIBLE))
    box(image(tree, name + 'Rim', name + 'Face', rbox(('0E1A28', 1.0), 7)))
    box(image(tree, name + 'Fill', name + 'Face', rbox((IVORY, 1.0), 5.5)), pad=(2, 2, 2, 2))
    box(text(tree, name + 'Text', name + 'Face', label_text, size * 0.52, True, (NAVY, 1.0)), h='center', v='center')
    return cap


def coin_text(tree, name, parent, value, px, colour=(IVORY, 1.0), coin=18, text_name=None):
    row = hbox(tree, name, parent)
    box(image(tree, name + 'Coin', name, tex('T_HUD_Coin', (coin, coin))), pad=(0, 0, 7, 0), v='center')
    t = label(tree, text_name or (name + 'Text'), name, value, px, True, colour)
    box(t, v='center')
    return row


# ======================================================================= the row's button (a common button, so the row's C++ can bind it)
def build_row_button():
    t = wbp('WBP_HUD_ShopRowButton', 'NU_CommonButtonWidget')
    overlay(t, 'RowButtonRoot', '')
    hover = rbox((IVORY, 0.06), 12)
    pressed = rbox((IVORY, 0.10), 12)
    box(button(t, 'Btn_Common', 'RowButtonRoot', nodraw(), hover, pressed, nodraw()))
    t.finish()


# ======================================================================= one row
def build_entry():
    t = wbp('WBP_HUD_ShopEntry', 'NUShopEntryWidget')
    root = t.add('SizeBox', 'RowSize', '')
    root.set_height_override(ROW_H)
    overlay(t, 'Row', 'RowSize')
    box(image(t, 'RowBack', 'Row', rbox(('0E1A28', 0.62), 12, (IVORY, 0.10), 1.0)))
    hi = box(overlay(t, 'SelectedHighlight', 'Row', VIS.HIT_TEST_INVISIBLE))
    box(image(t, 'SelectedFill', 'SelectedHighlight', rbox(('16293D', 0.95), 12, (GOLD, 0.78), 1.4)))
    box(image(t, 'SelectedBar', 'SelectedHighlight', rbox((GOLD_HI, 1.0), 2, None, 0.0, (4, 16))), pad=(1, 12, 0, 12), h='left')
    hi.set_visibility(VIS.COLLAPSED)

    content = box(hbox(t, 'RowContent', 'Row'), pad=(12, 8, 16, 8))
    content.set_visibility(VIS.HIT_TEST_INVISIBLE)
    frame = box(sized(t, 'IconFrame', 'RowContent', 48, 48), v='center')
    overlay(t, 'IconStack', 'IconFrame')
    box(image(t, 'IconBack', 'IconStack', rbox(('060C14', 1.0), 10)))
    box(image(t, 'IconImage', 'IconStack', tex('T_HUD_Coin', (44, 44))), h='center', v='center')
    col = box(vbox(t, 'TextColumn', 'RowContent'), pad=(14, 0, 8, 0), v='center', fill=1.0)
    box(label(t, 'NameText', 'TextColumn', '하급 체력 포션', 19, True, shadow=False))
    sub = box(label(t, 'SubText', 'TextColumn', '보유 5', 14, False, (MUTED, 1.0), shadow=False), pad=(0, 2, 0, 0))
    right = box(sized(t, 'RightSize', 'RowContent', 128, None), v='center')
    overlay(t, 'RightStack', 'RightSize')
    price = box(coin_text(t, 'PriceRow', 'RightStack', '40', 18, text_name='PriceText'), h='right', v='center')
    lock = box(t.add('Border', 'LockedBox', 'RightStack', VIS.HIT_TEST_INVISIBLE), h='right', v='center')
    lock.set_brush(rbox(('1E2C3C', 1.0), 13, (GOLD, 0.75), 1.2))
    lock.set_padding(unreal.Margin(12, 3, 12, 4))
    lock.set_horizontal_alignment(HA['center'])
    text(t, 'LockText', 'LockedBox', 'Lv. 10 이상', 13.5, True, (GOLD_HI, 1.0))
    lock.set_visibility(VIS.COLLAPSED)

    # the row is the button: buy rows show BuyButton, sell rows SellButton (C++ swaps them); both fill the row
    for name in ('BuyButton', 'SellButton'):
        box(t.add('WBP_HUD_ShopRowButton', name, 'Row'))
    t.finish()
    d = cdo(t)
    d.set_editor_property('price_number_only', True)
    t.finish()


# ======================================================================= the sheet
def tab(tree, name, indicator, caption):
    cell = box(sized(tree, name + 'Size', 'TabsRow', 150, 40), pad=(0, 0, 6, 0))
    btn = box(button(tree, name, name + 'Size', rbox((IVORY, 0.05), 12, (IVORY, 0.16), 1.0),
                     rbox((IVORY, 0.10), 12, (IVORY, 0.26), 1.0), rbox((IVORY, 0.14), 12, (IVORY, 0.30), 1.0)))
    stack = box(overlay(tree, name + 'Stack', name, VIS.HIT_TEST_INVISIBLE))
    ind = box(overlay(tree, indicator, name + 'Stack', VIS.HIT_TEST_INVISIBLE))
    box(image(tree, indicator + 'Fill', indicator, rbox(('D6BE8C', 0.16), 10)), pad=(-6, -4, -6, -4))
    box(image(tree, indicator + 'Line', indicator, rbox((GOLD_HI, 1.0), 1.5, None, 0.0, (40, 3))), pad=(14, 0, 14, -3), v='bottom')
    box(text(tree, name + 'Label', name + 'Stack', caption, 17, True), h='center', v='center')


def stepper_button(tree, name, parent, caption, w=36):
    cell = box(sized(tree, name + 'Size', parent, w, 34), v='center')
    btn = box(button(tree, name, name + 'Size', rbox((IVORY, 0.07), 9, (IVORY, 0.18), 1.0),
                     rbox((IVORY, 0.14), 9, (IVORY, 0.30), 1.0), rbox((IVORY, 0.20), 9, (GOLD, 0.6), 1.0),
                     rbox((IVORY, 0.03), 9, (IVORY, 0.08), 1.0)))
    box(text(tree, name + 'Label', name, caption, 17 if len(caption) == 1 else 13.5, True), h='center', v='center')


def build_shop():
    t = wbp('WBP_HUD_Shop', 'NUShopWindowWidget')
    t.add('CanvasPanel', 'Root', '', VIS.SELF_HIT_TEST_INVISIBLE)
    sheet = canvas(overlay(t, 'Sheet', 'Root'), (1, 0, 1, 0), (-EDGE, EDGE, 0, 0), (1, 0), auto=True)
    box(sized(t, 'SheetSize', 'Sheet', SHEET_W, None))
    content = glass(t, 'SheetGlass', 'SheetSize', 18, fill=('0E1A28', 0.90), line=(IVORY, 0.18))
    body = box(vbox(t, 'SheetBody', content), pad=(24, 22, 24, 24))

    # header: diamond + shop name, purse, close
    head = box(hbox(t, 'HeaderRow', 'SheetBody'))
    box(image(t, 'HeaderDiamond', 'HeaderRow', tex('T_HUD_Diamond', (10, 10))), pad=(2, 0, 10, 0), v='center')
    title = box(label(t, 'TitleText', 'HeaderRow', '잡화 좌판', 24, True, (GOLD_HI, 1.0)), v='center', fill=1.0)
    box(coin_text(t, 'PurseRow', 'HeaderRow', '10,000', 21, text_name='GoldText'), pad=(0, 0, 16, 0), v='center')
    close = box(sized(t, 'CloseSize', 'HeaderRow', 34, 34), v='center')
    btn = box(button(t, 'CloseButton', 'CloseSize', rbox((IVORY, 0.06), 17, (IVORY, 0.30), 1.0),
                     rbox((IVORY, 0.14), 17, (IVORY, 0.45), 1.0), rbox((IVORY, 0.20), 17, (GOLD, 0.6), 1.0)))
    box(image(t, 'CloseGlyph', 'CloseButton', tex('T_HUD_Close', (16, 16))), h='center', v='center')

    # tabs
    box(hbox(t, 'TabsRow', 'SheetBody'), pad=(0, 18, 0, 14))
    tab(t, 'BuyTabButton', 'BuyTabIndicator', '구매')
    tab(t, 'SellTabButton', 'SellTabIndicator', '판매')

    # list (ListBox is found by name)
    holder = box(sized(t, 'ListHolder', 'SheetBody'))
    holder.set_max_desired_height(LIST_ROWS * (ROW_H + 8))
    scroll = t.add('ScrollBox', 'ListScroll', 'ListHolder')
    scroll.set_editor_property('scrollbar_thickness', unreal.Vector2D(3, 3))
    scroll.set_editor_property('scroll_bar_visibility', VIS.COLLAPSED)
    lst = vbox(t, 'ListBox', 'ListScroll')
    lst.set_visibility(VIS.SELF_HIT_TEST_INVISIBLE)

    # the chosen item
    box(image(t, 'DetailRule', 'SheetBody', flat(IVORY, 0.16, (32, 1))), pad=(0, 16, 0, 16))
    det = box(vbox(t, 'DetailPanel', 'SheetBody'))
    top = box(hbox(t, 'DetailTop', 'DetailPanel'))
    frame = box(sized(t, 'DetailIconFrame', 'DetailTop', 64, 64), v='center')
    overlay(t, 'DetailIconStack', 'DetailIconFrame')
    box(image(t, 'DetailIconBack', 'DetailIconStack', rbox(('060C14', 1.0), 12)))
    box(image(t, 'DetailIcon', 'DetailIconStack', tex('T_HUD_Coin', (58, 58))), h='center', v='center')
    names = box(vbox(t, 'DetailNames', 'DetailTop'), pad=(16, 0, 0, 0), v='center', fill=1.0)
    box(label(t, 'DetailNameText', 'DetailNames', '하급 체력 포션', 21, True, shadow=False))
    box(label(t, 'DetailInfoText', 'DetailNames', '보유 5 · 재고 20', 14.5, False, (MUTED, 1.0), shadow=False), pad=(0, 4, 0, 0))
    desc = box(label(t, 'DetailDescText', 'DetailPanel', '단일 대상 HP 100 회복', 15.5, False, ('CDD5DB', 1.0), shadow=False, wrap=True),
               pad=(0, 12, 0, 0))
    q = box(hbox(t, 'QuantityRow', 'DetailPanel'), pad=(0, 16, 0, 0))
    box(label(t, 'QuantityCaption', 'QuantityRow', '수량', 16, True, (MUTED, 1.0), shadow=False), pad=(0, 0, 14, 0), v='center')
    stepper_button(t, 'QuantityMinusButton', 'QuantityRow', '-')
    qsize = box(sized(t, 'QuantitySize', 'QuantityRow', 48, None), v='center')
    box(label(t, 'QuantityText', 'QuantitySize', '3', 19, True, shadow=False), h='center', v='center')
    stepper_button(t, 'QuantityPlusButton', 'QuantityRow', '+')
    box(sized(t, 'QuantityGap', 'QuantityRow', 8, 8))
    stepper_button(t, 'QuantityMaxButton', 'QuantityRow', '최대', 52)
    box(sized(t, 'QuantitySpacer', 'QuantityRow', 8, 8), fill=1.0)
    box(coin_text(t, 'TotalRow', 'QuantityRow', '120', 24, coin=20, text_name='TotalText'), v='center')

    cell = box(sized(t, 'ConfirmSize', 'DetailPanel', None, 56), pad=(0, 18, 0, 0))
    gold_n = rbox(('D6BE8C', 1.0), 12, ('96763A', 1.0), 1.2)
    gold_h = rbox(('E6CF9E', 1.0), 12, ('96763A', 1.0), 1.2)
    gold_p = rbox(('C2A874', 1.0), 12, ('7A5F2C', 1.0), 1.2)
    gold_d = rbox(('6E6A62', 0.55), 12, (IVORY, 0.12), 1.0)
    box(button(t, 'ConfirmButton', 'ConfirmSize', gold_n, gold_h, gold_p, gold_d))
    cr = box(hbox(t, 'ConfirmRow', 'ConfirmButton'), h='center', v='center')
    cr.set_visibility(VIS.HIT_TEST_INVISIBLE)
    box(text(t, 'ConfirmLabel', 'ConfirmRow', '구매하기', 20, True, (NAVY, 1.0)), v='center')
    box(key_chip(t, 'ConfirmKey', 'ConfirmRow', 'F'), pad=(12, 0, 0, 0), v='center')

    box(label(t, 'KeyHints', 'SheetBody', '↑↓ 고르기   ←→ 수량   F 구매·판매   Esc 닫기', 13.5, False, (MUTED, 1.0), shadow=False),
        pad=(0, 14, 0, 0), h='center')
    t.finish()

    old = unreal.get_default_object(unreal.load_asset('/Game/Developers/dbals/UI/WBP_ShopWindow').generated_class())
    d = cdo(t)
    d.set_editor_property('entry_widget_class', unreal.load_class(None, D + 'WBP_HUD_ShopEntry.WBP_HUD_ShopEntry_C'))
    for prop in ('confirm_dialog_class',):
        try:
            d.set_editor_property(prop, old.get_editor_property(prop))
        except Exception as e:      # the base has no such default in this version --- fine
            log('kept default %s (%s)' % (prop, e))
    d.set_editor_property('open_inventory_with_shop', False)
    d.set_editor_property('hide_field_hud_while_open', True)
    d.set_editor_property('block_game_input_while_open', True)
    t.finish()


import_new_kit()
for step in (build_row_button, build_entry, build_shop):
    step()
    log('built ' + step.__name__)
log('DONE')
