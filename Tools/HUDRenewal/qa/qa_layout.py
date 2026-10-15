# -*- coding: utf-8 -*-
"""HUD QA: project every formation axis for 1..4 members (head +120 / chest +80 / feet) and every avatar to
screen pixels, so tag overlap for a full 4-player party can be checked on the screenshot. Writes JSON."""
import json
import unreal

OUT = r'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/Saved/HUDRenewal/battle_layout.json'


def proj(loc):
    r = unreal.GameplayStatics.project_world_to_screen(PC, loc, False)
    if isinstance(r, tuple):
        r = [v for v in r if isinstance(v, unreal.Vector2D)][0] if any(isinstance(v, unreal.Vector2D) for v in r) else None
    return None if r is None else [round(r.x, 1), round(r.y, 1)]


def run():
    data = {'viewport': None, 'axes': [], 'avatars': []}
    try:
        vs = unreal.WidgetLayoutLibrary.get_viewport_size(GAME_WORLD)
        data['viewport'] = [vs.x, vs.y, unreal.WidgetLayoutLibrary.get_viewport_scale(GAME_WORLD)]
    except Exception as e:
        data['viewport_error'] = str(e)
    for axis in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.NUBattleFormationAxis):
        loc = axis.get_actor_location(); fwd = axis.get_actor_forward_vector()
        try:
            row = str(axis.get_editor_property('row')); spacing = axis.get_editor_property('slot_spacing')
        except Exception as e:
            row = 'err ' + str(e); spacing = 150.0
        entry = {'name': axis.get_name(), 'row': row, 'spacing': spacing, 'counts': {}}
        for n in (1, 2, 3, 4):
            slots = []
            for i in range(n):
                c = i - (n - 1) / 2.0
                p = loc + fwd * (c * spacing)
                slots.append({'feet': proj(p), 'chest': proj(p + unreal.Vector(0, 0, 80)), 'head': proj(p + unreal.Vector(0, 0, 120))})
            entry['counts'][str(n)] = slots
        data['axes'].append(entry)
    for av in unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.NUBattleAvatar):
        p = av.get_actor_location()
        data['avatars'].append({'name': av.get_name(), 'class': av.get_class().get_name(), 'feet': proj(p),
                                'head': proj(p + unreal.Vector(0, 0, 120))})
    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
    log('layout axes=%d avatars=%d' % (len(data['axes']), len(data['avatars'])))


run()
