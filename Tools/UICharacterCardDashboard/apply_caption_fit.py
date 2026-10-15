"""Pull the gold card captions inside their glass cards (run after apply_pages.py).

The old ribbon titles were authored straddling the card's top edge: the caption box sits at
y 0..40 of the panel while the card surface starts at y 17..19. With the ribbons gone, the
captions hang over the border. Instead of moving content rows, each card surface is grown
upward to y = TOP so it wraps its caption; captions become left-aligned like the character tab.
Idempotent: surfaces already at TOP are left unchanged.
"""
import importlib, sys
import unreal

sys.path.insert(0, r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard")
import common
importlib.reload(common)
from common import *

ROOT = "/Game/Developers/MinkiCho/UI/CharacterMenu/"
TOP = -2
CARDS = {
    "WBP_CharacterStatsPage": [("PrimaryStatsPanelSurface", "PrimaryStatsPanelRibbonText"),
                               ("CombatStatsPanelSurface", "CombatStatsPanelRibbonText"),
                               ("SurvivalStatsPanelSurface", "SurvivalStatsPanelRibbonText"),
                               ("EquipmentBonusPanelSurface", "EquipmentBonusPanelRibbonText")],
    "WBP_CharacterTraitsPage": [("GraphCardSurface", "GraphJobRibbonText")],
    "WBP_CharacterNaiaPage": [("PartyCardSurface", "PartyRibbonText"), ("SkillsCardSurface", "SkillsRibbonText"),
                              ("StatsCardSurface", "StatsRibbonText"), ("StorageCardSurface", "StorageRibbonText")],
}


def run():
    failed, changed = [], []
    for page, pairs in CARDS.items():
        b = Builder(ROOT + page)
        for surface, caption in pairs:
            s = b.slot(surface)
            o = s.get_offsets()
            if round(o.top) != TOP:
                grow = o.top - TOP
                m = unreal.Margin()
                m.left, m.top, m.right, m.bottom = o.left, TOP, o.right, o.bottom + grow
                s.set_offsets(m)
                changed.append(f"{page}:{surface} top {o.top:.0f}->{TOP} h {o.bottom:.0f}->{o.bottom + grow:.0f}")
            b.prop(caption, "Justification", "Left")
            cs = b.slot(caption)
            cs.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
            cs.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        if page == "WBP_CharacterNaiaPage":
            # "나이아 구성" is a section heading outside any card: ivory like page text (gold = card captions only).
            for name, hx in (("FormationRibbonText", TEXT), ("FormationHelp", MUTED)):
                b.prop(name, "ColorAndOpacity", f"(SpecifiedColor={color(hx)})")
                f = WS.get_font(b.path, name)
                f.color = color(hx)
                b.ok(WS.set_font(b.path, name, f), f"font {name}")
        b.compile_save()
        failed += b.failed
    report = f"changed: {changed}\nfailed: {failed}\n"
    open(r"D:/UnrealEngine/ProjectM/Saved/UICharacterCardDashboard/caption_fit_report.txt", "w", encoding="utf-8").write(report)
    print(report)


run()
