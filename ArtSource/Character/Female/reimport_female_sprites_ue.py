# -*- coding: utf-8 -*-
"""
Unreal Engine headless Python script to reimport all 94 improved Female character
sprite sheets (ArtSource/Character/Female/WCh_*_Sheet.png) into
/Game/Art/Sprites/Player/Player/WPlayer/... and rebake all 752 PaperSprite render bounds.
"""
import os
import unreal

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
PROJ_DIR = unreal.Paths.project_dir()
ART_DIR = os.path.abspath(os.path.join(PROJ_DIR, "ArtSource/Character/Female")).replace("\\", "/")
WPLAYER_ROOT = "/Game/Art/Sprites/Player/Player/WPlayer"


def main():
    all_assets = EAL.list_assets(WPLAYER_ROOT, recursive=True, include_folder=False)
    sheet_assets = [p.split(".")[0] for p in all_assets if p.split(".")[0].endswith("_Sheet")]
    sprite_assets = [p.split(".")[0] for p in all_assets if p.split(".")[0].endswith("_Sprite")]

    unreal.log(f"[FemaleSpriteImprove] Found {len(sheet_assets)} sheet textures and {len(sprite_assets)} sprites under {WPLAYER_ROOT}")

    # 1. Reimport all 94 Texture2D sheets
    tasks = []
    tex_props = {}
    for asset_path in sorted(sheet_assets):
        name = asset_path.rsplit("/", 1)[1]
        png_path = f"{ART_DIR}/{name}.png"
        if not os.path.isfile(png_path):
            unreal.log_error(f"[FemaleSpriteImprove] Missing source PNG: {png_path}")
            continue

        tex = EAL.load_asset(asset_path)
        if tex:
            tex_props[asset_path] = (
                tex.get_editor_property("lod_group"),
                tex.get_editor_property("compression_settings"),
                tex.get_editor_property("filter"),
                tex.get_editor_property("srgb"),
            )

        dest_folder = asset_path.rsplit("/", 1)[0]
        task = unreal.AssetImportTask()
        task.filename = png_path
        task.destination_path = dest_folder
        task.destination_name = name
        task.replace_existing = True
        task.replace_existing_settings = False
        task.automated = True
        task.save = False
        tasks.append(task)

    AT.import_asset_tasks(tasks)

    # Restore Paper2D pixel art texture settings & save textures
    reimported_count = 0
    for asset_path in sorted(sheet_assets):
        tex = EAL.load_asset(asset_path)
        if not tex:
            continue
        if asset_path in tex_props:
            lod_group, comp, flt, srgb = tex_props[asset_path]
            tex.set_editor_property("lod_group", lod_group)
            tex.set_editor_property("compression_settings", comp)
            tex.set_editor_property("filter", flt)
            tex.set_editor_property("srgb", srgb)
        else:
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
            tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        EAL.save_loaded_asset(tex, False)
        reimported_count += 1

    unreal.log(f"[FemaleSpriteImprove] Reimported and saved {reimported_count} Texture2D sheets.")

    # 2. Rebake all 752 PaperSprite render geometries by toggling custom_pivot_point (+1, then restore)
    # Per .revvy/shared-memory.md: PaperSprite only rebuilds its baked render box when a property actually changes value.
    # IMPORTANT: Snapshot float(cp.x) and float(cp.y) first because get_editor_property("custom_pivot_point")
    # returns a live struct wrapper that mutates in-place when set_editor_property is called!
    rebaked_count = 0
    for sprite_path in sorted(sprite_assets):
        sp = EAL.load_asset(sprite_path)
        if not sp:
            continue
        pm = sp.get_editor_property("pivot_mode")
        cp = sp.get_editor_property("custom_pivot_point")
        orig_x = float(cp.x)
        orig_y = float(cp.y)
        sp.set_editor_property("pivot_mode", unreal.SpritePivotMode.CUSTOM)
        sp.set_editor_property("custom_pivot_point", unreal.Vector2D(orig_x + 1.0, orig_y))
        sp.set_editor_property("custom_pivot_point", unreal.Vector2D(orig_x, orig_y))
        sp.set_editor_property("pivot_mode", pm)
        EAL.save_loaded_asset(sp, False)
        rebaked_count += 1

    unreal.log(f"[FemaleSpriteImprove] Rebaked and saved {rebaked_count} PaperSprite assets.")
    unreal.log("[FemaleSpriteImprove] COMPLETE!")


if __name__ == "__main__":
    main()
