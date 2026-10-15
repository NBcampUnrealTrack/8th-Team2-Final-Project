"""Quest journal, concept C1 (quest cards + detail card, glass tone), 2026-10-15.

Concept: ArtSource/UI/QuestConcept_20261015/quest_C_cards.png. User follow-up: the status list scrolls with a
visible scrollbar (drag the thumb or use the wheel); no extra enclosing panel.
Targets: zxcv1/Quest/UI/WBP_QuestJournal (stage 1760x990), WBP_QuestJournalEntry, WBP_QuestJournalObjectiveRow,
WBP_QuestJournalRewardTile. Every C++ bound name is kept; decorative layers (rail tiles, mist, compass, feather,
rules) are collapsed, not deleted. The entry is rebuilt as a two-line card on a canvas (same bound names plus the
optional QR_EntryCaption / QR_EntryProgressBar / QR_EntryProgressText / QR_EntrySelectedBar read by C++).
Run with PIE stopped. Safe to re-run.
"""
import importlib, sys
import unreal

sys.path.insert(0, r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard")
import common
importlib.reload(common)
from common import *

UI = "/Game/Developers/zxcv1/Quest/UI/"
JOURNAL = UI + "WBP_QuestJournal"
ENTRY = UI + "WBP_QuestJournalEntry"
OBJECTIVE = UI + "WBP_QuestJournalObjectiveRow"
REWARD = UI + "WBP_QuestJournalRewardTile"
SLATE = "121A20"
CLEAR = "(ResourceObject=None,TintColor=(SpecifiedColor=(R=1,G=1,B=1,A=0)),DrawAs=NoDrawType)"
ZERO_PAD = "NormalPadding=(Left=0,Top=0,Right=0,Bottom=0),PressedPadding=(Left=0,Top=0,Right=0,Bottom=0)"

LIST_X, LIST_Y, LIST_W = 77, 143, 764      # tabs row top-left and column width (stage units)
LIST_TOP, LIST_BOTTOM = 212, 885
DETAIL_X, DETAIL_Y, DETAIL_W, DETAIL_H = 880, 141, 806, 770
CARD_H, CARD_GAP = 132, 12


def lin(hx, a=1.0):
    return unreal.LinearColor(*(common._lin(int(hx[i:i + 2], 16)) for i in (0, 2, 4)), a)


def slate_color(hx, a=1.0):
    c = unreal.SlateColor()
    c.specified_color = lin(hx, a)
    return c


def font_only(b, name, size, hx, bold=False, a=1.0, justify=None):
    f = unreal.WidgetFontInfo()
    f.font_family = FONT_BOLD if bold else FONT_SEMI
    f.typeface = "Default"
    f.size = size
    f.color = color(hx, a)
    b.ok(WS.set_font(b.path, name, f), f"font {name}")
    b.prop(name, "ColorAndOpacity", f"(SpecifiedColor={color(hx, a)})")
    if justify:
        b.prop(name, "Justification", justify)


def widget(b, name):
    asset = b.path.rsplit("/", 1)[1]
    return unreal.find_object(None, f"{b.path}.{asset}:WidgetTree.{name}")


def hide(b, *names):
    for n in names:
        if b.exists(n):
            b.vis(n, "Collapsed")


def pad(b, name, l, t, r, bt):
    widget(b, name).slot.set_padding(unreal.Margin(l, t, r, bt))


def size_box(b, name, w=None, h=None):
    sb = widget(b, name)
    if w is not None:
        sb.set_editor_property("width_override", w)
        sb.set_editor_property("override_width_override", True)
    if h is not None:
        sb.set_editor_property("height_override", h)
        sb.set_editor_property("override_height_override", True)


def pill(hx, a, outline=None, oa=0.0, ow=0.0):
    return rounded(hx, a, 12, outline, oa, ow)


TAB = (f"(Normal={pill(SLATE, 0.62, TEXT, 0.18, 1.0)},Hovered={pill('1C262E', 0.78, TEXT, 0.32, 1.0)},"
       f"Pressed={pill('243039', 0.85, TEXT, 0.32, 1.0)},Disabled={pill(SLATE, 0.3, TEXT, 0.08, 1.0)},{ZERO_PAD})")
TAB_SELECTED = (f"(Normal={pill('3A4752', 0.86, TEXT, 0.55, 1.2)},Hovered={pill('42505C', 0.9, TEXT, 0.65, 1.2)},"
                f"Pressed={pill('3A4752', 0.95, TEXT, 0.65, 1.2)},Disabled={pill(SLATE, 0.3, TEXT, 0.08, 1.0)},{ZERO_PAD})")
CARD = rounded(SLATE, 0.80, 14, TEXT, 0.12, 1.0)
PRIMARY = (f"(Normal={rounded(CREAM, 1.0, 32, half_height=True)},Hovered={rounded('EFE7D5', 1.0, 32, half_height=True)},"
           f"Pressed={rounded('C9BFA8', 1.0, 32, half_height=True)},Disabled={rounded(CREAM, 0.35, 32, half_height=True)},{ZERO_PAD})")
CLOSE = (f"(Normal={rounded(SLATE, 0.78, 12, TEXT, 0.2, 1.0)},Hovered={rounded('243039', 0.9, 12, TEXT, 0.4, 1.0)},"
         f"Pressed={rounded('2C3843', 0.95, 12, TEXT, 0.4, 1.0)},Disabled={rounded(SLATE, 0.4, 12)},{ZERO_PAD})")
SCROLLBAR = ("(VerticalBackgroundImage=" + rounded(TEXT, 0.06, 3) + ",HorizontalBackgroundImage=" + rounded(TEXT, 0.06, 3)
             + ",VerticalTopSlotImage=" + CLEAR + ",VerticalBottomSlotImage=" + CLEAR
             + ",HorizontalTopSlotImage=" + CLEAR + ",HorizontalBottomSlotImage=" + CLEAR
             + ",NormalThumbImage=" + rounded(TEXT, 0.32, 3) + ",HoveredThumbImage=" + rounded(TEXT, 0.55, 3)
             + ",DraggedThumbImage=" + rounded(GOLD, 0.8, 3) + ",Thickness=6.000000)")


# ------------------------------------------------------------------ journal
def journal():
    b = Builder(JOURNAL)
    b.prop("JournalBackground", "Background", CLEAR)
    hide(b, "QM_IndexMist", "QM_ReadingMist", "QL_RailDivider", "QMotion_GoldDivider", "QM_CompassWatermark",
         "QV4_FeatherSize", "QP_HeaderRule", "QP_HeaderDiamond", "QP_CategoryRule", "NF_JournalCrestSize", "QV4_Subtitle",
         "QL_AllTile", "QL_MainTile", "QL_SideTile", "QL_DailyTile", "QL_TutorialTile",
         "QR_TabSeparator1", "QR_TabSeparator2", "QR_TabSeparator3", "QR_TabSeparator4",
         "QP_InProgressDiamond", "QP_AvailableDiamond", "QP_CompletedDiamond")

    # Title outside the panels, top-left like the character menu.
    b.prop("JournalHeaderCard", "Background", CLEAR)
    b.prop("JournalHeaderCard", "Padding", "(Left=0,Top=0,Right=0,Bottom=0)")
    b.rect("JournalHeaderCard", LIST_X - 13, 50, 620, 72)  # the heading sits 13 in from its card
    b.prop("NF_JournalTitleBadge", "Background", CLEAR)
    b.prop("NF_JournalTitleBadge", "Padding", "(Left=0,Top=0,Right=0,Bottom=0)")
    size_box(b, "QV4_HeaderBadgeSize", 620, 72)
    b.prop("JournalHeading", "Text", "퀘스트 일지")
    font_only(b, "JournalHeading", 40, TEXT, True)

    b.rect("CloseButtonSizeBox", 1633, 58, 56, 56)
    size_box(b, "CloseButtonSizeBox", 56, 56)
    b.prop("CloseButton", "WidgetStyle", CLOSE)
    # The old close was a diamond: the button itself was rotated 45 degrees.
    for n in ("CloseButton", "QR_CloseInnerTrim", "CloseButton_Label"):
        b.prop(n, "RenderTransform", "(Translation=(X=0,Y=0),Scale=(X=1,Y=1),Shear=(X=0,Y=0),Angle=0)")
    b.prop("QR_CloseInnerTrim", "Background", CLEAR)
    font_only(b, "CloseButton_Label", 24, TEXT, False, justify="Center")

    # Filter tabs: five equal pills.
    b.rect("QV4_CategorySize", LIST_X, LIST_Y, LIST_W, 52)
    size_box(b, "QV4_CategorySize", LIST_W, 52)
    for i, n in enumerate(("AllCategoryButton", "MainCategoryButton", "SideCategoryButton", "DailyCategoryButton",
                           "TutorialCategoryButton")):
        b.prop(n, "WidgetStyle", TAB)
        b.prop(n, "BackgroundColor", "(R=1,G=1,B=1,A=1)")
        s = widget(b, n).slot
        s.set_size(unreal.SlateChildSize(1.0, unreal.SlateSizeRule.FILL))
        s.set_padding(unreal.Margin(0 if i == 0 else 6, 0, 0 if i == 4 else 6, 0))
        s.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        font_only(b, n + "_Label", 18, TEXT, False, a=0.72, justify="Center")

    # Status list: scrolls inside its column with a visible slim scrollbar.
    b.rect("QJ_IndexSize", LIST_X, LIST_TOP, LIST_W, LIST_BOTTOM - LIST_TOP)
    size_box(b, "QJ_IndexSize", LIST_W, LIST_BOTTOM - LIST_TOP)
    b.prop("JournalListCard", "Background", CLEAR)
    b.prop("JournalListCard", "Padding", "(Left=0,Top=0,Right=0,Bottom=0)")
    sbox = widget(b, "StateSectionScrollBox")
    sbox.set_editor_property("scroll_bar_visibility", unreal.SlateVisibility.VISIBLE)
    # Slate fades an idle scrollbar out; the list keeps it visible so the thumb can be dragged.
    sbox.set_editor_property("always_show_scrollbar", True)
    sbox.set_editor_property("scrollbar_thickness", unreal.Vector2D(6, 6))
    sbox.set_editor_property("scrollbar_padding", unreal.Margin(10, 0, 0, 0))
    b.prop("StateSectionScrollBox", "WidgetBarStyle", SCROLLBAR)
    b.prop("DetailScrollBox", "WidgetBarStyle", SCROLLBAR)
    dbox = widget(b, "DetailScrollBox")
    dbox.set_editor_property("scrollbar_thickness", unreal.Vector2D(6, 6))
    dbox.set_editor_property("scrollbar_padding", unreal.Margin(12, 0, 0, 0))
    for n in ("InProgressStateButton", "AvailableStateButton", "CompletedStateButton"):
        b.prop(n, "WidgetStyle", f"(Normal={CLEAR},Hovered={CLEAR},Pressed={CLEAR},Disabled={CLEAR},{ZERO_PAD})")
        font_only(b, n + "_Label", 22, GOLD, True)
    for n in ("QP_InProgressRow", "QP_AvailableRow", "QP_CompletedRow"):
        pad(b, n, 4, 12, 0, 10)
    font_only(b, "EmptyListText", 18, MUTED)

    # Detail: one glass card on the right.
    b.prop("DetailPanel", "Background", CARD)
    b.prop("DetailPanel", "Padding", "(Left=44,Top=40,Right=44,Bottom=36)")
    b.rect("DetailPanel", DETAIL_X, DETAIL_Y, DETAIL_W, DETAIL_H)
    hide(b, "QR_TitleBadgeSize", "QP_DescriptionRowIconSize", "QP_ObjectiveRowIconSize", "QP_DescriptionRowEnd",
         "QP_ObjectiveRowEnd", "QV4_RecordDivider", "QV4_ObjectiveDivider",
         "QP_RewardRowIconSize", "QV4_RewardDivider", "QP_RewardRowEnd")
    font_only(b, "RewardSectionHeading", 20, GOLD, True)
    b.prop("QR_RewardBand", "Background", CLEAR)
    font_only(b, "DetailTitleText", 40, TEXT, True)
    font_only(b, "DetailMetaText", 19, MUTED)
    pad(b, "DetailMetaText", 0, 6, 0, 4)
    font_only(b, "QR_LocationText", 19, MUTED)
    pad(b, "QR_LocationRow", 0, 2, 0, 12)
    for n in ("QR_LocationPinTip", "QR_LocationPinHead"):
        b.prop(n, "ColorAndOpacity", f"(R={lin(MUTED).r},G={lin(MUTED).g},B={lin(MUTED).b},A=1)")
    for heading in ("QV4_DescriptionHeading", "ObjectiveSectionHeading"):
        font_only(b, heading, 20, GOLD, True)
    pad(b, "QP_DescriptionRow", 0, 12, 0, 6)
    font_only(b, "DetailDescriptionText", 20, TEXT)
    pad(b, "QP_ObjectiveRow", 0, 18, 0, 0)
    b.prop("QR_ObjectiveBand", "Background", CLEAR)
    font_only(b, "DetailObjectiveText", 19, TEXT)
    b.prop("NF_JournalRewardCard", "Background", CLEAR)
    b.prop("NF_JournalRewardCard", "Padding", "(Left=0,Top=0,Right=0,Bottom=0)")
    pad(b, "NF_JournalRewardSize", 0, 18, 0, 0)
    font_only(b, "DetailRewardText", 19, TEXT)
    hide(b, "QP_TrackIconSize", "QP_TrackArrow")
    b.prop("TrackButton", "WidgetStyle", PRIMARY)
    font_only(b, "TrackButton_Label", 20, NAVY, True, justify="Center")
    size_box(b, "QM_TrackSize", 300, 60)
    pad(b, "ActionButtonRow", 0, 16, 0, 0)
    b.compile_save()

    cdo = unreal.get_default_object(unreal.load_object(None, JOURNAL + ".WBP_QuestJournal_C"))
    cdo.set_editor_property("use_tab_styles", True)
    path = JOURNAL + ".Default__WBP_QuestJournal_C"
    unreal.ObjectReflectionService.set_property(path, "TabStyle", TAB)
    unreal.ObjectReflectionService.set_property(path, "SelectedTabStyle", TAB_SELECTED)
    cdo.set_editor_property("tab_label_color", slate_color(TEXT, 0.72))
    cdo.set_editor_property("selected_tab_label_color", slate_color(TEXT))
    cdo.set_editor_property("state_label_color", slate_color(GOLD))
    unreal.EditorAssetLibrary.save_asset(JOURNAL, only_if_is_dirty=False)
    return b.failed


# ------------------------------------------------------------------ entry card
def entry():
    b = Builder(ENTRY)
    if b.exists("EntryHorizontalBox"):
        # The card stacks caption / title / location+progress; rebuild on a canvas with the same bound names.
        WS.remove_component(ENTRY, "EntryHorizontalBox", True)
    size_box(b, "EntrySizeBox", None, CARD_H + CARD_GAP)
    b.prop("EntryBackground", "Background", CARD)
    b.prop("EntryBackground", "Padding", "(Left=0,Top=0,Right=0,Bottom=0)")
    pad(b, "EntryBackground", 0, 0, 0, CARD_GAP)
    if not b.exists("EntryCanvas"):
        b.add("CanvasPanel", "EntryCanvas", "EntryBackground")
    b.vis("EntryCanvas", "SelfHitTestInvisible")

    def place(name, a, align, pos, size):
        s = b.slot(name)
        anchors = unreal.Anchors()
        anchors.minimum = unreal.Vector2D(*a[:2])
        anchors.maximum = unreal.Vector2D(*a[2:])
        s.set_anchors(anchors)
        s.set_alignment(unreal.Vector2D(*align))
        s.set_offsets(unreal.Margin(*pos, *size))

    b.add("Image", "QR_EntrySelectedBar", "EntryCanvas")
    b.prop("QR_EntrySelectedBar", "Brush", rounded(GOLD, 1.0, 2))
    place("QR_EntrySelectedBar", (0, 0, 0, 1), (0, 0), (0, 16), (4, 16))
    b.vis("QR_EntrySelectedBar", "Hidden")

    b.add("TextBlock", "QR_EntryCaption", "EntryCanvas")
    font_only(b, "QR_EntryCaption", 16, MUTED)
    place("QR_EntryCaption", (0, 0, 1, 0), (0, 0), (30, 18), (30, 24))
    b.vis("QR_EntryCaption", "HitTestInvisible")

    b.add("TextBlock", "QuestTitleText", "EntryCanvas")
    font_only(b, "QuestTitleText", 28, TEXT, True)
    place("QuestTitleText", (0, 0, 1, 0), (0, 0), (30, 42), (30, 40))
    b.vis("QuestTitleText", "HitTestInvisible")

    b.add("SizeBox", "QR_EntryLocationPin", "EntryCanvas")
    size_box(b, "QR_EntryLocationPin", 12, 12)
    place("QR_EntryLocationPin", (0, 0, 0, 0), (0, 0), (32, 98), (12, 12))
    b.add("Image", "QR_EntryLocationDot", "QR_EntryLocationPin")
    b.prop("QR_EntryLocationDot", "Brush", rounded(MUTED, 1.0, 6))
    b.vis("QR_EntryLocationPin", "HitTestInvisible")

    b.add("TextBlock", "QuestStatusText", "EntryCanvas")
    font_only(b, "QuestStatusText", 17, MUTED)
    place("QuestStatusText", (0, 0, 0, 0), (0, 0), (52, 90), (300, 28))
    b.vis("QuestStatusText", "HitTestInvisible")

    b.bar("QR_EntryProgressBar", "EntryCanvas", 0, 0, 10, 10, GOLD)
    place("QR_EntryProgressBar", (1, 0, 1, 0), (1, 0), (-104, 101), (300, 6))
    b.add("TextBlock", "QR_EntryProgressText", "EntryCanvas")
    font_only(b, "QR_EntryProgressText", 17, TEXT, False, justify="Right")
    place("QR_EntryProgressText", (1, 0, 1, 0), (1, 0), (-28, 90), (66, 28))
    b.vis("QR_EntryProgressText", "HitTestInvisible")
    b.compile_save()

    cdo = unreal.get_default_object(unreal.load_object(None, ENTRY + ".WBP_QuestJournalEntry_C"))
    cdo.set_editor_property("caption_color", slate_color(MUTED))
    cdo.set_editor_property("selected_caption_color", slate_color(GOLD))
    unreal.EditorAssetLibrary.save_asset(ENTRY, only_if_is_dirty=False)
    return b.failed


# ------------------------------------------------------------------ objective row / reward tile
def objective():
    b = Builder(OBJECTIVE)
    b.prop("ObjectiveRoot", "Background", CLEAR)
    b.prop("ObjectiveRoot", "Padding", "(Left=0,Top=8,Right=0,Bottom=8)")
    font_only(b, "ObjectiveLabel", 19, TEXT)
    font_only(b, "ObjectiveCount", 19, TEXT, False, justify="Right")
    b.compile_save()
    return b.failed


def reward():
    b = Builder(REWARD)
    size_box(b, "RewardIconSize", 52, 52)
    b.prop("RewardIconPlate", "Background", rounded(SLATE, 0.8, 26, TEXT, 0.18, 1.0))
    font_only(b, "RewardLabel", 17, MUTED)
    font_only(b, "RewardAmount", 19, TEXT, True)
    b.compile_save()
    return b.failed


def run():
    result = {"journal": journal(), "entry": entry(), "objective": objective(), "reward": reward()}
    print(result)
    return result
