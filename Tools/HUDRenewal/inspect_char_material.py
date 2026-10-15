# -*- coding: utf-8 -*-
"""Read-only: the character sprite master (M_NUCharacterSprite) --- blend, parameters with defaults, and what feeds each output."""
import unreal

L = unreal.MaterialEditingLibrary
P = unreal.MaterialProperty


def log(m):
    unreal.log('[HUD] ' + str(m))


mat = unreal.load_asset('/Game/Art/Materials/M_NUCharacterSprite')
log('blend=%s shading=%s pass=%s' % (mat.get_editor_property('blend_mode'), mat.get_editor_property('shading_model'),
                                     mat.get_editor_property('translucency_pass')))
for name in L.get_scalar_parameter_names(mat):
    log('scalar %s = %s' % (name, L.get_material_default_scalar_parameter_value(mat, name)))
for name in L.get_vector_parameter_names(mat):
    log('vector %s = %s' % (name, L.get_material_default_vector_parameter_value(mat, name)))
for name in L.get_static_switch_parameter_names(mat):
    log('switch %s = %s' % (name, L.get_material_default_static_switch_parameter_value(mat, name)))
for prop in ('MP_EMISSIVE_COLOR', 'MP_BASE_COLOR', 'MP_OPACITY', 'MP_OPACITY_MASK'):
    node = L.get_material_property_input_node(mat, getattr(P, prop))
    if node is None:
        log('%s <- none' % prop)
        continue
    pin = L.get_material_property_input_node_output_name(mat, getattr(P, prop))
    desc = node.get_editor_property('desc') if hasattr(node, 'desc') else ''
    log('%s <- %s.%s (%s) at %s,%s' % (prop, node.get_name(), pin, node.get_class().get_name(),
                                      node.get_editor_property('material_expression_editor_x'), node.get_editor_property('material_expression_editor_y')))
    for i, inp in enumerate(L.get_inputs_for_material_expression(mat, node) or []):
        if inp is not None:
            log('   in%d %s (%s)' % (i, inp.get_name(), inp.get_class().get_name()))
log('expressions %d' % len(L.get_material_expressions(mat)) if hasattr(L, 'get_material_expressions') else 'n/a')
log('DONE')
