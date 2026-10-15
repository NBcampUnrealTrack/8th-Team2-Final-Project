# -*- coding: utf-8 -*-
"""Imports the remade item icons (ArtSource/ItemIcons_HD2D/T_Item_*.png, 144 px = 48 dots x3) over the existing textures
in /Game/Developers/dbals/UI/ItemIcons, same names, so every screen that shows an item icon (inventory, shop, equipment,
rewards, quick slots...) picks them up without any reference change. The texture settings each old asset had are kept
(144 is not a power of two, so there are no mips; at 48 / 96 px the 3x3 dot blocks land exactly on screen pixels).
The old .uasset files are copied to Saved/ItemIconsHD2D/backup first (once). Revert: copy them back.
Headless: UnrealEditor-Cmd ProjectM.uproject -run=pythonscript -script=Tools/ItemIconsHD2D/import_icons.py
"""
import os
import shutil
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(ROOT, 'ArtSource', 'ItemIcons_HD2D')
FOLDER = '/Game/Developers/dbals/UI/ItemIcons'
BACKUP = os.path.join(ROOT, 'Saved', 'ItemIconsHD2D', 'backup')
E = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
KEEP = ('compression_settings', 'lod_group', 'srgb', 'never_stream', 'filter', 'mip_gen_settings')


def log(m):
    unreal.log('[ICON] ' + str(m))


names = sorted(f[:-4] for f in os.listdir(SRC) if f.startswith('T_Item_') and f.endswith('.png'))
existing = [n for n in names if E.does_asset_exist(FOLDER + '/' + n)]
log('%d new pictures, %d replace existing textures' % (len(names), len(existing)))

os.makedirs(BACKUP, exist_ok=True)
settings = {}
for n in existing:
    src = os.path.join(ROOT, 'Content', 'Developers', 'dbals', 'UI', 'ItemIcons', n + '.uasset')
    dst = os.path.join(BACKUP, n + '.uasset')
    if os.path.exists(src) and not os.path.exists(dst):
        shutil.copy2(src, dst)
    tex = unreal.load_asset(FOLDER + '/' + n)
    settings[n] = {k: tex.get_editor_property(k) for k in KEEP}

tasks = []
for n in names:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, n + '.png')
    t.destination_path = FOLDER
    t.destination_name = n
    t.automated = True
    t.replace_existing = True
    t.save = False
    tasks.append(t)
TOOLS.import_asset_tasks(tasks)

done = 0
for n in names:
    tex = unreal.load_asset(FOLDER + '/' + n)
    assert tex, n
    for k, v in settings.get(n, {}).items():
        tex.set_editor_property(k, v)
    if n not in settings:          # a brand-new icon: the settings the old set uses
        tex.set_editor_properties({'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                                   'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI, 'srgb': True, 'never_stream': True})
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    assert E.save_asset(FOLDER + '/' + n, False), n
    size = (tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    assert size == (144, 144), (n, size)
    done += 1
log('imported %d (backup %s)' % (done, BACKUP))
log('DONE')
