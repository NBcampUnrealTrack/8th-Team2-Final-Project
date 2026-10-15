"""Small layout fixes after review (2026-10-15). Absolute values, safe to re-run.

* Equipment: item-detail "주요 능력치" heading 10 px higher (y 292 -> 282).
* Inventory: the area card spans the whole page width so category/filter tabs, sort and
  view buttons all sit inside one glass card; the item detail card reads as nested in it.
* Naia: the selected-Naia name ("선택 없음" when empty) is centred in the name strip
  (strip x 2..318, y 268..324 -> centre 160, 296); level stays at the right on the same line.
"""
import importlib, sys
import unreal

sys.path.insert(0, r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard")
import common
importlib.reload(common)
from common import *

ROOT = "/Game/Developers/MinkiCho/UI/CharacterMenu/"


def run():
    failed = []

    b = Builder(ROOT + "WBP_CharacterEquipmentPage")
    b.rect("Align_ItemStatsHeading", 24, 282, 376, 32)
    b.compile_save(); failed += b.failed

    b = Builder(ROOT + "WBP_CharacterInventoryPage")
    b.rect("InventoryAreaCard", -14, -14, 1228, 628)
    b.slot("InventoryAreaCard").set_z_order(-1)
    b.compile_save(); failed += b.failed

    b = Builder(ROOT + "WBP_CharacterNaiaPage")
    b.rect("Align_SelectedNaiaName", 72, 280, 176, 32)
    b.prop("SelectedNaiaName", "Justification", "Center")
    ns = b.slot("SelectedNaiaName")
    ns.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    ns.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    b.rect("Align_SelectedNaiaLevel", 250, 280, 56, 32)
    b.prop("SelectedNaiaLevel", "Justification", "Right")
    ls = b.slot("SelectedNaiaLevel")
    ls.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    b.compile_save(); failed += b.failed

    print("failed:", failed)


run()
