# -*- coding: utf-8 -*-
"""Check helper for hair colour in the field (run on a field map): spawns three fake remote characters with the team's
debug command  ember.FakeRemote [id] [distance cm] [spirit] [race] [gender] [hair] [face] [job].
hair = hair part + colour * 1000 (NUAppearance::PackHairId): 2 = style 2 in its drawn colour, 7002 = style 2 blue, 3001 = style 1 red."""
import unreal

for fake_id, distance, hair in ((9001, 120, 2), (9002, 240, 7002), (9003, 360, 3001)):
    unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'ember.FakeRemote %d %d 0 1 0 %d 1 0' % (fake_id, distance, hair), PC)
log('fake remotes spawned')
