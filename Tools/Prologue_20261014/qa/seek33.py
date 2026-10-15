# PIE 도우미: 프롤로그를 33초(레터박스, 걸어 나오는 장면)로 옮기고 멈춘다.
import unreal
cues = unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.NUStorySequenceCue)
player = cues[0].get_editor_property('sequence_player') if cues else None
if player:
    params = unreal.MovieSceneSequencePlaybackParams()
    params.set_editor_property('position_type', unreal.MovieScenePositionType.TIME)
    params.set_editor_property('time', 33.0)
    params.set_editor_property('update_method', unreal.UpdatePositionMethod.JUMP)
    player.set_playback_position(params)
    player.pause()
    log('seek 33')
