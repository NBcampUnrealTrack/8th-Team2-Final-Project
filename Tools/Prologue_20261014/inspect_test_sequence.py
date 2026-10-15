# 읽기 전용: zxcv1 님의 프롤로그 시험 시퀀스 구조를 덤프한다(새 시퀀스 구성의 기준).
# 실행: UnrealEditor-Cmd.exe ProjectM.uproject -run=pythonscript -script=<this> -unattended -nosplash
import json
import unreal

OUT = r'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/Saved/Prologue_20261014/inspect_test_sequence.json'
seq = unreal.load_asset('/Game/Developers/zxcv1/Cinematics/Prologue/LS_PrologueTest')
da = unreal.load_asset('/Game/Developers/zxcv1/Cinematics/Prologue/DA_Prologue')
info = {'display_rate': str(seq.get_display_rate()), 
        'playback': [seq.get_playback_start_seconds(), seq.get_playback_end_seconds()], 'bindings': [], 'root_tracks': []}
for b in seq.get_bindings():
    try:
        cls = b.get_possessed_object_class()
        cls = cls.get_name() if cls else None
    except Exception as e:
        cls = repr(e)
    row = {'name': str(b.get_name()), 'class': cls, 'tracks': []}
    for t in b.get_tracks():
        tr = {'class': t.get_class().get_name(), 'name': str(t.get_display_name())}
        try:
            tr['property'] = str(t.get_property_name())
        except Exception:
            pass
        secs = []
        for s in t.get_sections():
            srow = {'range': [s.get_start_frame_seconds() if s.has_start_frame() else None, s.get_end_frame_seconds() if s.has_end_frame() else None], 'channels': []}
            for c in s.get_all_channels():
                keys = []
                try:
                    for k in c.get_keys():
                        keys.append([k.get_time().frame_number.value, str(k.get_value())])
                except Exception as e:
                    keys.append(repr(e))
                srow['channels'].append({'name': str(c.get_name()), 'keys': keys[:60]})
            secs.append(srow)
        tr['sections'] = secs
        row['tracks'].append(tr)
    info['bindings'].append(row)
for t in seq.get_tracks():
    info['root_tracks'].append({'class': t.get_class().get_name(), 'sections': len(t.get_sections())})
props = {}
for p in ['sequence', 'sound_mix', 'use_subtitles', 'allow_skip', 'seconds_per_line', 'seconds_per_character', 'photo_transition_seconds']:
    try:
        props[p] = str(da.get_editor_property(p))
    except Exception as e:
        props[p] = repr(e)
try:
    f = da.get_editor_property('subtitle_font')
    props['subtitle_font'] = str(f.get_editor_property('font_object')) + ' size ' + str(f.get_editor_property('size'))
except Exception as e:
    props['subtitle_font'] = repr(e)
slides = da.get_editor_property('slides')
props['slides'] = [{'image': str(s.get_editor_property('image')), 'lines': len(s.get_editor_property('lines'))} for s in slides]
info['data'] = props
import os
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(info, open(OUT, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
unreal.log('[Prologue] inspect written ' + OUT)
