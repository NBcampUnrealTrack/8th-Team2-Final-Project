# -*- coding: utf-8 -*-
"""HUD QA (read-only): what the NPC head markers in play use (widget, size, scale, pivot, material, tag)."""
import unreal

marker_class = unreal.load_class(None, '/Script/ProjectM.NUWorldMarkerComponent')
n = 0
for actor in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.Actor):
    for comp in actor.get_components_by_class(marker_class):
        widget = comp.get_user_widget_object()
        mat = comp.get_material(0)
        log('HEAD %s vis=%s widget=%s draw=%s scale=%s pivot=%s blend=%s mat=%s tag=%s' % (
            actor.get_name(), comp.is_visible(), widget.get_class().get_name() if widget else None, comp.get_draw_size(),
            comp.get_editor_property('relative_scale3d'), comp.get_pivot(), comp.get_editor_property('blend_mode'),
            mat.get_name() if mat else None, comp.get_marker_tag()))
        n += 1
log('HEAD markers %d' % n)
