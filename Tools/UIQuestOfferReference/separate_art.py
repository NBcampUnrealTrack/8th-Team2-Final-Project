"""Run migrate() in the editor after importing the two ArtSource/UI/QuestOffer PNGs.

Does not delete either design reference. Importing this module does not edit assets.
"""
import importlib.util
import json
import shutil
from datetime import datetime
from pathlib import Path

import unreal

WIDGET = '/Game/Developers/zxcv1/Quest/UI/WBP_QuestWidget'
REFERENCE = '/Game/Developers/MinkiCho/Art/퀘스트_수락창_시안'
DESTINATION = '/Game/Developers/MinkiCho/Art/Quest/Offer/Textures/'
PARTS = {
    'QO_Compass': ('T_QuestOffer_Compass', 117, 117),
    'QO_LowerCompass': ('T_QuestOffer_City', 198, 214),
}


def uv_region(width, height):
    return ('(Min=(X=%.12f,Y=%.12f),Max=(X=%.12f,Y=%.12f),bIsValid=True)'
            % (2/(width+4), 2/(height+4), (width+2)/(width+4), (height+2)/(height+4)))


def migrate():
    w = unreal.WidgetService
    if w.is_pie_running():
        raise RuntimeError('Stop PIE before migration.')
    root = Path(unreal.Paths.project_dir()).resolve()
    spec = importlib.util.spec_from_file_location('quest_offer_layout', root/'Tools/UIQuestOfferReference/apply.py')
    layout = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(layout)
    source = unreal.load_asset(REFERENCE)
    bp = unreal.load_asset(WIDGET)
    if not source or not bp:
        raise RuntimeError('Missing source or widget.')
    before = layout.snapshot()
    originals = {}
    textures = {}
    for name, (asset, width, height) in PARTS.items():
        current = w.get_brush(WIDGET, name, 'Brush').resource_path
        if current.split('.')[0] != REFERENCE:
            raise RuntimeError(f'Unexpected current resource for {name}: {current}')
        originals[name] = w.get_property(WIDGET, name, 'Brush')
        texture = unreal.load_asset(DESTINATION+asset)
        if not texture:
            raise RuntimeError(f'Import {asset}.png into {DESTINATION} first.')
        if texture.blueprint_get_size_x() != width+4 or texture.blueprint_get_size_y() != height+4:
            raise RuntimeError(f'Unexpected imported dimensions: {asset}')
        textures[name] = texture
    output = root/'Saved/QuestOfferSeparateArt'/datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    output.mkdir(parents=True)
    shutil.copy2(root/'Content/Developers/zxcv1/Quest/UI/WBP_QuestWidget.uasset', output/'WBP_QuestWidget.before.uasset')
    (output/'before.json').write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding='utf-8')
    # Match the reference's sampler/color settings; non-power-of-two UI art has no mip chain.
    for texture in textures.values():
        for prop in ('compression_settings', 'srgb', 'filter', 'lod_group', 'mip_gen_settings', 'never_stream'):
            texture.set_editor_property(prop, source.get_editor_property(prop))
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
            raise RuntimeError(f'Could not save texture {texture.get_path_name()}')
    with unreal.ScopedEditorTransaction('Separate quest offer artwork from design reference'):
        bp.modify()
        try:
            for name, (asset, width, height) in PARTS.items():
                resource = "Texture2D'" + textures[name].get_path_name() + "'"
                if not w.set_property(WIDGET, name, 'Brush.ResourceObject', resource):
                    raise RuntimeError(f'Could not replace resource: {name}')
                if not w.set_property(WIDGET, name, 'Brush.UVRegion', uv_region(width, height)):
                    raise RuntimeError(f'Could not replace UV region: {name}')
            after = layout.snapshot()
            # Only the two image brushes may change; layout, events and the tracker remain intact.
            def without_brush(rows):
                result = json.loads(json.dumps(rows))
                for row in result:
                    if row['name'] in PARTS:
                        row['properties'] = {k:v for k,v in row['properties'].items()
                                             if not (k == 'Brush' or k.startswith('Brush.'))}
                return result
            if without_brush(before) != without_brush(after):
                raise RuntimeError('Unexpected change outside the two brushes.')
            compiled = unreal.BlueprintService.compile_blueprint(WIDGET)
            if not compiled.success or compiled.num_errors:
                raise RuntimeError(str(compiled.errors))
            for name, (asset, _, _) in PARTS.items():
                if w.get_brush(WIDGET, name, 'Brush').resource_path.split('.')[0] != DESTINATION+asset:
                    raise RuntimeError(f'Resource readback failed: {name}')
            if not unreal.EditorAssetLibrary.save_asset(WIDGET):
                raise RuntimeError('Widget save failed.')
        except Exception:
            for name, value in originals.items():
                w.set_property(WIDGET, name, 'Brush', value)
            raise
    # Asset registry dependencies can retain the old package until its file is rescanned.
    unreal.AssetRegistryHelpers.get_asset_registry().scan_modified_asset_files(
        [str(root/'Content/Developers/zxcv1/Quest/UI/WBP_QuestWidget.uasset')])
    refs = list(unreal.AssetDiscoveryService.get_asset_referencers(REFERENCE))
    report = {'original_referencers': refs, 'compile_errors': compiled.num_errors,
              'compile_warnings': compiled.num_warnings,
              'brushes': {name: w.get_property(WIDGET, name, 'Brush') for name in PARTS}}
    (output/'after.json').write_text(json.dumps(after, ensure_ascii=False, indent=2), encoding='utf-8')
    (output/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if refs:
        raise RuntimeError('Reference still has users; keep the design reference.')
    return report
