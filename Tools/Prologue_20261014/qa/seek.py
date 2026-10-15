# PIE 도우미: 프롤로그 시퀀스를 SEEK_TIMES 의 다음 시각으로 옮기고 멈춘다(전환 순간을 정확히 찍기 위해).
# 시각 목록은 같은 폴더 seek_times.json, 몇 번째인지는 GAME_WORLD 에 붙인 카운터로 센다.
import json, os, unreal
here = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else r'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/Tools/Prologue_20261014/qa'
times = json.load(open(os.path.join(r'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/Tools/Prologue_20261014/qa', 'seek_times.json')))
idx = int(os.environ.get('PQA_SEEK_INDEX', '0'))
os.environ['PQA_SEEK_INDEX'] = str(idx + 1)
t = float(times[min(idx, len(times) - 1)])
cues = unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.NUStorySequenceCue)
player = cues[0].get_editor_property('sequence_player') if cues else None
if player:
    params = unreal.MovieSceneSequencePlaybackParams()
    params.set_editor_property('position_type', unreal.MovieScenePositionType.TIME)
    params.set_editor_property('time', t)
    params.set_editor_property('update_method', unreal.UpdatePositionMethod.JUMP)
    player.set_playback_position(params)
    player.pause()
    log('seek %.2f -> %.2f' % (t, player.get_current_time().time.frame_number.value / 30.0))
else:
    log('seek: no cue')
