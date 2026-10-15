# -*- coding: utf-8 -*-
"""HUD QA: close the starter Naia window and show a quest offer (the quest component's offer delegate, same call an NPC's quest
option makes). Quest 10004 has experience, gold and an item reward; its giver is NPC 4005."""
import os
import unreal

cls = unreal.load_class(None, '/Script/ProjectM.NUStarterNaiaSelectionWidget')
for w in unreal.ObjectIterator(cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        w.request_close()
quest = os.environ.get('QA_OFFER_QUEST', '10004')
comp_cls = unreal.load_class(None, '/Script/ProjectM.NUQuestComponent')
for comp in unreal.ObjectIterator(comp_cls):
    if comp.get_world() == GAME_WORLD:
        comp.on_quest_offer_requested.broadcast(quest)
        log('qa_sel_offer.py offer %s' % quest)
        break
