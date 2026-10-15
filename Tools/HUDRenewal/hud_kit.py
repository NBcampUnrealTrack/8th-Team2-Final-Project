# -*- coding: utf-8 -*-
"""Shared pieces for the new in-game HUD widgets (concept A, glass edge). Imported by the apply_* scripts, not run alone.

Generic tree / brush / font helpers come from the character-creation pipeline (Tools/CharacterCreateGlass/ccg_kit.py),
so the HUD and the creation / select screens are made of the same parts. HUD art lives in /Game/Art/UI/HUD2026/Kit
(source: ArtSource/UI/HUD2026/kit, drawn by make_hud_kit.py). Nothing of the previous HUD is used.
"""
import importlib
import json
import os
import sys
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(ROOT, 'Tools/CharacterCreateGlass'))
import ccg_kit
importlib.reload(ccg_kit)
from ccg_kit import (W, B, E, TOOLS, VIS, HA, VA, lin, slate, vec2, nodraw, flat, rbox, font, Tree, box, canvas,  # noqa: F401
                     text, image, sized, button, widget_blueprint, material_brush)

KIT_SRC = os.path.join(ROOT, 'ArtSource/UI/HUD2026/kit')
BACKUP = os.path.join(ROOT, 'Saved/HUDRenewal/Backup')
BASE = '/Game/Art/UI/HUD2026'
KIT = BASE + '/Kit'
FIELD = BASE + '/Field'
BATTLE = BASE + '/Battle'
FX = BASE + '/Fx'

IVORY, MUTED, GOLD, GOLD_HI, NAVY = 'F1EDE6', 'AAB6BF', 'D6BE8C', 'F0D79D', '16293D'
PARTY, FOE, GOOD = 'A6DCF2', 'E3705D', 'B9E6A4'
GLASS = ('0E1A28', 0.80)          # the creation sheet's navy, a touch denser over the bright field
GLASS_LINE = ('F1EDE6', 0.16)
META = json.load(open(os.path.join(KIT_SRC, 'kit.json'), encoding='utf-8'))


def log(message):
    unreal.log('[HUD] ' + str(message))


def tex(name, size=None, tint=('FFFFFF', 1.0), nine=True):
    """A kit texture at its 1x size (or `size`). Kit 9-slice margins are applied unless nine=False."""
    asset = unreal.load_asset(KIT + '/' + name)
    assert asset, name
    meta = META[name]
    brush = unreal.SlateBrush()
    brush.set_editor_property('resource_object', asset)
    brush.image_size = vec2(*(size or meta['size']))
    brush.tint_color = slate(*tint)
    if meta['margin'] and nine:
        brush.draw_as = unreal.SlateBrushDrawType.BOX
        brush.margin = unreal.Margin(*meta['margin'])
    else:
        brush.draw_as = unreal.SlateBrushDrawType.IMAGE
    return brush


def icon_brush(name, px=26, tint=('FFFFFF', 1.0)):
    return tex('T_HUD_Icon_' + name.capitalize(), (px, px), tint)


def import_kit():
    tasks = []
    for name in META:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(KIT_SRC, name + '.png')
        task.destination_path = KIT; task.destination_name = name
        task.automated = True; task.replace_existing = True; task.save = False
        tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    for name, meta in META.items():
        texture = unreal.load_asset(KIT + '/' + name)
        assert texture, name
        texture.set_editor_properties({
            'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI,
            'never_stream': True,
            'srgb': True,
        })
        # set both ways: a texture reimported over an old pixel-art one keeps its old filter otherwise
        texture.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST if meta['pixel'] else unreal.TextureFilter.TF_DEFAULT)
        if meta.get('mips'):
            texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        assert E.save_asset(KIT + '/' + name, False)
    log('kit textures: %d' % len(META))


# ---------- the glass panel ----------
def glass(tree, name, parent, radius=16, blur=True, strength=12.0, fill=GLASS, line=GLASS_LINE, shadow='T_HUD_Shadow', shadow_pad=32):
    """An Overlay with drop shadow, background blur, the navy fill and a hairline. Returns the name of the content
    Overlay to put children in. Blur in UE 5.8 must stay square (a corner radius stops it drawing), so it sits inset
    by ~0.3 radius inside the rounded fill."""
    outer = tree.add('Overlay', name, parent, VIS.SELF_HIT_TEST_INVISIBLE)
    try:
        box(outer)       # fill the parent slot (an Overlay parent would otherwise shrink the panel to its content)
    except Exception:
        pass             # canvas slots have no alignment --- the caller places those
    if shadow:
        box(image(tree, name + '_Shadow', name, tex(shadow)), pad=(-shadow_pad, -shadow_pad + 8, -shadow_pad, -shadow_pad - 8))
    if blur:
        inset = 12 if radius == 'pill' else max(4, int(radius * 0.32) + 1)   # pill: fits ends up to ~40 px radius
        b = box(tree.add('BackgroundBlur', name + '_Blur', name, VIS.HIT_TEST_INVISIBLE), pad=(inset, inset, inset, inset))
        b.set_editor_property('blur_strength', strength)
        b.set_editor_property('apply_alpha_to_blur', True)
    r = None if radius == 'pill' else radius
    box(image(tree, name + '_Fill', name, rbox(fill, r, line, 1.0)))
    content = tree.add('Overlay', name + '_Content', name, VIS.SELF_HIT_TEST_INVISIBLE)
    box(content)
    return name + '_Content'


def keycap(tree, name, parent, label, gold=False, px=12.5, h=24):
    """A key label in a small rounded box. Returns the TextBlock (C++ may rewrite it for pads)."""
    fill = ('D6BE8C', 1.0) if gold else ('F1EDE6', 0.09)
    outline = ('D6BE8C', 1.0) if gold else ('F1EDE6', 0.38)
    item = tree.add('Border', name, parent, VIS.HIT_TEST_INVISIBLE)
    item.set_brush(rbox(fill, 6, outline, 1.0))
    item.set_padding(unreal.Margin(7, 2, 7, 2))
    item.set_horizontal_alignment(HA['center']); item.set_vertical_alignment(VA['center'])
    t = text(tree, name + '_Text', name, label, px, True, (NAVY if gold else IVORY, 1.0))
    sizebox = None
    return t


def shadowed(item, alpha=0.65):
    """Floating text over the scene: a soft dark shadow (Slate draws one offset copy)."""
    item.set_shadow_offset(unreal.Vector2D(0, 1.5))
    item.set_shadow_color_and_opacity(lin('000000', alpha))
    return item


def backup(path):
    """Copy an existing asset's .uasset to Saved/HUDRenewal/Backup once, before the first change."""
    src = os.path.join(ROOT, path.replace('/Game/', 'Content/') + '.uasset')
    dst = os.path.join(BACKUP, path.strip('/').replace('/', '_') + '.uasset')
    if os.path.exists(src) and not os.path.exists(dst):
        os.makedirs(BACKUP, exist_ok=True)
        import shutil
        shutil.copy2(src, dst)
        log('backup ' + path)


# ---------- shared widget helpers (field and battle builders) ----------
def plain(tree, kind, name, parent, hit=None):
    """A widget that is NOT a Blueprint variable: the C++ class already has a member of the same name that it fills
    by name lookup (PartyFrame, ChatScrollBox, ChatTabBox), so a generated variable would collide."""
    result = W.add_component(tree.path, kind, name, parent, False)
    assert result.success, (kind, name, parent, result.error_message)
    item = tree.get(name)
    if hit is not None:
        item.set_visibility(hit)
    return item


def cdo(tree):
    return unreal.get_default_object(tree.bp.generated_class())


def clip(widget):
    widget.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    return widget


def label(tree, name, parent, value, px, bold=True, colour=(IVORY, 1.0), spacing=0, shadow=True, wrap=False):
    item = text(tree, name, parent, value, px, bold, colour, spacing, wrap)
    if shadow:
        shadowed(item)
    return item


def hbox(tree, name, parent):
    return tree.add('HorizontalBox', name, parent, VIS.SELF_HIT_TEST_INVISIBLE)


def vbox(tree, name, parent):
    return tree.add('VerticalBox', name, parent, VIS.SELF_HIT_TEST_INVISIBLE)


def overlay(tree, name, parent, hit=VIS.SELF_HIT_TEST_INVISIBLE):
    return tree.add('Overlay', name, parent, hit)


def bar(tree, name, parent, fill_tex, track=True, height=8):
    """A ProgressBar with a kit fill and a dark rounded track."""
    item = tree.add('ProgressBar', name, parent, VIS.HIT_TEST_INVISIBLE)
    style = item.get_editor_property('widget_style').copy()
    style.background_image = rbox(('FFFFFF', 0.10 if track else 0.0), height / 2.0, None, 0.0, (16, height))
    fill = tex(fill_tex, (64, height))
    fill.draw_as = unreal.SlateBrushDrawType.ROUNDED_BOX if False else unreal.SlateBrushDrawType.IMAGE
    style.fill_image = fill
    style.marquee_image = nodraw()
    item.set_editor_property('widget_style', style)
    item.set_editor_property('fill_color_and_opacity', unreal.LinearColor(1, 1, 1, 1))
    item.set_percent(0.8)
    return item


def flat_button(tree, name, parent, normal=None, hovered=None, pressed=None):
    return button(tree, name, parent, normal or nodraw(), hovered or normal or nodraw(), pressed or hovered or normal or nodraw())


def pill_button_brushes(gold):
    if gold:
        return (rbox(('D6BE8C', 1.0), None), rbox(('F0D79D', 1.0), None), rbox(('C2A874', 1.0), None))
    return (rbox(('F1EDE6', 0.06), None, ('F1EDE6', 0.28), 1.0), rbox(('F1EDE6', 0.12), None, ('F1EDE6', 0.40), 1.0),
            rbox(('F1EDE6', 0.18), None, ('F1EDE6', 0.45), 1.0))
