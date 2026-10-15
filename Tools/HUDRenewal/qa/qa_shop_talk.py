# -*- coding: utf-8 -*-
"""HUD QA: talk to the shop NPC (same call as the F key)."""
import unreal

pawn = PC.get_controlled_pawn()
npc_class = unreal.load_class(None, '/Script/ProjectM.NUNPCBase')
for actor in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, npc_class):
    if 'BP_NPC_4002' in actor.get_class().get_name():
        log('talk to %s: %s' % (actor.get_name(), actor.call_method('Interact', (pawn,))))
        break
entry_cls = unreal.load_class(None, '/Script/ProjectM.NUInteractionOptionEntryWidget')
for entry in unreal.ObjectIterator(entry_cls):
    if entry.get_world() == GAME_WORLD:
        log('row: %s' % entry.get_option().get_editor_property('display_text'))
