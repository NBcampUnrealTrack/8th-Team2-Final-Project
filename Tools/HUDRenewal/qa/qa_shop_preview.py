# -*- coding: utf-8 -*-
"""HUD QA: close the real shop (Escape path through the UI manager is not reachable from here, so close its windows) and open
Minki's shop design page preview (nu.Shop.Preview, no transactions)."""
import unreal

shop_cls = unreal.load_class(None, '/Script/ProjectM.NUShopWindowWidget')
for w in unreal.ObjectIterator(shop_cls):
    if w.get_world() == GAME_WORLD and w.is_in_viewport():
        w.remove_from_parent()
        log('closed real shop %s' % w.get_name())
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Shop.Preview', PC)
log('preview opened')
