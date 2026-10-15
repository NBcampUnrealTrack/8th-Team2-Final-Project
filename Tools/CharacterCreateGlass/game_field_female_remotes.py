# -*- coding: utf-8 -*-
"""Check helper for hair colour on female job bodies (run on a field map). Female job bodies have their own hair layer
(Hair_Female_<Job>), found for any hair part id, so hair = colour * 1000 is enough: 0 = drawn colour, 7000 = blue, 3000 = red.
ember.FakeRemote [id] [distance cm] [spirit] [race] [gender] [hair] [face] [job]"""
import unreal

for fake_id, distance, hair, job in ((9101, 120, 0, 1005), (9102, 240, 7000, 1005), (9103, 360, 3000, 1005),
                                     (9104, 480, 7000, 1002), (9105, 600, 9000, 1004)):
    unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'ember.FakeRemote %d %d 0 1 1 %d 0 %d' % (fake_id, distance, hair, job), PC)
log('female fake remotes spawned')
