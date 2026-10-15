# -*- coding: utf-8 -*-
"""Adds two tone parameters to the character sprite master /Game/Art/Materials/M_NUCharacterSprite, for NPC brightness:
  colour -> pow(colour, ToneGamma) * ToneGain -> EyeAdaptationInverse (the exposure give-back) -> emissive.
Both default to 1, so every sprite looks exactly as before until something sets them (ANUNPCBase::ApplyVisualTuning sets them
per NPC from NU NPC Visual). Skips if the parameters are already there. Back up of the master: Saved/HUDRenewal/backup_npcvisual.
Headless: Tools/HUDRenewal/run.ps1 -Steps apply_char_tone"""
import os
import shutil
import unreal

L = unreal.MaterialEditingLibrary
P = unreal.MaterialProperty
E = unreal.EditorAssetLibrary
PATH = '/Game/Art/Materials/M_NUCharacterSprite'
ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def log(m):
    unreal.log('[HUD] ' + str(m))


def link(a, ap, b, bp):
    assert L.connect_material_expressions(a, ap, b, bp), (a.get_name(), ap, b.get_name(), bp)


mat = unreal.load_asset(PATH)
if 'ToneGamma' in [str(n) for n in L.get_scalar_parameter_names(mat)]:
    log('tone parameters already there')
else:
    src = os.path.join(ROOT, 'Content/Art/Materials/M_NUCharacterSprite.uasset')
    dst = os.path.join(ROOT, 'Saved/HUDRenewal/backup_npcvisual/M_NUCharacterSprite.uasset')
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
    eye = L.get_material_property_input_node(mat, P.MP_EMISSIVE_COLOR)
    assert eye and eye.get_class().get_name() == 'MaterialExpressionEyeAdaptationInverse', eye
    inputs = L.get_inputs_for_material_expression(mat, eye)
    colour = inputs[0]
    assert colour is not None
    x = eye.get_editor_property('material_expression_editor_x')
    y = eye.get_editor_property('material_expression_editor_y')
    gamma = L.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x - 420, y + 260)
    gamma.set_editor_property('parameter_name', 'ToneGamma'); gamma.set_editor_property('default_value', 1.0)
    gain = L.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x - 420, y + 360)
    gain.set_editor_property('parameter_name', 'ToneGain'); gain.set_editor_property('default_value', 1.0)
    power = L.create_material_expression(mat, unreal.MaterialExpressionPower, x - 220, y + 180)
    mul = L.create_material_expression(mat, unreal.MaterialExpressionMultiply, x - 80, y + 180)
    link(colour, '', power, 'Base')
    link(gamma, '', power, 'Exp')
    link(power, '', mul, 'A')
    link(gain, '', mul, 'B')
    # the EyeAdaptationInverse input pin is the LightValueInput property; its pin name differs by version
    for pin in ('LightValueInput', 'Light Value Input', 'Light Value', 'LightValue', ''):
        if L.connect_material_expressions(mul, '', eye, pin):
            log('linked to EyeAdaptationInverse pin %r' % pin)
            break
    after = L.get_inputs_for_material_expression(mat, eye)[0]
    assert after == mul, after
    L.recompile_material(mat)
    assert E.save_asset(PATH, False), PATH
    log('tone parameters added: %s' % [str(n) for n in L.get_scalar_parameter_names(mat)])
log('DONE')
