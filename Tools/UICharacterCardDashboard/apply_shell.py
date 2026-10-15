"""Restyle WBP_CharacterMenuBase to the concept-C glass shell.

Keeps every bound widget (NamedSlot_PageContent, Nav*Button, CloseButton) and the names
UNUCharacterMenuBase::UpdateNavigation looks up (Nav<X>Selected / Icon / Label).
Ornamental ivory/compass/leaf layers are collapsed, not deleted, so the change is reversible.
New optional names read by C++: PageTitleText (selected tab label), HeaderGoldText.
"""
import importlib, sys, os
import unreal

sys.path.insert(0, r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard")
import common
importlib.reload(common)
from common import *

PATH = "/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterMenuBase"
ICONS = "/Game/Art/UI/HUD2026/Kit/"
NAV = (("Character", "T_HUD_Icon_Character"), ("Equipment", "T_HUD_Icon_Sword"), ("Inventory", "T_HUD_Icon_Bag"),
       ("Stats", "T_HUD_Icon_Stats"), ("Traits", "T_HUD_Icon_Growth"), ("Naia", "T_HUD_Icon_Naia"))


def icon_brush(name, size):
    return (f"(ResourceObject=/Script/Engine.Texture2D'{ICONS}{name}.{name}',DrawAs=Image,"
            f"ImageSize=(X={size}.000000,Y={size}.000000))")


def anchor(b, name, amin, amax, offsets, alignment=(0.0, 0.0)):
    s = b.slot(name)
    a = unreal.Anchors()
    a.minimum = unreal.Vector2D(*amin)
    a.maximum = unreal.Vector2D(*amax)
    s.set_anchors(a)
    m = unreal.Margin()
    m.left, m.top, m.right, m.bottom = offsets
    s.set_offsets(m)
    s.set_alignment(unreal.Vector2D(*alignment))


def run():
    b = Builder(PATH)
    shell = "ShellCanvas"

    # 1. Collapse the ivory/navy ornamental shell.
    hide = ["IvorySurface", "NavySurface", "NavyGrain", "RailDivider", "RailDividerHighlight", "RailFooterCompass",
            "RailFooterLeft", "RailFooterRight", "MenuTitleBlock", "BackdropLayers", "AmbientLeavesFar",
            "AmbientLeavesNear", "ShellFrameMotifs", "MajorMenuHeaderHeight"]
    hide += [f"{k}{i}" for k in ("OuterNavyFrame", "GoldFrame", "InnerFrame") for i in range(8)]
    hide += [f"Nav{k}Separator" for k, _ in NAV]
    hide += [f"Close{k}{i}" for k in ("Outer", "Inner") for i in range(4)]
    for n in hide:
        if b.exists(n):
            b.vis(n, "Collapsed")

    # 2. Blurred, dimmed field behind everything.
    b.vis("WorldBlur", "HitTestInvisible")
    b.prop("WorldBlur", "BlurStrength", 7)
    b.vis("WorldTint", "HitTestInvisible")
    b.prop("WorldTint", "Brush", f"(TintColor=(SpecifiedColor={color('060A12', 0.55)}),DrawAs=Image,ImageSize=(X=64.000000,Y=64.000000))")

    # 3. Left glass rail (stretches vertically).
    b.image("RailPanel", shell, 0, 0, 0, 0, rounded(GLASS, 0.82, 16, TEXT, 0.10, 1.0), z=1)
    anchor(b, "RailPanel", (0, 0), (0, 1), (40, 40, 280, 40))
    b.text("RailCaption", shell, 72, 70, 200, 24, "메뉴", 14, GOLD, bold=True, spacing=60, z=2)
    for i, (key, icon) in enumerate(NAV):
        btn = f"Nav{key}Button"
        b.rect(btn, 52, 112 + i * 66, 256, 58)
        b.slot(btn).set_z_order(9)
        sel = f"Nav{key}Selected"
        b.rect(sel, 0, 0, 256, 58)
        b.prop(f"Nav{key}SelectedBody", "Brush", rounded(TEXT, 0.09, 10, TEXT, 0.10, 1.0))
        b.rect(f"Nav{key}SelectedBody", 0, 0, 256, 58)
        for part in ["Tip", "Highlight", "Jewel"] + [f"Frame{f}" for f in range(7)]:
            if b.exists(f"Nav{key}Selected{part}"):
                b.vis(f"Nav{key}Selected{part}", "Collapsed")
        b.image(f"Nav{key}SelectedBar", sel, 0, 15, 4, 28, rounded(GOLD, 1.0, 2), z=3)
        b.rect(f"Nav{key}Foreground", 0, 0, 256, 58)
        b.prop(f"Nav{key}Icon", "Brush", icon_brush(icon, 26))
        b.rect(f"Nav{key}Icon", 24, 16, 26, 26)
        b.rect(f"Align_Nav{key}Label", 66, 0, 180, 58)
        f = unreal.WidgetFontInfo()
        f.font_family, f.typeface, f.size, f.letter_spacing, f.color = FONT_SEMI, "Default", 17, 0, color(MUTED)
        b.ok(WS.set_font(PATH, f"Nav{key}Label", f), f"font Nav{key}Label")
        b.prop(f"Nav{key}Label", "Justification", "Left")
        ls = b.slot(f"Nav{key}Label")
        ls.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        ls.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

    # 4. Page heading, gold and key hint (top/bottom bands of the content column).
    b.text("PageTitleText", shell, 368, 44, 600, 56, "캐릭터", 36, TEXT, bold=True, z=6)
    b.image("HeaderGoldCoin", shell, 0, 0, 0, 0, rounded(GOLD, 1.0, 9, "F4D78F", 0.9, 1.5), z=6)
    anchor(b, "HeaderGoldCoin", (1, 0), (1, 0), (-300, 64, 18, 18))
    b.text("HeaderGoldText", shell, 0, 0, 0, 0, "—", 18, TEXT, bold=True, z=6)
    anchor(b, "HeaderGoldText", (1, 0), (1, 0), (-274, 58, 150, 30))
    # Esc hint row is built by apply_spacing.py (centred key cap + label).

    # 5. Close button: square glass key with a thin ivory cross.
    anchor(b, "CloseButton", (1, 0), (1, 0), (-100, 44, 56, 56))
    b.prop("CloseCanvas", "RenderTransform", "(Translation=(X=0,Y=0),Scale=(X=1,Y=1),Shear=(X=0,Y=0),Angle=0)")
    b.prop("CloseFace", "Brush", rounded(GLASS, 0.85, 12, TEXT, 0.16, 1.0))
    b.prop("CloseFace", "RenderTransform", "(Translation=(X=0,Y=0),Scale=(X=1,Y=1),Shear=(X=0,Y=0),Angle=0)")
    b.rect("CloseFace", 0, 0, 56, 56)
    for cross in ("CloseCrossA", "CloseCrossB"):
        b.prop(cross, "Brush", f"(TintColor=(SpecifiedColor={color(TEXT, 0.95)}),DrawAs=Image,ImageSize=(X=24.000000,Y=2.000000))")
        b.rect(cross, 16, 27, 24, 2)

    # 6a. All pages are glass now: drop the interim ivory backing card if an older pass added it.
    for old in ("PageBackdrop", "PageBackdropArt"):
        if b.exists(old):
            WS.remove_component(PATH, old, False)

    # 6. Content column below the heading, right of the rail.
    anchor(b, "MajorMenuColumn", (0, 0), (1, 1), (368, 120, 40, 104))
    b.prop("PageContentPadding", "Padding", "(Left=0,Top=0,Right=0,Bottom=0)")

    # 7. Navigation colours used by UpdateNavigation (selected ivory, idle muted).
    cls = unreal.load_object(None, PATH + ".WBP_CharacterMenuBase_C")
    cdo = unreal.get_default_object(cls)
    for prop, hx in (("selected_navigation_color", TEXT), ("idle_navigation_color", MUTED)):
        r, g, bb = (common._lin(int(hx[i:i + 2], 16)) for i in (0, 2, 4))
        cdo.set_editor_property(prop, unreal.LinearColor(r, g, bb, 1.0))

    b.compile_save()
    open(r"D:/UnrealEngine/ProjectM/Saved/UICharacterCardDashboard/shell_report.txt", "w", encoding="utf-8").write(
        "failed: %r\n" % (b.failed,))
    print("failed:", b.failed)


run()
