# -*- coding: utf-8 -*-
"""Read-only: how the NPC head marker is drawn today (for the head-marker rework).
Logs M_WorldMarker (blend, expressions), WBP_WorldMarker (tree, CDO maps) and the marker component of each NPC blueprint
that carries one (widget class, draw size, pivot, scale, place)."""
import unreal

E = unreal.EditorAssetLibrary


def log(m):
    unreal.log('[HUD] ' + str(m))


mat = unreal.load_asset('/Game/Developers/zxcv1/UI/Materials/M_WorldMarker')
log('material %s domain=%s blend=%s shading=%s twosided=%s' % (mat.get_name(), mat.get_editor_property('material_domain'),
                                                               mat.get_editor_property('blend_mode'), mat.get_editor_property('shading_model'),
                                                               mat.get_editor_property('two_sided')))
for prop in ('opacity_mask_clip_value', 'translucency_pass', 'enable_responsive_aa', 'output_translucent_velocity'):
    try:
        log('  %s=%s' % (prop, mat.get_editor_property(prop)))
    except Exception as error:
        log('  %s ? %s' % (prop, error))
for expr in unreal.MaterialEditingLibrary.get_material_expressions(mat) if hasattr(unreal.MaterialEditingLibrary, 'get_material_expressions') else []:
    log('  expr %s' % expr.get_class().get_name())
try:
    for name in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat):
        log('  scalar %s' % name)
    for name in unreal.MaterialEditingLibrary.get_texture_parameter_names(mat):
        log('  texture %s' % name)
except Exception as error:
    log('  params ? %s' % error)

bp = unreal.load_asset('/Game/Developers/zxcv1/UI/WBP_WorldMarker')
cls = bp.generated_class()
cdo = unreal.get_default_object(cls)
for prop in ('marker_textures', 'marker_tints', 'pulse_tags'):
    try:
        log('wbp %s=%s' % (prop, cdo.get_editor_property(prop)))
    except Exception as error:
        log('wbp %s ? %s' % (prop, error))
try:
    root = unreal.WidgetService.get_root_widget('/Game/Developers/zxcv1/UI/WBP_WorldMarker')
    tree = unreal.find_object(bp, 'WidgetTree')
    log('wbp root %s' % root)

    def walk(w, depth):
        if w is None:
            return
        slot = w.get_editor_property('slot')
        extra = ''
        if isinstance(slot, unreal.CanvasPanelSlot):
            extra = ' canvas %s' % slot.get_layout()
        log('  ' * depth + '%s %s%s' % (w.get_class().get_name(), w.get_name(), extra))
        if isinstance(w, unreal.Image):
            brush = w.get_editor_property('brush')
            log('  ' * depth + '   brush size %s res %s' % (brush.image_size, brush.get_editor_property('resource_object')))
        if isinstance(w, unreal.PanelWidget):
            for i in range(w.get_children_count()):
                walk(w.get_child_at(i), depth + 1)
        elif isinstance(w, unreal.ContentWidget):
            walk(w.get_content(), depth + 1)
    walk(unreal.find_object(tree, root) if root else None, 1)
except Exception as error:
    log('wbp tree ? %s' % error)

reg = unreal.AssetRegistryHelpers.get_asset_registry()
seen = 0
for data in reg.get_assets_by_path('/Game/Developers/zxcv1', recursive=True):
    if str(data.asset_class_path.asset_name) != 'Blueprint':
        continue
    path = str(data.package_name)
    if '/NPC' not in path and '/Quest' not in path and '/Interaction' not in path:
        continue
    asset = data.get_asset()
    try:
        gen = asset.generated_class()
    except Exception:
        continue
    if gen is None:
        continue
    subsys = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsys.k2_gather_subobject_data_for_blueprint(asset)
    for h in handles:
        data_ = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data_)
        if obj is None or not isinstance(obj, unreal.WidgetComponent) or 'Marker' not in obj.get_class().get_name():
            continue
        seen += 1
        log('npc %s comp=%s class=%s widget=%s draw=%s pivot=%s scale=%s loc=%s space=%s' % (
            path.rsplit('/', 1)[-1], obj.get_name(), obj.get_class().get_name(), obj.get_editor_property('widget_class'),
            obj.get_editor_property('draw_size'), obj.get_editor_property('pivot'), obj.get_editor_property('relative_scale3d'),
            obj.get_editor_property('relative_location'), obj.get_editor_property('space')))
log('marker components %d' % seen)
log('DONE')
