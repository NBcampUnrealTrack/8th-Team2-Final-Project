"""Spacing pass after review (2026-10-15). Absolute values, safe to re-run.

Shell (WBP_CharacterMenuBase)
  * rail-to-content gap 24 -> 48: content column and page title start at x 368 (rail ends at 320).
  * Esc hint rebuilt as one row: [Esc key cap with centred text][닫기], vertically centred together.
Inventory page (1200x600 page space)
  * area card left edge on the page edge like the other tabs; top/bottom extend 14 for room.
  * tabs/filters inset 24 from the card edge, item grid inset 36, item detail card nested
    with 16 right inset. UniformGrid cells shrink to the narrower area through C++ (DesignSlotMin).
"""
import importlib, sys
import unreal

sys.path.insert(0, r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard")
import common
importlib.reload(common)
from common import *

ROOT = "/Game/Developers/MinkiCho/UI/CharacterMenu/"
GAP_LEFT = 368


def anchor(b, name, amin, amax, offsets, alignment=(0.0, 0.0)):
    s = b.slot(name)
    a = unreal.Anchors()
    a.minimum, a.maximum = unreal.Vector2D(*amin), unreal.Vector2D(*amax)
    s.set_anchors(a)
    m = unreal.Margin()
    m.left, m.top, m.right, m.bottom = offsets
    s.set_offsets(m)
    s.set_alignment(unreal.Vector2D(*alignment))


def shell():
    path = ROOT + "WBP_CharacterMenuBase"
    b = Builder(path)
    anchor(b, "MajorMenuColumn", (0, 0), (1, 1), (GAP_LEFT, 120, 40, 104))
    b.rect("PageTitleText", GAP_LEFT, 44, 600, 56)

    # Esc hint: HorizontalBox anchored bottom-right; key cap = SizeBox(52x32) > Overlay > [Image, Text].
    for old in ("KeyHintEscCap", "KeyHintEscText", "KeyHintEscLabel"):
        if b.exists(old) and b.slot(old).get_class().get_name() == "CanvasPanelSlot":
            WS.remove_component(path, old, False)
    if not b.exists("KeyHintRow"):
        b.add("HorizontalBox", "KeyHintRow", "ShellCanvas")
        b.add("SizeBox", "KeyHintEscSize", "KeyHintRow")
        b.add("Overlay", "KeyHintEscKey", "KeyHintEscSize")
        b.add("Image", "KeyHintEscCap", "KeyHintEscKey")
        b.add("TextBlock", "KeyHintEscText", "KeyHintEscKey")
        b.add("TextBlock", "KeyHintEscLabel", "KeyHintRow")
    anchor(b, "KeyHintRow", (1, 1), (1, 1), (-40, -44, 0, 0), (1.0, 1.0))
    b.slot("KeyHintRow").set_auto_size(True)
    b.slot("KeyHintRow").set_z_order(6)
    b.vis("KeyHintRow", "HitTestInvisible")
    b.prop("KeyHintEscSize", "WidthOverride", 52)
    b.prop("KeyHintEscSize", "HeightOverride", 32)
    b.prop("KeyHintEscSize", "bOverride_WidthOverride", True)
    b.prop("KeyHintEscSize", "bOverride_HeightOverride", True)
    b.slot("KeyHintEscSize").set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    b.prop("KeyHintEscCap", "Brush", rounded(TEXT, 0.06, 6, TEXT, 0.35, 1.0))
    cap = b.slot("KeyHintEscCap")
    cap.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    cap.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    for name, value, hx, bold in (("KeyHintEscText", "Esc", TEXT, True), ("KeyHintEscLabel", "닫기", MUTED, False)):
        b.prop(name, "Text", value)
        f = unreal.WidgetFontInfo()
        f.font_family, f.typeface, f.size, f.letter_spacing = (FONT_BOLD if bold else FONT_SEMI), "Default", 14 if bold else 15, 0
        f.color = color(hx)
        b.ok(WS.set_font(path, name, f), f"font {name}")
        b.prop(name, "ColorAndOpacity", f"(SpecifiedColor={color(hx)})")
        b.prop(name, "Justification", "Center" if bold else "Left")
    t = b.slot("KeyHintEscText")
    t.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    t.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    lbl = b.slot("KeyHintEscLabel")
    lbl.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    pad = unreal.Margin()
    pad.left = 10
    lbl.set_padding(pad)
    b.compile_save()
    return b.failed


def inventory():
    """Card fills the page (0..1200 x 0..600; the page clips anything outside it).
    Inset 20-24 for tabs/filters, 36 for the grid; the detail card's lower block moves up 22
    so the nested card ends 14 above the outer card's bottom edge."""
    b = Builder(ROOT + "WBP_CharacterInventoryPage")
    b.rect("InventoryAreaCard", 0, 0, 1200, 600)
    b.slot("InventoryAreaCard").set_z_order(-1)
    for i, key in enumerate(("CategoryEquipment", "CategoryConsumable", "CategoryMaterial", "CategoryOther")):
        b.rect(f"{key}Button", 24 + 206 * i, 20, 196, 40)
    for i, key in enumerate(("FilterAll", "FilterWeapon", "FilterArmor", "FilterAccessory")):
        b.rect(f"{key}Button", 24 + 130 * i, 74, 120, 38)
    b.rect("Align_InventoryCount", 968, 20, 208, 40)
    b.rect("SortButton", 720, 74, 296, 38)
    b.rect("SortOptions", 720, 116, 296, 76)
    b.rect("ViewLargeButton", 1056, 74, 56, 38)
    b.rect("ViewCompactButton", 1120, 74, 56, 38)
    b.rect("InventoryArea", 36, 124, 708, 462)
    b.rect("InventoryScroll", 0, 0, 708, 462)
    b.rect("ItemDetailArea", 760, 124, 424, 462)
    b.rect("ItemDetailCardSurface", 1, 1, 422, 460)
    b.rect("SelectedItemDescription", 24, 146, 376, 52)
    for name, top in (("Align_ItemStatsHeading", 204), ("ItemDescriptionRule", 232),
                      ("Align_ItemAttackLabel", 234), ("Align_ItemAttackValue", 234), ("ItemAttackRule", 262),
                      ("Align_ItemStrengthLabel", 262), ("Align_ItemStrengthValue", 262), ("ItemStrengthRule", 290),
                      ("Align_ItemCriticalLabel", 290), ("Align_ItemCriticalValue", 290), ("ItemCriticalRule", 318),
                      ("Align_ItemSkillLabel", 318), ("Align_ItemSkillValue", 318),
                      ("EquipActionButton", 356), ("DismantleButton", 414), ("LockButton", 414)):
        b.prop(name, "Position Y", top)
    b.compile_save()
    return b.failed


def run():
    failed = shell() + inventory()
    print("failed:", failed)


run()
