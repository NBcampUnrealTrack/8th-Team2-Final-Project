# -*- coding: utf-8 -*-
"""HUD QA: give the player a few items to show on the sell side, then stand next to the Libregrove shop NPC (BP_NPC_4002)."""
import unreal

for row, count in (('Potion_SmallHeal', 5), ('Potion_MediumHeal', 2), ('Potion_SmallSP', 3)):
    unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Inv.Add %s %d' % (row, count), PC)
pawn = PC.get_controlled_pawn()
npc_class = unreal.load_class(None, '/Script/ProjectM.NUNPCBase')
shop = None
for actor in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, npc_class):
    if 'BP_NPC_4002' in actor.get_class().get_name():
        shop = actor
        break
if shop:
    loc = shop.get_actor_location()
    pawn.set_actor_location(unreal.Vector(loc.x - 70.0, loc.y - 30.0, loc.z + 5.0), False, True)
    log('qa_shop_go.py next to %s at %s' % (shop.get_name(), loc))
else:
    log('qa_shop_go.py: no shop NPC')
