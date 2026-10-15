# Ember Chronicle proto: import sliced frames -> textures -> sprites -> flipbooks, plus a blank map.
# Run headless:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nosplash -stdout
import os
import unreal

RAW = r"C:\Users\Ndus_072\Documents\Unreal Projects\ProjectM\RawAssets\frames"
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

PPU = 0.25  # pixels per unreal unit -> 22px sprite becomes 88uu tall


def log(msg):
    unreal.log_warning("[EmberGen] " + msg)


def import_texture(png_path, dest_path):
    task = unreal.AssetImportTask()
    task.filename = png_path
    task.destination_path = dest_path
    task.automated = True
    task.save = False
    task.replace_existing = True
    ASSET_TOOLS.import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    if not paths:
        raise RuntimeError("import failed: " + png_path)
    tex = unreal.load_asset(paths[0])
    # pixel-art settings (design doc §1.5)
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("srgb", True)
    return tex


def make_sprite(tex, dest_path, name):
    full = dest_path + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    factory = unreal.PaperSpriteFactory()
    try:
        factory.set_editor_property("initial_texture", tex)
    except Exception:
        pass
    sprite = ASSET_TOOLS.create_asset(name, dest_path, unreal.PaperSprite, factory)
    if sprite is None:
        raise RuntimeError("sprite create failed: " + name)
    try:
        sprite.set_editor_property("source_texture", tex)
    except Exception:
        pass
    sprite.set_editor_property("pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
    sprite.set_editor_property("pixels_per_unreal_unit", PPU)
    return sprite


def make_flipbook(sprites, dest_path, name, fps):
    full = dest_path + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    factory = unreal.PaperFlipbookFactory()
    keyframes = []
    for s in sprites:
        kf = unreal.PaperFlipbookKeyFrame()
        kf.set_editor_property("sprite", s)
        kf.set_editor_property("frame_run", 1)
        keyframes.append(kf)
    used_factory_frames = True
    try:
        factory.set_editor_property("key_frames", keyframes)
    except Exception:
        used_factory_frames = False
    book = ASSET_TOOLS.create_asset(name, dest_path, unreal.PaperFlipbook, factory)
    if book is None:
        raise RuntimeError("flipbook create failed: " + name)
    if not used_factory_frames:
        book.set_editor_property("key_frames", keyframes)
    book.set_editor_property("frames_per_second", fps)
    return book


def build_set(kind, frames_dir, tex_path, sprite_path, fb_path):
    """kind: player | slime"""
    if kind == "player":
        dirs = {"Down": "walk_down", "Up": "walk_up", "Left": "walk_left", "Right": "walk_right"}
        for facing, stem in dirs.items():
            sprites = []
            for i in range(6):
                png = os.path.join(frames_dir, "%s_%d.png" % (stem, i))
                tex = import_texture(png, tex_path)
                sprites.append(make_sprite(tex, sprite_path, "S_Player_%s_%d" % (facing, i)))
            make_flipbook(sprites, fb_path, "FB_Player_Walk_%s" % facing, 10.0)
            make_flipbook([sprites[0]], fb_path, "FB_Player_Idle_%s" % facing, 1.0)
            log("player %s done" % facing)
    else:
        sprites = []
        for i in range(4):
            png = os.path.join(frames_dir, "slime_idle_%d.png" % i)
            tex = import_texture(png, tex_path)
            sprites.append(make_sprite(tex, sprite_path, "S_Slime_Idle_%d" % i))
        make_flipbook(sprites, fb_path, "FB_Slime_Idle", 6.0)
        log("slime done")


build_set("player", os.path.join(RAW, "Player"),
          "/Game/Art/Sprites/Player/Tex", "/Game/Art/Sprites/Player/Frames", "/Game/Art/Sprites/Player")
build_set("slime", os.path.join(RAW, "Slime"),
          "/Game/Art/Sprites/Slime/Tex", "/Game/Art/Sprites/Slime/Frames", "/Game/Art/Sprites/Slime")

EAL.save_directory("/Game/Art", only_if_is_dirty=False, recursive=True)
log("assets saved")

# blank map the runtime GameMode fills procedurally
if not EAL.does_asset_exist("/Game/Maps/L_Proto"):
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/L_Proto")
    log("map saved: %s" % ok)
else:
    log("map exists")

log("ALL DONE")
