# -*- coding: utf-8 -*-
"""HUD QA: the next indicator's current vertical offset (it bobs while a finished line waits)."""
import unreal

for ind in unreal.ObjectIterator(unreal.HorizontalBox):
    if ind.get_name() != 'NextIndicator' or ind.get_world() != GAME_WORLD:
        continue
    t = ind.get_editor_property('render_transform').get_editor_property('translation')
    log('next indicator y %.2f visible %s (%s)' % (t.y, ind.get_visibility(), ind.get_path_name().rsplit('.', 3)[-3]))
