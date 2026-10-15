# Reimport already-generated sprite textures from the cleaned PNG frames.
# Sprites/flipbooks reference these textures, so no other assets need touching.
import os
import unreal

RAW = r"C:\Users\Ndus_072\Documents\Unreal Projects\ProjectM\RawAssets\frames"
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("[EmberReimport] " + msg)


def reimport(png_path, dest_path):
    task = unreal.AssetImportTask()
    task.filename = png_path
    task.destination_path = dest_path
    task.automated = True
    task.save = True
    task.replace_existing = True
    ASSET_TOOLS.import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    if not paths:
        raise RuntimeError("reimport failed: " + png_path)
    tex = unreal.load_asset(paths[0])
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("srgb", True)
    EAL.save_asset(paths[0], only_if_is_dirty=False)


count = 0
for stem in ("walk_down", "walk_up", "walk_left", "walk_right"):
    for i in range(6):
        reimport(os.path.join(RAW, "Player", "%s_%d.png" % (stem, i)), "/Game/Art/Sprites/Player/Tex")
        count += 1
for i in range(4):
    reimport(os.path.join(RAW, "Slime", "slime_idle_%d.png" % i), "/Game/Art/Sprites/Slime/Tex")
    count += 1

log("reimported %d textures" % count)
log("ALL DONE")
