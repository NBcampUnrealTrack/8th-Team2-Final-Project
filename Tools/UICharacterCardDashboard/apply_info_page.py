"""Rebuild WBP_CharacterInfoPage as the concept-C card dashboard (1200x600 page design space).

Keeps the BindWidget pair CharacterPortrait / PortraitFallback; every other data widget is
found by name in UNUCharacterInfoPage::RefreshPage (all optional).
Run in the editor (PIE stopped):  exec(open(r".../apply_info_page.py", encoding="utf-8").read())
"""
import importlib, sys, os
import unreal

sys.path.insert(0, os.path.dirname(r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard/common.py"))
import common
importlib.reload(common)
from common import *

PATH = "/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterInfoPage"


def run():
    b = Builder(PATH)
    root = "PageCanvas"

    # 1. Drop the old ivory info table and the portrait atmosphere/floor layers.
    for old in ("InfoContent", "PortraitAtmosphere", "PortraitFloorLayer"):
        if b.exists(old):
            WS.remove_component(PATH, old, True)

    # 2. Cards.  Row 1: profile 668 + vitals 516.  Row 2: stats 504 + equipment 312 + Naia 352 (16 gaps).
    profile = b.card("CardProfile", root, 0, 0, 668, 292, "캐릭터 정보")
    vitals = b.card("CardVitals", root, 684, 0, 516, 292, "생명력 · 정신력")
    stats = b.card("CardStats", root, 0, 308, 504, 292, "능력치")
    equip = b.card("CardEquipment", root, 520, 308, 312, 292, "장비")
    naia = b.card("CardNaia", root, 848, 308, 352, 292, "나이아 파트너")

    # 3. Profile: live preview on the left (BindWidget, stays in PageCanvas above the card).
    b.rect("CharacterPortrait", 44, 60, 165, 220, 5)
    b.rect("PortraitFallback", 47, 74, 160, 192, 5)
    b.image("PortraitShadow", profile, 57, 252, 140, 20, rounded("000000", 0.35, 11, half_height=True), z=1)
    b.text("NameText", profile, 248, 58, 396, 52, "모험가", 34, TEXT, bold=True, z=2)
    b.image("LevelBadge", profile, 248, 122, 84, 34, rounded(GOLD, 0.0, 8, GOLD, 0.9, 1.5), z=2)
    b.text("LevelText", profile, 248, 127, 84, 26, "Lv. 1", 15, GOLD, bold=True, justify="Center", z=3)
    b.text("JobText", profile, 346, 126, 300, 28, "노비스", 18, TEXT, z=2)
    b.text("RaceText", profile, 248, 166, 300, 24, "인간", 14, MUTED, z=2)
    b.text("ExperienceLabel", profile, 248, 208, 120, 22, "EXP", 14, TEXT, bold=True, z=2)
    b.text("ExperiencePercentText", profile, 524, 208, 120, 22, "0%", 14, TEXT, justify="Right", z=2)
    b.bar("ExperienceBar", profile, 248, 238, 396, 8, GOLD, z=2)
    b.text("ExperienceText", profile, 248, 254, 396, 22, "EXP 0 / 0", 13, MUTED, z=2)

    # 4. Vitals + current area (real location instead of an invented status line).
    b.text("HealthLabel", vitals, 28, 66, 120, 26, "HP", 16, TEXT, bold=True, z=2)
    b.text("HealthText", vitals, 248, 66, 240, 26, "— / —", 16, TEXT, justify="Right", z=2)
    b.bar("HealthBar", vitals, 28, 100, 460, 10, GREEN, z=2)
    b.text("SPLabel", vitals, 28, 136, 120, 26, "SP", 16, TEXT, bold=True, z=2)
    b.text("SPText", vitals, 248, 136, 240, 26, "— / —", 16, TEXT, justify="Right", z=2)
    b.bar("SPBar", vitals, 28, 170, 460, 10, BLUE, z=2)
    b.image("VitalsRule", vitals, 28, 208, 460, 1, rounded(TEXT, 0.12, 0), z=2)
    b.text("LocationLabel", vitals, 28, 230, 120, 26, "현재 지역", 14, MUTED, z=2)
    b.text("LocationText", vitals, 148, 228, 340, 30, "—", 17, TEXT, bold=True, z=2)

    # 5. Stats: radar (shape cue) + exact list.  Radar centre (138,170), radius 96.
    b.image("StatRadarShape", stats, 42, 74, 192, 192, "(ResourceObject=/Script/Engine.Material'"
            + RADAR_MATERIAL + "',DrawAs=Image,ImageSize=(X=192.000000,Y=192.000000))", z=2)
    for name, label, x, y, just in (
            ("RadarLabelAttack", "공격력", 98, 50, "Center"),
            ("RadarLabelDefense", "방어력", 222, 124, "Left"),
            ("RadarLabelMagic", "마법력", 182, 262, "Left"),
            ("RadarLabelSpeed", "속도", 14, 262, "Right"),
            ("RadarLabelCritical", "치명타", 0, 124, "Right")):
        b.text(name, stats, x, y, 80 if just != "Center" else 80, 20, label, 12, MUTED, justify=just, z=3)
    rows = (("Attack", "공격력"), ("Defense", "방어력"), ("Magic", "마법력"), ("Speed", "속도"), ("Critical", "치명타"))
    for i, (key, label) in enumerate(rows):
        y = 62 + i * 44
        b.text(f"StatLabel{key}", stats, 300, y, 110, 26, label, 15, TEXT, z=2)
        b.text(f"Stat{key}Value", stats, 380, y, 100, 26, "—", 15, TEXT, bold=True, justify="Right", z=2)
        b.image(f"StatRule{key}", stats, 300, y + 34, 180, 1, rounded(TEXT, 0.12, 0), z=2)

    # 6. Equipment rows: label + 48px slot with icon / empty mark.
    for i, (key, label) in enumerate((("Weapon", "무기"), ("Head", "머리"), ("Chest", "몸"), ("Accessory", "장신구"))):
        y = 60 + i * 56
        b.text(f"Equip{key}Label", equip, 28, y + 12, 120, 24, label, 15, TEXT, z=2)
        b.image(f"Equip{key}Slot", equip, 236, y, 48, 48, rounded(GLASS, 0.9, 8, TEXT, 0.14, 1.0), z=2)
        b.image(f"Equip{key}Icon", equip, 240, y + 4, 40, 40,
                "(TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,ImageSize=(X=40.000000,Y=40.000000))", z=3)
        b.text(f"Equip{key}Empty", equip, 236, y + 12, 48, 24, "—", 14, MUTED, justify="Center", z=3)
        if i < 3:
            b.image(f"EquipRule{key}", equip, 28, y + 52, 256, 1, rounded(TEXT, 0.10, 0), z=2)

    # 7. Naia partner.
    b.add("NUFlipbookImage", "NaiaPortrait", naia)
    # Bottom-centre pivot: preview cells keep the creature at the bottom of the cell.
    for k, v in (("Position X", 176), ("Position Y", 164)):
        b.prop("NaiaPortrait", k, v)
    s = b.slot("NaiaPortrait")
    s.set_alignment(unreal.Vector2D(0.5, 1.0))
    s.set_auto_size(True)
    s.set_z_order(2)
    b.vis("NaiaPortrait", "HitTestInvisible")
    b.text("NaiaNameText", naia, 24, 166, 304, 30, "—", 20, TEXT, bold=True, justify="Center", z=2)
    b.text("NaiaBondText", naia, 40, 200, 110, 22, "유대 Lv.0", 13, MUTED, z=2)
    b.bar("NaiaBondBar", naia, 148, 208, 164, 8, GOLD, z=2)
    b.text("NaiaEmptyText", naia, 24, 110, 304, 26, "함께하는 나이아가 없어요", 15, MUTED, justify="Center", z=2)
    b.add("Button", "NaiaViewButton", naia)
    pill = rounded(CREAM, 1.0, 22, half_height=True)
    hover = rounded("EDE5D2", 1.0, 22, half_height=True)
    pressed = rounded("CFC5AE", 1.0, 22, half_height=True)
    b.prop("NaiaViewButton", "WidgetStyle",
           f"(Normal={pill},Hovered={hover},Pressed={pressed},Disabled={rounded(CREAM, 0.4, 22, half_height=True)},"
           "NormalPadding=(Left=0,Top=0,Right=0,Bottom=0),PressedPadding=(Left=0,Top=0,Right=0,Bottom=0))")
    b.rect("NaiaViewButton", 24, 230, 304, 44, 3)
    b.vis("NaiaViewButton", "Visible")
    b.add("CanvasPanel", "NaiaViewContent", "NaiaViewButton")
    b.vis("NaiaViewContent", "HitTestInvisible")
    # Button content slot defaults to (4,2,4,2) padding, which pushed the arrow disc 2 px low.
    b.slot("NaiaViewContent").set_padding(unreal.Margin())
    b.text("NaiaViewLabel", "NaiaViewContent", 0, 9, 304, 26, "나이아 보기", 16, NAVY, bold=True, justify="Center", z=1)
    b.image("NaiaViewArrowBg", "NaiaViewContent", 264, 6, 32, 32, rounded(NAVY, 1.0, 16, half_height=True), z=1)
    b.text("NaiaViewArrow", "NaiaViewContent", 264, 8.5, 32, 26, "→", 15, TEXT, bold=True, justify="Center", z=2)

    b.compile_save()
    print("failed:", b.failed)
    open(r"D:/UnrealEngine/ProjectM/Saved/UICharacterCardDashboard/info_page_report.txt", "w", encoding="utf-8").write("failed: %r\n" % (b.failed,))


run()
