# -*- coding: utf-8 -*-
"""Character creation stage: inn door backdrop, one stand, fixed exposure.

Headless:  UnrealEditor-Cmd.exe ProjectM.uproject -run=pythonscript -script=Tools/CharacterCreateGlass/apply_stage.py
Imports the baked backdrop (Tools/CharacterCreateGlass/bake_backdrop.py), makes its material instance and re-stages both
creation levels in place. Safe to run again: the first run keeps a copy of each map and a receipt of the old values in
Saved/CharacterCreateGlass/Backup, later runs only re-apply.

Stage numbers (1080p): the camera sits 1030 units in front of the backdrop plane with a 90 degree horizontal view at 16:9,
so a 16:9 frame there is 2060 x 1158.75 (1 cell of the stage = 5 screen px). The stage art is 2560:1080 wide (512 x 216 cells),
so the plane is 2746.67 x 1158.75 and the camera keeps its vertical angle at every screen shape.
A stand at depth 530 shows 960 * scale / 530 screen px per sprite px, so scale 2.7604 gives the same 5 px.
"""
import json
import os
import shutil
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(ROOT, 'ArtSource/UI/CharacterCreateGlass_20261013')
BACKUP = os.path.join(ROOT, 'Saved/CharacterCreateGlass/Backup')
DST = '/Game/Art/UI/CharacterCreate/Glass/Stage'
MAPS = ['/Game/Maps/L_CharacterCreate', '/Game/Developers/dbals/Map/L_CharacterCreate']
E = unreal.EditorAssetLibrary

PLANE_SCALE = unreal.Vector(27.466667, 11.5875, 1.0)       # 2746.67 x 1158.75: the 2560 x 1080 stage art
STAND_SCALE = 2.7604
STAND_LOCATION = unreal.Vector(140.8, -100.0, -15.3)   # feet at screen (705, 930)
SPARE_STANDS = ('Stand_Elf', 'Stand_Dwarf', 'Stand_Beastfolk')
EXPOSURE_BIAS = float(os.environ.get('CC_EXPOSURE_BIAS', '0'))
LIGHT_INTENSITY = os.environ.get('CC_LIGHT', '')           # empty = leave the directional light as it is


def log(message):
    unreal.log('[CCG] ' + str(message))


def import_backdrop():
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, 'T_CCG_InnDoor_Backdrop.png')
    task.destination_path = DST
    task.destination_name = 'T_CCG_InnDoor_Backdrop'
    task.automated = True; task.replace_existing = True; task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(DST + '/T_CCG_InnDoor_Backdrop')
    assert texture, 'backdrop import failed'
    texture.set_editor_properties({
        'compression_settings': unreal.TextureCompressionSettings.TC_BC7,   # dark gradients band under DXT1
        'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI,
        'never_stream': True,
        'srgb': True,
    })
    assert E.save_asset(texture.get_path_name().split('.')[0], False)
    log('backdrop %dx%d' % (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
    return texture


def backdrop_material(texture):
    path = DST + '/MI_CCG_InnDoor_Backdrop'
    instance = unreal.load_asset(path) if E.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'MI_CCG_InnDoor_Backdrop', DST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    M = unreal.MaterialEditingLibrary
    M.set_material_instance_parent(instance, unreal.load_asset('/Game/Developers/dbals/UI/Backgrounds/M_BG_Unlit'))
    M.set_material_instance_texture_parameter_value(instance, 'BG', texture)
    M.update_material_instance(instance)
    assert E.save_asset(path, False)
    return instance


def vec(v):
    return [round(v.x, 4), round(v.y, 4), round(v.z, 4)]


def stage(map_path, material):
    file_name = map_path.replace('/Game/', 'Content/') + '.umap'
    key = map_path.strip('/').replace('/', '_')
    keep = os.path.join(BACKUP, key + '.umap')
    first_run = not os.path.exists(keep)
    if first_run:
        os.makedirs(BACKUP, exist_ok=True)
        shutil.copy2(os.path.join(ROOT, file_name), keep)

    assert unreal.EditorLoadingAndSavingUtils.load_map(map_path), map_path
    actors = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
    receipt = {}

    plane = actors['BG_Inn']
    receipt['BG_Inn'] = {'scale': vec(plane.get_actor_scale3d()), 'material': plane.static_mesh_component.get_material(0).get_path_name()}
    plane.modify()
    plane.static_mesh_component.set_material(0, material)
    plane.set_actor_scale3d(PLANE_SCALE)

    human = actors['Stand_Human']
    receipt['Stand_Human'] = {'location': vec(human.get_actor_location()), 'scale': vec(human.get_actor_scale3d())}
    human.modify()
    human.set_actor_location(STAND_LOCATION, False, False)
    human.set_actor_scale3d(unreal.Vector(STAND_SCALE, STAND_SCALE, STAND_SCALE))
    # The flipbooks default to a lit material; on this stage the sprite must show its painted colours.
    # The game's own character material does that and is drawn after temporal anti-aliasing, so the pixels stay as crisp as the backdrop
    # (a masked sprite goes through it and comes out soft). The camera's fixed exposure keeps its colours exact.
    unlit = unreal.load_asset('/Game/Art/Materials/M_NUCharacterSprite')
    assert unlit, 'character sprite material missing'
    for sprite in human.get_components_by_class(unreal.PaperFlipbookComponent):
        sprite.modify()
        sprite.set_material(0, unlit)

    # one stage, one character: the locked races stay in the level but are neither drawn nor clickable
    for label in SPARE_STANDS:
        spare = actors.get(label)
        if spare is None:
            continue
        receipt[label] = {'hidden': spare.is_hidden_ed(), 'location': vec(spare.get_actor_location())}
        spare.modify()
        spare.set_actor_hidden_in_game(True)
        spare.set_actor_enable_collision(False)

    # the backdrop is an unlit picture: fixed exposure, no film curve, no bloom or vignette, so the screen shows its colours as painted
    camera = actors['Cam_Overview'].camera_component
    camera.modify()
    # Keep the height: the 90 degree view is horizontal at 16:9, so its vertical angle is fixed and other shapes only change
    # how much width is seen (wider screens see more of the stage, 16:10 and 4:3 lose the sides). Pixels stay 5 px at 1080p.
    camera.set_editor_property('override_aspect_ratio_axis_constraint', True)
    camera.set_editor_property('aspect_ratio_axis_constraint', unreal.AspectRatioAxisConstraint.ASPECT_RATIO_MAINTAIN_YFOV)
    camera.set_editor_property('aspect_ratio', 16.0 / 9.0)
    pp = camera.get_editor_property('post_process_settings')
    for name, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL), ('auto_exposure_bias', EXPOSURE_BIAS),
                        ('auto_exposure_apply_physical_camera_exposure', False), ('tone_curve_amount', 0.0), ('expand_gamut', 0.0),
                        ('bloom_intensity', 0.0), ('vignette_intensity', 0.0),
                        # the project default (0.8) flattens contrast: darks came out a third brighter than painted
                        ('local_exposure_highlight_contrast_scale', 1.0), ('local_exposure_shadow_contrast_scale', 1.0)):
        pp.set_editor_property('override_' + name, True)
        pp.set_editor_property(name, value)
    camera.set_editor_property('post_process_settings', pp)

    if LIGHT_INTENSITY:
        light = actors['DirectionalLight'].get_component_by_class(unreal.DirectionalLightComponent)
        receipt['DirectionalLight'] = {'intensity': light.get_editor_property('intensity')}
        light.modify()
        light.set_editor_property('intensity', float(LIGHT_INTENSITY))

    assert unreal.EditorLevelLibrary.save_current_level(), map_path
    if first_run:
        with open(os.path.join(BACKUP, key + '_before.json'), 'w', encoding='utf-8') as handle:
            json.dump(receipt, handle, indent=1)
    log('staged %s (%s)' % (map_path, 'first run, backup kept' if first_run else 're-applied'))


texture = import_backdrop()
material = backdrop_material(texture)
for map_path in MAPS:
    stage(map_path, material)
log('DONE')
