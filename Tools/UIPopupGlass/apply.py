"""Common popups, concept A "glass card" without key hints (2026-10-15).

Concept: ArtSource/UI/PopupConcept_20261015/popup_A_glass_card.png
Targets: dbals/UI/WBP_ConfirmDialog, dbals/UI/WBP_CountDialog, zxcv1/Quest/UI/WBP_RewardPopup.
Keeps every C++ contract name (MessageText, ConfirmButton, CancelButton, ConfirmLabelText, CancelLabelText,
CountTextBox, PriceText, TitleText, EntryText, CloseButton). New optional contracts: IntentIconDestructive/Positive,
DecreaseButton/IncreaseButton/MaxButton, RewardRows + Reward*Template.
Private assets under /Game/Art/UI/PopupGlass (icons, button subclasses) so shared buttons stay untouched.
Run with PIE stopped. Safe to re-run.
"""
import importlib, sys
import unreal

sys.path.insert(0, r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard")
import common
importlib.reload(common)
from common import *

ART = "/Game/Art/UI/PopupGlass"
ICON_SRC = r"D:/UnrealEngine/ProjectM/ArtSource/UI/PopupConcept_20261015/icons/"
CONFIRM = "/Game/Developers/dbals/UI/WBP_ConfirmDialog"
COUNT = "/Game/Developers/dbals/UI/WBP_CountDialog"
REWARD = "/Game/Developers/zxcv1/Quest/UI/WBP_RewardPopup"
PALETTE = "/Game/Developers/MinkiCho/Art/UITheme/CommonPopupPalette/"
CREST = "/Game/Art/UI/HubA2/T_HubA2_Crest.T_HubA2_Crest"
RED, RED_TEXT = "C46A5E", "E8A79C"
W_CARD = 520
PAD = 28
BTN_W, BTN_H = 150, 48
ZERO_PAD = "NormalPadding=(Left=0,Top=0,Right=0,Bottom=0),PressedPadding=(Left=0,Top=0,Right=0,Bottom=0)"


def pill(hx, a, outline=None, oa=0.0, ow=0.0):
    return rounded(hx, a, BTN_H / 2, outline, oa, ow, half_height=True)


def fg(hx):
    return f"(SpecifiedColor={color(hx)})"


PRIMARY = (f"(Normal={pill(CREAM, 1.0)},Hovered={pill('EFE7D5', 1.0)},Pressed={pill('C9BFA8', 1.0)},"
           f"Disabled={pill(CREAM, 0.35)},{ZERO_PAD},NormalForeground={fg(NAVY)},HoveredForeground={fg(NAVY)},"
           f"PressedForeground={fg(NAVY)})")
GHOST = (f"(Normal={pill(TEXT, 0.0, TEXT, 0.55, 1.2)},Hovered={pill(TEXT, 0.08, TEXT, 0.85, 1.2)},"
         f"Pressed={pill(TEXT, 0.14, TEXT, 0.85, 1.2)},Disabled={pill(TEXT, 0.0, TEXT, 0.2, 1.2)},{ZERO_PAD},"
         f"NormalForeground={fg(TEXT)},HoveredForeground={fg(TEXT)},PressedForeground={fg(TEXT)})")
DESTRUCTIVE = (f"(Normal={pill('2A1A20', 0.9, RED, 0.95, 1.5)},Hovered={pill('3A2228', 0.95, 'D9806F', 1.0, 1.5)},"
               f"Pressed={pill('22151A', 0.95, RED, 1.0, 1.5)},Disabled={pill('2A1A20', 0.4, RED, 0.3, 1.5)},{ZERO_PAD})")
SQUARE = (f"(Normal={rounded(TEXT, 0.05, 10, TEXT, 0.5, 1.2)},Hovered={rounded(TEXT, 0.12, 10, TEXT, 0.85, 1.2)},"
          f"Pressed={rounded(TEXT, 0.16, 10, TEXT, 0.7, 1.0)},Disabled={rounded(TEXT, 0.0, 10, TEXT, 0.15, 1.0)},{ZERO_PAD})")
CARD_BRUSH = rounded(GLASS, 0.94, 14, TEXT, 0.12, 1.0)
DIM = "(ResourceObject=None,TintColor=(SpecifiedColor=(R=0,G=0,B=0,A=0.55)),DrawAs=Image)"
# 2px: a 1px rule vanishes once the popup is DPI-scaled below 1.0.
RULE = f"(ResourceObject=None,TintColor=(SpecifiedColor={color(GOLD, 0.40)}),DrawAs=Image,ImageSize=(X=2,Y=2))"
# The reward rows copy this brush; its ImageSize is the divider thickness.
DIVIDER = f"(ResourceObject=None,TintColor=(SpecifiedColor={color(TEXT, 0.12)}),DrawAs=Image,ImageSize=(X=2,Y=2))"


def tex_brush(path, size, hx="FFFFFF", a=1.0):
    return (f"(ResourceObject=Texture2D'{path}',TintColor=(SpecifiedColor={color(hx, a)}),DrawAs=Image,"
            f"ImageSize=(X={size},Y={size}))")


def icon_path(name):
    return f"{ART}/T_PopupIcon_{name}.T_PopupIcon_{name}"


# ------------------------------------------------------------------ shared assets
def setup_assets():
    tasks = []
    for n in ("Warning", "Check", "Box", "Star"):
        if unreal.EditorAssetLibrary.does_asset_exist(f"{ART}/T_PopupIcon_{n}"):
            continue
        t = unreal.AssetImportTask()
        t.filename = ICON_SRC + f"T_PopupIcon_{n}.png"
        t.destination_path = ART
        t.destination_name = f"T_PopupIcon_{n}"
        t.automated = True
        t.replace_existing = False
        tasks.append(t)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for n in ("Warning", "Check", "Box", "Star"):
        p = f"{ART}/T_PopupIcon_{n}"
        tex = unreal.load_asset(p)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
        unreal.EditorAssetLibrary.save_asset(p)
    for src, dst in (("WBP_PopupPalettePrimary", "WBP_PopupGlassPrimary"), ("WBP_PopupPaletteSecondary", "WBP_PopupGlassSecondary")):
        if not unreal.EditorAssetLibrary.does_asset_exist(f"{ART}/{dst}"):
            unreal.EditorAssetLibrary.duplicate_asset(PALETTE + src, f"{ART}/{dst}")
    for name, style, label in (("WBP_PopupGlassPrimary", PRIMARY, NAVY), ("WBP_PopupGlassSecondary", GHOST, TEXT)):
        b = Builder(f"{ART}/{name}")
        b.prop("Btn_Common", "WidgetStyle", style)
        b.prop("Btn_Common", "RenderTransform", "(Translation=(X=0,Y=0),Scale=(X=1,Y=1),Shear=(X=0,Y=0),Angle=0)")
        font_only(b, "Txt_Label", 18, label, True)
        b.prop("Txt_Label", "Justification", "Center")
        b.compile_save()
        cdo = unreal.get_default_object(unreal.load_object(None, f"{ART}/{name}.{name}_C"))
        r, g, bb = (common._lin(int(label[i:i + 2], 16)) for i in (0, 2, 4))
        cdo.set_editor_property("label_color", unreal.LinearColor(r, g, bb, 1.0))
        cdo.set_editor_property("label_font_size", 18)
        unreal.EditorAssetLibrary.save_asset(f"{ART}/{name}", only_if_is_dirty=False)


# ------------------------------------------------------------------ helpers
def font_only(b, name, size, hx, bold=False, justify=None):
    f = unreal.WidgetFontInfo()
    f.font_family = FONT_BOLD if bold else FONT_SEMI
    f.typeface = "Default"
    f.size = size
    f.color = color(hx)
    b.ok(WS.set_font(b.path, name, f), f"font {name}")
    b.prop(name, "ColorAndOpacity", f"(SpecifiedColor={color(hx)})")
    if justify:
        b.prop(name, "Justification", justify)


def widget(b, name):
    asset = b.path.rsplit("/", 1)[1]
    return unreal.find_object(None, f"{b.path}.{asset}:WidgetTree.{name}")


def anchor(b, name, a, align, pos, size, autosize=False):
    s = b.slot(name)
    anchors = unreal.Anchors()
    anchors.minimum = unreal.Vector2D(*a)
    anchors.maximum = unreal.Vector2D(*a)
    s.set_anchors(anchors)
    s.set_alignment(unreal.Vector2D(*align))
    s.set_position(unreal.Vector2D(*pos))
    s.set_size(unreal.Vector2D(*size))
    s.set_auto_size(autosize)


def center_child(b, name):
    s = widget(b, name).slot
    if hasattr(s, "set_horizontal_alignment"):
        s.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    if hasattr(s, "set_vertical_alignment"):
        s.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)


def fill_child(b, name):
    s = widget(b, name).slot
    if hasattr(s, "set_horizontal_alignment"):
        s.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    if hasattr(s, "set_vertical_alignment"):
        s.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    if hasattr(s, "set_padding"):
        s.set_padding(unreal.Margin(0, 0, 0, 0))


def header(b, layout, icon_names, title_name, title=None):
    """Icon + title + crest + gold hairline: the shared top of every popup."""
    for n, (tex, vis) in icon_names.items():
        b.image(n, layout, PAD, 26, 28, 28, tex_brush(icon_path(tex), 28), z=2, vis=vis)
    if not b.exists(title_name):
        b.add("TextBlock", title_name, layout)
    if title is not None:
        b.prop(title_name, "Text", title)
    font_only(b, title_name, 22, TEXT, True)
    b.rect(title_name, 68, 22, 340, 36, z=2)
    b.vis(title_name, "HitTestInvisible")
    b.image("HeaderCrest", layout, W_CARD - PAD - 34, 23, 34, 34, tex_brush(CREST, 34, GOLD, 0.8), z=2)


def rule(b, name, layout, y):
    if not b.exists(name):
        b.add("Image", name, layout)
    b.prop(name, "Brush", RULE)
    b.rect(name, PAD, y, W_CARD - 2 * PAD, 2, z=1)
    b.vis(name, "HitTestInvisible")


def card(b, height):
    b.prop("DimBG", "Background", DIM)
    b.prop("DimBG", "BrushColor", "(R=1,G=1,B=1,A=1)")
    b.prop("PanelBG", "Background", CARD_BRUSH)
    b.prop("PanelBG", "BrushColor", "(R=1,G=1,B=1,A=1)")
    b.prop("PanelBG", "Padding", "(Left=0,Top=0,Right=0,Bottom=0)")
    # The old confirm card was drawn at 0.75 render scale; every card is authored at final size now.
    b.prop("PanelBG", "RenderTransform", "(Translation=(X=0,Y=0),Scale=(X=1,Y=1),Shear=(X=0,Y=0),Angle=0)")
    anchor(b, "PanelBG", (0.5, 0.5), (0.5, 0.5), (0, 0), (W_CARD, height))


def button_label(b, name, size, hx):
    font_only(b, name, size, hx, True, "Center")
    center_child(b, name)


# ------------------------------------------------------------------ confirm
def confirm():
    b = Builder(CONFIRM)
    h = 280
    # The old design scaled a 1672x941 reference stage; 1920x1080 makes stage units equal 1080p pixels.
    # The old design scaled a reference stage to fit the screen (twice-scaled next to the other popups);
    # unscaled, the card uses the same DPI units as the count and reward popups.
    b.prop("ReferenceFit", "Stretch", "None")
    b.prop("ReferenceStageSize", "WidthOverride", 1920)
    b.prop("ReferenceStageSize", "HeightOverride", 1080)
    card(b, h)
    fill_child(b, "ConfirmLayout")
    for n in ("AlertSeal", "ConfirmFooterRule", "ReferenceCloseButton"):
        b.vis(n, "Collapsed")
    header(b, "ConfirmLayout", {"IntentIconDestructive": ("Warning", "HitTestInvisible"),
                                "IntentIconPositive": ("Check", "Collapsed")}, "AlertHeading")
    rule(b, "AlertHeaderRule", "ConfirmLayout", 70)
    b.rect("ConfirmMessageScroll", PAD, 90, W_CARD - 2 * PAD, h - 90 - 104, z=1)
    font_only(b, "MessageText", 19, TEXT, justify="Left")
    b.prop("MessageText", "AutoWrapText", "True")
    anchor(b, "ConfirmActions", (1, 1), (1, 1), (-PAD, -PAD), (0, 0), autosize=True)
    b.prop("ConfirmButton", "WidgetStyle", DESTRUCTIVE)
    # An earlier pass baked the destructive red into BackgroundColor; it tinted the positive cream pill pink.
    b.prop("ConfirmButton", "BackgroundColor", "(R=1,G=1,B=1,A=1)")
    # PanelIntro animated the old card height (slot Bottom 366) and opacity; this card simply appears like the others.
    for anim in unreal.ObjectIterator(unreal.WidgetAnimation):
        if anim.get_outermost().get_name() == CONFIRM and anim.get_name() == "PanelIntro":
            for binding in unreal.MovieSceneSequenceExtensions.get_bindings(anim):
                for track in list(binding.get_tracks()):
                    binding.remove_track(track)
    b.prop("CancelButton", "WidgetStyle", GHOST)
    for size_box in ("ConfirmButtonSize", "CancelButtonSize"):
        b.prop(size_box, "WidthOverride", BTN_W)
        b.prop(size_box, "HeightOverride", BTN_H)
        fill_child(b, size_box)
    widget(b, "CancelButton").slot.set_padding(unreal.Margin(12, 0, 0, 0))
    widget(b, "ConfirmButton").slot.set_padding(unreal.Margin(0, 0, 0, 0))
    button_label(b, "ConfirmLabelText", 18, RED_TEXT)
    button_label(b, "CancelLabelText", 18, TEXT)
    b.compile_save()
    # Positive / notice confirms switch to the cream primary pill (NUConfirmDialog::bUseIntentStyles).
    cdo_path = f"{CONFIRM}.Default__WBP_ConfirmDialog_C"
    cdo = unreal.get_default_object(unreal.load_object(None, f"{CONFIRM}.WBP_ConfirmDialog_C"))
    cdo.set_editor_property("use_intent_styles", True)
    unreal.ObjectReflectionService.set_property(cdo_path, "PositiveConfirmStyle", PRIMARY)
    lc = unreal.SlateColor()
    lc.specified_color = unreal.LinearColor(*(common._lin(int(NAVY[i:i + 2], 16)) for i in (0, 2, 4)), 1.0)
    cdo.set_editor_property("positive_label_color", lc)
    unreal.EditorAssetLibrary.save_asset(CONFIRM, only_if_is_dirty=False)
    return b.failed


# ------------------------------------------------------------------ count
def count():
    b = Builder(COUNT)
    h = 316
    if b.exists("ContentBox"):
        # The vertical box cannot right-align the buttons or hold a header; rebuild on a canvas with the same names.
        WS.remove_component(COUNT, "ContentBox", True)
    card(b, h)
    if not b.exists("CountLayout"):
        b.add("CanvasPanel", "CountLayout", "PanelBG")
    fill_child(b, "CountLayout")
    L = "CountLayout"
    header(b, L, {"TitleIcon": ("Box", "HitTestInvisible")}, "TitleText", "수량 입력")
    rule(b, "TitleRule", L, 70)
    b.add("TextBlock", "MessageText", L)
    font_only(b, "MessageText", 19, TEXT)
    b.prop("MessageText", "AutoWrapText", "True")
    b.rect("MessageText", PAD, 86, W_CARD - 2 * PAD, 50, z=1)
    b.vis("MessageText", "HitTestInvisible")

    y = 140
    for name, label, x, w, size in (("DecreaseButton", "−", PAD, 48, 24), ("IncreaseButton", "+", 232, 48, 24),
                                    ("MaxButton", "최대", 288, 72, 16)):
        b.add("Button", name, L)
        b.prop(name, "WidgetStyle", SQUARE)
        b.rect(name, x, y, w, 48, z=1)
        lbl = name.replace("Button", "Label")
        b.add("TextBlock", lbl, name)
        b.prop(lbl, "Text", label)
        button_label(b, lbl, size, TEXT)
    b.add("Border", "CountFrame", L)
    b.prop("CountFrame", "Background", rounded(CARD, 0.9, 10, TEXT, 0.25, 1.0))
    b.prop("CountFrame", "Padding", "(Left=8,Top=0,Right=8,Bottom=0)")
    b.prop("CountFrame", "VerticalAlignment", "VAlign_Center")
    b.prop("CountFrame", "HorizontalAlignment", "HAlign_Fill")
    b.rect("CountFrame", 84, y, 140, 48, z=1)
    b.add("EditableTextBox", "CountTextBox", "CountFrame")
    box = widget(b, "CountTextBox")
    st = box.get_editor_property("widget_style")
    for key in ("background_image_normal", "background_image_hovered", "background_image_focused", "background_image_read_only"):
        brush = unreal.SlateBrush()
        brush.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
        st.set_editor_property(key, brush)
    c = unreal.SlateColor()
    c.specified_color = unreal.LinearColor(*(common._lin(int(TEXT[i:i + 2], 16)) for i in (0, 2, 4)), 1.0)
    st.set_editor_property("foreground_color", c)
    st.set_editor_property("focused_foreground_color", c)
    st.set_editor_property("padding", unreal.Margin(0, 6, 0, 6))
    ts = st.get_editor_property("text_style")
    ts.set_editor_property("color_and_opacity", c)
    fnt = ts.get_editor_property("font")
    fnt.set_editor_property("font_object", unreal.load_object(None, FONT_BOLD))
    fnt.set_editor_property("size", 22)
    ts.set_editor_property("font", fnt)
    st.set_editor_property("text_style", ts)
    box.set_editor_property("widget_style", st)
    b.prop("CountTextBox", "Justification", "Center")
    b.prop("CountTextBox", "Text", "1")

    b.add("TextBlock", "PriceText", L)
    font_only(b, "PriceText", 15, GOLD)
    b.rect("PriceText", PAD, 200, W_CARD - 2 * PAD, 24, z=1)
    b.vis("PriceText", "Collapsed")

    b.add("HorizontalBox", "ButtonRow", L)
    anchor(b, "ButtonRow", (1, 1), (1, 1), (-PAD, -PAD), (0, 0), autosize=True)
    for box_name, btn, cls, pad in (("ConfirmButtonBox", "ConfirmButton", "WBP_PopupGlassPrimary", 0),
                                    ("CancelButtonBox", "CancelButton", "WBP_PopupGlassSecondary", 12)):
        b.add("SizeBox", box_name, "ButtonRow")
        b.prop(box_name, "WidthOverride", BTN_W)
        b.prop(box_name, "HeightOverride", BTN_H)
        widget(b, box_name).slot.set_padding(unreal.Margin(pad, 0, 0, 0))
        if not b.exists(btn):
            r = WS.add_component(COUNT, f"{ART}/{cls}.{cls}_C", btn, box_name, True)
            if not r.success:
                r = WS.add_component(COUNT, f"{cls}_C", btn, box_name, True)
            b.ok(r, f"add {btn}")
        fill_child(b, btn)
    b.prop("ConfirmButton", "LabelText", "확인")
    b.prop("CancelButton", "LabelText", "취소")
    # NativePreConstruct applies the instance LabelColor/LabelFontSize over the button's own text style.
    b.prop("ConfirmButton", "LabelColor", color(NAVY))
    b.prop("CancelButton", "LabelColor", color(TEXT))
    b.prop("ConfirmButton", "LabelFontSize", 18)
    b.prop("CancelButton", "LabelFontSize", 18)
    b.compile_save()
    return b.failed


# ------------------------------------------------------------------ reward
def reward():
    b = Builder(REWARD)
    h = 400
    card(b, h)
    fill_child(b, "RewardLayout")
    L = "RewardLayout"
    b.vis("AlertSeal", "Collapsed")
    b.vis("RewardFooterRule", "Collapsed")
    header(b, L, {"TitleIcon": ("Star", "HitTestInvisible")}, "TitleText")
    rule(b, "AlertHeaderRule", L, 70)
    font_only(b, "RewardCaption", 15, MUTED)
    b.rect("RewardCaption", PAD, 84, W_CARD - 2 * PAD, 24, z=1)
    b.rect("RewardScroll", PAD, 112, W_CARD - 2 * PAD, h - 112 - 100, z=1)
    font_only(b, "EntryText", 19, TEXT)
    b.add("VerticalBox", "RewardRows", "RewardScroll")
    for n, size, hx in (("RewardLabelTemplate", 19, TEXT), ("RewardAmountTemplate", 19, GOLD)):
        b.add("TextBlock", n, L)
        font_only(b, n, size, hx, n == "RewardAmountTemplate")
        b.rect(n, 0, 0, 10, 10)
        b.vis(n, "Collapsed")
    b.image("RewardBulletTemplate", L, 0, 0, 10, 10, f"(ResourceObject=Texture2D'/Game/Art/UI/HUD2026/Kit/T_HUD_Diamond.T_HUD_Diamond',TintColor=(SpecifiedColor={color(GOLD, 0.9)}),DrawAs=Image,ImageSize=(X=14,Y=14))", vis="Collapsed")
    b.image("RewardDividerTemplate", L, 0, 0, 10, 10, DIVIDER, vis="Collapsed")
    b.prop("CloseButton", "WidgetStyle", PRIMARY)
    anchor(b, "CloseButton", (1, 1), (1, 1), (-PAD, -PAD), (BTN_W, BTN_H))
    button_label(b, "CloseButtonText", 18, NAVY)
    b.compile_save()
    return b.failed


def run():
    setup_assets()
    result = {"confirm": confirm(), "count": count(), "reward": reward()}
    print(result)
    return result


if __name__ == "__main__":
    run()
