# -*- coding: utf-8 -*-
"""
Unreal Engine 5.8 headless Python script to:
  1) Reimport the 6 separated (headless) Female Novice Body sheets:
     /Game/Art/Sprites/Player/Player/WPlayer/Base/Body/Field/{Idle,Walk}/WCh_Body_{Idle,Walk}_{Front,Side,Back}_Sheet
     and rebake their 48 PaperSprite assets.
  2) Import the 6 Female Novice Hair sheets and create their 48 PaperSprites + 6 PaperFlipbooks:
     /Game/Art/Sprites/Player/Player/WPlayer/Base/Hair/Field/{Idle,Walk}/WCh_Hair_{Idle,Walk}_{Front,Side,Back}*
  3) Import the 4 Female Novice FaceEye sheets and create their 32 PaperSprites + 4 PaperFlipbooks:
     /Game/Art/Sprites/Player/Player/WPlayer/Base/Face/Field/{Idle,Walk}/WCh_FaceEye_{Idle,Walk}_{Front,Side}*
  4) Import the 2 PartIcons:
     /Game/Developers/dbals/UI/PartIcons/T_PartIcon_Hair_Female_1_Front
     /Game/Developers/dbals/UI/PartIcons/T_PartIcon_Face_Female_1_Front
  5) Create PaperZDAnimationSkin_Flipbook assets:
     /Game/Developers/dbals/Data/Appearance/SK_Hair_Female
     /Game/Developers/dbals/Data/Appearance/SK_Eyes_Female
  6) Add Female Novice rows (RaceId=1, Gender=1, JobId=0, PartId=1, SortOrder=4) to:
     /Game/Developers/dbals/Data/Appearance/DT_HairPart (row 'Hair_Female')
     /Game/Developers/dbals/Data/Appearance/DT_FacePart (row 'Eyes_Female')
"""
import os
import json
import unreal

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
PROJ_DIR = unreal.Paths.project_dir()
ART_DIR = os.path.abspath(os.path.join(PROJ_DIR, "ArtSource/Character/Female")).replace("\\", "/")

BASE_ROOT = "/Game/Art/Sprites/Player/Player/WPlayer/Base"
APPEARANCE_DIR = "/Game/Developers/dbals/Data/Appearance"
PART_ICON_DIR = "/Game/Developers/dbals/UI/PartIcons"

FIELD_ANIMS = [
    ("Idle", "Front"),
    ("Idle", "Side"),
    ("Idle", "Back"),
    ("Walk", "Front"),
    ("Walk", "Side"),
    ("Walk", "Back"),
]


def log(msg):
    unreal.log(f"[NoviceSeparate] {msg}")


def import_pixel_texture(png_path, dest_folder, dest_name):
    task = unreal.AssetImportTask()
    task.filename = png_path
    task.destination_path = dest_folder
    task.destination_name = dest_name
    task.replace_existing = True
    task.replace_existing_settings = False
    task.automated = True
    task.save = False
    AT.import_asset_tasks([task])

    asset_path = f"{dest_folder}/{dest_name}"
    tex = EAL.load_asset(asset_path)
    assert tex, f"Failed to import {asset_path} from {png_path}"
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property("srgb", True)
    EAL.save_loaded_asset(tex, False)
    return tex


def rebake_sprite(sp, new_tex=None):
    if new_tex is not None:
        sp.set_editor_property("source_texture", new_tex)
    pm = sp.get_editor_property("pivot_mode")
    uv = sp.get_editor_property("source_uv")
    orig_x = float(uv.x) + 30.0
    orig_y = 96.0
    sp.set_editor_property("pivot_mode", unreal.SpritePivotMode.CUSTOM)
    sp.set_editor_property("custom_pivot_point", unreal.Vector2D(orig_x + 1.0, orig_y))
    sp.set_editor_property("custom_pivot_point", unreal.Vector2D(orig_x, orig_y))
    sp.set_editor_property("pivot_mode", pm)
    EAL.save_loaded_asset(sp, False)


def setup_novice_sprites_and_flipbooks():
    # 1. Reimport headless Body sheets & rebake Body sprites
    for state, dir_name in FIELD_ANIMS:
        anim = f"{state}_{dir_name}"
        body_folder = f"{BASE_ROOT}/Body/Field/{state}"
        sheet_name = f"WCh_Body_{anim}_Sheet"
        tex = import_pixel_texture(f"{ART_DIR}/{sheet_name}.png", body_folder, sheet_name)
        for f in range(8):
            sp_path = f"{body_folder}/WCh_Body_{anim}_{f}_Sprite"
            sp = EAL.load_asset(sp_path)
            assert sp, f"Missing body sprite {sp_path}"
            rebake_sprite(sp, tex)
        log(f"Reimported headless {sheet_name} and rebaked 8 sprites")

    # 2. Import Hair (6 anims) and FaceEye (4 anims) sheets, sprites, and flipbooks
    for part_folder_name, part_prefix, has_back in [("Hair", "Hair", True), ("Face", "FaceEye", False)]:
        for state, dir_name in FIELD_ANIMS:
            if not has_back and dir_name == "Back":
                continue
            anim = f"{state}_{dir_name}"
            src_body_folder = f"{BASE_ROOT}/Body/Field/{state}"
            dst_folder = f"{BASE_ROOT}/{part_folder_name}/Field/{state}"
            if not EAL.does_directory_exist(dst_folder):
                EAL.make_directory(dst_folder)

            sheet_name = f"WCh_{part_prefix}_{anim}_Sheet"
            tex = import_pixel_texture(f"{ART_DIR}/{sheet_name}.png", dst_folder, sheet_name)

            # Create or update 8 PaperSprites by duplicating from Body sprite (preserves exact UV, dim, pivot, material)
            part_sprites = []
            for f in range(8):
                src_sp_path = f"{src_body_folder}/WCh_Body_{anim}_{f}_Sprite"
                dst_sp_path = f"{dst_folder}/WCh_{part_prefix}_{anim}_{f}_Sprite"
                if EAL.does_asset_exist(dst_sp_path):
                    sp = EAL.load_asset(dst_sp_path)
                else:
                    sp = EAL.duplicate_asset(src_sp_path, dst_sp_path)
                assert sp, f"Failed to create/load sprite {dst_sp_path}"
                rebake_sprite(sp, tex)
                part_sprites.append(sp)

            # Create or update PaperFlipbook by duplicating from Body flipbook (preserves exact FPS and keyframe timings)
            src_fb_path = f"{src_body_folder}/WCh_Body_{anim}"
            dst_fb_path = f"{dst_folder}/WCh_{part_prefix}_{anim}"
            if EAL.does_asset_exist(dst_fb_path):
                fb = EAL.load_asset(dst_fb_path)
            else:
                fb = EAL.duplicate_asset(src_fb_path, dst_fb_path)
            assert fb, f"Failed to create/load flipbook {dst_fb_path}"

            kfs = list(fb.get_editor_property("key_frames"))
            for f in range(min(8, len(kfs))):
                kfs[f].set_editor_property("sprite", part_sprites[f])
            fb.set_editor_property("key_frames", kfs)
            EAL.save_loaded_asset(fb, False)
            log(f"Created/updated {dst_fb_path} with 8 sprites")


def setup_part_icons():
    for icon_name in ["T_PartIcon_Hair_Female_1_Front", "T_PartIcon_Face_Female_1_Front"]:
        import_pixel_texture(f"{ART_DIR}/{icon_name}.png", PART_ICON_DIR, icon_name)
        log(f"Imported icon {PART_ICON_DIR}/{icon_name}")


def setup_skins():
    # Create SK_Hair_Female from SK_Hair_Female_Tracker and SK_Eyes_Female from SK_Eyes_Female_Tracker
    ORS = getattr(unreal, "ObjectReflectionService", None)

    for src_name, dst_name, old_sub, new_sub in [
        (
            "SK_Hair_Female_Tracker",
            "SK_Hair_Female",
            "/Game/Art/Sprites/Player/Player/WPlayer/Tracker/Field/",
            "/Game/Art/Sprites/Player/Player/WPlayer/Base/Hair/Field/",
        ),
        (
            "SK_Eyes_Female_Tracker",
            "SK_Eyes_Female",
            "/Game/Art/Sprites/Player/Player/WPlayer/Tracker/Field/",
            "/Game/Art/Sprites/Player/Player/WPlayer/Base/Face/Field/",
        ),
    ]:
        src_path = f"{APPEARANCE_DIR}/{src_name}"
        dst_path = f"{APPEARANCE_DIR}/{dst_name}"
        if EAL.does_asset_exist(dst_path):
            EAL.delete_asset(dst_path)
        skin = EAL.duplicate_asset(src_path, dst_path)
        assert skin, f"Failed to duplicate {src_path} -> {dst_path}"

        # Try ObjectReflectionService first, or direct editor property access
        updated = False
        if ORS is not None:
            raw_text = ORS.get_property(dst_path, "SkinsPerAnimation")
            log(f"{dst_name} SkinsPerAnimation before: {raw_text}")
            if raw_text:
                new_text = raw_text.replace(old_sub, new_sub).replace("WCh_Tracker_Hair_", "WCh_Hair_").replace("WCh_Tracker_FaceEye_", "WCh_FaceEye_")
                ok = ORS.set_property(dst_path, "SkinsPerAnimation", new_text)
                log(f"{dst_name} set_property via ORS: ok={ok}, after={ORS.get_property(dst_path, 'SkinsPerAnimation')}")
                updated = bool(ok)

        if not updated:
            try:
                spa = skin.get_editor_property("skins_per_animation")
                log(f"{dst_name} skins_per_animation via get_editor_property: {spa}")
            except Exception as e:
                log(f"{dst_name} get_editor_property error: {e}")

        EAL.save_loaded_asset(skin, False)
        log(f"Saved {dst_path}")


def setup_datatables():
    DTFL = getattr(unreal, "DataTableFunctionLibrary", None)
    ORS = getattr(unreal, "ObjectReflectionService", None)

    for dt_name, row_name, icon_name, idle_fb, walk_fb, skin_name in [
        (
            "DT_HairPart",
            "Hair_Female",
            f"{PART_ICON_DIR}/T_PartIcon_Hair_Female_1_Front.T_PartIcon_Hair_Female_1_Front",
            f"{BASE_ROOT}/Hair/Field/Idle/WCh_Hair_Idle_Front.WCh_Hair_Idle_Front",
            f"{BASE_ROOT}/Hair/Field/Walk/WCh_Hair_Walk_Front.WCh_Hair_Walk_Front",
            f"{APPEARANCE_DIR}/SK_Hair_Female.SK_Hair_Female",
        ),
        (
            "DT_FacePart",
            "Eyes_Female",
            f"{PART_ICON_DIR}/T_PartIcon_Face_Female_1_Front.T_PartIcon_Face_Female_1_Front",
            f"{BASE_ROOT}/Face/Field/Idle/WCh_FaceEye_Idle_Front.WCh_FaceEye_Idle_Front",
            f"{BASE_ROOT}/Face/Field/Walk/WCh_FaceEye_Walk_Front.WCh_FaceEye_Walk_Front",
            f"{APPEARANCE_DIR}/SK_Eyes_Female.SK_Eyes_Female",
        ),
    ]:
        dt_path = f"{APPEARANCE_DIR}/{dt_name}"
        dt = EAL.load_asset(dt_path)
        assert dt, f"Missing {dt_path}"

        # Export existing DataTable to JSON string
        json_str = None
        if hasattr(dt, "export_to_json_string"):
            json_str = dt.export_to_json_string()
        elif DTFL and hasattr(DTFL, "export_data_table_to_json_string"):
            json_str = DTFL.export_data_table_to_json_string(dt)

        log(f"{dt_name} exported JSON: {json_str}")
        if json_str:
            rows = json.loads(json_str)
            # Remove existing row_name if present so we can add/update cleanly at the end
            rows = [r for r in rows if r.get("Name") != row_name]
            new_row = {
                "Name": row_name,
                "PartId": 1,
                "Icon": icon_name,
                "Flipbook": idle_fb,
                "WalkFlipbook": walk_fb,
                "FieldSkin": skin_name,
                "Tint": {"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0},
                "RaceId": 1,
                "Gender": 1,
                "JobId": 0,
                "SortOrder": 4,
            }
            rows.append(new_row)
            new_json_str = json.dumps(rows, ensure_ascii=False)
            if DTFL and hasattr(DTFL, "fill_data_table_from_json_string"):
                ok = DTFL.fill_data_table_from_json_string(dt, new_json_str)
                log(f"{dt_name} fill_data_table_from_json_string ok={ok}")
            elif hasattr(dt, "fill_from_json_string"):
                ok = dt.fill_from_json_string(new_json_str)
                log(f"{dt_name} fill_from_json_string ok={ok}")
            EAL.save_loaded_asset(dt, False)
            log(f"Saved {dt_path} with row {row_name}")


def main():
    setup_novice_sprites_and_flipbooks()
    setup_part_icons()
    setup_skins()
    setup_datatables()
    log("COMPLETE!")


if __name__ == "__main__":
    main()
