# PIE 도우미: 오프라인 더미 캐릭터를 만들고 골라 필드로 들어간다(새 캐릭터 첫 진입 재현)
import unreal
gi = unreal.GameplayStatics.get_game_instance(GAME_WORLD)
mgr = None
for obj in unreal.ObjectIterator(unreal.NUCharacterManager):
    if obj.get_outer() == gi:
        mgr = obj
log('manager %s' % mgr)
before = len(mgr.get_characters())
mgr.create_character_auto()
chars = mgr.get_characters()
log('characters %d -> %d' % (before, len(chars)))
if chars:
    c = chars[-1]
    log('select %s idx=%d level=%d' % (c.name, c.character_idx, c.level))
    mgr.select_character(c.character_idx)
