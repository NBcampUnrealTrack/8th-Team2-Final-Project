# -*- coding: utf-8 -*-
"""Read-only: exports the title screen pictures to Saved/GoogleLogin/art/*.png (to design the Google login screen on top)."""
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Saved/GoogleLogin/art')
os.makedirs(OUT, exist_ok=True)
for path in ('/Game/Developers/MinkiCho/Art/Login/T_Title_Background', '/Game/Developers/MinkiCho/Art/Login/T_Title_Image',
             '/Game/Developers/MinkiCho/Art/Login/T_Title_Bar', '/Game/Developers/MinkiCho/Art/Login/T_TitleText',
             '/Game/Art/UI/T_Title_MenuBox', '/Game/Developers/MinkiCho/Art/Button/T_Title_Button'):
    texture = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = os.path.join(OUT, path.rsplit('/', 1)[1] + '.png')
    task.automated = True; task.replace_identical = True; task.prompt = False
    task.exporter = unreal.TextureExporterPNG()
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log('[LOGIN] export %s %s %dx%d' % (path, ok, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
unreal.log('[LOGIN] DONE')
