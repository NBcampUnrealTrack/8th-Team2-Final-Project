# PIE 도우미: 지금 시퀀스 시각·컷 상태와 폰 위치를 기록한다(캡처 직전 상태 확인용)
import json, os, unreal
row = {}
try:
    cues = unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.NUStorySequenceCue)
    if cues:
        cue = cues[0]
        player = cue.get_editor_property('sequence_player')
        row['seq_time'] = player.get_current_time().time.frame_number.value / 30.0 if player else None
        for p in ('slide_index', 'line_index', 'photo_opacity', 'backdrop_opacity', 'title_opacity', 'pawn_move_y'):
            row[p] = cue.get_editor_property(p)
    else:
        row['cue'] = 'none'
    pawn = PC.get_controlled_pawn()
    if pawn:
        l = pawn.get_actor_location(); row['pawn'] = [round(l.x), round(l.y), round(l.z)]
except Exception as e:
    row['error'] = repr(e)
path = os.environ.get('PQA_LOG', '')
if path:
    with open(path, 'a', encoding='utf-8') as f:
        f.write(json.dumps(row) + '\n')
log('stamp ' + json.dumps(row))
