# PIE 도우미: 리베르그로브 남문 포탈(16007)로 걸어 들어가 펀브룩 입구(레벨 인스턴스, 약 100000,-100000)로 넘어간다.
# 사용자가 본 상황(내 캐릭터가 펀브룩에 있고 마을은 1km 밖) 재현용. 요구 조건 우회 없이 포탈 겹침으로만 넘어간다.
import time
import unreal

pawn = PC.get_controlled_pawn()
pawn.character_movement.stop_movement_immediately()
pawn.set_actor_location(unreal.Vector(0, -3040, 53), False, True)


class _ToFernbrook:
    def __init__(self):
        self.start = time.monotonic()
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def stop(self, why):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log('to_fernbrook: ' + why)

    def tick(self, dt):
        try:
            p = PC.get_controlled_pawn()
            if p is None:
                return
            pos = p.get_actor_location()
            if pos.x > 50000:
                p.character_movement.stop_movement_immediately()
                self.stop('arrived %d,%d,%d after %.1fs' % (pos.x, pos.y, pos.z, time.monotonic() - self.start))
                return
            if time.monotonic() - self.start > 30:
                self.stop('no transition in 30s at %d,%d' % (pos.x, pos.y))
                return
            delta = unreal.Vector(-pos.x, -3350 - pos.y, 0)
            dist = delta.length()
            if dist > 15:
                p.add_movement_input(delta / dist, 1.0, False)
        except Exception as error:
            self.stop('error %r' % error)


unreal._pqa_to_fern = _ToFernbrook()
log('to_fernbrook queued')
