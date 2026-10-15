# 시험: 짧은 무음 WAV 를 임시 경로로 임포트해 import_bgm 이 동작하는지 보고 지운다(프롤로그 에셋은 건드리지 않음).
import os, struct, wave, unreal, importlib.util
ROOT = 'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM'
tmp = ROOT + '/Saved/Prologue_20261014/test_silence.wav'
w = wave.open(tmp, 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(44100); w.writeframes(b'\0' * 44100 * 4); w.close()
src = open(ROOT + '/Tools/Prologue_20261014/build_prologue.py', encoding='utf-8').read().replace('\nmain()\n', '\n')
ns = {'__name__': 'bp'}
exec(compile(src, 'build_prologue.py', 'exec'), ns)
dest = '/Game/NUA/Prologue/_ImportTest/BGM_Test'
ok = ns['import_bgm'](tmp, dest)
asset = unreal.load_asset(dest)
unreal.log('[Prologue] import test ok=%s class=%s duration=%s' % (ok, asset.get_class().get_name() if asset else None, asset.get_editor_property('duration') if asset else None))
unreal.EditorAssetLibrary.delete_directory('/Game/NUA/Prologue/_ImportTest')
os.remove(tmp)
unreal.log('[Prologue] import test cleaned exists=%s' % unreal.EditorAssetLibrary.does_directory_exist('/Game/NUA/Prologue/_ImportTest'))
