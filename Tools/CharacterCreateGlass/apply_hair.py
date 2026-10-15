# -*- coding: utf-8 -*-
"""Hair colour: builds the three materials that tint the hair layer. Run headless through run.ps1 -Steps hair.

  /Game/Art/Materials/HairTint/M_NUCharacterHairTint   field, battle and the creation-screen stand: a copy of M_NUCharacterSprite with one node swapped
  /Game/Art/Materials/HairTint/M_NUHairTint_UI         UI images: select-screen portrait, hair thumbnails

Both do the same thing: keep only the brightness of the hair drawing and colour it with the chosen hair colour.
The world material reads the colour from the vertex colour (the sprite colour the C++ sets on the hair layer); the UI one from a parameter.
The hair art is a light blond with a black outline, so brightness alone carries the shading; the outline stays black.

The field material is a COPY: when M_NUCharacterSprite's graph changes, run this step again (run.ps1 deletes the two assets first).
Paths are the defaults of UNUAppearanceDataSettings (project settings > NU Appearance Data > Hair Color).
"""
import unreal

FOLDER = '/Game/Art/Materials/HairTint'
SOURCE = '/Game/Art/Materials/M_NUCharacterSprite'
FIELD = FOLDER + '/M_NUCharacterHairTint'
UI = FOLDER + '/M_NUHairTint_UI'
REF_LUMA = 0.60      # brightness (linear) of the hair art that shows the chosen colour as it is
HIGHLIGHT = 0.35     # how far brighter strokes lift toward white

L = unreal.MaterialEditingLibrary
E = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
P = unreal.MaterialProperty

TINT_CODE = '''// Hair colour: keep the drawing's brightness, colour it with Tint. Ref = the brightness that shows Tint unchanged.
float luma = dot(Tex, float3(0.2126, 0.7152, 0.0722));
float shade = luma / max(Ref, 0.001);
float3 base = saturate(shade) * Tint;
float3 tinted = base + (1.0 - base) * saturate(shade - 1.0) * Hi;
// On = 0 draws the picture as the plain character material would (texture x vertex colour): a part can keep this material
// and switch tinting off, instead of swapping materials (a swapped-in material hides the part until its shaders are ready)
return lerp(Tex * Tint, tinted, saturate(On));'''


def log(message):
    unreal.log('[CCG] ' + str(message))


def custom(material, code, inputs, output, x, y, description):
    node = L.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', output)
    node.set_editor_property('description', description)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    node.set_editor_property('inputs', pins)
    return node


def scalar(material, name, value, x, y):
    node = L.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    node.set_editor_property('group', 'Hair Tint')
    return node


def link(source, pin, target, target_pin):
    assert L.connect_material_expressions(source, pin, target, target_pin), (source.get_name(), pin, target.get_name(), target_pin)


def tint_node(material, x, y):
    """The colourise node with its two tuning parameters attached. Still needs Tex and Tint."""
    node = custom(material, TINT_CODE, ['Tex', 'Tint', 'Ref', 'Hi', 'On'], unreal.CustomMaterialOutputType.CMOT_FLOAT3, x, y, 'Hair tint')
    link(scalar(material, 'HairRefLuma', REF_LUMA, x - 260, y + 120), '', node, 'Ref')
    link(scalar(material, 'HairHighlight', HIGHLIGHT, x - 260, y + 200), '', node, 'Hi')
    link(scalar(material, 'HairTintOn', 1.0, x - 260, y + 280), '', node, 'On')
    return node


def finish(material, path):
    L.recompile_material(material)
    assert E.save_asset(path, False), path
    log('saved ' + path)


def new_material(path):
    folder, name = path.rsplit('/', 1)
    assert not E.does_asset_exist(path), path + ' exists: run through run.ps1 -Steps hair (it removes the old files first)'
    material = TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    assert material, path
    return material


def sprite_sample(material, x, y):
    node = L.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property('parameter_name', 'SpriteTexture')      # Paper2D feeds the sprite's texture into this name
    node.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
    return node


def build_field():
    """Copy the character material and put the colourise node where the texture was multiplied by the vertex colour."""
    assert not E.does_asset_exist(FIELD), FIELD + ' exists: run through run.ps1 -Steps hair'
    material = E.duplicate_asset(SOURCE, FIELD)
    assert material, FIELD
    nodes = [o for o in unreal.ObjectIterator(unreal.MaterialExpression) if o.get_outer() == material]

    def feeds(node):
        return [s for s in L.get_inputs_for_material_expression(material, node)]

    def is_rgb_mask(node):
        return isinstance(node, unreal.MaterialExpressionComponentMask) and node.get_editor_property('r') and node.get_editor_property('g') \
            and node.get_editor_property('b') and not node.get_editor_property('a')

    vertex = [n for n in nodes if isinstance(n, unreal.MaterialExpressionVertexColor)]
    assert len(vertex) == 1, 'vertex colour nodes: %d' % len(vertex)
    vertex = vertex[0]
    # texture RGB: the RGB mask fed by the pixel-sampling custom node
    texture_rgb = [n for n in nodes if is_rgb_mask(n) and isinstance(feeds(n)[0], unreal.MaterialExpressionCustom)]
    assert len(texture_rgb) == 1, 'texture RGB masks: %d' % len(texture_rgb)
    texture_rgb = texture_rgb[0]
    # the multiply that tints it: inputs are that mask and the vertex colour
    multiply = [n for n in nodes if isinstance(n, unreal.MaterialExpressionMultiply) and texture_rgb in feeds(n) and vertex in feeds(n)]
    assert len(multiply) == 1, 'tint multiplies: %d' % len(multiply)
    multiply = multiply[0]
    after = [n for n in nodes if multiply in feeds(n)]
    assert len(after) == 1 and is_rgb_mask(after[0]), 'the multiply should feed one RGB mask, found %s' % [n.get_name() for n in after]

    node = tint_node(material, multiply.get_editor_property('material_expression_editor_x'), multiply.get_editor_property('material_expression_editor_y') - 220)
    link(texture_rgb, '', node, 'Tex')
    link(vertex, '', node, 'Tint')
    link(node, '', after[0], '')
    L.delete_material_expression(material, multiply)
    finish(material, FIELD)


def build_ui():
    material = new_material(UI)
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    # which part of the texture to draw: a sprite is one cell of a sheet (UVRect = start x, start y, size x, size y)
    coords = L.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1300, 0)
    rect = L.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1300, 160)
    rect.set_editor_property('parameter_name', 'UVRect')
    rect.set_editor_property('default_value', unreal.LinearColor(0, 0, 1, 1))
    cell = custom(material, 'return float2(X, Y) + UV * float2(W, H);', ['UV', 'X', 'Y', 'W', 'H'], unreal.CustomMaterialOutputType.CMOT_FLOAT2, -1000, 0, 'Sprite cell')
    link(coords, '', cell, 'UV')
    for pin, name in (('R', 'X'), ('G', 'Y'), ('B', 'W'), ('A', 'H')):
        link(rect, pin, cell, name)
    sample = sprite_sample(material, -700, 0)
    link(cell, '', sample, 'UVs')
    colour = L.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, 300)
    colour.set_editor_property('parameter_name', 'HairTint')
    colour.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    node = tint_node(material, -300, 0)
    link(sample, 'RGB', node, 'Tex')
    link(colour, '', node, 'Tint')
    # the image's own tint and opacity (Slate passes them as the vertex colour)
    vertex = L.create_material_expression(material, unreal.MaterialExpressionVertexColor, -300, 420)
    rgb = L.create_material_expression(material, unreal.MaterialExpressionMultiply, 0, 0)
    link(node, '', rgb, 'A')
    link(vertex, '', rgb, 'B')
    alpha = L.create_material_expression(material, unreal.MaterialExpressionMultiply, 0, 300)
    link(sample, 'A', alpha, 'A')
    link(vertex, 'A', alpha, 'B')
    assert L.connect_material_property(rgb, '', P.MP_EMISSIVE_COLOR)
    assert L.connect_material_property(alpha, '', P.MP_OPACITY)
    finish(material, UI)


build_field()
build_ui()
for path in (FIELD, UI):
    stats = L.get_statistics(unreal.load_asset(path))
    log('%s: pixel shader instructions %s, samplers %s' % (path.rsplit('/', 1)[1], stats.num_pixel_shader_instructions, stats.num_samplers))
log('DONE')
