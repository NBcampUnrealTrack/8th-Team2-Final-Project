"""Author the quest-offer layout from the supplied navy/paper design reference.

Uses existing widgets and textures. Importing this module does not mutate assets.
Call apply() to preview the changes; apply(save=True) also saves the asset.
Runtime data is populated by NUQuestWidget.cpp, using optional row templates.
"""

import json
import shutil
from datetime import datetime
from pathlib import Path

import unreal


WIDGET = "/Game/Developers/zxcv1/Quest/UI/WBP_QuestWidget"
ART = "/Game/Developers/MinkiCho/Art/Quest/JournalV4/Textures/T_QuestV4_"
TITLE_FONT = "/Game/Art/Font/MaruBuri-Bold_Font"
OFFER_ART = "/Game/Developers/MinkiCho/Art/Quest/Offer/Textures/"
OFFER_PARTS = {
    "QO_Compass": ("T_QuestOffer_Compass", 117, 117),
    "QO_LowerCompass": ("T_QuestOffer_City", 198, 214),
}
WIDTH, HEIGHT, RAIL = 980, 660, 238
INK = "(R=.014,G=.034,B=.055,A=1)"
MUTED = "(R=.18,G=.23,B=.28,A=1)"
GOLD = "(R=.64,G=.49,B=.27,A=1)"
IVORY = "(R=.90,G=.85,B=.70,A=1)"
W = unreal.WidgetService
B = unreal.BlueprintService
E = unreal.EditorAssetLibrary
T = unreal.EditorTransactionService
BUTTON_SOURCES = {
    "AcceptButton": ("/Game/Developers/zxcv1/Quest/UI/WBP_RewardPopup", "CloseButton"),
    "DeclineButton": ("/Game/Developers/dbals/UI/WBP_ConfirmDialog", "CancelButton"),
}

REQUIRED = {
    "OfferPanel": "Border", "OfferContentBox": "VerticalBox",
    "OfferBodyScroll": "ScrollBox", "OfferBodyContent": "VerticalBox",
    "OfferTypeHeading": "TextBlock", "OfferTitleText": "TextBlock",
    "OfferSummaryText": "TextBlock", "OfferObjectiveHeading": "TextBlock",
    "OfferObjectiveText": "TextBlock", "OfferRewardText": "TextBlock",
    "OfferActionRow": "HorizontalBox", "AcceptButton": "Button",
    "DeclineButton": "Button", "AcceptButton_Label": "TextBlock",
    "DeclineButton_Label": "TextBlock",
}


def widget(name):
    return unreal.find_object(unreal.find_object(unreal.load_asset(WIDGET), "WidgetTree"), name)


def set_property(name, prop, value):
    if not W.set_property(WIDGET, name, prop, str(value)):
        raise RuntimeError(f"Could not set {name}.{prop}: {value}")


def attach(name, parent):
    child, container = widget(name), widget(parent)
    if child.get_parent() == container:
        return
    child.modify()
    if child.get_parent():
        child.get_parent().modify()
    container.modify()
    if not container.add_child(child):
        raise RuntimeError(f"Could not attach {name} to {parent}")


def add(kind, name, parent):
    existing = widget(name)
    if existing and unreal.SystemLibrary.is_valid(existing):
        matches = existing.get_class().get_name() == kind+"_C" if kind.startswith("WBP_") else isinstance(existing, getattr(unreal, kind))
        if not matches:
            raise RuntimeError(f"Unexpected existing component: {name}")
        attach(name, parent)
    else:
        result = W.add_component(WIDGET, kind, name, parent, True)
        if not result.success:
            raise RuntimeError(str(result.error_message))
    set_property(name, "Visibility", "SelfHitTestInvisible")


def padding(name, left=0, top=0, right=0, bottom=0, prop="Slot.Padding"):
    set_property(name, prop, f"(Left={left},Top={top},Right={right},Bottom={bottom})")


def canvas(name, x, y, width, height, z=0):
    for key, value in {
        "Anchor Min X": 0, "Anchor Max X": 0,
        "Anchor Min Y": 0, "Anchor Max Y": 0,
        "Position X": x, "Position Y": y,
        "Size X": width, "Size Y": height,
        "Slot.LayoutData.Alignment": "(X=0,Y=0)", "Slot.bAutoSize": "False", "Slot.ZOrder": z,
    }.items():
        set_property(name, key, value)


def brush(name, prop, texture=None, tint="(R=1,G=1,B=1,A=1)", draw="Image"):
    info = unreal.WidgetBrushInfo()
    info.resource_path = ART + texture if texture else ""
    info.draw_as = draw
    info.tint_color = tint
    info.image_size = "(X=128,Y=128)"
    info.margin = "(Left=.12,Top=.09,Right=.12,Bottom=.09)"
    if texture == "Control":
        info.margin = "(Left=.18,Top=.28,Right=.18,Bottom=.28)"
    if not W.set_brush(WIDGET, name, prop, info):
        raise RuntimeError(f"Could not set brush: {name}.{prop}")
    if not texture:
        set_property(name, prop+".ResourceObject", "None")


def offer_art(name, rect):
    image(name, "QO_Stage", rect, z=2)
    asset, w, h = OFFER_PARTS[name]
    info = unreal.WidgetBrushInfo()
    info.resource_path = OFFER_ART + asset
    info.draw_as = "Image"
    info.tint_color = "(R=1,G=1,B=1,A=1)"
    if not W.set_brush(WIDGET, name, "Brush", info):
        raise RuntimeError(name)
    # Two source pixels around each crop preserve filtering at the region edges.
    set_property(name, "Brush.UVRegion",
                 f"(Min=(X={2/(w+4)},Y={2/(h+4)}),Max=(X={(w+2)/(w+4)},Y={(h+2)/(h+4)}),bIsValid=True)")


def font(name, size, color, bold=False, family=None):
    info = W.get_font(WIDGET, name)
    info.size = size
    if family:
        info.font_family = family
        info.typeface = "Default"
    else:
        info.typeface = "Bold" if bold else "Regular"
    info.color = f"(SpecifiedColor={color},ColorUseRule=UseColor_Specified)"
    info.outline_size = 0
    info.shadow_offset = "(X=0,Y=0)"
    info.shadow_color = "(R=0,G=0,B=0,A=0)"
    if not W.set_font(WIDGET, name, info):
        raise RuntimeError(f"Could not set font: {name}")


def image(name, parent, rect, texture=None, tint="(R=1,G=1,B=1,A=1)", z=0):
    add("Image", name, parent)
    brush(name, "Brush", texture, tint)
    canvas(name, *rect, z=z)
    set_property(name, "Visibility", "HitTestInvisible")


def label(name, parent, rect, text, size, color):
    add("TextBlock", name, parent)
    set_property(name, "Text", text)
    set_property(name, "Justification", "Center")
    font(name, size, color)
    canvas(name, *rect, z=2)
    set_property(name, "Visibility", "HitTestInvisible")


def slot_snapshot(slot):
    # Struct repr includes a transient address; compare only persistent values.
    return {
        "type": slot.slot_type,
        "anchor_min": [slot.anchor_min.x, slot.anchor_min.y],
        "anchor_max": [slot.anchor_max.x, slot.anchor_max.y],
        "offsets": [slot.offsets.left, slot.offsets.top, slot.offsets.right, slot.offsets.bottom],
        "alignment": [slot.alignment.x, slot.alignment.y],
        "z_order": slot.z_order, "auto_size": slot.auto_size,
        "size_rule": slot.size_rule, "size_value": slot.size_value,
        "padding": [slot.padding.left, slot.padding.top, slot.padding.right, slot.padding.bottom],
        "horizontal_alignment": str(slot.horizontal_alignment),
        "vertical_alignment": str(slot.vertical_alignment),
    }


def snapshot():
    return [{
        "name": s.widget_name, "class": s.widget_class, "parent": s.parent_widget,
        "children": list(s.children), "slot": slot_snapshot(s.slot_info),
        "properties": {p.property_name: p.current_value for p in s.properties},
    } for s in W.get_widget_snapshot(WIDGET)]


def descendants(rows, root):
    names = {root}
    changed = True
    while changed:
        old = len(names)
        names.update(row["name"] for row in rows if row["parent"] in names)
        changed = len(names) != old
    return names


def style_action_buttons():
    """Reuse the shipped reward/confirmation button states without replacing bindings."""
    for name, (source_path, source_name) in BUTTON_SOURCES.items():
        source_bp = unreal.load_asset(source_path)
        source = unreal.find_object(unreal.find_object(source_bp, "WidgetTree"), source_name)
        target = widget(name)
        if not isinstance(source, unreal.Button) or not isinstance(target, unreal.Button):
            raise RuntimeError(f"Missing button style source or target: {name}")
        current = target.get_editor_property("widget_style")
        style = source.get_editor_property("widget_style").copy()
        # Retain any offer-specific audio while sharing the visual states and press offset.
        for prop in ("pressed_slate_sound", "clicked_slate_sound", "hovered_slate_sound"):
            setattr(style, prop, getattr(current, prop))
        target.modify()
        target.set_style(style)
        target.set_background_color(unreal.LinearColor(1, 1, 1, 1))
        target.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
    font("AcceptButton_Label", 21, IVORY, bold=True)
    font("DeclineButton_Label", 20, IVORY)


def build():
    # Keep the original subtree attached throughout service edits/compilation.
    stage = widget("QO_Stage")
    if not stage or not unreal.SystemLibrary.is_valid(stage):
        add("CanvasPanel", "QO_Stage", widget("OfferPanel").get_parent().get_name())
    attach("OfferContentBox", "QO_Stage")
    attach("QO_Stage", "OfferPanel")
    padding("OfferPanel", prop="Padding")
    brush("OfferPanel", "Background", draw="NoDrawType")
    for key in ("Anchor Min X", "Anchor Max X", "Anchor Min Y", "Anchor Max Y"):
        set_property("OfferPanel", key, .5)
    for key, value in {"Position X": 0, "Position Y": 0, "Size X": WIDTH,
                       "Size Y": HEIGHT, "Slot.LayoutData.Alignment": "(X=.5,Y=.5)",
                       "Slot.bAutoSize": "False", "Slot.ZOrder": 20}.items():
        set_property("OfferPanel", key, value)

    # Solid-image rules inherit opacity, including when the whole offer is faded.
    image("QO_GoldFrame", "QO_Stage", (0, 0, WIDTH, HEIGHT), tint=GOLD)
    image("QO_NavyFrame", "QO_Stage", (2, 2, WIDTH-4, HEIGHT-4), tint=INK)
    image("QO_Rail", "QO_Stage", (10, 10, RAIL-10, HEIGHT-20), tint="(R=.007,G=.022,B=.039,A=1)")
    for suffix, rect in {
        "Left": (14, 14, 1, HEIGHT-28), "Top": (14, 14, RAIL-28, 1),
        "Bottom": (14, HEIGHT-15, RAIL-28, 1), "Spine": (RAIL-1, 10, 2, HEIGHT-20),
    }.items():
        image("QO_RailRule"+suffix, "QO_Stage", rect, tint=GOLD, z=1)
    offer_art("QO_Compass", (58, 44, 122, 122))
    attach("OfferTypeHeading", "QO_Stage")
    canvas("OfferTypeHeading", 18, 192, RAIL-36, 40, z=2)
    set_property("OfferTypeHeading", "Text", "새로운 의뢰")
    set_property("OfferTypeHeading", "Justification", "Center")
    font("OfferTypeHeading", 23, IVORY, family=TITLE_FONT)
    label("QO_NewQuest", "QO_Stage", (20, 234, RAIL-40, 28), "NEW QUEST", 13, GOLD)
    image("QO_RailDivider", "QO_Stage", (38, 282, RAIL-76, 1), tint=GOLD, z=1)
    offer_art("QO_LowerCompass", (18, 315, RAIL-36, 235))
    image("QO_RailFooterRule", "QO_Stage", (24, 560, RAIL-48, 1), tint=GOLD, z=1)
    label("QO_RailFooter", "QO_Stage", (24, 576, RAIL-48, 65),
          "SMALL REQUESTS\nMAKE A BIGGER\nTOMORROW.", 11, GOLD)

    add("Border", "QO_Paper", "QO_Stage")
    canvas("QO_Paper", RAIL+2, 10, WIDTH-RAIL-12, HEIGHT-20)
    brush("QO_Paper", "Background", tint="(R=.88,G=.855,B=.80,A=1)")
    padding("QO_Paper", prop="Padding")
    set_property("QO_Paper", "Clipping", "ClipToBounds")
    add("Overlay", "QO_PaperLayers", "QO_Paper")
    add("SizeBox", "QO_WatermarkSize", "QO_PaperLayers")
    set_property("QO_WatermarkSize", "WidthOverride", 240)
    set_property("QO_WatermarkSize", "HeightOverride", 240)
    set_property("QO_WatermarkSize", "Slot.HorizontalAlignment", "HAlign_Right")
    set_property("QO_WatermarkSize", "Slot.VerticalAlignment", "VAlign_Top")
    padding("QO_WatermarkSize", top=32, right=-45)
    add("Image", "QO_Watermark", "QO_WatermarkSize")
    brush("QO_Watermark", "Brush", "Compass", "(R=.20,G=.23,B=.25,A=.06)")
    set_property("QO_WatermarkSize", "Visibility", "HitTestInvisible")
    add("Border", "QO_ContentMargin", "QO_PaperLayers")
    set_property("QO_ContentMargin", "Slot.HorizontalAlignment", "HAlign_Fill")
    set_property("QO_ContentMargin", "Slot.VerticalAlignment", "VAlign_Fill")
    brush("QO_ContentMargin", "Background", draw="NoDrawType")
    padding("QO_ContentMargin", 52, 38, 42, 28, prop="Padding")
    attach("OfferContentBox", "QO_ContentMargin")

    add("HorizontalBox", "QO_LocationRow", "OfferBodyContent")
    add("TextBlock", "QO_LocationMarker", "QO_LocationRow")
    set_property("QO_LocationMarker", "Text", "◆")
    font("QO_LocationMarker", 13, INK)
    padding("QO_LocationMarker", right=9)
    add("TextBlock", "QO_LocationText", "QO_LocationRow")
    set_property("QO_LocationText", "Text", "리베르그로브")
    font("QO_LocationText", 16, INK)
    add("VerticalBox", "QO_ObjectiveRows", "OfferBodyContent")
    add("TextBlock", "QO_RewardHeading", "OfferBodyContent")
    set_property("QO_RewardHeading", "Text", "✦  보상  ─────────────────────")
    font("QO_RewardHeading", 21, INK, bold=True)
    add("WrapBox", "QO_RewardRows", "OfferBodyContent")
    set_property("QO_RewardRows", "InnerSlotPadding", "(X=22,Y=14)")
    add("WBP_QuestJournalRewardTile", "QO_RewardTemplate", "OfferBodyContent")
    add("WBP_QuestJournalObjectiveRow", "QO_ObjectiveTemplate", "OfferBodyContent")
    set_property("QO_RewardTemplate", "Visibility", "Collapsed")
    set_property("QO_ObjectiveTemplate", "Visibility", "Collapsed")
    order = ["OfferTitleText", "QO_LocationRow", "OfferSummaryText", "OfferObjectiveHeading",
             "QO_ObjectiveRows", "OfferObjectiveText", "QO_RewardHeading", "QO_RewardRows",
             "OfferRewardText", "QO_RewardTemplate", "QO_ObjectiveTemplate"]
    parent = widget("OfferBodyContent")
    # Reattach without service calls in between so every object stays live.
    for name in order:
        child = widget(name)
        child.modify()
        child.remove_from_parent()
        parent.add_child(child)
        set_property(name, "Slot.HorizontalAlignment", "HAlign_Fill")
    padding("QO_LocationRow", bottom=20)
    padding("QO_ObjectiveRows", bottom=26)
    padding("QO_RewardHeading", bottom=14)
    padding("QO_RewardRows", left=8, bottom=16)

    set_property("OfferBodyScroll", "Slot.Size", "(SizeRule=Fill,Value=1)")
    set_property("OfferBodyScroll", "ScrollBarVisibility", "Collapsed")
    set_property("OfferBodyScroll", "AllowOverscroll", "False")
    padding("OfferBodyScroll")
    set_property("OfferActionRow", "Slot.Size", "(SizeRule=Automatic,Value=1)")
    padding("OfferActionRow", top=20)

    font("OfferTitleText", 34, INK, family=TITLE_FONT)
    font("OfferSummaryText", 18, INK)
    font("OfferObjectiveHeading", 21, INK, bold=True)
    set_property("OfferObjectiveHeading", "Text", "✦  목표  ─────────────────────")
    font("OfferObjectiveText", 17, MUTED)
    # Keep the existing localized reward header and actual values supplied by C++.
    font("OfferRewardText", 18, INK)
    for name in ("OfferTitleText", "OfferSummaryText", "OfferObjectiveText", "OfferRewardText"):
        set_property(name, "AutoWrapText", "True")
        set_property(name, "WrapTextAt", 0)
        set_property(name, "Justification", "Left")
        set_property(name, "Slot.Size", "(SizeRule=Automatic,Value=1)")
    padding("OfferTitleText", bottom=22)
    padding("OfferSummaryText", bottom=28)
    padding("OfferObjectiveHeading", bottom=12)
    padding("OfferObjectiveText", left=8, bottom=28)
    padding("OfferRewardText", left=8, bottom=12)

    # Keep the existing Button objects, bindings, sounds, and focus behavior.
    for name, factor in (("AcceptButton", 1.35), ("DeclineButton", 1.0)):
        size_box = widget(name).get_parent()
        if not isinstance(size_box, unreal.SizeBox):
            raise RuntimeError(f"Unexpected button container: {name}")
        container = size_box.get_name()
        set_property(container, "bOverride_WidthOverride", "False")
        set_property(container, "HeightOverride", 62)
        set_property(container, "Slot.Size", f"(SizeRule=Fill,Value={factor})")
        set_property(container, "Slot.HorizontalAlignment", "HAlign_Fill")
        set_property(container, "Slot.VerticalAlignment", "VAlign_Fill")
        set_property(name, "Slot.HorizontalAlignment", "HAlign_Fill")
        set_property(name, "Slot.VerticalAlignment", "VAlign_Fill")
        padding(container)
        set_property(name, "IsFocusable", "True")
        set_property(name, "BackgroundColor", "(R=1,G=1,B=1,A=1)")
        set_property(name, "ColorAndOpacity", "(R=1,G=1,B=1,A=1)")
    if widget("Spacer_145"):
        set_property("Spacer_145", "Size", "(X=22,Y=1)")
        set_property("Spacer_145", "Slot.Size", "(SizeRule=Automatic,Value=1)")
    set_property("AcceptButton_Label", "Text", "수락하기")
    set_property("DeclineButton_Label", "Text", "나중에")
    style_action_buttons()


def apply(save=False):
    """Apply in one Undo transaction; never save unless explicitly requested."""
    if W.is_pie_running():
        raise RuntimeError("Stop PIE before changing the quest-offer layout.")
    bp = unreal.load_asset(WIDGET)
    if not bp:
        raise RuntimeError(f"Missing asset: {WIDGET}")
    before = snapshot()
    types = {s["name"]: s["class"] for s in before}
    for name, kind in REQUIRED.items():
        if types.get(name) != kind:
            raise RuntimeError(f"Preflight: {name} must be {kind}, found {types.get(name)}")
    for path in [TITLE_FONT] + [OFFER_ART+p[0] for p in OFFER_PARTS.values()] + [ART+n for n in ("Compass",)] + [p for p, _ in BUTTON_SOURCES.values()]:
        if not E.does_asset_exist(path):
            raise RuntimeError(f"Missing design asset: {path}")
    tracker_before = [s for s in before if s["name"] in descendants(before, "TrackerPanel")
                      or s["name"] in descendants(before, "TrackerReopenButton")]
    output = Path(unreal.Paths.project_saved_dir()) / "UIQuestOfferReference" / datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    output.mkdir(parents=True, exist_ok=False)
    (output / "before.json").write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding="utf-8")
    asset_file = Path(unreal.Paths.project_content_dir()) / (WIDGET.removeprefix("/Game/")+".uasset")
    shutil.copy2(asset_file, output / "WBP_QuestWidget.before.uasset")
    if not T.begin_transaction("Quest offer: navy rail and paper layout"):
        raise RuntimeError("Could not begin Undo transaction")
    try:
        bp.modify()
        unreal.find_object(bp, "WidgetTree").modify()
        for row in before:
            item = widget(row["name"])
            item.modify()
            if item.slot:
                item.slot.modify()
        build()
        after = snapshot()
        offer_names = descendants(after, "OfferPanel")
        if not set(REQUIRED).issubset(offer_names):
            raise RuntimeError("A required offer widget was detached")
        tracker_after = [s for s in after if s["name"] in descendants(after, "TrackerPanel")
                         or s["name"] in descendants(after, "TrackerReopenButton")]
        if tracker_before != tracker_after:
            raise RuntimeError("Tracker changed unexpectedly")
        valid = W.validate(WIDGET)
        if not valid.is_valid:
            raise RuntimeError(str(valid.errors))
        compiled = B.compile_blueprint(WIDGET)
        if not compiled.success or compiled.num_errors:
            raise RuntimeError(str(compiled))
    except Exception:
        T.end_transaction()
        result = T.undo()
        if not result.success:
            unreal.log_error("Quest offer rollback failed; asset was not saved. Inspect the Undo history.")
        raise
    T.end_transaction()
    (output / "after.json").write_text(json.dumps(after, ensure_ascii=False, indent=2), encoding="utf-8")
    if save and not E.save_asset(WIDGET):
        raise RuntimeError("Compile succeeded but saving WBP_QuestWidget failed")
    unreal.log(f"Quest offer layout prepared. Saved={save}. Snapshot={output}")
    return output
