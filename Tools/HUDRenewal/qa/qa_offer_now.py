# -*- coding: utf-8 -*-
"""HUD QA: close the starter Naia window and ask the quest widget to show a quest offer (the quest component's offer delegate,
the same call an NPC's quest option makes). Quest 10004 has experience, gold and an item reward. Runs inside pie_capture.py."""
import os
import unreal

QUEST = os.environ.get('QA_OFFER_QUEST', '10004')
naia_cls = unreal.load_class(None, '/Script/ProjectM.NUStarterNaiaSelectionWidget')
for w in unreal.ObjectIterator(naia_cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        w.remove_from_parent()
comp_cls = unreal.load_class(None, '/Script/ProjectM.NUQuestComponent')
done = False
for comp in unreal.ObjectIterator(comp_cls):
    if comp.get_world() == GAME_WORLD:
        comp.on_quest_offer_requested.broadcast(QUEST)
        done = True
        log('qa_offer_now.py offer %s through %s' % (QUEST, comp.get_name()))
        break
if not done:
    log('qa_offer_now.py: no quest component')
