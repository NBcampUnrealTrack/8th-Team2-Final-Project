# -*- coding: utf-8 -*-
"""Read-only: export the first idle frame source of every NPC flipbook (and the player's novice body/hair for reference) to
Saved/HUDRenewal/npc_art/, with each frame's region and pivot in npc_art.json --- for measuring NPC height and tone."""
import json
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(ROOT, 'Saved/HUDRenewal/npc_art')
os.makedirs(OUT, exist_ok=True)
REG = unreal.AssetRegistryHelpers.get_asset_registry()


def log(m):
    unreal.log('[HUD] ' + str(m))


def export_texture(tex):
    path = os.path.join(OUT, tex.get_name() + '.png')
    if not os.path.exists(path):
        task = unreal.AssetExportTask()
        task.object = tex
        task.filename = path
        task.automated = True; task.replace_identical = True; task.prompt = False
        task.exporter = unreal.TextureExporterPNG()
        unreal.Exporter.run_asset_export_task(task)
    return path


rows = []
paths = ['/Game/Art/Sprites/NPC', '/Game/Developers/zxcv1/NPC', '/Game/Art/Sprites/Player/Player/MPlayer/Base']
for root in paths:
    for data in REG.get_assets_by_path(root, recursive=True):
        if str(data.asset_class_path.asset_name) != 'PaperFlipbook':
            continue
        name = str(data.asset_name)
        if 'Idle' not in name or ('MCh_' in name and 'Front' not in name):
            continue
        fb = data.get_asset()
        if fb.get_num_frames() == 0:
            continue
        sprite = fb.get_sprite_at_frame(0)
        tex = sprite.get_editor_property('source_texture')
        if tex is None:
            continue
        uv = sprite.get_editor_property('source_uv')
        dim = sprite.get_editor_property('source_dimension')
        piv = sprite.get_editor_property('custom_pivot_point')
        rows.append({'flipbook': str(data.package_name), 'texture': tex.get_name(), 'png': export_texture(tex),
                     'uv': [uv.x, uv.y], 'dim': [dim.x, dim.y], 'pivot': [piv.x, piv.y],
                     'pivot_mode': str(sprite.get_editor_property('pivot_mode')),
                     'ppu': sprite.get_editor_property('pixels_per_unreal_unit'),
                     'material': fb.get_editor_property('default_material').get_name() if fb.get_editor_property('default_material') else None})
        log('fb %s tex=%s dim=%s' % (name, tex.get_name(), rows[-1]['dim']))
json.dump(rows, open(os.path.join(OUT, 'npc_art.json'), 'w', encoding='utf-8'), indent=1)
log('exported %d' % len(rows))
log('DONE')
