# 일회성 패치: build_prologue.py 를 타임라인 v2(크로스페이드·연출 트랙·마을 다중 카메라·곡)로 바꾼다.
import ast
import sys

p = sys.argv[1]
s = open(p, encoding='utf-8').read()

start = s.index("    slides = TL['slides']\n    slide_keys")
end = s.index("    EAL.save_loaded_asset(seq)\n    log('sequence saved')")
new = '''    slides = TL['slides']
    fade = TL.get('caption_fade', 0.6)
    slide_keys, line_keys, caption = [(0.0, 0)], [(0.0, -1)], [(0.0, 0.0)]
    panx, pany, walkx, walky = [], [], [(0.0, 0.0)], [(0.0, 0.0)]
    for i, s in enumerate(slides):
        if i > 0:
            slide_keys.append((s['switch'], i))   # 그림이 있는 장면끼리는 위젯이 크로스페이드한다
        for li, line in enumerate(s['lines']):
            # 문장 전체를 한 번에 띄우고 페이드로 넣고 뺀다(한 글자씩 쓰는 효과는 쓰지 않는다).
            line_keys.append((line['start'], li))
            line_keys.append((line['end'], -1))
            caption += [(line['start'], 0.0), (line['start'] + fade, 1.0), (line['end'] - fade, 1.0), (line['end'], 0.0)]
        for t, x, y in s.get('pan', []):
            panx.append((t, x))
            pany.append((t, y))
        if 'pawn_walk' in s:
            w = s['pawn_walk']
            walkx += [(w['start'], 0.0), (w['start'] + 0.3, w['x']), (w['end'] - 0.4, w['x']), (w['end'], 0.0)]
            walky += [(w['start'], 0.0), (w['start'] + 0.3, w['y']), (w['end'] - 0.4, w['y']), (w['end'], 0.0)]
    int_keys(property_track(cue_binding, unreal.MovieSceneIntegerTrack, 'SlideIndex'), sorted(slide_keys), 0)
    int_keys(property_track(cue_binding, unreal.MovieSceneIntegerTrack, 'LineIndex'), sorted(line_keys), -1)
    # 글자 수는 고정(전부 보임). 예전처럼 키를 두면 문장 사이에서 값이 줄며 글자가 거꾸로 지워졌다.
    float_keys(property_track(cue_binding, unreal.MovieSceneFloatTrack, 'RevealedCharacters'), [], 9999.0)
    float_keys(property_track(cue_binding, unreal.MovieSceneFloatTrack, 'CaptionOpacity'), sorted(caption), 0.0)
    float_keys(property_track(cue_binding, unreal.MovieSceneFloatTrack, 'PhotoPanX'), sorted(panx), 0.0)
    float_keys(property_track(cue_binding, unreal.MovieSceneFloatTrack, 'PhotoPanY'), sorted(pany), 0.0)
    float_keys(property_track(cue_binding, unreal.MovieSceneFloatTrack, 'PawnMoveX'), sorted(walkx), 0.0)
    float_keys(property_track(cue_binding, unreal.MovieSceneFloatTrack, 'PawnMoveY'), sorted(walky), 0.0)
    # 연출 트랙(빛·레터박스·입자·타이틀 등): 부드러운 곡선 보간. 위젯이 0~1 로 자른다.
    defaults = {'PhotoOpacity': 0.0, 'BackdropOpacity': 1.0, 'FlashOpacity': 0.0, 'LetterboxAmount': 0.0,
                'MoteIntensity': 0.0, 'TitleOpacity': 0.0, 'TitleScale': 1.0}
    for prop, keys in TL['tracks'].items():
        float_keys(property_track(cue_binding, unreal.MovieSceneFloatTrack, prop), sorted((float(t), float(v)) for t, v in keys),
                   defaults.get(prop, 0.0), unreal.MovieSceneKeyInterpolation.AUTO)
    log('cue tracks written')

    # ---- 마을 컷: 곡 프레이즈마다 카메라 하나 + 사람·나이아 스프라이트
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cut_track = add_root_track(seq, unreal.MovieSceneCameraCutTrack)
    for shot in SHOT['shots']:
        cam_actor = actor_sub.spawn_actor_from_class(unreal.CineCameraActor, unreal.Vector(0, 0, 0))
        cam_comp = cam_actor.get_cine_camera_component()
        focus = cam_comp.get_editor_property('focus_settings')
        focus.set_editor_property('focus_method', unreal.CameraFocusMethod.DISABLE)
        cam_comp.set_editor_property('focus_settings', focus)
        cam_comp.set_editor_property('current_focal_length', shot.get('focal_length', SHOT['focal_length']))
        cam_binding = seq.add_spawnable_from_instance(cam_actor)
        cam_binding.set_display_name(shot['name'])
        xs = infinite_section(cam_binding.add_track(unreal.MovieScene3DTransformTrack))
        channels = xs.get_all_channels()   # Location XYZ, Rotation Roll/Pitch/Yaw, Scale XYZ
        for key in shot['keys']:
            loc, rot = key['loc'], key['rot']   # rot = [pitch, yaw, roll]
            values = [loc[0], loc[1], loc[2], rot[2], rot[0], rot[1], 1, 1, 1]
            for ch, v in zip(channels, values):
                ch.add_key(frame(key['t']), float(v), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, unreal.MovieSceneKeyInterpolation.AUTO)
        cut = cut_track.add_section()
        cut.set_range_seconds(shot['start'], shot['end'])
        cut.set_camera_binding_id(binding_id(seq, cam_binding))
        log('camera %s %.2f-%.2f' % (shot['name'], shot['start'], shot['end']))

    for sprite in SHOT['sprites']:
        actor = actor_sub.spawn_actor_from_class(unreal.PaperFlipbookActor, unreal.Vector(*sprite['loc']), unreal.Rotator(pitch=0.0, yaw=sprite.get('yaw', 180.0), roll=0.0))
        comp = actor.get_editor_property('render_component')
        comp.set_flipbook(unreal.load_asset(sprite['flipbook']))
        if sprite.get('material'):
            comp.set_material(0, unreal.load_asset(sprite['material']))
        comp.set_editor_property('cast_shadow', True)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        sc = sprite.get('scale', 1.0)
        actor.set_actor_scale3d(unreal.Vector(sc, sc, sc))
        b = seq.add_spawnable_from_instance(actor)
        b.set_display_name(sprite['name'])
    log('%d village sprites' % len(SHOT['sprites']))

    # ---- 음악: 처음부터 곡을 그대로 깔고 끝에서만 줄인다.
    music = TL.get('music', {})
    audio = add_root_track(seq, unreal.MovieSceneAudioTrack)
    sec = audio.add_section()
    sec.set_range_seconds(0.0, TL['length'])
    sec.set_sound(make_bgm_cue(music.get('volume', 0.8)))
    try:
        vol = [c for c in sec.get_all_channels() if 'Volume' in c.get_name()][0]
        vol.set_default(1.0)
        f0, f1 = music.get('fade_out', [TL['length'] - 2.0, TL['length']])
        for t, v in ((0.0, 1.0), (f0, 1.0), (f1, 0.0)):
            vol.add_key(frame(t), v, 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, unreal.MovieSceneKeyInterpolation.LINEAR)
    except Exception as error:
        log('audio volume keys skipped %r' % error)

'''
s = s[:start] + new + s[end:]


def rep(a, b):
    global s
    assert s.count(a) == 1, a[:60]
    s = s.replace(a, b)


rep('''def float_keys(channel, keys, default):
    """keys: [(seconds, value), ...] 선형 보간."""
    channel.set_default(default)
    for t, v in keys:
        channel.add_key(frame(t), float(v), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, unreal.MovieSceneKeyInterpolation.LINEAR)''',
'''def float_keys(channel, keys, default, interp=None):
    """keys: [(seconds, value), ...] 기본 선형 보간."""
    channel.set_default(default)
    interp = interp or unreal.MovieSceneKeyInterpolation.LINEAR
    for t, v in keys:
        channel.add_key(frame(t), float(v), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, interp)''')
rep("def make_bgm_cue():", "def make_bgm_cue(volume=0.8):")
rep("    cue.set_editor_property('volume_multiplier', 0.8)", "    cue.set_editor_property('volume_multiplier', volume)")
rep("    da.set_editor_property('subtitle_bottom_fraction', 0.07)",
"""    da.set_editor_property('subtitle_bottom_fraction', 0.035)   # 레터박스(11%) 띠 안에 자막이 앉는다
    da.set_editor_property('crossfade_photos', True)
    da.set_editor_property('photo_transition_seconds', TL.get('crossfade', 1.0))
    da.set_editor_property('flash_color', unreal.LinearColor(1.0, 0.92, 0.74, 1.0))
    da.set_editor_property('letterbox_fraction', 0.11)""")
ast.parse(s)
open(p, 'w', encoding='utf-8').write(s)
print('ok')
