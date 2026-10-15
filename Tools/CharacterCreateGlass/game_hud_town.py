# -*- coding: utf-8 -*-
"""HUD mock-up backdrop helper (field): two other players next to the local one, so name plates have someone to sit on.
ember.FakeRemote [id] [distance cm] [spirit] [race] [gender] [hair] [face] [job]"""
import unreal

for fake_id, distance, gender, hair, job in ((9201, 150, 1, 7000, 1005), (9202, 270, 0, 2, 1004)):
    unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'ember.FakeRemote %d %d 0 1 %d %d 0 %d' % (fake_id, distance, gender, hair, job), PC)
log('hud town remotes spawned')
