# -*- coding: utf-8 -*-
"""Check helper (field map): female NOVICE bodies now have separate hair/face layers. Spawns fake remotes:
novice hair 0 (drawn colour), 7000 (blue), 3000 (red), and a female druid with 7000 to confirm job hair rows still win.
ember.FakeRemote [id] [distance cm] [spirit] [race] [gender] [hair] [face] [job]"""
import unreal
exec(open(r'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/Tools/CharacterCreateGlass/game_hud_village.py', encoding='utf-8').read())
for fake_id, distance, hair, job in ((9111, 140, 0, 0), (9112, 260, 7000, 0), (9113, 380, 3000, 0), (9114, 500, 7000, 1005)):
    unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'ember.FakeRemote %d %d 0 1 1 %d 0 %d' % (fake_id, distance, hair, job), PC)
log('female novice remotes spawned')
