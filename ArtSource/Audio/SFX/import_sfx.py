# 효과음 임포트와 효과음 표 채우기 (에디터 콘솔에서: py "ArtSource/Audio/SFX/import_sfx.py" [단계 ...] [소리 id ...], 단계를 안 주면 import tables clean check 차례로,
#   소리 id 를 주면 import 는 그 소리만 한다 --- 예: py "<이 파일>" import check Battle_Imp_Earth)
#   import : Saved/SFX/wav/<id>.wav => /Game/Audio/SFX/<폴더>/SFX_<id> (사운드 클래스 SC_SFX). 이미 있으면 같은 애셋에 다시 임포트. 태그 없는 줄(겹칠 재료)은 건너뛴다
#   tables : DA_SFX_UI / DA_SFX_Field / DA_SFX_Battle (UNUSFXConfig) 을 만들고 태그 => 소리 줄, 이펙트 애셋 => 태그(전투 표)를 채운다
#   clean  : 목록에 없는 소리 애셋을 지운다 (표를 먼저 다시 채운 뒤에)
#   check  : 애셋마다 사운드 클래스·길이·표의 줄 수를 로그로
import json
import os
import sys

import unreal

TAG = "[SFXImport]"
ROOT = os.path.normpath(os.path.dirname(os.path.abspath(__file__)) + "/../../..").replace("\\", "/") + "/"
WAV = ROOT + "Saved/SFX/wav/"
BASE = "/Game/Audio/SFX"
DATA = "/Game/Audio/Data"
# 태그 앞머리 => 표
TABLE = {"UI": "DA_SFX_UI", "Item": "DA_SFX_UI", "Shop": "DA_SFX_UI", "Trade": "DA_SFX_UI", "Social": "DA_SFX_UI", "Char": "DA_SFX_UI",
         "Dialogue": "DA_SFX_Field", "Quest": "DA_SFX_Field", "Field": "DA_SFX_Field", "Battle": "DA_SFX_Battle"}
# 연타되는 소리의 최소 간격(초)과 음높이 흔들림. 앞머리로 적은 것은 그 아래 태그 전부에 먹는다 (더 긴 앞머리가 먼저)
INTERVAL = {"UI.Hover": 0.08, "UI.Cursor": 0.05, "UI.Step": 0.06, "UI.Click": 0.03, "UI.Notice": 0.2, "UI.Error": 0.2, "UI.WindowOpen": 0.1, "UI.WindowClose": 0.1,
            "UI.Tab": 0.05, "Item.Acquire": 0.15, "Social.Chat": 0.3, "Dialogue.Advance": 0.05, "Quest.Progress": 0.2, "Field.Footstep": 0.12, "Field.FootstepMale": 0.12, "Field.FootstepArmor": 0.12,
            "Battle.Hit": 0.04, "Battle.Critical": 0.05, "Battle.Weak": 0.05, "Battle.Resist": 0.05, "Battle.Status": 0.1, "Battle.Heal": 0.1, "Trade.Offer": 0.05,
            "Battle.Impact": 0.03, "Battle.Strike": 0.03}
PITCH = {"Field.Footstep": 0.06, "Field.FootstepMale": 0.06, "Field.FootstepArmor": 0.06, "UI.Cursor": 0.02, "UI.Hover": 0.02, "Battle.Hit": 0.05, "Item.PickUp": 0.03, "Item.Drop": 0.03, "Dialogue.Advance": 0.02,
         "Battle.Impact": 0.04, "Battle.Strike": 0.04, "Battle.Attack": 0.03, "Battle.Impact.Finale": 0.0, "Battle.Attack.Roar": 0.02, "Battle.Attack.Wheel": 0.0,
         "Battle.Attack.Ascend": 0.0}


def _palette():
    return json.load(open(ROOT + "ArtSource/Audio/SFX/sfx_palette.json", encoding="utf-8"))


def _setting(table, name):
    """가장 긴 앞머리의 값"""
    best = max((key for key in table if name == key or name.startswith(key + ".")), key=len, default=None)
    return table[best] if best else 0.0


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="NU.SFX.%s")' % name)
    return tag


def _run(step):
    eal = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    sfx_class = unreal.load_asset("/Game/Audio/SC_SFX")
    palette = _palette()
    rows = [r for r in palette["sounds"] if r["tags"]]
    if step == "import":
        done = 0
        if _ONLY:
            assert set(_ONLY) <= {r["id"] for r in rows}, "목록에 없거나 태그 없는 id: %s" % sorted(set(_ONLY) - {r["id"] for r in rows})
            rows = [r for r in rows if r["id"] in _ONLY]
        for row in rows:
            folder = "%s/%s" % (BASE, row["folder"])
            name = "SFX_" + row["id"]
            wav = WAV + row["id"] + ".wav"
            if not os.path.exists(wav):
                unreal.log_warning("%s 파일 없음 %s" % (TAG, wav))
                continue
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", wav)
            task.set_editor_property("destination_path", folder)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("replace_existing_settings", False)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", False)
            tools.import_asset_tasks([task])
            wave = unreal.load_asset("%s/%s" % (folder, name))
            if wave is None:
                unreal.log_warning("%s 임포트 실패 %s" % (TAG, name))
                continue
            wave.set_editor_property("sound_class_object", sfx_class)
            eal.save_loaded_asset(wave, False)
            done += 1
        unreal.log("%s import %d / %d" % (TAG, done, len(rows)))
    elif step == "tables":
        entries = {}
        for row in rows:
            wave = unreal.load_asset("%s/%s/SFX_%s" % (BASE, row["folder"], row["id"]))
            for name in row["tags"]:
                entries.setdefault(name, []).append(wave)
        tables = {}
        for name, waves in entries.items():
            entry = unreal.NUSFXEntry()
            entry.set_editor_property("sounds", waves)
            entry.set_editor_property("volume", 1.0)
            entry.set_editor_property("pitch_random", _setting(PITCH, name))
            entry.set_editor_property("min_interval", _setting(INTERVAL, name))
            tables.setdefault(TABLE[name.split(".")[0]], []).append((name, entry))
        for table, items in tables.items():
            path = "%s/%s" % (DATA, table)
            asset = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(table, DATA, unreal.NUSFXConfig, unreal.DataAssetFactory())
            mapping = unreal.Map(unreal.GameplayTag, unreal.NUSFXEntry)
            for name, entry in items:
                mapping[_tag(name)] = entry
            asset.set_editor_property("entries", mapping)
            # 이펙트 애셋 => 태그는 그 태그의 소리가 든 표에 넣는다
            sources = unreal.Map(unreal.SoftObjectPath, unreal.GameplayTag)
            missing = []
            for package, name in palette.get("sources", {}).items():
                if TABLE[name.split(".")[0]] != table:
                    continue
                if not eal.does_asset_exist(package):
                    missing.append(package)
                sources[unreal.SoftObjectPath("%s.%s" % (package, package.rsplit("/", 1)[-1]))] = _tag(name)
            asset.set_editor_property("source_events", sources)
            unreal.log("%s %s 줄 %d, 이펙트 짝 %d (없는 이펙트 %s) saved=%s" % (
                TAG, table, len(asset.get_editor_property("entries")), len(asset.get_editor_property("source_events")), missing, eal.save_loaded_asset(asset, False)))
    elif step == "clean":
        keep = {"%s/%s/SFX_%s" % (BASE, row["folder"], row["id"]) for row in rows}
        removed = []
        for path in eal.list_assets(BASE, recursive=True, include_folder=False):
            package = path.split(".")[0]
            if package not in keep:
                removed.append("%s=%s" % (package.rsplit("/", 1)[-1], eal.delete_asset(package)))
        unreal.log("%s 목록에 없는 소리 %d개 지움: %s" % (TAG, len(removed), removed))
    else:
        bad = 0
        total = 0.0
        for row in rows:
            wave = unreal.load_asset("%s/%s/SFX_%s" % (BASE, row["folder"], row["id"]))
            cls = wave.get_editor_property("sound_class_object") if wave else None
            if wave is None or cls is None or cls.get_name() != "SC_SFX" or wave.get_editor_property("looping"):
                bad += 1
                unreal.log_warning("%s 이상: %s class=%s" % (TAG, row["id"], cls.get_name() if cls else None))
            total += wave.get_editor_property("duration") if wave else 0.0
        unreal.log("%s 소리 %d개, 이상 %d, 길이 합 %.1f초, 폴더의 애셋 %d개" % (TAG, len(rows), bad, total, len(eal.list_assets(BASE, recursive=True, include_folder=False))))
        for table in sorted(set(TABLE.values())):
            asset = unreal.load_asset("%s/%s" % (DATA, table))
            entries = asset.get_editor_property("entries") if asset else {}
            empty = [str(k.get_editor_property("tag_name")) for k, v in entries.items() if len(v.get_editor_property("sounds")) == 0 or None in list(v.get_editor_property("sounds"))]
            unreal.log("%s %s 줄 %d, 소리 빈 줄 %s, 이펙트 짝 %d" % (TAG, table, len(entries), empty, len(asset.get_editor_property("source_events")) if asset else 0))
    unreal.log("%s done" % TAG)


_STEPS = ("import", "tables", "clean", "check")
# 단계가 아닌 인자는 소리 id --- import 를 그 소리만 한다 (전부 다시 임포트하면 소리가 같은 애셋도 다시 저장돼 바뀜으로 뜬다)
_ONLY = [a for a in sys.argv[1:] if a not in _STEPS]
for _step in [a for a in sys.argv[1:] if a in _STEPS] or list(_STEPS):
    _run(_step)
del _run
