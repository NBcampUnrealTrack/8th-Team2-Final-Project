# -*- coding: utf-8 -*-
"""
Unreal Engine 5.8 headless Python script to import and configure:
  - 4 Male Hairstyles (PartId 1..4) & 4 Female Hairstyles (PartId 1..4)
  - 5 Male Mabinogi-Style Eye Expressions (PartId 1..5) & 5 Female Eye Expressions (PartId 1..5)
  - Textures (Sheet PNGs + UI PartIcons)
  - PaperSprites (8 frames per animation, pivot (30, 96))
  - PaperFlipbooks (Idle/Walk Front/Side/Back)
  - PaperZDAnimationSkin_Flipbook assets
  - DataTable rows in DT_HairPart (4 Male + 4 Female + Job rows) and DT_FacePart (5 Male + 5 Female + Job rows)
"""
import os
import json
import unreal

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
ORS = getattr(unreal, "ObjectReflectionService", None)
DTFL = getattr(unreal, "DataTableFunctionLibrary", None)

PROJ_DIR = unreal.Paths.project_dir()
MALE_ART_DIR = os.path.abspath(os.path.join(PROJ_DIR, "ArtSource/Character/Novice")).replace("\\", "/")
FEMALE_ART_DIR = os.path.abspath(os.path.join(PROJ_DIR, "ArtSource/Character/Female")).replace("\\", "/")

M_BASE_ROOT = "/Game/Art/Sprites/Player/Player/MPlayer/Base"
W_BASE_ROOT = "/Game/Art/Sprites/Player/Player/WPlayer/Base"
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
    unreal.log(f"[CustomPartsUE] {msg}")


def import_pixel_texture(png_path, dest_folder, dest_name):
    if not EAL.does_directory_exist(dest_folder):
        EAL.make_directory(dest_folder)
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


def rebake_sprite(sp, new_tex=None, force_base_pivot=True):
    if new_tex is not None:
        sp.set_editor_property("source_texture", new_tex)
    pm = sp.get_editor_property("pivot_mode")
    if force_base_pivot:
        uv = sp.get_editor_property("source_uv")
        orig_x = float(uv.x) + 30.0
        orig_y = 96.0
    else:
        cp = sp.get_editor_property("custom_pivot_point")
        orig_x = float(cp.x)
        orig_y = float(cp.y)
    sp.set_editor_property("pivot_mode", unreal.SpritePivotMode.CUSTOM)
    sp.set_editor_property("custom_pivot_point", unreal.Vector2D(orig_x + 1.0, orig_y))
    sp.set_editor_property("custom_pivot_point", unreal.Vector2D(orig_x, orig_y))
    sp.set_editor_property("pivot_mode", pm)
    EAL.save_loaded_asset(sp, False)


def import_and_bake_male_set(base_sub, base_prefix, dst_sub, dst_prefix):
    src_folder = f"{M_BASE_ROOT}/{base_sub}"
    dst_folder = f"{M_BASE_ROOT}/{dst_sub}"
    if not EAL.does_directory_exist(dst_folder):
        EAL.make_directory(dst_folder)

    for state, dir_name in FIELD_ANIMS:
        anim = f"{state}_{dir_name}"
        sheet_name = f"{dst_prefix}_{anim}_Sheet"
        tex = import_pixel_texture(f"{MALE_ART_DIR}/{sheet_name}.png", dst_folder, sheet_name)

        part_sprites = []
        for f in range(8):
            src_sp_path = f"{src_folder}/{base_prefix}_{anim}_{f}_Sprite"
            dst_sp_path = f"{dst_folder}/{dst_prefix}_{anim}_{f}_Sprite"
            if EAL.does_asset_exist(dst_sp_path):
                sp = EAL.load_asset(dst_sp_path)
            else:
                sp = EAL.duplicate_asset(src_sp_path, dst_sp_path)
            assert sp, f"Failed sprite {dst_sp_path}"
            rebake_sprite(sp, tex, force_base_pivot=True)
            part_sprites.append(sp)

        if dst_folder != src_folder:
            src_fb_path = f"{src_folder}/{base_prefix}_{anim}"
            dst_fb_path = f"{dst_folder}/{dst_prefix}_{anim}"
            if EAL.does_asset_exist(dst_fb_path):
                fb = EAL.load_asset(dst_fb_path)
            else:
                fb = EAL.duplicate_asset(src_fb_path, dst_fb_path)
            assert fb, f"Failed flipbook {dst_fb_path}"
            kfs = list(fb.get_editor_property("key_frames"))
            for f in range(min(8, len(kfs))):
                kfs[f].set_editor_property("sprite", part_sprites[f])
            fb.set_editor_property("key_frames", kfs)
            EAL.save_loaded_asset(fb, False)


def setup_male_parts():
    # 0. Reimport all 6 MCh_Body sheets (Front, Side, Back) & rebake with canonical (uv.x + 30.0, 96.0)
    for state, dir_name in FIELD_ANIMS:
        anim = f"{state}_{dir_name}"
        body_folder = f"{M_BASE_ROOT}/Body/Field/{state}"
        sheet_name = f"MCh_Body_{anim}_Sheet"
        tex = import_pixel_texture(f"{MALE_ART_DIR}/{sheet_name}.png", body_folder, sheet_name)
        for f in range(8):
            sp_path = f"{body_folder}/MCh_Body_{anim}_{f}_Sprite"
            sp = EAL.load_asset(sp_path)
            if sp:
                rebake_sprite(sp, tex, force_base_pivot=True)

    # PartId = 1
    import_and_bake_male_set("Hair", "MCh_Hair1", "Hair", "MCh_Hair1")
    import_and_bake_male_set("Face", "MCh_FaceEye", "Face", "MCh_FaceEye")
    # Hair 2..3
    for style_id in range(2, 4):
        import_and_bake_male_set("Hair", "MCh_Hair1", f"Hair{style_id}", f"MCh_Hair{style_id}")
    # Face 2..5
    for style_id in range(2, 6):
        import_and_bake_male_set("Face", "MCh_FaceEye", f"Face{style_id}", f"MCh_FaceEye{style_id}")
    log("Reimported and rebaked all Male Body, Hair (1..3), and Face (1..5) assets.")


def import_and_bake_female_set(base_sub, base_prefix, dst_sub, dst_prefix, has_back):
    for state, dir_name in FIELD_ANIMS:
        if not has_back and dir_name == "Back":
            continue
        anim = f"{state}_{dir_name}"
        src_folder = f"{W_BASE_ROOT}/{base_sub}/Field/{state}"
        dst_folder = f"{W_BASE_ROOT}/{dst_sub}/Field/{state}"
        if not EAL.does_directory_exist(dst_folder):
            EAL.make_directory(dst_folder)

        sheet_name = f"{dst_prefix}_{anim}_Sheet"
        tex = import_pixel_texture(f"{FEMALE_ART_DIR}/{sheet_name}.png", dst_folder, sheet_name)

        part_sprites = []
        for f in range(8):
            src_sp_path = f"{src_folder}/{base_prefix}_{anim}_{f}_Sprite"
            dst_sp_path = f"{dst_folder}/{dst_prefix}_{anim}_{f}_Sprite"
            if EAL.does_asset_exist(dst_sp_path):
                sp = EAL.load_asset(dst_sp_path)
            else:
                sp = EAL.duplicate_asset(src_sp_path, dst_sp_path)
            assert sp, f"Failed sprite {dst_sp_path}"
            rebake_sprite(sp, tex, force_base_pivot=True)
            part_sprites.append(sp)

        if dst_folder != src_folder:
            src_fb_path = f"{src_folder}/{base_prefix}_{anim}"
            dst_fb_path = f"{dst_folder}/{dst_prefix}_{anim}"
            if EAL.does_asset_exist(dst_fb_path):
                fb = EAL.load_asset(dst_fb_path)
            else:
                fb = EAL.duplicate_asset(src_fb_path, dst_fb_path)
            assert fb, f"Failed flipbook {dst_fb_path}"
            kfs = list(fb.get_editor_property("key_frames"))
            for f in range(min(8, len(kfs))):
                kfs[f].set_editor_property("sprite", part_sprites[f])
            fb.set_editor_property("key_frames", kfs)
            EAL.save_loaded_asset(fb, False)


def setup_female_parts():
    # 1. Reimport all 6 WCh_Body sheets (Front, Side, Back) & rebake with canonical (uv.x + 30.0, 96.0)
    for state, dir_name in FIELD_ANIMS:
        anim = f"{state}_{dir_name}"
        body_folder = f"{W_BASE_ROOT}/Body/Field/{state}"
        sheet_name = f"WCh_Body_{anim}_Sheet"
        tex = import_pixel_texture(f"{FEMALE_ART_DIR}/{sheet_name}.png", body_folder, sheet_name)
        for f in range(8):
            sp_path = f"{body_folder}/WCh_Body_{anim}_{f}_Sprite"
            sp = EAL.load_asset(sp_path)
            if sp:
                rebake_sprite(sp, tex, force_base_pivot=True)

    # 2. Reimport WCh_Hair (PartId=1) and WCh_FaceEye (PartId=1)
    import_and_bake_female_set("Hair", "WCh_Hair", "Hair", "WCh_Hair", True)
    import_and_bake_female_set("Face", "WCh_FaceEye", "Face", "WCh_FaceEye", False)

    # 3. Import WCh_Hair2..3 (6 anims) and WCh_FaceEye2..5 (4 anims)
    for style_id in range(2, 4):
        import_and_bake_female_set("Hair", "WCh_Hair", f"Hair{style_id}", f"WCh_Hair{style_id}", True)
    for style_id in range(2, 6):
        import_and_bake_female_set("Face", "WCh_FaceEye", f"Face{style_id}", f"WCh_FaceEye{style_id}", False)

    # 4. Rebake all WPlayer Job sprites (Gambler, Druid, Mage, Tracker) preserving their exact HEAD CustomPivotPoint
    wplayer_root = "/Game/Art/Sprites/Player/Player/WPlayer"
    all_w_assets = EAL.list_assets(wplayer_root, recursive=True, include_folder=False)
    job_sprites = [
        p.split(".")[0]
        for p in all_w_assets
        if p.split(".")[0].endswith("_Sprite") and "/Base/" not in p
    ]
    for sp_path in sorted(job_sprites):
        sp = EAL.load_asset(sp_path)
        if sp:
            rebake_sprite(sp, None, force_base_pivot=False)

    log("Reimported and rebaked all Female Hair (1..3), Face (1..5), Body, and Job sprites.")


def setup_part_icons():
    for i in range(1, 4):
        m_hair_icon = f"T_PartIcon_Hair_{i}_Front"
        w_hair_icon = f"T_PartIcon_Hair_Female_{i}_Front"
        import_pixel_texture(f"{MALE_ART_DIR}/{m_hair_icon}.png", PART_ICON_DIR, m_hair_icon)
        import_pixel_texture(f"{FEMALE_ART_DIR}/{w_hair_icon}.png", PART_ICON_DIR, w_hair_icon)
    for i in range(1, 6):
        m_eye_icon = f"T_PartIcon_Eyes_{i}_Front"
        w_eye_icon = f"T_PartIcon_Face_Female_{i}_Front"
        import_pixel_texture(f"{MALE_ART_DIR}/{m_eye_icon}.png", PART_ICON_DIR, m_eye_icon)
        import_pixel_texture(f"{FEMALE_ART_DIR}/{w_eye_icon}.png", PART_ICON_DIR, w_eye_icon)
    log("Imported all 16 PartIcon textures into " + PART_ICON_DIR)


def duplicate_and_rebind_skin(src_skin_name, dst_skin_name, replacements):
    src_path = f"{APPEARANCE_DIR}/{src_skin_name}"
    dst_path = f"{APPEARANCE_DIR}/{dst_skin_name}"
    if EAL.does_asset_exist(dst_path):
        skin = EAL.load_asset(dst_path)
    else:
        skin = EAL.duplicate_asset(src_path, dst_path)
    assert skin, f"Failed to load/duplicate {src_path} -> {dst_path}"

    assert ORS is not None, "ObjectReflectionService is required to update SkinsPerAnimation"
    raw_text = ORS.get_property(src_path, "SkinsPerAnimation")
    assert raw_text, f"Empty SkinsPerAnimation on {src_path}"
    new_text = raw_text
    for old_s, new_s in replacements:
        new_text = new_text.replace(old_s, new_s)
    ok = ORS.set_property(dst_path, "SkinsPerAnimation", new_text)
    assert ok, f"Failed ORS.set_property on {dst_path}"
    EAL.save_loaded_asset(skin, False)
    log(f"Created/updated skin {dst_path}")


def setup_skins():
    for i in range(2, 4):
        duplicate_and_rebind_skin(
            "SK_Hair_1",
            f"SK_Hair_{i}",
            [
                ("/Base/Hair/", f"/Base/Hair{i}/"),
                ("MCh_Hair1_", f"MCh_Hair{i}_"),
            ],
        )
        duplicate_and_rebind_skin(
            "SK_Hair_Female",
            f"SK_Hair_Female_{i}",
            [
                ("/Base/Hair/Field/", f"/Base/Hair{i}/Field/"),
                ("WCh_Hair_", f"WCh_Hair{i}_"),
            ],
        )
    for i in range(2, 6):
        duplicate_and_rebind_skin(
            "SK_Eyes_1",
            f"SK_Eyes_{i}",
            [
                ("/Base/Face/", f"/Base/Face{i}/"),
                ("MCh_FaceEye_", f"MCh_FaceEye{i}_"),
            ],
        )
        duplicate_and_rebind_skin(
            "SK_Eyes_Female",
            f"SK_Eyes_Female_{i}",
            [
                ("/Base/Face/Field/", f"/Base/Face{i}/Field/"),
                ("WCh_FaceEye_", f"WCh_FaceEye{i}_"),
            ],
        )


def setup_datatables():
    # 1. Update DT_HairPart (3 Male + 3 Female + Job rows)
    hair_dt_path = f"{APPEARANCE_DIR}/DT_HairPart"
    hair_dt = EAL.load_asset(hair_dt_path)
    assert hair_dt, f"Missing {hair_dt_path}"
    existing_hair_rows = json.loads(DTFL.export_data_table_to_json_string(hair_dt))
    job_hair_rows = [r for r in existing_hair_rows if r.get("JobId", 0) != 0]

    new_hair_rows = list(job_hair_rows)
    for i in range(1, 4):
        sub = "Hair" if i == 1 else f"Hair{i}"
        pref = f"MCh_Hair{i}"
        new_hair_rows.append(
            {
                "Name": f"Hair_{i}",
                "PartId": i,
                "Icon": f"{PART_ICON_DIR}/T_PartIcon_Hair_{i}_Front.T_PartIcon_Hair_{i}_Front",
                "Flipbook": f"{M_BASE_ROOT}/{sub}/{pref}_Idle_Front.{pref}_Idle_Front",
                "WalkFlipbook": f"{M_BASE_ROOT}/{sub}/{pref}_Walk_Front.{pref}_Walk_Front",
                "FieldSkin": f"{APPEARANCE_DIR}/SK_Hair_{i}.SK_Hair_{i}",
                "Tint": {"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0},
                "RaceId": 1,
                "Gender": 0,
                "JobId": 0,
                "SortOrder": i - 1,
            }
        )
    for i in range(1, 4):
        row_name = "Hair_Female" if i == 1 else f"Hair_Female_{i}"
        sub = "Hair" if i == 1 else f"Hair{i}"
        pref = "WCh_Hair" if i == 1 else f"WCh_Hair{i}"
        skin_name = "SK_Hair_Female" if i == 1 else f"SK_Hair_Female_{i}"
        new_hair_rows.append(
            {
                "Name": row_name,
                "PartId": i,
                "Icon": f"{PART_ICON_DIR}/T_PartIcon_Hair_Female_{i}_Front.T_PartIcon_Hair_Female_{i}_Front",
                "Flipbook": f"{W_BASE_ROOT}/{sub}/Field/Idle/{pref}_Idle_Front.{pref}_Idle_Front",
                "WalkFlipbook": f"{W_BASE_ROOT}/{sub}/Field/Walk/{pref}_Walk_Front.{pref}_Walk_Front",
                "FieldSkin": f"{APPEARANCE_DIR}/{skin_name}.{skin_name}",
                "Tint": {"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0},
                "RaceId": 1,
                "Gender": 1,
                "JobId": 0,
                "SortOrder": 10 + i - 1,
            }
        )

    ok_h = DTFL.fill_data_table_from_json_string(hair_dt, json.dumps(new_hair_rows, ensure_ascii=False))
    assert ok_h, "Failed fill_data_table_from_json_string on DT_HairPart"
    EAL.save_loaded_asset(hair_dt, False)
    log(f"Updated DT_HairPart with {len(new_hair_rows)} total rows (3 Male + 3 Female + {len(job_hair_rows)} Job rows)")

    # 2. Update DT_FacePart (5 Male + 5 Female + Job rows)
    face_dt_path = f"{APPEARANCE_DIR}/DT_FacePart"
    face_dt = EAL.load_asset(face_dt_path)
    assert face_dt, f"Missing {face_dt_path}"
    existing_face_rows = json.loads(DTFL.export_data_table_to_json_string(face_dt))
    job_face_rows = [r for r in existing_face_rows if r.get("JobId", 0) != 0]

    new_face_rows = list(job_face_rows)
    for i in range(1, 6):
        sub = "Face" if i == 1 else f"Face{i}"
        pref = "MCh_FaceEye" if i == 1 else f"MCh_FaceEye{i}"
        new_face_rows.append(
            {
                "Name": f"Eyes_{i}",
                "PartId": i,
                "Icon": f"{PART_ICON_DIR}/T_PartIcon_Eyes_{i}_Front.T_PartIcon_Eyes_{i}_Front",
                "Flipbook": f"{M_BASE_ROOT}/{sub}/{pref}_Idle_Front.{pref}_Idle_Front",
                "WalkFlipbook": f"{M_BASE_ROOT}/{sub}/{pref}_Walk_Front.{pref}_Walk_Front",
                "FieldSkin": f"{APPEARANCE_DIR}/SK_Eyes_{i}.SK_Eyes_{i}",
                "Tint": {"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0},
                "RaceId": 1,
                "Gender": 0,
                "JobId": 0,
                "SortOrder": i - 1,
            }
        )
    for i in range(1, 6):
        row_name = "Eyes_Female" if i == 1 else f"Eyes_Female_{i}"
        sub = "Face" if i == 1 else f"Face{i}"
        pref = "WCh_FaceEye" if i == 1 else f"WCh_FaceEye{i}"
        skin_name = "SK_Eyes_Female" if i == 1 else f"SK_Eyes_Female_{i}"
        new_face_rows.append(
            {
                "Name": row_name,
                "PartId": i,
                "Icon": f"{PART_ICON_DIR}/T_PartIcon_Face_Female_{i}_Front.T_PartIcon_Face_Female_{i}_Front",
                "Flipbook": f"{W_BASE_ROOT}/{sub}/Field/Idle/{pref}_Idle_Front.{pref}_Idle_Front",
                "WalkFlipbook": f"{W_BASE_ROOT}/{sub}/Field/Walk/{pref}_Walk_Front.{pref}_Walk_Front",
                "FieldSkin": f"{APPEARANCE_DIR}/{skin_name}.{skin_name}",
                "Tint": {"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0},
                "RaceId": 1,
                "Gender": 1,
                "JobId": 0,
                "SortOrder": 10 + i - 1,
            }
        )

    ok_f = DTFL.fill_data_table_from_json_string(face_dt, json.dumps(new_face_rows, ensure_ascii=False))
    assert ok_f, "Failed fill_data_table_from_json_string on DT_FacePart"
    EAL.save_loaded_asset(face_dt, False)
    log(f"Updated DT_FacePart with {len(new_face_rows)} total rows (5 Male + 5 Female + {len(job_face_rows)} Job rows)")


def main():
    setup_male_parts()
    setup_female_parts()
    setup_part_icons()
    setup_skins()
    setup_datatables()
    log("ALL CUSTOM PARTS RE-IMPORTED, REBAKED, AND REGISTERED SUCCESSFULLY!")


if __name__ == "__main__":
    main()
