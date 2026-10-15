# -*- coding: utf-8 -*-
"""HUD QA: load the painted battle set package before the fight, so the stage streams in under the encounter's
5 s wait (a cold load in a busy editor misses it and the battle starts with no set and the default formation)."""
import unreal
for p in ('/Game/Maps/BattleArena/L_Battle_Grassland03',):
    try:
        unreal.load_package(p)
        log('preloaded ' + p)
    except Exception as e:
        log('preload failed %s %s' % (p, e))
