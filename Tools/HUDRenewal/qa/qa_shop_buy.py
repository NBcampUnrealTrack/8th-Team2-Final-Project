# -*- coding: utf-8 -*-
"""HUD QA: call HandleConfirmClicked on the open shop window 1x (the same handler its button calls)."""
import unreal

cls = unreal.load_class(None, '/Script/ProjectM.NUShopWindowWidget')
for w in unreal.ObjectIterator(cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        for _ in range(1):
            w.call_method('HandleConfirmClicked')
        log('%s x1 on %s' % ('HandleConfirmClicked', w.get_class().get_name()))
