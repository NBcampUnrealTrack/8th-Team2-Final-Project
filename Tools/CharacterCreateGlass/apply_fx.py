# -*- coding: utf-8 -*-
"""Motion for the creation and select screens that needs no widget tick: two UI materials driven by time.

  /Game/Art/UI/CharacterCreate/Glass/Fx/M_CCG_StageAmbience   lanterns and the right window breathe, and embers (1 art pixel,
                                                              on the 5 px grid) drift up out of the light. Drawn over the stage
                                                              frame (2560 x 1080); coordinates are the stage's 1080p units.
  /Game/Art/UI/CharacterCreate/Glass/Fx/M_CCG_Shine           a light band crosses the main button every few seconds,
                                                              cut to the pill's shape. The C++ shows it only while the button works.

Run headless through run.ps1 -Steps fx (the two assets are made from nothing every time).
"""
import importlib
import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/CharacterCreateGlass'))
import ccg_kit
importlib.reload(ccg_kit)
from ccg_kit import *           # noqa: F401,F403

FX = BASE + '/Fx'
L = unreal.MaterialEditingLibrary
P = unreal.MaterialProperty

AMBIENCE = '''// Stage ambience. p = 1080p units of the middle 16:9 frame (the stage art is 320 units wider on each side).
float2 p = UV * float2(FrameW, FrameH) - float2(Shift, 0.0);

// the two lanterns and the lit window breathe a little (each on its own beat)
float glow = 0.0;
float3 lamps[3] = { float3(262.0, 318.0, 150.0), float3(782.0, 318.0, 150.0), float3(1145.0, 400.0, 230.0) };
for (int i = 0; i < 3; i++)
{
    float d = length((p - lamps[i].xy) / lamps[i].z);
    float g = saturate(1.0 - d);
    float f = 0.5 + 0.5 * sin(T * (5.3 + i * 1.7) + i * 2.3) * sin(T * (2.1 + i * 0.9) + i * 1.1);
    glow += g * g * f;
}

// embers: squares of one or two art pixels on the stage's 5 px grid, rising out of the light and fading
float2 cell = floor(p / 5.0);
float mote = 0.0;
for (int k = 0; k < 24; k++)
{
    float h1 = frac(sin(k * 12.9898 + 1.0) * 43758.5453);
    float h2 = frac(sin(k * 78.233 + 2.0) * 43758.5453);
    float h3 = frac(sin(k * 39.425 + 3.0) * 43758.5453);
    float period = 8.0 + h2 * 7.0;
    float life = frac(T / period + h1);
    int src = k % 3;
    float2 origin = src == 0 ? float2(262.0, 380.0) : (src == 1 ? float2(782.0, 380.0) : float2(1145.0, 470.0));
    float2 pos = origin + float2((h3 - 0.5) * 240.0 + sin(T * 0.7 + k) * 16.0, 40.0 - life * 320.0);
    float2 rel = cell - floor(pos / 5.0);
    float size = h2 > 0.7 ? 2.0 : 1.0;
    if (rel.x >= 0.0 && rel.y >= 0.0 && rel.x < size && rel.y < size)
    {
        float fade = smoothstep(0.0, 0.15, life) * (1.0 - smoothstep(0.55, 1.0, life));
        mote = max(mote, fade * (0.55 + 0.45 * h3));
    }
}

float a = saturate(glow * GlowStrength + mote * MoteStrength);
float3 c = lerp(float3(1.0, 0.72, 0.42), float3(1.0, 0.88, 0.62), saturate(mote * 1.5));
return float4(c, a);'''

SHINE = '''// A light band crossing a pill-shaped button. Size = the button in 1080p units (x, y of the vector).
float2 S = Size.xy;
float2 px = UV * S;
float r = S.y * 0.5;
float2 q = abs(px - S * 0.5) - (S * 0.5 - r);
float sd = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
float inside = saturate(0.5 - sd);
float t = frac(T / Period) * Period;
float x = (px.x + px.y * 0.6) / (S.x + S.y * 0.6);
float head = t / Sweep * 1.4 - 0.2;
float band = saturate(1.0 - abs(x - head) / Width);
return float4(1.0, 0.98, 0.94, band * band * inside * Strength);'''


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
    for index, (name, value) in enumerate(params):
        param = L.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, index * 80)
        param.set_editor_property('parameter_name', name); param.set_editor_property('default_value', value)
        link(param, '', node, name)
    for index, (name, value) in enumerate(vectors):
        param = L.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, 500 + index * 120)
        param.set_editor_property('parameter_name', name); param.set_editor_property('default_value', value)
        link(param, '', node, name)
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


ui_material(FX + '/M_CCG_StageAmbience', AMBIENCE,
            [('FrameW', 2560.0), ('FrameH', 1080.0), ('Shift', 320.0), ('GlowStrength', 0.10), ('MoteStrength', 0.8)], [],
            'Stage ambience: lantern breath + embers')
# Size is a vector (x, y used): the main buttons are 426 x 64
ui_material(FX + '/M_CCG_Shine', SHINE,
            [('Period', 4.5), ('Sweep', 0.9), ('Width', 0.08), ('Strength', 0.45)], [('Size', unreal.LinearColor(426, 64, 0, 0))],
            'Button shine')
log('DONE')
