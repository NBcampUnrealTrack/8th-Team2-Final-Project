# -*- coding: utf-8 -*-
"""Read-only: exports the baked field map textures to PNG for the HUD mock-ups (minimap art). Saves no assets.
Run:  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -nopause -nosplash -NullRHI"""
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'HUDRenewal', 'maps')
os.makedirs(OUT, exist_ok=True)
for path in ('/Game/Art/UI/Maps/T_Map_L_Libregrove', '/Game/Art/UI/Maps/T_Map_L_Fernbrook_Entrance'):
    texture = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = os.path.join(OUT, path.rsplit('/', 1)[1] + '.png')
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log('[HUD] export %s -> %s %s' % (path, task.filename, ok))
