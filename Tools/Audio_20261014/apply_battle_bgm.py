# -*- coding: utf-8 -*-
"""일반 전투 BGM 을 루프 파일로 바꾼다.

1. ArtSource/Audio/Battle_20261014/BGM_Battle_Normal_Loop.wav → /Game/Audio/BGM/BGM_Battle_NiaUnbound (반복 재생, SC_BGM 소속)
2. DA_BGMConfig 의 DefaultBattle 항목이 이 곡을 가리키게 한다(음량 0.3·페이드는 기존 값 유지).
PRE_ONLY=1 이면 바꾸지 않고 현재 상태만 출력한다.
"""
import json
import os
import unreal

ROOT = 'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM'
# 환경 변수로 다른 곡(보스전 등)에도 쓴다: BGM_WAV, BGM_NAME, BGM_KEY
WAV = ROOT + '/ArtSource/Audio/Battle_20261014/' + os.environ.get('BGM_WAV', 'BGM_Battle_Normal_Loop.wav')
DEST_DIR, DEST_NAME = '/Game/Audio/BGM', os.environ.get('BGM_NAME', 'BGM_Battle_NiaUnbound')
KEY = os.environ.get('BGM_KEY', 'DefaultBattle')
CONFIG = '/Game/Audio/Data/DA_BGMConfig'
BGM_CLASS = '/Game/Audio/SC_BGM'
OUT = ROOT + '/Saved/Audio_20261014/apply_%s.json' % os.environ.get('BGM_KEY', 'DefaultBattle')
PRE_ONLY = os.environ.get('PRE_ONLY') == '1'
EAL = unreal.EditorAssetLibrary
report = {}


def dump_entries(cfg):
    rows = {}
    for key, entry in cfg.get_editor_property('entries').items():
        snd = entry.get_editor_property('sound')
        rows[str(key)] = {'sound': snd.get_path_name() if snd else None,
                          'volume': entry.get_editor_property('volume'),
                          'fade_in': entry.get_editor_property('fade_in_seconds'),
                          'fade_out': entry.get_editor_property('fade_out_seconds'),
                          'looping': bool(snd.get_editor_property('looping')) if isinstance(snd, unreal.SoundWave) else None}
    return rows


cfg = unreal.load_asset(CONFIG)
report['before'] = dump_entries(cfg)

if not PRE_ONLY:
    task = unreal.AssetImportTask()
    task.filename = WAV
    task.destination_path = DEST_DIR
    task.destination_name = DEST_NAME
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    wave = unreal.load_asset(DEST_DIR + '/' + DEST_NAME)
    wave.set_editor_property('looping', True)
    wave.set_editor_property('sound_class_object', unreal.load_asset(BGM_CLASS))
    report['wave_saved'] = bool(EAL.save_loaded_asset(wave, False))
    report['wave'] = {'path': wave.get_path_name(), 'duration': wave.get_editor_property('duration'), 'looping': wave.get_editor_property('looping')}

    entries = cfg.get_editor_property('entries')
    key = unreal.Name(KEY)
    if key in entries:
        entry = entries.get(key)
    else:
        # 새 항목은 다른 BGM 과 같은 음량 0.3, 페이드 1초
        entry = unreal.NUBGMEntry()
        entry.set_editor_property('volume', 0.3)
        entry.set_editor_property('fade_in_seconds', 1.0)
        entry.set_editor_property('fade_out_seconds', 1.0)
    entry.set_editor_property('sound', wave)
    # 음량·페이드는 팀이 맞춘 기존 값(0.3)을 그대로 둔다. 파일은 원곡 수준(-16 LUFS)이라 실효 약 -26.5 LUFS(마을보다 약 3dB 낮음).
    entries[key] = entry
    cfg.modify()
    cfg.set_editor_property('entries', entries)
    # 맵 속성만 바꾸면 패키지가 '변경됨'으로 표시되지 않아 기본 저장(변경된 것만)이 건너뛴다 → 강제 저장
    ok = EAL.save_loaded_asset(cfg, False)
    report['config_saved'] = bool(ok)
    report['after'] = dump_entries(unreal.load_asset(CONFIG))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(report, open(OUT, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
unreal.log('[BattleBGM] ' + json.dumps(report, ensure_ascii=False)[:2000])
