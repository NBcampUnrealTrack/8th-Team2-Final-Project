# -*- coding: utf-8 -*-
"""Time-driven UI materials for the new HUD (no widget tick needed). Run headless through run.ps1 -Steps fx
(the assets are made from nothing every time).

  M_HUD_Shine   a light band crossing a rounded box every few seconds (toast, area banner, the active command).
                Size = the box in 1080p units (x, y), Radius = its corner radius (y/2 for a pill).
  M_HUD_Pulse   a ring that grows from the centre and fades (map markers, the interaction key, the current turn).
  M_HUD_Parry   the battle parry ring: a ring closing on the timing circle (Speed = loops per second).
  M_HUD_KeyRing the interaction key: ivory rings easing out of the key, each with a navy shade just outside so they
                read on bright grass as well as dark stone.
  MI_HUD_KeyShine  M_HUD_Shine sized for the 36 px key cap (a quicker glint).
  MI_HUD_HeadRing  M_HUD_KeyRing for the NPC head marker: warmer, slower, starting at the badge edge.
  M_HUD_WorldMarker  copy of the world marker material (zxcv1 M_WorldMarker: camera-facing, depth-tested, unlit) drawn
                as premultiplied translucency (AlphaComposite) instead of masked, so the badge edges and its shade are soft.
"""
import importlib
import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/HUDRenewal'))
import hud_kit
importlib.reload(hud_kit)
from hud_kit import FX, E, TOOLS, log

L = unreal.MaterialEditingLibrary
P = unreal.MaterialProperty

SHINE = '''// A light band crossing a rounded box. Size = the box in 1080p units, Radius = its corner radius.
float2 S = Size.xy;
float2 px = UV * S;
float r = min(Radius, min(S.x, S.y) * 0.5);
float2 q = abs(px - S * 0.5) - (S * 0.5 - r);
float sd = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
float inside = saturate(0.5 - sd);
float t = frac(T / Period) * Period;
float x = (px.x + px.y * 0.6) / (S.x + S.y * 0.6);
float head = t / Sweep * 1.4 - 0.2;
float band = saturate(1.0 - abs(x - head) / Width);
return float4(1.0, 0.97, 0.90, band * band * inside * Strength);'''

PULSE = '''// A ring growing out of the centre and fading; two rings half a period apart.
float2 d = (UV - 0.5) * 2.0;
float rr = length(d);
float a = 0.0;
for (int i = 0; i < 2; i++)
{
    float k = frac(T / Period + i * 0.5);
    float ring = saturate(1.0 - abs(rr - (Inner + k * (1.0 - Inner))) / Width);
    a += ring * ring * (1.0 - k) * (1.0 - k);
}
return float4(Color.rgb, saturate(a * Strength) * step(rr, 1.0));'''

PARRY = '''// Battle parry cue: the timing circle (fixed) and a ring closing onto it. Speed = loops per second.
float2 d = (UV - 0.5) * 2.0;
float rr = length(d);
float k = frac(T * Speed);
float closing = lerp(1.0, Target, k);
float ringA = saturate(1.0 - abs(rr - Target) / 0.035);
float ringB = saturate(1.0 - abs(rr - closing) / 0.025) * (0.35 + 0.65 * k);
float glow = saturate(1.0 - abs(rr - Target) / 0.12) * 0.35;
float a = max(max(ringA, ringB), glow);
float3 c = lerp(float3(1.0, 0.97, 0.9), Color.rgb, ringA);
return float4(c, a * step(rr, 1.0));'''

KEYRING = '''// Interaction key rings: an ivory core with a navy shade just outside it, two rings half a period apart.
float2 d = (UV - 0.5) * 2.0;
float rr = length(d);
float ac = 0.0;
float sh = 0.0;
for (int i = 0; i < 2; i++)
{
    float k = frac(T / Period + i * 0.5);
    float e = 1.0 - (1.0 - k) * (1.0 - k);
    float r = Inner + e * (1.0 - Inner);
    float fade = pow(1.0 - k, 1.4);
    float core = saturate(1.0 - abs(rr - r) / Width);
    float shade = saturate(1.0 - abs(rr - (r + Width * 1.1)) / (Width * 1.5));
    ac = max(ac, core * core * fade);
    sh = max(sh, shade * fade * 0.55);
}
float A = saturate(ac + sh * (1.0 - ac));
float3 C = (Color.rgb * ac + Shade.rgb * sh * (1.0 - ac)) / max(A, 0.0001);
return float4(C, A * Strength * step(rr, 1.0));'''


def custom(material, code, inputs, x, y, description):
    node = L.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    node.set_editor_property('description', description)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    node.set_editor_property('inputs', pins)
    return node


def link(source, pin, target, target_pin):
    assert L.connect_material_expressions(source, pin, target, target_pin), (source.get_name(), pin, target.get_name(), target_pin)


def ui_material(path, code, params, vectors, description):
    folder, name = path.rsplit('/', 1)
    assert not E.does_asset_exist(path), path + ' exists: run through run.ps1 -Steps fx'
    material = TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    inputs = ['UV', 'T'] + [n for n, _ in params] + [n for n, _ in vectors]
    node = custom(material, code, inputs, -300, 0, description)
    link(L.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -700, -200), '', node, 'UV')
    time = L.create_material_expression(material, unreal.MaterialExpressionTime, -700, -100)
    time.set_editor_property('ignore_pause', True)
    link(time, '', node, 'T')
    for index, (pname, value) in enumerate(params):
        param = L.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, index * 80)
        param.set_editor_property('parameter_name', pname); param.set_editor_property('default_value', value)
        link(param, '', node, pname)
    for index, (pname, value) in enumerate(vectors):
        param = L.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, 600 + index * 120)
        param.set_editor_property('parameter_name', pname); param.set_editor_property('default_value', value)
        link(param, '', node, pname)
    rgb = L.create_material_expression(material, unreal.MaterialExpressionComponentMask, 0, -60)
    for c, on in (('r', True), ('g', True), ('b', True), ('a', False)):
        rgb.set_editor_property(c, on)
    alpha = L.create_material_expression(material, unreal.MaterialExpressionComponentMask, 0, 60)
    for c, on in (('r', False), ('g', False), ('b', False), ('a', True)):
        alpha.set_editor_property(c, on)
    link(node, '', rgb, ''); link(node, '', alpha, '')
    assert L.connect_material_property(rgb, '', P.MP_EMISSIVE_COLOR)
    assert L.connect_material_property(alpha, '', P.MP_OPACITY)
    L.recompile_material(material)
    assert E.save_asset(path, False), path
    log('saved ' + path)


GOLD = unreal.LinearColor(0.96, 0.84, 0.56, 1.0)
ui_material(FX + '/M_HUD_Shine', SHINE, [('Period', 5.0), ('Sweep', 1.0), ('Width', 0.07), ('Strength', 0.35), ('Radius', 16.0)],
            [('Size', unreal.LinearColor(560, 64, 0, 0))], 'HUD shine')
ui_material(FX + '/M_HUD_Pulse', PULSE, [('Period', 1.6), ('Width', 0.12), ('Strength', 0.9), ('Inner', 0.35)],
            [('Color', GOLD)], 'HUD pulse ring')
ui_material(FX + '/M_HUD_Parry', PARRY, [('Speed', 0.9), ('Target', 0.55)], [('Color', GOLD)], 'HUD parry ring')
ui_material(FX + '/M_HUD_KeyRing', KEYRING, [('Period', 1.5), ('Width', 0.075), ('Strength', 1.0), ('Inner', 0.42)],
            [('Color', unreal.LinearColor(1.0, 0.93, 0.74, 1.0)), ('Shade', unreal.LinearColor(0.008, 0.02, 0.04, 1.0))], 'HUD interaction key rings')


def ui_instance(path, parent, scalars, vectors):
    folder, name = path.rsplit('/', 1)
    assert not E.does_asset_exist(path), path + ' exists: run through run.ps1 -Steps fx'
    mi = TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    L.set_material_instance_parent(mi, unreal.load_asset(parent))
    for pname, value in scalars:
        L.set_material_instance_scalar_parameter_value(mi, pname, value)
    for pname, value in vectors:
        L.set_material_instance_vector_parameter_value(mi, pname, value)
    L.update_material_instance(mi)
    assert E.save_asset(path, False), path
    log('saved ' + path)


ui_instance(FX + '/MI_HUD_HeadRing', FX + '/M_HUD_KeyRing', [('Period', 2.2), ('Width', 0.085), ('Strength', 1.0), ('Inner', 0.5)],
            [('Color', unreal.LinearColor(1.0, 0.86, 0.52, 1.0))])


def world_marker_material(path):
    """The widget component renders Slate into a render target with premultiplied colour (Slate blends SrcAlpha / InvSrcAlpha
    onto transparent black), so AlphaComposite (out = emissive + dst * (1 - opacity)) is the matching blend."""
    assert not E.does_asset_exist(path), path + ' exists: run through run.ps1 -Steps fx'
    source = '/Game/Developers/zxcv1/UI/Materials/M_WorldMarker'
    material = E.duplicate_asset(source, path)
    assert material, path
    mask = L.get_material_property_input_node(material, P.MP_OPACITY_MASK)
    pin = L.get_material_property_input_node_output_name(material, P.MP_OPACITY_MASK)
    emissive = L.get_material_property_input_node(material, P.MP_EMISSIVE_COLOR)
    log('world marker: mask %s.%s emissive %s' % (mask.get_class().get_name() if mask else None, pin,
                                                 emissive.get_class().get_name() if emissive else None))
    assert mask, 'M_WorldMarker has no opacity mask input'
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    material.set_editor_property('translucency_pass', unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    assert L.connect_material_property(mask, pin, P.MP_OPACITY), 'opacity'
    # unlit colour takes the map's fixed exposure (the village is ~0.5x): give it back, like the character sprite material
    # (EyeAdaptationInverse), times Brightness for tuning. Premultiplied, so scaling the colour keeps the alpha edges right.
    epin = L.get_material_property_input_node_output_name(material, P.MP_EMISSIVE_COLOR)
    eye = L.create_material_expression(material, unreal.MaterialExpressionEyeAdaptationInverse, -260, 420)
    gain = L.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -260, 520)
    gain.set_editor_property('parameter_name', 'Brightness'); gain.set_editor_property('default_value', 1.0)
    mul1 = L.create_material_expression(material, unreal.MaterialExpressionMultiply, -80, 380)
    mul2 = L.create_material_expression(material, unreal.MaterialExpressionMultiply, 60, 380)
    link(emissive, epin, mul1, 'A'); link(eye, '', mul1, 'B')
    link(mul1, '', mul2, 'A'); link(gain, '', mul2, 'B')
    assert L.connect_material_property(mul2, '', P.MP_EMISSIVE_COLOR), 'emissive'
    L.recompile_material(material)
    assert E.save_asset(path, False), path
    log('saved ' + path)


world_marker_material(FX + '/M_HUD_WorldMarker')
ui_instance(FX + '/MI_HUD_KeyShine', FX + '/M_HUD_Shine', [('Period', 2.4), ('Sweep', 0.5), ('Width', 0.2), ('Strength', 0.75), ('Radius', 9.0)],
            [('Size', unreal.LinearColor(36, 36, 0, 0))])
log('DONE')
