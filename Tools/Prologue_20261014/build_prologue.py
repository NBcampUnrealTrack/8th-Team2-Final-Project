# -*- coding: utf-8 -*-
"""게임 시작 프롤로그 에셋을 timeline.json 에서 다시 만든다(여러 번 돌려도 같은 결과).

만드는 것 (/Game/NUA/Prologue)
  Textures/T_Prologue_0N_*            그림책 그림. 최종 그림이 없으면 *_Placeholder(임시 카드)를 쓴다.
  DA_Prologue_Spirit                  UNUStorySlideshowData (자막·그림·타이틀·길게 눌러 스킵)
  LS_Prologue_Spirit                  레벨 시퀀스. zxcv1 님의 LS_PrologueTest 를 복제해 Cue 바인딩을 물려받고 트랙을 새로 쓴다.
  SCue_Prologue_BGM                   프롤로그 음악. SC_Master 소속이라 BGM 끄기 믹스(SMix_StoryMuteBGM)에 같이 꺼지지 않는다.

실행: UnrealEditor-Cmd.exe ProjectM.uproject -run=pythonscript -script=<this> -unattended -nosplash
팀 에셋(LS_PrologueTest, T_TitleText, SMix_StoryMuteBGM, 원본 BGM)은 읽기만 한다.
"""
import json
import os
import unreal

ROOT = 'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM'
TOOLS = ROOT + '/Tools/Prologue_20261014'
ART = ROOT + '/ArtSource/Prologue_20261014'
PKG = '/Game/NUA/Prologue'
TEX = PKG + '/Textures'
DA_PATH = PKG + '/DA_Prologue_Spirit'
LS_PATH = PKG + '/LS_Prologue_Spirit'
TEMPLATE_LS = '/Game/Developers/zxcv1/Cinematics/Prologue/LS_PrologueTest'
TITLE_TEX = '/Game/Developers/MinkiCho/Art/Login/T_TitleText'
SOUND_MIX = '/Game/Developers/zxcv1/Cinematics/SMix_StoryMuteBGM'
FONT = '/Game/Art/Font/MaruBuri-SemiBold_Font'
MASTER_CLASS = '/Game/Audio/SC_Master'
NAIA_MAT = '/Game/Art/Materials/M_NUCharacterSprite'
# BGM: Suno 곡을 Audio/ 에 임포트하면 그걸, 없으면 타이틀 곡을 임시로 쓴다.
BGM_FINAL = PKG + '/Audio/BGM_Prologue_Spirit'
BGM_TEMP = '/Game/Audio/BGM/BGM_Title_Dawn_Sacred_Breath'
FPS = 30
TL = json.load(open(TOOLS + '/timeline.json', encoding='utf-8'))
SHOT = json.load(open(TOOLS + '/village_shot.json', encoding='utf-8'))
REPORT = {'steps': []}


def log(msg):
    unreal.log('[Prologue] ' + str(msg))
    REPORT['steps'].append(str(msg))


def frame(seconds):
    return unreal.FrameNumber(int(round(seconds * FPS)))


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary


# ---------------------------------------------------------------- textures
def import_texture(png, dest_folder, name):
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = dest_folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    asset_tools.import_asset_tasks([task])
    tex = unreal.load_asset(dest_folder + '/' + name)
    if tex is None:
        raise RuntimeError('import failed ' + png)
    # 도트 그림: 최근접 필터, 밉 없음, UI 그룹, 무압축에 가까운 UI 압축
    tex.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('never_stream', True)
    tex.set_editor_property('srgb', True)
    EAL.save_loaded_asset(tex)
    return tex


def slide_texture(name):
    """최종 그림(ArtSource/.../final/<name>.png)이 있으면 그것, 없으면 임시 카드."""
    final_png = ART + '/final/' + name + '.png'
    if os.path.exists(final_png):
        return import_texture(final_png, TEX, name), 'final'
    ph = name + '_Placeholder'
    if not EAL.does_asset_exist(TEX + '/' + ph):
        import_texture(ART + '/placeholder/' + ph + '.png', TEX, ph)
    return unreal.load_asset(TEX + '/' + ph), 'placeholder'


# ---------------------------------------------------------------- audio
def import_bgm(wav=None, dest=None):
    """ArtSource/Prologue_20261014/audio/BGM_Prologue_Spirit.wav(Suno 곡)가 있으면 SoundWave 로 임포트한다."""
    wav = wav or ART + '/audio/BGM_Prologue_Spirit.wav'
    dest = dest or BGM_FINAL
    if not os.path.exists(wav):
        return False
    folder, name = dest.rsplit('/', 1)
    task = unreal.AssetImportTask()
    task.filename = wav
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    asset_tools.import_asset_tasks([task])
    ok = EAL.does_asset_exist(dest)
    log('bgm import %s -> %s %s' % (wav, dest, 'ok' if ok else 'FAILED'))
    return ok


def make_bgm_cue(volume=0.8):
    """Suno 곡이 들어오면 _Final 큐, 아니면 타이틀 곡을 감싼 _Temp 큐. 둘 다 SC_Master 소속."""
    import_bgm()
    final = EAL.does_asset_exist(BGM_FINAL)
    wave_path = BGM_FINAL if final else BGM_TEMP
    name = 'SCue_Prologue_BGM_' + ('Final' if final else 'Temp')
    path = PKG + '/' + name
    cue = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if cue is None:
        factory = unreal.SoundCueFactoryNew()
        factory.set_editor_property('initial_sound_waves', [unreal.load_asset(wave_path)])
        cue = asset_tools.create_asset(name, PKG, unreal.SoundCue, factory)
    cue.set_editor_property('sound_class_object', unreal.load_asset(MASTER_CLASS))
    cue.set_editor_property('volume_multiplier', volume)
    EAL.save_loaded_asset(cue)
    log('bgm cue %s <- %s' % (path, wave_path))
    REPORT['bgm'] = wave_path
    return cue


# ---------------------------------------------------------------- data asset
def build_data(textures):
    da = unreal.load_asset(DA_PATH) if EAL.does_asset_exist(DA_PATH) else None
    if da is None:
        da = asset_tools.create_asset('DA_Prologue_Spirit', PKG, unreal.NUStorySlideshowData, unreal.DataAssetFactory())
    slides = []
    for i, s in enumerate(TL['slides']):
        slide = unreal.NUStorySlide()
        slide.set_editor_property('image', textures[i])
        slide.set_editor_property('lines', [unreal.Text(l['text']) for l in s['lines']])
        slides.append(slide)
    da.set_editor_property('slides', slides)
    da.set_editor_property('sequence', unreal.load_asset(LS_PATH))
    da.set_editor_property('sound_mix', unreal.load_asset(SOUND_MIX))
    da.set_editor_property('use_subtitles', True)
    font = unreal.SlateFontInfo()
    font.set_editor_property('font_object', unreal.load_asset(FONT))
    font.set_editor_property('size', 26)
    # 밝은 게임 화면 위에서도 읽히게 얇은 외곽선
    outline = font.get_editor_property('outline_settings')
    outline.set_editor_property('outline_size', 2)
    outline.set_editor_property('outline_color', unreal.LinearColor(0.02, 0.03, 0.05, 0.75))
    font.set_editor_property('outline_settings', outline)
    da.set_editor_property('subtitle_font', font)
    da.set_editor_property('allow_skip', True)
    da.set_editor_property('hold_to_skip', True)
    da.set_editor_property('hold_to_skip_seconds', 1.0)
    da.set_editor_property('pixel_perfect_photos', True)
    da.set_editor_property('title_image', unreal.load_asset(TITLE_TEX))
    da.set_editor_property('title_width_fraction', 0.40)
    da.set_editor_property('title_center_height', 0.30)
    da.set_editor_property('subtitle_bottom_fraction', 0.035)   # 레터박스(11%) 띠 안에 자막이 앉는다
    da.set_editor_property('crossfade_photos', True)
    da.set_editor_property('photo_transition_seconds', TL.get('crossfade', 1.0))
    da.set_editor_property('flash_color', unreal.LinearColor(1.0, 0.92, 0.74, 1.0))
    da.set_editor_property('letterbox_fraction', 0.11)
    da.set_editor_property('seconds_per_character', 0.001)   # 글자 단위 표시는 쓰지 않는다(CaptionOpacity 페이드)
    EAL.save_loaded_asset(da)
    log('data asset saved, %d slides' % len(slides))


# ---------------------------------------------------------------- sequence helpers
def add_root_track(seq, cls):
    for name in ('add_track', 'add_master_track'):
        fn = getattr(seq, name, None)
        if fn:
            return fn(cls)
    raise RuntimeError('no add_track')


def root_tracks(seq):
    for name in ('get_tracks', 'get_master_tracks'):
        fn = getattr(seq, name, None)
        if fn:
            return list(fn())
    return []


def remove_root_track(seq, track):
    for name in ('remove_track', 'remove_master_track'):
        fn = getattr(seq, name, None)
        if fn:
            return fn(track)


def binding_id(seq, binding):
    for attempt in (lambda: seq.get_portable_binding_id(seq, binding),
                    lambda: unreal.MovieSceneSequenceExtensions.get_portable_binding_id(seq, seq, binding),
                    lambda: seq.get_binding_id(binding),
                    lambda: binding.get_binding_id()):
        try:
            return attempt()
        except Exception:
            continue
    raise RuntimeError('binding id unavailable')


def infinite_section(track):
    section = track.add_section()
    section.set_start_frame_bounded(False)
    section.set_end_frame_bounded(False)
    return section


def property_track(binding, cls, prop):
    track = binding.add_track(cls)
    track.set_property_name_and_path(prop, prop)
    return infinite_section(track).get_all_channels()[0]


def float_keys(channel, keys, default, interp=None):
    """keys: [(seconds, value), ...] 기본 선형 보간."""
    channel.set_default(default)
    interp = interp or unreal.MovieSceneKeyInterpolation.LINEAR
    for t, v in keys:
        channel.add_key(frame(t), float(v), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, interp)


def int_keys(channel, keys, default):
    channel.set_default(default)
    for t, v in keys:
        channel.add_key(frame(t), int(v), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE)


def ramp(t0, t1, a, b):
    return [(t0, a), (t1, b)]


# ---------------------------------------------------------------- sequence
def build_sequence(temp_world_ready):
    if not EAL.does_asset_exist(LS_PATH):
        seq = asset_tools.duplicate_asset('LS_Prologue_Spirit', PKG, unreal.load_asset(TEMPLATE_LS))
        log('sequence duplicated from template')
    seq = unreal.load_asset(LS_PATH)
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    length = TL['length']
    seq.set_playback_start_seconds(0.0)
    seq.set_playback_end_seconds(length)

    cue_binding = None
    for b in seq.get_bindings():
        cls = None
        try:
            cls = b.get_possessed_object_class()
        except Exception:
            pass
        if cls is not None and cls.get_name() == 'NUStorySequenceCue':
            cue_binding = b
        else:
            b.remove()
    for t in root_tracks(seq):
        remove_root_track(seq, t)
    if cue_binding is None:
        raise RuntimeError('cue binding missing in template')
    for t in list(cue_binding.get_tracks()):
        cue_binding.remove_track(t)

    slides = TL['slides']
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
    cam_bindings = {}
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
        cam_bindings[shot['name']] = cam_binding

    # 데우기 컷: 시작 검은 화면 아래에서 마을 카메라로 잠깐씩 찍는다(키 이전 시각이라 각 컷의 첫 자세).
    for warm in SHOT.get('warmup', []):
        cut = cut_track.add_section()
        cut.set_range_seconds(warm['start'], warm['end'])
        cut.set_camera_binding_id(binding_id(seq, cam_bindings[warm['shot']]))
        log('warmup %s %.2f-%.2f' % (warm['shot'], warm['start'], warm['end']))

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

    EAL.save_loaded_asset(seq)
    log('sequence saved')


def main():
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)   # 스폰 템플릿용 임시 월드(저장하지 않는다)
    if not EAL.does_directory_exist(TEX):
        EAL.make_directory(TEX)
    textures, kinds = [], []
    for s in TL['slides']:
        if s.get('image'):
            tex, kind = slide_texture(s['image'])
        else:
            tex, kind = None, 'none'
        textures.append(tex)
        kinds.append(kind)
    REPORT['textures'] = kinds
    build_sequence(True)
    build_data(textures)
    out = ROOT + '/Saved/Prologue_20261014/build_report.json'
    json.dump(REPORT, open(out, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    log('report ' + out)


main()
