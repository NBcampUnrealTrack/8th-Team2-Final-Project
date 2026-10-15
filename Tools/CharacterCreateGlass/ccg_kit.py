# -*- coding: utf-8 -*-
"""Shared pieces for the glass-sheet screens (creation, select): paths, colours, brushes, fonts and the widget-tree helper.
Imported by apply_ui.py and apply_select.py; not run on its own."""
import json
import os
import shutil
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
KIT_SRC = os.path.join(ROOT, 'ArtSource/UI/CharacterCreateGlass_20261013/kit')
BACKUP = os.path.join(ROOT, 'Saved/CharacterCreateGlass/Backup')
BASE = '/Game/Art/UI/CharacterCreate/Glass'
KIT = BASE + '/Kit'
MAIN = BASE + '/WBP_CharacterCreate_Glass'
SEG = BASE + '/WBP_CCG_SegButton'
CHIP = BASE + '/WBP_CCG_RaceChip'
SWATCH = BASE + '/WBP_CCG_Swatch'
HAIR_COLOURS = 11        # swatches put in the row; the C++ folds the ones the palette (project settings) does not have
EMBLEMS = '/Game/Developers/dbals/UI/RaceEmblems/T_RaceEmblem_'
GAME_MODES = ['/Game/GameMode/BP_CharCreateGameMode', '/Game/Developers/dbals/BP_dbals_CharCreateGameMode']
FONT_BOLD = '/Game/Art/Font/MaruBuri-Bold_Font'
FONT_SEMI = '/Game/Art/Font/MaruBuri-SemiBold_Font'

W = unreal.WidgetService
B = unreal.BlueprintService
E = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
VIS = unreal.SlateVisibility
HA = {'fill': unreal.HorizontalAlignment.H_ALIGN_FILL, 'left': unreal.HorizontalAlignment.H_ALIGN_LEFT,
      'center': unreal.HorizontalAlignment.H_ALIGN_CENTER, 'right': unreal.HorizontalAlignment.H_ALIGN_RIGHT}
VA = {'fill': unreal.VerticalAlignment.V_ALIGN_FILL, 'top': unreal.VerticalAlignment.V_ALIGN_TOP,
      'center': unreal.VerticalAlignment.V_ALIGN_CENTER, 'bottom': unreal.VerticalAlignment.V_ALIGN_BOTTOM}

IVORY, NAVY, GOLD = 'F1EDE6', '16293D', 'D6C59C'


def log(message):
    unreal.log('[CCG] ' + str(message))


# ---------- colour, brush, font ----------
def lin(hex_rgb, alpha=1.0):
    """sRGB hex -> the linear colour Slate stores."""
    parts = [int(hex_rgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return unreal.LinearColor(*[v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in parts], alpha)


def slate(hex_rgb, alpha=1.0):
    return unreal.SlateColor(specified_color=lin(hex_rgb, alpha))


def vec2(x, y):
    kind = getattr(unreal, 'DeprecateSlateVector2D', None)
    if kind is None:
        return unreal.Vector2D(x, y)
    value = kind()      # no (x, y) constructor and no fields in Python: fill it from text
    value.import_text('(X=%f,Y=%f)' % (x, y))
    return value


def nodraw():
    brush = unreal.SlateBrush(); brush.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
    return brush


def flat(hex_rgb, alpha=1.0, size=(8, 8)):
    """A plain colour (hairlines)."""
    brush = unreal.SlateBrush(); brush.draw_as = unreal.SlateBrushDrawType.IMAGE
    brush.tint_color = slate(hex_rgb, alpha); brush.image_size = vec2(*size)
    return brush


def rbox(fill=('FFFFFF', 0.0), radius=None, outline=None, width=0.0, size=(16, 16)):
    """Rounded box drawn by Slate. radius None = half the height (a pill / a circle)."""
    brush = unreal.SlateBrush(); brush.draw_as = unreal.SlateBrushDrawType.ROUNDED_BOX
    brush.tint_color = slate(*fill); brush.image_size = vec2(*size)
    o = brush.outline_settings
    if radius is None:
        o.rounding_type = unreal.SlateBrushRoundingType.HALF_HEIGHT_RADIUS
    else:
        o.rounding_type = unreal.SlateBrushRoundingType.FIXED_RADIUS
        o.corner_radii = unreal.Vector4(radius, radius, radius, radius)
    o.width = width
    o.color = slate(*outline) if outline else slate('FFFFFF', 0.0)
    brush.outline_settings = o
    return brush


def tex(name, size, margin=None, tint=('FFFFFF', 1.0)):
    """A kit texture. size = the 1x size it was drawn for; margin (l,t,r,b fractions) makes it a 9-slice."""
    asset = unreal.load_asset(KIT + '/' + name)
    assert asset, name
    brush = unreal.SlateBrush()
    brush.set_editor_property('resource_object', asset)
    brush.image_size = vec2(*size)
    brush.tint_color = slate(*tint)
    if margin:
        brush.draw_as = unreal.SlateBrushDrawType.BOX
        brush.margin = unreal.Margin(*margin)
    else:
        brush.draw_as = unreal.SlateBrushDrawType.IMAGE
    return brush


def typeface(path):
    font = unreal.load_asset(path)
    assert font, path
    try:
        fonts = font.get_editor_property('composite_font').get_editor_property('default_typeface').get_editor_property('fonts')
        return font, str(fonts[0].get_editor_property('name'))
    except Exception as error:
        log('typeface lookup failed (%s), using Default' % error)
        return font, 'Default'


FONTS = {}


def font(widget, px, bold=False, spacing=0):
    key = FONT_BOLD if bold else FONT_SEMI
    if key not in FONTS:
        FONTS[key] = typeface(key)
    info = unreal.SlateFontInfo()
    info.font_object, info.typeface_font_name = FONTS[key]
    info.size = px * 0.75
    info.letter_spacing = spacing
    widget.set_font(info)


# ---------- widget tree ----------
class Tree:
    def __init__(self, path):
        self.path = path
        self.bp = unreal.load_asset(path)
        assert self.bp, path
        self.tree = unreal.find_object(self.bp, 'WidgetTree')
        self.bp.modify(); self.tree.modify()

    def clear(self):
        """Empty the tree for a rebuild. The root itself cannot be removed, so it is kept and reused."""
        self.root = W.get_root_widget(self.path) or ''
        if not self.root:
            return
        root = self.get(self.root)
        for name in [root.get_child_at(i).get_name() for i in range(root.get_children_count())]:
            result = W.remove_component(self.path, name, True)
            assert result.success, (name, result.error_message)

    def get(self, name):
        item = unreal.find_object(self.tree, name)
        assert item, (self.path, name)
        return item

    def add(self, kind, name, parent='', hit=None):
        if not (parent == '' and name == getattr(self, 'root', '')):      # the kept root of an earlier run
            result = W.add_component(self.path, kind, name, parent, True)
            assert result.success, (kind, name, parent, result.error_message)
        item = self.get(name)
        if hit is not None:
            item.set_visibility(hit)
        return item

    def finish(self):
        result = B.compile_blueprint(self.path)
        assert result.success, (self.path, str(result))
        assert E.save_asset(self.path, False), self.path
        log('compiled + saved ' + self.path)


def box(widget, pad=(0, 0, 0, 0), h='fill', v='fill', fill=None):
    """Slot of a vertical / horizontal box, overlay, border or button."""
    slot = widget.slot
    slot.set_padding(unreal.Margin(*pad))
    slot.set_horizontal_alignment(HA[h]); slot.set_vertical_alignment(VA[v])
    if fill is not None:
        slot.set_size(unreal.SlateChildSize(value=fill, size_rule=unreal.SlateSizeRule.FILL))
    return widget


def canvas(widget, anchors, offsets, align=(0, 0), auto=False, z=0):
    slot = widget.slot
    slot.set_anchors(unreal.Anchors(unreal.Vector2D(anchors[0], anchors[1]), unreal.Vector2D(anchors[2], anchors[3])))
    slot.set_alignment(unreal.Vector2D(*align))
    slot.set_offsets(unreal.Margin(*offsets))
    slot.set_auto_size(auto); slot.set_z_order(z)
    return widget


def text(tree, name, parent, value, px, bold=False, colour=(IVORY, 1.0), spacing=0, wrap=False):
    item = tree.add('TextBlock', name, parent, VIS.HIT_TEST_INVISIBLE)
    item.set_text(value)
    font(item, px, bold, spacing)
    item.set_color_and_opacity(slate(*colour))
    item.set_shadow_offset(unreal.Vector2D(0, 0))
    if wrap:
        item.set_auto_wrap_text(True)
    return item


def image(tree, name, parent, brush, hit=VIS.HIT_TEST_INVISIBLE):
    item = tree.add('Image', name, parent, hit)
    item.set_brush(brush)
    return item


def sized(tree, name, parent, width=None, height=None):
    item = tree.add('SizeBox', name, parent)
    if width is not None:
        item.set_width_override(width)
    if height is not None:
        item.set_height_override(height)
    return item


def button(tree, name, parent, normal, hovered=None, pressed=None, disabled=None, pad=(0, 0, 0, 0)):
    item = tree.add('Button', name, parent)
    style = item.get_editor_property('widget_style').copy()
    style.normal = normal
    style.hovered = hovered or normal
    style.pressed = pressed or hovered or normal
    style.disabled = disabled or normal
    style.normal_padding = unreal.Margin(*pad); style.pressed_padding = unreal.Margin(*pad)
    item.set_style(style)
    return item


def material_brush(path, size, tint=('FFFFFF', 1.0)):
    """A UI material drawn as an image (the motion that needs no widget tick)."""
    material = unreal.load_asset(path)
    assert material, path + ' missing: run.ps1 -Steps fx first'
    brush = unreal.SlateBrush(); brush.draw_as = unreal.SlateBrushDrawType.IMAGE
    brush.set_editor_property('resource_object', material)
    brush.image_size = vec2(*size)
    brush.tint_color = slate(*tint)
    return brush


STAGE_W, STAGE_H, STAGE_SHIFT = 2560, 1080, 320       # the stage art at 1080p; the approved 16:9 frame starts at x 320
FX = BASE + '/Fx'


def stage_frame(tree, name, parent):
    """A 2560 x 1080 layer scaled like the stage camera: height fills the screen, width overflows centred (wider screens
    see more street, narrower ones lose the sides). Returns the canvas inside it; place things in stage 1080p units."""
    scale = canvas(tree.add('ScaleBox', name, parent, VIS.HIT_TEST_INVISIBLE), (0, 0, 1, 1), (0, 0, 0, 0))
    scale.set_editor_property('stretch', unreal.Stretch.SCALE_TO_FIT_Y)
    sized(tree, name + 'Frame', name, STAGE_W, STAGE_H)
    return tree.add('CanvasPanel', name + 'Layer', name + 'Frame', VIS.HIT_TEST_INVISIBLE)


def ambience(tree, name, parent):
    """Lanterns breathing and embers rising (M_CCG_StageAmbience), over the whole stage frame."""
    return canvas(image(tree, name, parent, material_brush(FX + '/M_CCG_StageAmbience', (STAGE_W, STAGE_H))), (0, 0, 0, 0), (0, 0, STAGE_W, STAGE_H))


def intro_veil(tree, parent):
    """A black sheet over everything that the C++ lifts when the screen opens (rests invisible)."""
    veil = canvas(image(tree, 'IntroVeil', parent, flat('05070B', 1.0)), (0, 0, 1, 1), (0, 0, 0, 0), z=10)
    veil.set_render_opacity(0.0)
    return veil


def shine(tree, name, parent):
    """The light band over a 426 x 64 main button (M_CCG_Shine). Hidden until the C++ says the button works."""
    item = box(image(tree, name, parent, material_brush(FX + '/M_CCG_Shine', (426, 64))))
    item.set_visibility(VIS.COLLAPSED)
    return item


def ghost(alpha_fill, alpha_line=0.22):
    return rbox(('FFFFFF', alpha_fill), None, ('FFFFFF', alpha_line), 1.0)


# ---------- assets ----------
def import_kit():
    meta = json.load(open(os.path.join(KIT_SRC, 'kit.json'), encoding='utf-8'))
    tasks = []
    for name in meta:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(KIT_SRC, name + '.png')
        task.destination_path = KIT; task.destination_name = name
        task.automated = True; task.replace_existing = True; task.save = False
        tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    for name in meta:
        texture = unreal.load_asset(KIT + '/' + name)
        assert texture, name
        texture.set_editor_properties({
            'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,   # "UserInterface2D": uncompressed
            'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI,
            'never_stream': True,
            'srgb': True,
        })
        if 'Sparkle' in name:      # pixel art: keep the cells sharp
            texture.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
        assert E.save_asset(KIT + '/' + name, False)
    log('kit textures: %d' % len(meta))


def widget_blueprint(path, parent_class):
    if not E.does_asset_exist(path):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property('parent_class', unreal.load_class(None, parent_class))
        folder, name = path.rsplit('/', 1)
        assert TOOLS.create_asset(name, folder, unreal.WidgetBlueprint, factory), path
        log('created ' + path)
    tree = Tree(path)
    tree.clear()
    return tree
