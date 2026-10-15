# PIE 도우미: 프롤로그를 재생하고(PQA_TRACE_PLAY=1, 기본) 끝날 때까지 매 프레임 시간·시퀀스 시각·카메라 대상을 기록한다.
# 결과: PQA_TRACE(jsonl) — 한 줄 = 한 프레임 {t, dt, seq, vt, cam}. 컷 전환 뒤 프레임이 끊기는지(긴 dt) 보는 용도.
import json
import os
import time
import unreal

OUT = os.environ.get('PQA_TRACE', '')
if os.environ.get('PQA_TRACE_PLAY', '1') == '1':
    unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Prologue.Play', PC)


class _Trace:
    def __init__(self):
        self.start = time.monotonic()
        self.last = self.start
        self.seen_cue = False
        self.rows = []
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def stop(self, why):
        unreal.unregister_slate_post_tick_callback(self.handle)
        if OUT:
            with open(OUT, 'w', encoding='utf-8') as f:
                for row in self.rows:
                    f.write(json.dumps(row) + '\n')
        log('trace: %s, %d frames' % (why, len(self.rows)))

    def tick(self, dt):
        try:
            now = time.monotonic()
            row = {'t': round(now - self.start, 4), 'dt': round(now - self.last, 4)}
            self.last = now
            cues = unreal.GameplayStatics.get_all_actors_of_class(GAME_WORLD, unreal.NUStorySequenceCue)
            if cues:
                self.seen_cue = True
                player = cues[0].get_editor_property('sequence_player')
                if player:
                    row['seq'] = round(player.get_current_time().time.frame_number.value / 30.0, 3)
            elif self.seen_cue:
                self.rows.append(row)
                self.stop('cue gone')
                return
            target = PC.get_view_target()
            row['vt'] = target.get_name() if target else None
            cam = PC.player_camera_manager
            if cam:
                loc = cam.get_camera_location()
                row['cam'] = [round(loc.x), round(loc.y), round(loc.z)]
            self.rows.append(row)
            if now - self.start > 120:
                self.stop('timeout')
        except Exception as error:
            self.stop('error %r' % error)


unreal._pqa_trace = _Trace()
log('trace started')
