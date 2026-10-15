# -*- coding: utf-8 -*-
"""Read-only: exports the NPC quest marker pictures (zxcv1 QuestStatus) to Saved/HUDRenewal/npc_markers/*.png and logs the world marker
widget class the NPCs use, for the marker proposal."""
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Saved/HUDRenewal/npc_markers')
os.makedirs(OUT, exist_ok=True)
for data in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/Developers/zxcv1/Interaction/UI/QuestStatus', recursive=True):
    asset = data.get_asset()
    unreal.log('[HUD] asset %s %s' % (data.package_name, asset.get_class().get_name()))
    if isinstance(asset, unreal.Texture2D):
        task = unreal.AssetExportTask()
        task.object = asset
        task.filename = os.path.join(OUT, str(data.asset_name) + '.png')
        task.automated = True; task.replace_identical = True; task.prompt = False
        task.exporter = unreal.TextureExporterPNG()
        unreal.log('[HUD] export %s %s %dx%d filter=%s' % (data.asset_name, unreal.Exporter.run_asset_export_task(task), asset.blueprint_get_size_x(),
                                                          asset.blueprint_get_size_y(), asset.get_editor_property('filter')))
cdo = unreal.get_default_object(unreal.load_class(None, '/Script/ProjectM.NUWorldMarkerComponent'))
unreal.log('[HUD] marker widget class=%s draw=%s' % (cdo.get_editor_property('widget_class'), cdo.get_editor_property('draw_size')))
unreal.log('[HUD] DONE')
