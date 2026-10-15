# 읽기 전용: L_Libregrove 의 시작 지점·NPC·광장 소품 위치를 덤프한다(프롤로그 마을 컷 구도용). 저장하지 않는다.
import json
import os
import unreal

OUT = r'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/Saved/Prologue_20261014/libregrove_actors.json'
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Libregrove/Level_V8/Maps/L_Libregrove')
actors = unreal.EditorLevelLibrary.get_all_level_actors()
rows = []
KEYS = ('PlayerStart', 'EntryPoint', 'NPC', 'Fountain', 'Well', 'Plaza', 'Shot', 'Camera', 'Portal', 'Market', 'Stall', 'Bridge', 'Sanctuary', 'Crystal', 'LocationTrigger', 'Naia', 'Spirit', 'Inn', 'Hall', 'Mill')
for a in actors:
    cls = a.get_class().get_name()
    label = a.get_actor_label()
    if not any(k.lower() in (cls + label).lower() for k in KEYS):
        continue
    loc = a.get_actor_location()
    rot = a.get_actor_rotation()
    row = {'label': label, 'class': cls, 'loc': [round(loc.x), round(loc.y), round(loc.z)], 'yaw': round(rot.yaw), 'hidden': a.is_hidden_ed()}
    for prop in ('entry_point_id', 'location_id', 'portal_id', 'npc_id', 'shot_id', 'tags'):
        try:
            row[prop] = str(a.get_editor_property(prop))
        except Exception:
            pass
    rows.append(row)
ws = world.get_world_settings()
info = {'count': len(actors), 'game_mode': str(ws.get_editor_property('default_game_mode')), 'rows': rows}
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(info, open(OUT, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
unreal.log('[Prologue] libregrove actors %d -> %s' % (len(rows), OUT))
