# -*- coding: utf-8 -*-
"""Imports the HUD art kit into /Game/Art/UI/HUD2026/Kit. Run headless through run.ps1 -Steps kit."""
import importlib
import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools/HUDRenewal'))
import hud_kit
importlib.reload(hud_kit)

hud_kit.import_kit()
hud_kit.log('DONE')
