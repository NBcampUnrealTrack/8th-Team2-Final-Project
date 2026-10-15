"""Main menu hub, concept A2 (glass edge rail + emblem crest + glass profile card), 2026-10-15.

Concept: ArtSource/UI/HubConcept_20261015/hub_A2_emblem_title.png
Keeps every name the C++ reads (MainMenuTileView, OptionsButton, CharacterSelectButton, QuitGameButton,
HubCloseButton, HubName/HubLevel/HubJob/HubExperience(Bar)/HubLocation/HubGold/HubMottoInput) and the
motion roots (HubLeftFit, HubFit, HubDeskFit, HubDepthForeground). Old art layers are collapsed, not deleted.
Left rail space: 568x899 (height-fit). Profile space: 1672x941 (fit, top/right inset 24).
Safe to re-run.
"""
import importlib, sys
import unreal

sys.path.insert(0, r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard")
import common
importlib.reload(common)
from common import *

HUB = "/Game/Developers/dbals/UI/WBP_GameMenu"
TILE = "/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuTileEntry"
SYSTEM = "/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuSystem"
KIT = "/Game/Art/UI/HUD2026/Kit/"
ART = "/Game/Art/UI/HubA2/"
ICONS = {"CharacterInfo": "Character", "Equipment": "Sword", "Stats": "Stats", "Skills": "Spark", "GrowthTree": "Growth",
         "NaiaRoster": "Naia", "QuestJournal": "Quest", "WorldMap": "Map", "Inventory": "Bag"}
SYSTEM_ICONS = {"OptionsButton": "Gear", "CharacterSelectButton": "Party", "QuitGameButton": "Flee"}
ROW_H = 58
# Label colour follows the button state (Txt_Label uses the foreground). The old light hover brush had a navy
# hover foreground, which vanished on glass: hover/press now turn the label gold.
FOREGROUNDS = (f"NormalForeground=(SpecifiedColor={color(TEXT)}),HoveredForeground=(SpecifiedColor={color(GOLD)}),"
               f"PressedForeground=(SpecifiedColor={color('B89D68')}),DisabledForeground=(SpecifiedColor={color(MUTED, 0.5)})")


def tex(path):
    return unreal.load_asset(path)


def image_brush(path, w, h):
    name = path.rsplit("/", 1)[1]
    return f"(ResourceObject=/Script/Engine.Texture2D'{path}.{name}',DrawAs=Image,ImageSize=(X={w},Y={h}))"


def anchor(b, name, amin, amax, offsets, alignment=(0.0, 0.0)):
    s = b.slot(name)
    a = unreal.Anchors()
    a.minimum, a.maximum = unreal.Vector2D(*amin), unreal.Vector2D(*amax)
    s.set_anchors(a)
    m = unreal.Margin()
    m.left, m.top, m.right, m.bottom = offsets
    s.set_offsets(m)
    s.set_alignment(unreal.Vector2D(*alignment))


def font(b, name, size, hx, bold=False, justify="Left"):
    f = unreal.WidgetFontInfo()
    f.font_family, f.typeface, f.size, f.letter_spacing = (FONT_BOLD if bold else FONT_SEMI), "Default", size, 0
    f.color = color(hx)
    b.ok(WS.set_font(b.path, name, f), f"font {name}")
    b.prop(name, "ColorAndOpacity", f"(SpecifiedColor={color(hx)})")
    b.prop(name, "Justification", justify)


def tile():
    b = Builder(TILE)
    clear = rounded(TEXT, 0.0, 10)
    b.prop("Btn_Common", "WidgetStyle",
           f"(Normal={clear},Hovered={rounded(TEXT, 0.07, 10, TEXT, 0.12, 1.0)},Pressed={rounded(GOLD, 0.14, 10, GOLD, 0.4, 1.0)},"
           f"Disabled={clear},NormalPadding=(Left=0,Top=0,Right=0,Bottom=0),PressedPadding=(Left=0,Top=0,Right=0,Bottom=0),"
           f"{FOREGROUNDS})")
    b.prop("Btn_Common", "RenderTransform", "(Translation=(X=0,Y=0),Scale=(X=1,Y=1),Shear=(X=0,Y=0),Angle=0)")
    # The old atlas rows baked their icons, so this image was hidden (opacity 0, debug tint).
    b.prop("Img_Icon", "Brush", "(ResourceObject=None,TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,ImageSize=(X=28,Y=28))")
    b.prop("Img_Icon", "RenderOpacity", 1)
    b.rect("Img_Icon", 19, 14, 30, 30)
    b.rect("Txt_Label", 62, 4, 200, 34)  # glyphs sit low in the line box; this centres them on the icon
    font(b, "Txt_Label", 18, TEXT)
    # NativePreConstruct re-applies these class defaults over the designer font/colour.
    cdo = unreal.get_default_object(unreal.load_object(None, TILE + ".WBP_GameMenuTileEntry_C"))
    r, g, bb = (common._lin(int(TEXT[i:i + 2], 16)) for i in (0, 2, 4))
    cdo.set_editor_property("label_color", unreal.LinearColor(r, g, bb, 1.0))
    cdo.set_editor_property("label_font_size", 18)
    cdo.set_editor_property("icon_size", unreal.Vector2D(30, 30))
    for n in ("HubStatBar1", "HubStatBar2", "HubStatBar3", "HubStatBaseline"):
        b.vis(n, "Collapsed")  # the stats row now uses the HUD stats icon
    b.compile_save()
    return b.failed


def system():
    b = Builder(SYSTEM)
    b.prop("Img_Icon", "Brush", "(ResourceObject=None,TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=1)),DrawAs=Image,ImageSize=(X=28,Y=28))")
    b.prop("Img_Icon", "RenderOpacity", 1)  # hidden (0) in the old label-only design
    b.rect("Img_Icon", 10, 10, 28, 28)
    style = unreal.find_object(None, f"{SYSTEM}.WBP_GameMenuSystem:WidgetTree.Btn_Common").get_editor_property("widget_style")
    for key, hx in (("hovered_foreground", GOLD), ("pressed_foreground", "B89D68")):
        c = unreal.SlateColor()
        c.specified_color = unreal.LinearColor(*(common._lin(int(hx[i:i + 2], 16)) for i in (0, 2, 4)), 1.0)
        style.set_editor_property(key, c)
    unreal.find_object(None, f"{SYSTEM}.WBP_GameMenuSystem:WidgetTree.Btn_Common").set_editor_property("widget_style", style)
    b.compile_save()
    return b.failed


def hub():
    b = Builder(HUB)
    left, right = "HubLeftCanvas", "HubCanvas"

    # Old art layers (backplates, desk, shades, foreground leaves, text title) step aside.
    for n in ("HubBackplate", "HubTopRightFade", "HubProfileContactShadow", "HubProfileMovingShade", "HubBannerProject",
              "HubJobLabel", "HubCharacterIdLabel", "HubCharacterId", "HubDeskFit", "HubDepthForeground"):
        if b.exists(n):
            b.vis(n, "Collapsed")

    # Left: glass rail with emblem crest, nine rows and an icon-only system row.
    b.prop("HubLeftBackplate", "Brush", rounded(GLASS, 0.92, 16, TEXT, 0.10, 1.0))  # opens over the bright field: denser than menu glass
    b.rect("HubLeftBackplate", 32, 28, 312, 842)
    b.vis("HubLeftBackplate", "HitTestInvisible")
    b.image("HubEmblem", left, 135, 32, 106, 106, image_brush(ART + "T_HubA2_Crest", 106, 106), z=2)
    b.image("HubLogotype", left, 68, 148, 240, 60, image_brush(ART + "T_HubA2_Wordmark", 240, 60), z=2)
    b.image("HubRailRule", left, 56, 238, 264, 1, rounded(TEXT, 0.12, 0), z=2)
    b.rect("MainMenuTileView", 52, 250, 272, ROW_H * 9)
    tv = unreal.find_object(None, f"{HUB}.WBP_GameMenu:WidgetTree.MainMenuTileView")
    tv.set_editor_property("entry_width", 272.0)
    tv.set_editor_property("entry_height", float(ROW_H))
    b.image("HubRailFooterRule", left, 56, 784, 264, 1, rounded(TEXT, 0.12, 0), z=2)
    for i, name in enumerate(("OptionsButton", "CharacterSelectButton", "QuitGameButton")):
        b.rect(name, 104 + i * 64, 800, 48, 48)
        w = unreal.find_object(None, f"{HUB}.WBP_GameMenu:WidgetTree.{name}")
        w.set_editor_property("icon_texture", tex(KIT + "T_HUD_Icon_" + SYSTEM_ICONS[name]))
        w.set_editor_property("icon_size", unreal.Vector2D(28, 28))
        w.set_editor_property("icon_only", True)

    # Entry icons from the HUD2026 kit (same set as the field HUD dock).
    cdo = unreal.get_default_object(unreal.load_object(None, HUB + ".WBP_GameMenu_C"))
    cdo.set_editor_property("menu_entry_icons", {k: tex(KIT + "T_HUD_Icon_" + v) for k, v in ICONS.items()})

    # Right: glass profile card (1672x941 space).
    x0, y0, cw, ch = 1136, 36, 502, 404
    b.image("HubProfileCard", right, x0, y0, cw, ch, rounded(CARD, 0.92, 16, TEXT, 0.11, 1.0), z=1)
    bx = x0 + 40
    b.rect("HubCloseButton", x0 + cw - 62, y0 + 18, 44, 44)
    b.slot("HubCloseButton").set_z_order(5)
    b.prop("HubCloseButton", "WidgetStyle",
           f"(Normal={rounded(GLASS, 0.85, 10, TEXT, 0.16, 1.0)},Hovered={rounded(TEXT, 0.10, 10, TEXT, 0.30, 1.0)},"
           f"Pressed={rounded(GOLD, 0.16, 10, GOLD, 0.5, 1.0)},Disabled={rounded(GLASS, 0.5, 10)},"
           "NormalPadding=(Left=0,Top=0,Right=0,Bottom=0),PressedPadding=(Left=0,Top=0,Right=0,Bottom=0))")
    placements = (
        ("HubName", bx, y0 + 46, 340, 48, 34, TEXT, True, "Left"),
        ("HubLevel", bx, y0 + 104, 76, 30, 15, GOLD, True, "Center"),
        ("HubJob", bx + 92, y0 + 104, 220, 30, 17, TEXT, False, "Left"),
        ("HubExpLabel", bx, y0 + 158, 100, 24, 15, TEXT, True, "Left"),
        ("HubExperience", bx + 262, y0 + 158, 160, 24, 15, TEXT, False, "Right"),
        ("HubLocationLabel", bx, y0 + 240, 160, 28, 15, MUTED, False, "Left"),
        ("HubLocation", bx + 182, y0 + 240, 240, 28, 17, TEXT, True, "Right"),
        ("HubGoldLabel", bx, y0 + 288, 160, 28, 15, MUTED, False, "Left"),
        ("HubGold", bx + 182, y0 + 288, 240, 28, 17, TEXT, False, "Right"),
    )
    for name, x, y, w, h, size, hx, bold, just in placements:
        if not b.exists(name):
            b.add("TextBlock", name, right)
        b.rect(name, x, y, w, h)
        b.slot(name).set_z_order(3)
        font(b, name, size, hx, bold, just)
        b.vis(name, "HitTestInvisible")
    b.prop("HubExpLabel", "Text", "EXP")
    b.prop("HubLocationLabel", "Text", "현재 위치")
    b.prop("HubGoldLabel", "Text", "소지 골드")
    b.image("HubLevelBadge", right, bx, y0 + 104, 76, 30, rounded(GOLD, 0.0, 8, GOLD, 0.9, 1.5), z=2)
    b.bar("HubExperienceBar", right, bx, y0 + 190, 422, 8, GOLD, z=3)
    for i, y in enumerate((y0 + 222, y0 + 276, y0 + 324)):
        b.image(f"HubProfileRule{i}", right, bx, y, 422, 1, rounded(TEXT, 0.11 if i == 0 else 0.08, 0), z=2)
    b.rect("HubMottoInput", bx, y0 + 340, 422, 36)
    b.slot("HubMottoInput").set_z_order(3)
    # Greeting input: no box until focused; muted ivory text like the concept's last line.
    motto = unreal.find_object(None, f"{HUB}.WBP_GameMenu:WidgetTree.HubMottoInput")
    st = motto.get_editor_property("widget_style")
    for key, a in (("background_image_normal", 0.0), ("background_image_hovered", 0.05), ("background_image_focused", 0.08),
                   ("background_image_read_only", 0.0)):
        brush = unreal.SlateBrush()
        brush.draw_as = unreal.SlateBrushDrawType.ROUNDED_BOX
        tint = unreal.SlateColor()
        r, g, bb = (common._lin(int(TEXT[i:i + 2], 16)) for i in (0, 2, 4))
        tint.specified_color = unreal.LinearColor(r, g, bb, a)
        brush.tint_color = tint
        st.set_editor_property(key, brush)
    mr, mg, mb = (common._lin(int(MUTED[i:i + 2], 16)) for i in (0, 2, 4))
    fg = unreal.SlateColor()
    fg.specified_color = unreal.LinearColor(mr, mg, mb, 1.0)
    st.set_editor_property("foreground_color", fg)
    text_style = st.get_editor_property("text_style")
    text_style.set_editor_property("color_and_opacity", fg)
    st.set_editor_property("text_style", text_style)
    motto.set_editor_property("widget_style", st)
    if b.exists("HubCloseX"):
        font(b, "HubCloseX", 20, TEXT, False, "Center")

    # Esc hint, bottom-right, same as the character menu.
    if not b.exists("HubKeyHintRow"):
        b.add("HorizontalBox", "HubKeyHintRow", "RootCanvas")
        b.add("SizeBox", "HubKeyHintEscSize", "HubKeyHintRow")
        b.add("Overlay", "HubKeyHintEscKey", "HubKeyHintEscSize")
        b.add("Image", "HubKeyHintEscCap", "HubKeyHintEscKey")
        b.add("TextBlock", "HubKeyHintEscText", "HubKeyHintEscKey")
        b.add("TextBlock", "HubKeyHintEscLabel", "HubKeyHintRow")
    anchor(b, "HubKeyHintRow", (1, 1), (1, 1), (-40, -44, 0, 0), (1.0, 1.0))
    b.slot("HubKeyHintRow").set_auto_size(True)
    b.slot("HubKeyHintRow").set_z_order(20)
    b.vis("HubKeyHintRow", "HitTestInvisible")
    for k, v in (("WidthOverride", 52), ("HeightOverride", 32), ("bOverride_WidthOverride", True), ("bOverride_HeightOverride", True)):
        b.prop("HubKeyHintEscSize", k, v)
    b.slot("HubKeyHintEscSize").set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    b.prop("HubKeyHintEscCap", "Brush", rounded(GLASS, 0.9, 6, TEXT, 0.35, 1.0))
    b.image("HubKeyHintBack", "RootCanvas", 0, 0, 0, 0, rounded(GLASS, 0.82, 10, TEXT, 0.10, 1.0))
    anchor(b, "HubKeyHintBack", (1, 1), (1, 1), (-28, -34, 136, 52), (1.0, 1.0))
    b.slot("HubKeyHintBack").set_z_order(19)
    cap = b.slot("HubKeyHintEscCap")
    cap.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    cap.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    b.prop("HubKeyHintEscText", "Text", "Esc")
    font(b, "HubKeyHintEscText", 14, TEXT, True, "Center")
    t = b.slot("HubKeyHintEscText")
    t.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    t.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    b.prop("HubKeyHintEscLabel", "Text", "닫기")
    font(b, "HubKeyHintEscLabel", 15, MUTED)
    lbl = b.slot("HubKeyHintEscLabel")
    lbl.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    pad = unreal.Margin()
    pad.left = 10
    lbl.set_padding(pad)

    b.compile_save()
    return b.failed


def run():
    failed = tile() + system() + hub()
    open(r"D:/UnrealEngine/ProjectM/Saved/UIHubA2/apply_report.txt", "w", encoding="utf-8").write(f"failed: {failed!r}\n")
    print("failed:", failed)


run()
