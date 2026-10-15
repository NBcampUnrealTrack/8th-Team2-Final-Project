# -*- coding: utf-8 -*-
"""HUD mock-up backdrop helper (village): the offline save can start the player in Fernbrook (streamed at 100000,-100000),
so put the pawn on the village PlayerStart (the one near the world origin), then add two other players beside it.
ember.FakeRemote [id] [distance cm] [spirit] [race] [gender] [hair] [face] [job]"""
import unreal

pawn = unreal.GameplayStatics.get_player_pawn(GAME_WORLD, 0)
starts = [a for a in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.PlayerStart)
          if abs(a.get_actor_location().x) < 20000 and abs(a.get_actor_location().y) < 20000]
if pawn and starts:
    spot = starts[0].get_actor_location() + unreal.Vector(0, 0, 60)
    pawn.set_actor_location(spot, False, True)
    log('pawn moved to %s' % spot)
else:
    log('no village start (pawn %s, starts %d)' % (pawn, len(starts)))
for fake_id, distance, gender, hair, job in ((9301, 150, 1, 7000, 1005), (9302, 270, 0, 2, 1004)):
    unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'ember.FakeRemote %d %d 0 1 %d %d 0 %d' % (fake_id, distance, gender, hair, job), PC)
