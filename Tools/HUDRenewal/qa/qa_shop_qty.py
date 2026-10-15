# -*- coding: utf-8 -*-
"""HUD QA: call HandleQuantityPlus on the open shop window 2x (the same handler its button calls)."""
import unreal

cls = unreal.load_class(None, '/Script/ProjectM.NUShopWindowWidget')
for w in unreal.ObjectIterator(cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        for _ in range(2):
            w.call_method('HandleQuantityPlus')
        log('%s x2 on %s' % ('HandleQuantityPlus', w.get_class().get_name()))
