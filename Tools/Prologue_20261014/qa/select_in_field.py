# PIE 도우미: 필드 맵에서 더미 캐릭터를 만들어 '선택된 캐릭터'로 둔다(이동 없음).
# 캐릭터 선택 화면을 거친 새 캐릭터와 같은 조건을 만들어 자동 재생을 확인한다(오프라인 필드 입장은 서버가 없어 실패 화면이 뜨므로).
import unreal
gi = unreal.GameplayStatics.get_game_instance(GAME_WORLD)
mgr = [o for o in unreal.ObjectIterator(unreal.NUCharacterManager) if o.get_outer() == gi][0]
if not mgr.get_characters():
    mgr.create_character_auto()
c = mgr.get_characters()[-1]
mgr.select_character(c.character_idx)
log('field select %s idx=%d level=%d' % (c.name, c.character_idx, c.level))
