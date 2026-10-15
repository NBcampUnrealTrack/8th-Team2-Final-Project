"""Restyle the five remaining character-menu pages to the glass-card language (concepts
ArtSource/UI/CharacterMenuPagesConcept_20261014/page_*.png). Layout, names and bindings stay;
only brushes, tints, text colours and button styles change.

Rules come from the shared authoring vocabulary of these WBPs:
  <Panel>Frame0-7 / <Panel>{TL,TR,BL,BR}{H,V,Diamond} / *Paper / *Ribbon{Fill,Edge,Jewel} /
  Arch*, Celestial, Crest, Watermark ...  -> decorative, collapsed
  *Surface -> glass card;  *Well -> inset glass;  *Rule/*Separator -> hairline
  Slot corner images (prefix has a sibling <prefix>Button) stay: C++ tints them gold on selection.
Runtime contracts kept:
  * inventory tab groups read selected style from the FIRST button and idle style from the SECOND;
  * FNUCharacterMenuSlot::SetSelected reuses the idle slot brush and recolours its outline;
  * FNUCharacterMenuItemCard dims disabled action buttons through render opacity.
"""
import importlib, re, sys
import unreal

sys.path.insert(0, r"D:/UnrealEngine/ProjectM/Tools/UICharacterCardDashboard")
import common
importlib.reload(common)
from common import *

ROOT = "/Game/Developers/MinkiCho/UI/CharacterMenu/"
PAGES = ["WBP_CharacterEquipmentPage", "WBP_CharacterInventoryPage", "WBP_CharacterInventoryCell",
         "WBP_CharacterStatsPage", "WBP_CharacterTraitsPage", "WBP_CharacterNaiaPage"]

DECOR = re.compile(r"(Frame\d+$|Frame(TL|TR|BL|BR)(H|V|Diamond)$|Card(TL|TR|BL|BR)(H|V|Diamond)$|Panel(TL|TR|BL|BR)(H|V|Diamond)$|"
                   r"Paper$|Ribbon(Fill|Edge\d*|Jewel)$|RibbonEdge\d+$|Arch(Outer|Gold)\d+$|Celestial|Crest$|Watermark|"
                   r"CelestialUnderlay|^PortraitCompass$|EquipmentFloor$|TopCrest$|TitleStar|TraitsModeFill$|GraphJobRibbonFill$)")
SLOT_CORNER = re.compile(r"^(.*?)(TLH|TLV|TRH|TRV|BLH|BLV|BRH|BRV)$")
PRIMARY = {"EquipActionButton", "AcquireTraitButton", "MoveToStorageButton"}
PRIMARY_LABELS = {"EquipActionLabel", "AcquireTraitLabel", "MoveToStorageLabel"}
BACK_CARDS = {
    "WBP_CharacterEquipmentPage": {"EquipmentAreaCard": (0, 0, 744, 490)},
    "WBP_CharacterInventoryPage": {"InventoryAreaCard": (-14, -14, 780, 616)},
}
TAB_GROUPS = [("CategoryEquipment", "CategoryConsumable", "CategoryMaterial", "CategoryOther"),
              ("FilterAll", "FilterWeapon", "FilterArmor", "FilterAccessory"),
              ("ViewLarge", "ViewCompact"), ("SortOptionKind", "SortOptionGrade")]


def lum(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def parse(value, key="SpecifiedColor="):
    if key not in value:
        return None
    s = value.split(key, 1)[1]
    m = re.match(r"\(R=([\d.\-]+),G=([\d.\-]+),B=([\d.\-]+),A=([\d.\-]+)\)", s)
    return tuple(float(m.group(i)) for i in range(1, 5)) if m else None


def button_style(normal, hovered, pressed, disabled, pad=0):
    p = f"(Left={pad},Top={pad},Right={pad},Bottom={pad})"
    return f"(Normal={normal},Hovered={hovered},Pressed={pressed},Disabled={disabled},NormalPadding={p},PressedPadding={p})"


SLOT_STYLE = button_style(rounded(GLASS, 0.92, 8, TEXT, 0.14, 1.0), rounded(GLASS, 0.96, 8, TEXT, 0.34, 1.0),
                          rounded(GLASS, 1.0, 8, GOLD, 0.7, 1.0), rounded(GLASS, 0.6, 8, TEXT, 0.08, 1.0))
PRIMARY_STYLE = button_style(rounded(CREAM, 1.0, 22, half_height=True), rounded("EDE5D2", 1.0, 22, half_height=True),
                             rounded("CFC5AE", 1.0, 22, half_height=True), rounded(CREAM, 0.55, 22, half_height=True))
SECONDARY_STYLE = button_style(rounded(TEXT, 0.05, 8, TEXT, 0.22, 1.0), rounded(TEXT, 0.10, 8, TEXT, 0.40, 1.0),
                               rounded(GOLD, 0.16, 8, GOLD, 0.7, 1.0), rounded(TEXT, 0.03, 8, TEXT, 0.10, 1.0))
TAB_SELECTED = button_style(rounded(TEXT, 0.12, 8, GOLD, 0.75, 1.0), rounded(TEXT, 0.16, 8, GOLD, 0.9, 1.0),
                            rounded(GOLD, 0.18, 8, GOLD, 0.9, 1.0), rounded(TEXT, 0.05, 8, TEXT, 0.1, 1.0))
TAB_IDLE = button_style(rounded(GLASS, 0.6, 8, TEXT, 0.12, 1.0), rounded(TEXT, 0.08, 8, TEXT, 0.30, 1.0),
                        rounded(GOLD, 0.14, 8, GOLD, 0.6, 1.0), rounded(GLASS, 0.4, 8, TEXT, 0.06, 1.0))


def text_color(name, c):
    """New (hex, alpha, bold?) for a TextBlock given its name and current colour."""
    if name in PRIMARY_LABELS:
        return NAVY, 1.0, True
    if re.search(r"(RibbonText|Heading)$", name):
        return GOLD, 1.0, True
    if c is None:
        return None
    r, g, b, a = c
    if b > r + 0.15 and b > 0.25:            # blue accent values on ivory
        return BLUE, 1.0, None
    if r > 0.8 and g > 0.8:                  # already ivory
        return TEXT, a, None
    if r > 0.9 and g > 0.8 and b < 0.75:     # warm ivory
        return TEXT, a, None
    if re.search(r"(Code|Help|Hint|Kind|Type|Empty)$", name):
        return MUTED, 1.0, None
    if lum(c) < 0.06:                        # dark navy values and titles
        return TEXT, 1.0, None
    return "DCE1E5", 1.0, None               # mid-dark labels


def image_brush(name, props, siblings):
    """Return (brush or None, visibility or None)."""
    if DECOR.search(name):
        return None, "Collapsed"
    m = SLOT_CORNER.match(name)
    if m and (m.group(1) + "Button") in siblings:
        return f"(TintColor=(SpecifiedColor={color(GOLD, 0.0)}),DrawAs=Image,ImageSize=(X=8.000000,Y=1.000000))", None
    if m:  # panel corner ornament without a slot button
        return None, "Collapsed"
    if name.endswith("Surface"):
        return rounded(CARD, 0.86, 14, TEXT, 0.11, 1.0), None
    if name.endswith("Well") or name in ("PassiveIconWell", "SelectedItemWell"):
        return rounded(GLASS, 0.92, 10, TEXT, 0.14, 1.0), None
    if re.search(r"(Rule|Separator|Column\d+|LabelRule|TabEdge\d+)$", name):
        return f"(TintColor=(SpecifiedColor={color(TEXT, 0.11)}),DrawAs=Image,ImageSize=(X=8.000000,Y=1.000000))", None
    if re.search(r"(RowTint|Row)$", name):
        return f"(TintColor=(SpecifiedColor={color(TEXT, 0.025)}),DrawAs=Image,ImageSize=(X=8.000000,Y=8.000000))", None
    if name.endswith("EmptyMark"):
        return f"(TintColor=(SpecifiedColor={color(TEXT, 0.22)}),DrawAs=Image,ImageSize=(X=4.000000,Y=4.000000))", None
    if name.endswith("Marker") or name == "PointsMarker":
        return f"(TintColor=(SpecifiedColor={color(GOLD, 0.85)}),DrawAs=Image,ImageSize=(X=4.000000,Y=4.000000))", None
    if name == "CellEmpty":
        return rounded(GLASS, 0.9, 8, TEXT, 0.16, 1.0), None
    if name in ("NaiaNameStrip", "PointsBadge", "GrowthTreeTab", "DetailStatusBadge", "SortOptionsBack"):
        return rounded(TEXT if name != "SortOptionsBack" else CARD, 0.06 if name != "SortOptionsBack" else 0.97, 8,
                       TEXT, 0.14, 1.0), None
    if name.endswith("HealthTrack"):
        return rounded(TEXT, 0.10, 3, half_height=True), None
    if name.endswith("HealthFill"):
        return rounded(GREEN, 1.0, 3, half_height=True), None
    if re.search(r"(Arrow\d*|Chevron[LR])$", name):
        return f"(TintColor=(SpecifiedColor={color(MUTED)}),DrawAs=Image,ImageSize=(X=8.000000,Y=8.000000))", None
    if name == "CellEquippedBadge":
        return rounded(GOLD, 0.95, 4), None
    if name.startswith("RadialLink"):
        c = parse(props.get("Brush", ""))
        if c and c[0] > 0.4:                 # acquired path, warm
            return f"(TintColor=(SpecifiedColor={color(GOLD, 0.9)}),DrawAs=Image,ImageSize=(X=8.000000,Y=1.000000))", None
        return f"(TintColor=(SpecifiedColor={color(TEXT, 0.16)}),DrawAs=Image,ImageSize=(X=8.000000,Y=1.000000))", None
    if name in ("PortraitFallbackCompass",):
        return None, None
    return None, None


def restyle(page):
    path = ROOT + page
    b = Builder(path)
    snaps = WS.get_widget_snapshot(path)
    names = {s.widget_name for s in snaps}
    stats = {"collapsed": 0, "images": 0, "texts": 0, "buttons": 0}
    for s in snaps:
        n, cls = s.widget_name, s.widget_class
        props = {x.property_name: x.current_value for x in s.properties}
        if cls == "Image":
            brush, vis = image_brush(n, props, names)
            if vis:
                b.vis(n, vis); stats["collapsed"] += 1
            if brush:
                b.prop(n, "Brush", brush); stats["images"] += 1
        elif cls == "TextBlock":
            new = text_color(n, parse(props.get("ColorAndOpacity", "")))
            if new:
                hx, a, bold = new
                b.prop(n, "ColorAndOpacity", f"(SpecifiedColor={color(hx, a)})")
                if bold:
                    f = WS.get_font(path, n)
                    f.color = color(hx, a)
                    f.font_family = FONT_BOLD
                    b.ok(WS.set_font(path, n, f), f"font {n}")
                stats["texts"] += 1
        elif cls == "Button" and not n.startswith("RadialNode"):
            if n in PRIMARY:
                style = PRIMARY_STYLE
            elif n.startswith("Cell") or re.match(r"^(Equip(?!Action)\w+|PartySlot\d+|StorageSlot\d+)Button$", n):
                style = SLOT_STYLE
            else:
                style = None
                for group in TAB_GROUPS:
                    if n[:-len("Button")] in group:
                        style = TAB_SELECTED if n[:-len("Button")] == group[0] else TAB_IDLE
                style = style or SECONDARY_STYLE
            b.prop(n, "WidgetStyle", style); stats["buttons"] += 1
    # Tab-group label/image tints: first = selected (ivory), others idle (muted).
    for group in TAB_GROUPS:
        for i, prefix in enumerate(group):
            if f"{prefix}Label" in names:
                b.prop(f"{prefix}Label", "ColorAndOpacity", f"(SpecifiedColor={color(TEXT if i == 0 else MUTED)})")
            for s in snaps:
                if s.parent_widget and s.widget_class == "Image" and s.widget_name.startswith(prefix) and s.widget_name != f"{prefix}Button":
                    if re.match(rf"^{prefix}\d+$", s.widget_name):
                        b.prop(s.widget_name, "Brush", f"(TintColor=(SpecifiedColor={color(TEXT if i == 0 else MUTED)}),DrawAs=Image,ImageSize=(X=8.000000,Y=8.000000))")
    if "CategorySelectedRule" in names:
        b.prop("CategorySelectedRule", "Brush", f"(TintColor=(SpecifiedColor={color(GOLD)}),DrawAs=Image,ImageSize=(X=8.000000,Y=2.000000))")
    # Large backing cards that frame the main area, as in the page concepts (behind everything).
    for card, (x, y, w, h) in BACK_CARDS.get(page, {}).items():
        if not b.exists(card):
            b.add("Image", card, "PageCanvas")
        b.prop(card, "Brush", rounded(CARD, 0.86, 14, TEXT, 0.11, 1.0))
        b.rect(card, x, y, w, h)
        b.slot(card).set_z_order(-1)
        b.vis(card, "HitTestInvisible")
    b.compile_save()
    return stats, b.failed


def slot_style_defaults():
    """Grade colours readable on dark glass (rarity strips, card kind text)."""
    grades = {"COMMON": "AAB6BF", "UNCOMMON": "8FD3A4", "RARE": "7FBFEA", "HEROIC": "C9A2F0", "LEGENDARY": "E8C77D"}
    for page in ("WBP_CharacterEquipmentPage", "WBP_CharacterInventoryPage"):
        cls = unreal.load_object(None, f"{ROOT}{page}.{page}_C")
        cdo = unreal.get_default_object(cls)
        st = cdo.get_editor_property("slot_style")
        m = {}
        for k, hx in grades.items():
            r, g, bb = (common._lin(int(hx[i:i + 2], 16)) for i in (0, 2, 4))
            m[getattr(unreal.NUItemGrade, k)] = unreal.LinearColor(r, g, bb, 1.0)
        st.set_editor_property("grade_colors", m)
        gr, gg, gb = (common._lin(int(GOLD[i:i + 2], 16)) for i in (0, 2, 4))
        st.set_editor_property("selected_outline_color", unreal.LinearColor(gr, gg, gb, 1.0))
        st.set_editor_property("selected_corner_tint", unreal.LinearColor(gr, gg, gb, 0.0))
        cdo.set_editor_property("slot_style", st)


def run(pages=PAGES):
    report = {}
    for page in pages:
        report[page] = restyle(page)
    slot_style_defaults()
    for page in ("WBP_CharacterEquipmentPage", "WBP_CharacterInventoryPage"):
        Builder(ROOT + page).compile_save()
    open(r"D:/UnrealEngine/ProjectM/Saved/UICharacterCardDashboard/pages_report.txt", "w", encoding="utf-8").write(repr(report))
    print(report)


run()
