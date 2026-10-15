# -*- coding: utf-8 -*-
"""Proposal plates: stand by the quest NPC (qa_interact.py first) and hide, in the play world only, every NPC head marker
(the real F cue stays), so a mock marker can be drawn on a clean frame. Nothing is saved."""
import unreal

marker_class = unreal.load_class(None, '/Script/ProjectM.NUWorldMarkerComponent')
hidden = 0
for actor in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.Actor):
    for comp in actor.get_components_by_class(marker_class):
        comp.set_visibility(False, True)
        hidden += 1
log('hid %d markers' % hidden)
