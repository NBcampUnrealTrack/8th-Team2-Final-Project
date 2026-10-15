"""기획 패시브 스킬 테이블을 UE DataTable 임포트용 CSV로 변환한다.

기획 원본은 CSV 또는 xlsx로 받을 수 있다. 두 형식 모두 4행의 컬럼명을 기준으로 읽고,
6행부터 Passive_ID가 있는 행만 변환한다. 기획 컬럼명과 FNUPassiveDefinition 프로퍼티명,
한글 enum 값의 차이는 이 스크립트가 매핑한다.

액티브 스킬과 달리 출력이 하나다. 패시브는 한 행이 효과 하나뿐이라 1:N이 없다.
대신 그 때문에 설명의 절반이 잘린 행이 넷 있고(2008·2009·2030·2031), 리포트로 알린다.

수치는 전부 비율로 본다. 시트에 IsPercent 컬럼이 없고 40행의 값이 모두 배율이다.
고정값 패시브가 생기면 시트에 컬럼을 받아야 하므로, 값이 1을 크게 넘으면 실패시킨다.

Status_ID는 상태이상 테이블에 실제로 있는 ID인지 교차 검증한다.
면역 패시브는 상태이상 표의 Passive_ID와 양방향으로 맞는지도 본다.

사용법:
    python Scripts/ConvertPassiveTable.py "경로/데이터 테이블 종합본.xlsx"

출력: Scripts/Data/PassiveTable.csv
"""

import argparse
import csv
import sys
from decimal import Decimal, InvalidOperation
from pathlib import Path


# 기획 문서 구조: 1~3행은 그룹 헤더, 4행은 컬럼명, 5행은 타입 표기, 6행부터 데이터
DEFAULT_SHEET = "패시브 스킬 테이블"
STATUS_SHEET = "상태이상 테이블"

HEADER_ROW = 4
FIRST_DATA_ROW = 6

# 기획 컬럼 -> FNUPassiveDefinition 프로퍼티
COLUMN_MAP = [
    ("Passive_ID", "PassiveId", "int"),
    ("Passive_Key", "PassiveKey", "name"),
    ("Passive_KR", "KoreanName", "text"),
    ("Passive_EN", "EnglishName", "text"),
    ("AcquireElement", "AcquireElement", "element"),
    ("Grade", "Grade", "int"),
    ("TriggerType", "Trigger", "trigger"),
    ("TriggerChance", "TriggerChance", "float"),
    ("TriggerThreshold", "TriggerThreshold", "float"),
    ("EffectType", "EffectType", "effect_type"),
    ("TargetScope", "TargetScope", "target_scope"),
    ("Effect_Stat", "AffectedStat", "affected_stat"),
    ("Effect_Value", "EffectValue", "float"),
    ("Status_ID", "StatusEffectId", "int"),
    ("LinkedElement", "LinkedElement", "element"),
    ("Desc", "Description", "text"),
]

# 기획 컬럼명이 바뀔 수 있는 것들. 왼쪽이 이 스크립트가 쓰는 이름이다
COLUMN_ALIASES = {
    "Passive_ID": ("Passive_Id", "PassiveID"),
    "Desc": ("Desc_KR", "설명", "Description"),
}

# CSV 로 내보내는 컬럼. 시트에서 읽는 것(COLUMN_MAP)보다 좁다.
#
# ★ 값 칸(TriggerType·EffectType·Effect_Value…)은 일부러 빠져 있다.
#   패시브의 "무엇을 하는가" 가 PassiveSkillEffectTable 로 옮겨갔고 게임은 그쪽만 읽는다.
#   그래도 시트에서는 계속 읽는다 — 아래 검증들이 그 값을 보고 기획 시트의 오류를 잡기 때문이다.
#   읽는 것과 내보내는 것을 가른 이유가 이것이다.
OUTPUT_PROPERTIES = [
    "Name",
    "PassiveId",
    "PassiveKey",
    "KoreanName",
    "EnglishName",
    "AcquireElement",
    "Grade",
    "Description",
]

# 기획 문서에 아직 없어도 실패하지 않는 컬럼
#
# ★ 값 칸 일곱이 여기 들어온 이유 — 기획이 패시브의 "무엇을 하는가" 를 통째로
#   "패시브 스킬 효과 테이블" 시트로 옮겼다. 내보내는 칸은 원래부터 이것들을 안 실었고
#   (OUTPUT_PROPERTIES 참조), 읽은 이유는 오로지 검증이었다.
#   그래서 칸이 없으면 그 검증들만 쉰다 — 지우지 않는 이유는 기획 시트가 왔다 갔다 하고,
#   이 검사들이 예전에 실제로 오류를 잡았기 때문이다.
OPTIONAL_SHEET_COLUMNS = {
    "LinkedElement",
    "TriggerThreshold",
    "TriggerType",
    "TriggerChance",
    "EffectType",
    "TargetScope",
    "Effect_Stat",
    "Effect_Value",
    "Status_ID",
}

# TriggerType -> ENUPassiveTrigger
TRIGGER = {
    "상시": "Always",
    "턴시작": "OnTurnStart",
    "전투시작": "OnBattleStart",
    "공격시": "OnAttack",
    "피격시": "OnHitTaken",
    "치명피격시": "OnCriticalTaken",
    "처치시": "OnKill",
    "전투불능시": "OnDefeated",
    "저체력시": "OnLowHealth",
    "상태이상시": "OnStatusEffect",
}

# EffectType -> ENUPassiveEffectType
EFFECT_TYPE = {
    "스탯증가": "StatModifier",
    "스탯감소": "StatModifier",
    "상태부여": "StatusApply",
    "상태면역": "StatusImmune",
    "회복": "Heal",
    "피해증가": "DamageAmplify",
    "피해감소": "DamageReduce",
    "회피": "Evade",
    "특수": "Special",
}

# TargetScope -> ENUPassiveTargetScope
TARGET_SCOPE = {
    "자신": "Self",
    "적": "Enemy",
    "공격자": "Attacker",
    "랜덤적": "RandomEnemy",
}

# Effect_Stat -> ENUStatusAffectedStat. 액티브 변환기와 같은 표를 쓴다
AFFECTED_STAT = {
    "속도": "Speed",
    "Speed": "Speed",
    "방어": "Defense",
    "방어력": "Defense",
    "DEF": "Defense",
    "공격": "AttackPower",
    "공격력": "AttackPower",
    "ATK": "AttackPower",
    "마법공격": "MagicAttackPower",
    "마법공격력": "MagicAttackPower",
    "MATK": "MagicAttackPower",
    "치명타확률": "CriticalRate",
    "CritRate": "CriticalRate",
    "HP": "MaxHealth",
    "SP": "MaxSP",
}

# 회복은 최대치가 아니라 현재치를 올린다 (액티브 변환기와 같은 규약)
HEAL_EFFECT_TYPES = {"Heal"}
HEAL_STAT_OVERRIDE = {
    "MaxHealth": "Health",
    "MaxSP": "SP",
}

ELEMENT = {
    "불": "NU.Element.Fire",
    "물": "NU.Element.Water",
    "풀": "NU.Element.Grass",
    "번개": "NU.Element.Lightning",
    "땅": "NU.Element.Earth",
    "바람": "NU.Element.Wind",
}

# 속성을 가리지 않는다는 표기. 태그를 비워서 "제한 없음"으로 만든다
ELEMENT_ANY = {"all", "전체", "무속성", "공용"}

# EffectType이 반드시 요구하는 값
#   prop, 사람이 읽을 이름
REQUIRED_BY_EFFECT_TYPE = {
    "StatModifier": (("AffectedStat", "대상 스탯"), ("EffectValue", "증감치")),
    "StatusApply": (("StatusEffectId", "상태이상 ID"),),
    "StatusImmune": (("StatusEffectId", "상태이상 ID"),),
    "DamageAmplify": (("EffectValue", "증감치"),),
    "DamageReduce": (("EffectValue", "증감치"),),
}

# 트리거가 반드시 요구하는 값
REQUIRED_BY_TRIGGER = {
    "OnLowHealth": (("TriggerThreshold", "체력 기준"),),
}

# 수치가 비율이라는 전제. 이걸 넘으면 시트가 고정값을 쓰기 시작한 것이다
MAX_RATIO = Decimal("3")

# 설명이 "A +20%, B -20%" 처럼 두 효과를 담고 있는데 컬럼이 하나뿐인 행을 찾는 기준
NOTE_SEPARATORS = ("·", ",", "／", "/")

# 값이 비었다는 뜻으로 기획에서 사용하는 표기
EMPTY_MARKS = {"", "-", "none", "null"}


def is_empty(value):
    return value is None or str(value).strip().lower() in EMPTY_MARKS


class Report:
    """변환 중 사람이 확인해야 할 항목을 모은다."""

    def __init__(self):
        self.unknown_enums = []
        self.missing_required = []
        self.out_of_range = []
        self.unknown_status_ids = []
        self.duplicate_rows = []
        self.heal_stats = []
        self.note_partial = []
        self.heal_without_stat = []
        self.ignored_columns = []
        self.immunity_mismatch = []
        self.effect_checks_skipped = False
        self.status_check_skipped = False
        self.immunity_check_skipped = False


def to_int(value):
    if is_empty(value):
        return "0"
    try:
        number = Decimal(str(value).strip())
    except InvalidOperation as error:
        raise ValueError(f"정수로 변환할 수 없는 값: {value}") from error
    if not number.is_finite() or number != number.to_integral_value():
        raise ValueError(f"정수 컬럼에 정수가 아닌 값이 있다: {value}")
    return str(int(number))


def to_float(value):
    if is_empty(value):
        return "0"
    try:
        number = Decimal(str(value).strip())
    except InvalidOperation as error:
        raise ValueError(f"실수로 변환할 수 없는 값: {value}") from error
    if not number.is_finite():
        raise ValueError(f"유한한 실수가 아닌 값: {value}")
    if number == 0:
        return "0"
    return format(number.normalize(), "f")


def lookup(table, value, passive_id, prop, report):
    key = "" if value is None else str(value).strip()
    if key in table:
        return table[key]

    report.unknown_enums.append((passive_id, prop, key))
    return ""


def convert_cell(kind, value, passive_id, prop, report):
    if kind == "int":
        return to_int(value)

    if kind == "float":
        return to_float(value)

    if kind in ("name", "text"):
        return "" if is_empty(value) else str(value).strip()

    if kind == "element":
        # 속성 제한이 없으면 태그를 비운다
        if is_empty(value) or str(value).strip().lower() in ELEMENT_ANY:
            return ""
        return lookup(ELEMENT, value, passive_id, prop, report)

    if kind == "trigger":
        return lookup(TRIGGER, value, passive_id, "Trigger", report)

    if kind == "effect_type":
        return lookup(EFFECT_TYPE, value, passive_id, "EffectType", report)

    if kind == "target_scope":
        return lookup(TARGET_SCOPE, value, passive_id, "TargetScope", report)

    if kind == "affected_stat":
        if is_empty(value):
            return "None"
        return lookup(AFFECTED_STAT, value, passive_id, "AffectedStat", report)

    raise ValueError(f"알 수 없는 변환 종류: {kind}")


def cell_of(source_row, header, column_name):
    index = header.get(column_name)
    if index is None or index >= len(source_row):
        return None
    return source_row[index]


def check_record(record, raw_desc, report):
    """행 하나가 조용히 무동작이 되는 조건을 본다."""
    passive_id = record["PassiveId"]
    effect_type = record["EffectType"]
    trigger = record["Trigger"]

    for prop, label in REQUIRED_BY_EFFECT_TYPE.get(effect_type, ()):
        text = record.get(prop, "")
        if text in ("", "0", "None"):
            report.missing_required.append((passive_id, effect_type, f"{prop} ({label})"))

    for prop, label in REQUIRED_BY_TRIGGER.get(trigger, ()):
        text = record.get(prop, "")
        if text in ("", "0"):
            report.missing_required.append((passive_id, trigger, f"{prop} ({label})"))

    # 비율 전제가 깨졌는지. 고정값이 들어오기 시작하면 IsPercent 컬럼을 받아야 한다
    value_text = record.get("EffectValue", "0")
    if value_text not in ("", "0"):
        magnitude = abs(Decimal(value_text))
        if magnitude > MAX_RATIO:
            report.out_of_range.append((passive_id, "EffectValue", value_text))

    # 회복인데 대상 스탯이 없는 행은 "피해량의 N%" 같은 다른 뜻이다 (2006 흡혈)
    if effect_type == "Heal" and record.get("AffectedStat", "None") == "None":
        report.heal_without_stat.append((passive_id, record["KoreanName"], raw_desc))

    # 효과 슬롯이 하나뿐이라 설명의 뒷부분이 잘린 행
    if any(mark in raw_desc for mark in NOTE_SEPARATORS) and effect_type == "StatModifier":
        report.note_partial.append((passive_id, record["KoreanName"], raw_desc))


def read_csv_rows(source_path):
    with source_path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.reader(handle))


def read_xlsx_rows(source_path, sheet_name=None):
    """워크북에서 시트를 이름으로 골라 읽는다.

    기획이 표를 종합본 하나로 합쳐서, 첫 시트를 읽으면 다른 표를 읽게 된다.
    이름을 못 찾으면 첫 시트로 폴백하되 무엇을 읽었는지 반드시 알린다.
    """
    try:
        import openpyxl
    except ImportError as error:
        raise RuntimeError("xlsx 입력에는 openpyxl이 필요하다.") from error

    workbook = openpyxl.load_workbook(source_path, data_only=True, read_only=True)

    worksheet = None
    if sheet_name:
        for candidate in workbook.worksheets:
            if candidate.title.strip() == sheet_name.strip():
                worksheet = candidate
                break

    if worksheet is None:
        worksheet = workbook.worksheets[0]
        if sheet_name:
            print(
                f"[경고] '{sheet_name}' 시트가 없어 첫 시트 '{worksheet.title}'를 읽는다."
                f" 이 파일의 시트: {', '.join(w.title for w in workbook.worksheets)}"
            )

    print(f"[확인] 시트 '{worksheet.title}'")

    return [list(row) for row in worksheet.iter_rows(values_only=True)]


def read_source_rows(source_path, sheet_name=None):
    suffix = source_path.suffix.lower()
    if suffix == ".csv":
        return read_csv_rows(source_path)
    if suffix in (".xlsx", ".xlsm"):
        return read_xlsx_rows(source_path, sheet_name)
    raise ValueError(f"지원하지 않는 입력 형식: {source_path.suffix}")


def read_status_immunities(source_path, sheet_name):
    """상태이상 시트에서 {Status_ID: Passive_ID} 를 모은다.

    같은 워크북에 상태이상 시트가 없으면 None 을 돌려주고 교차 검증을 건너뛴다.
    """
    if source_path.suffix.lower() not in (".xlsx", ".xlsm"):
        return None

    try:
        import openpyxl
    except ImportError:
        return None

    try:
        workbook = openpyxl.load_workbook(source_path, data_only=True, read_only=True)
    except OSError:
        return None

    worksheet = None
    for candidate in workbook.worksheets:
        if candidate.title.strip() == sheet_name.strip():
            worksheet = candidate
            break

    if worksheet is None:
        return None

    rows = [list(row) for row in worksheet.iter_rows(values_only=True)]
    if len(rows) < HEADER_ROW:
        return None

    header = {}
    for index, value in enumerate(rows[HEADER_ROW - 1]):
        if not is_empty(value):
            header[str(value).strip()] = index

    if "Status_ID" not in header or "Passive_ID" not in header:
        return None

    pairs = {}
    for row in rows[FIRST_DATA_ROW - 1 :]:
        status_raw = row[header["Status_ID"]] if header["Status_ID"] < len(row) else None
        if is_empty(status_raw):
            continue

        passive_raw = row[header["Passive_ID"]] if header["Passive_ID"] < len(row) else None
        if is_empty(passive_raw):
            continue

        try:
            pairs[to_int(status_raw)] = to_int(passive_raw)
        except ValueError:
            continue

    return pairs


def read_valid_status_ids(status_csv_path):
    """상태이상 CSV에서 유효한 ID 집합을 읽는다. 못 읽으면 None."""
    try:
        with status_csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
            rows = list(csv.DictReader(handle))
    except OSError:
        return None

    ids = set()
    for row in rows:
        raw = row.get("StatusEffectId") or row.get("Name")
        if raw and str(raw).strip().isdigit():
            ids.add(int(str(raw).strip()))
    return ids or None


def make_header(rows):
    if len(rows) < HEADER_ROW:
        raise ValueError(f"입력 문서에 {HEADER_ROW}행 헤더가 없다.")

    header = {}
    for column_index, value in enumerate(rows[HEADER_ROW - 1]):
        if is_empty(value):
            continue
        name = str(value).strip()
        if name in header:
            raise ValueError(
                f"중복된 컬럼명: {name}"
                f" ({HEADER_ROW}행 {header[name] + 1}번째 칸과 {column_index + 1}번째 칸)"
                " — 어느 쪽을 쓸지 알 수 없으므로 시트에서 하나를 지워야 한다"
            )
        header[name] = column_index

    # 다른 이름으로 적혀 있어도 이어준다
    for new_name, old_names in COLUMN_ALIASES.items():
        if new_name in header:
            continue
        for old_name in old_names:
            if old_name in header:
                header[new_name] = header[old_name]
                break

    return header


def output_properties():
    return OUTPUT_PROPERTIES


def write_csv(path, field_names, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    # UE CSV 임포터용 파일은 BOM 붙은 UTF-8과 CRLF로 기록한다 (다른 테이블과 동일)
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        # ★ 읽기만 하고 내보내지 않는 칸이 record 에 남아 있다 (OUTPUT_PROPERTIES 위 주석).
        #   ignore 를 안 주면 DictWriter 가 "fieldnames 에 없는 칸" 이라며 통째로 터진다
        writer = csv.DictWriter(handle, fieldnames=field_names, lineterminator="\r\n",
                                extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def convert(source_path, csv_path, status_csv_path, allow_incomplete,
            sheet_name=DEFAULT_SHEET):
    try:
        source_rows = read_source_rows(source_path, sheet_name)
        header = make_header(source_rows)
    except (OSError, ValueError, RuntimeError) as error:
        sys.stdout.flush()
        print(f"[실패] 입력 파일을 읽을 수 없다: {error}", file=sys.stderr)
        return 1

    missing = sorted(
        {
            sheet_col
            for sheet_col, _, _ in COLUMN_MAP
            if sheet_col not in header and sheet_col not in OPTIONAL_SHEET_COLUMNS
        }
    )
    if missing:
        print(f"[실패] 기획 문서에 없는 컬럼: {', '.join(missing)}", file=sys.stderr)
        print(
            "       기획 컬럼명이 바뀌었다면 COLUMN_MAP 또는 COLUMN_ALIASES를 갱신해야 한다.",
            file=sys.stderr,
        )
        return 1

    report = Report()
    known = {sheet_col for sheet_col, _, _ in COLUMN_MAP}
    report.ignored_columns = sorted(set(header.keys()) - known)

    # 값 칸이 시트에 남아 있을 때만 그 칸을 보는 검사를 돌린다
    has_effect_columns = "EffectType" in header
    report.effect_checks_skipped = not has_effect_columns

    valid_status_ids = read_valid_status_ids(status_csv_path)
    report.status_check_skipped = valid_status_ids is None

    status_immunities = read_status_immunities(source_path, STATUS_SHEET)
    report.immunity_check_skipped = status_immunities is None

    output_rows = []
    seen_ids = set()
    seen_keys = set()

    for row_index, source_row in enumerate(
        source_rows[FIRST_DATA_ROW - 1 :], start=FIRST_DATA_ROW
    ):
        raw_id = cell_of(source_row, header, "Passive_ID")
        if is_empty(raw_id):
            continue

        try:
            passive_id = to_int(raw_id)
        except ValueError as error:
            print(f"[실패] {row_index}행 Passive_ID: {error}", file=sys.stderr)
            return 1

        if passive_id in seen_ids:
            report.duplicate_rows.append((passive_id, "Passive_ID"))
            continue
        seen_ids.add(passive_id)

        record = {"Name": passive_id}
        for sheet_col, prop, kind in COLUMN_MAP:
            # 시트에 없는 선택 칸은 아예 담지 않는다 — 빈 값을 넣으면
            # "모르는 enum 값" 으로 잡혀서 없는 오류가 40행씩 찍힌다
            if sheet_col not in header:
                continue

            raw = cell_of(source_row, header, sheet_col)
            try:
                record[prop] = convert_cell(kind, raw, passive_id, prop, report)
            except ValueError as error:
                print(f"[실패] {row_index}행 {sheet_col}: {error}", file=sys.stderr)
                return 1

        # 회복은 현재치를 올린다. 최대치 스탯이 들어왔으면 현재치로 바꾼다
        if has_effect_columns and record["EffectType"] in HEAL_EFFECT_TYPES:
            resolved = HEAL_STAT_OVERRIDE.get(record["AffectedStat"])
            if resolved:
                report.heal_stats.append((passive_id, record["AffectedStat"], resolved))
                record["AffectedStat"] = resolved

        key = record["PassiveKey"]
        if key:
            if key in seen_keys:
                report.duplicate_rows.append((passive_id, f"Passive_Key '{key}'"))
            seen_keys.add(key)

        status_id = record.get("StatusEffectId", "0")
        if has_effect_columns and valid_status_ids is not None and status_id != "0":
            if int(status_id) not in valid_status_ids:
                report.unknown_status_ids.append((passive_id, status_id))

        # 면역 패시브는 상태이상 표와 양방향으로 맞아야 한다
        if has_effect_columns and status_immunities is not None \
                and record["EffectType"] == "StatusImmune":
            expected = status_immunities.get(status_id)
            if expected != passive_id:
                report.immunity_mismatch.append((passive_id, status_id, expected))

        raw_desc = record.get("Description", "")
        if has_effect_columns:
            check_record(record, raw_desc, report)

        output_rows.append(record)

    output_rows.sort(key=lambda row: int(row["Name"]))

    # 아래는 임포트는 되지만 패시브가 조용히 잘못 동작하게 만든다
    blocking = []
    if has_effect_columns:
        empty_types = [
            r["Name"] for r in output_rows if not r["EffectType"] or not r["Trigger"]
        ]
        if empty_types:
            blocking.append(f"EffectType·Trigger 미정 {len(empty_types)}행")
    if report.missing_required:
        blocking.append(f"필수 값 누락 {len(report.missing_required)}건")
    if report.out_of_range:
        blocking.append(f"비율이 아닌 값 {len(report.out_of_range)}건")
    if report.unknown_status_ids:
        blocking.append(f"없는 상태이상 ID {len(report.unknown_status_ids)}건")
    if report.duplicate_rows:
        blocking.append(f"중복 행 {len(report.duplicate_rows)}건")
    if report.immunity_mismatch:
        blocking.append(f"면역 짝 불일치 {len(report.immunity_mismatch)}건")

    if blocking and not allow_incomplete:
        print_report(output_rows, csv_path, report, written=False)
        sys.stdout.flush()
        print(
            f"\n[실패] {' / '.join(blocking)} — CSV를 쓰지 않았다."
            " 채우는 중이라면 --allow-incomplete 를 붙인다.",
            file=sys.stderr,
        )
        return 1

    write_csv(csv_path, output_properties(), output_rows)
    print_report(output_rows, csv_path, report, written=True)
    return 0


def print_report(rows, csv_path, report, written):
    if written:
        print(f"[완료] 패시브 {len(rows)}행 -> {csv_path.resolve()}")
        from collections import Counter

        if rows and rows[0].get("Trigger") is not None:
            by_trigger = Counter(r["Trigger"] for r in rows)
            by_effect = Counter(r["EffectType"] for r in rows)
            print("       트리거별: " + ", ".join(f"{k}={v}" for k, v in by_trigger.most_common()))
            print("       효과별:   " + ", ".join(f"{k}={v}" for k, v in by_effect.most_common()))

    if report.unknown_enums:
        print(f"\n[경고] 매핑에 없는 값 {len(report.unknown_enums)}건 — 빈 값으로 출력했다")
        for passive_id, prop, value in report.unknown_enums[:15]:
            print(f"       {passive_id}: {prop} = '{value}'")

    if report.missing_required:
        print(f"\n[실패] 필요한 값 누락 {len(report.missing_required)}건")
        for passive_id, kind, label in report.missing_required:
            print(f"       {passive_id}: {kind} 인데 {label} 가 비었다")

    if report.out_of_range:
        print(f"\n[실패] 비율로 보기 어려운 값 {len(report.out_of_range)}건")
        print("       패시브 수치는 전부 비율이라는 전제다. 고정값이 필요하면 시트에")
        print("       IsPercent 컬럼을 받고 이 검사를 바꿔야 한다")
        for passive_id, prop, value in report.out_of_range:
            print(f"       {passive_id}: {prop} = {value}")

    if report.unknown_status_ids:
        print(f"\n[실패] 상태이상 테이블에 없는 ID {len(report.unknown_status_ids)}건")
        for passive_id, status_id in report.unknown_status_ids:
            print(f"       {passive_id}: {status_id}")

    if report.duplicate_rows:
        print(f"\n[실패] 중복된 행 {len(report.duplicate_rows)}건")
        for passive_id, what in report.duplicate_rows:
            print(f"       {passive_id}: {what}")

    if report.immunity_mismatch:
        print(f"\n[실패] 면역 패시브 짝이 상태이상 표와 다름 {len(report.immunity_mismatch)}건")
        print("       패시브의 Status_ID 와 상태이상의 Passive_ID 가 서로를 가리켜야 한다")
        for passive_id, status_id, expected in report.immunity_mismatch:
            got = expected if expected is not None else "없음"
            print(f"       패시브 {passive_id} -> 상태이상 {status_id}, 그쪽은 패시브 {got} 를 가리킴")

    if report.heal_stats:
        print(f"\n[확인] 회복 대상을 현재치로 바꿨다 {len(report.heal_stats)}건")
        for passive_id, before, after in report.heal_stats:
            print(f"       {passive_id}: {before} -> {after}")

    if report.heal_without_stat:
        print(f"\n[확인] 회복인데 대상 스탯이 없는 행 {len(report.heal_without_stat)}건")
        print("       스탯 비율이 아니라 '피해량의 N%' 같은 다른 뜻이다. 구현할 때 설명을 봐야 한다")
        for passive_id, name, desc in report.heal_without_stat:
            print(f"       {passive_id} {name}: {desc}")

    if report.note_partial:
        print(f"\n[확인] 설명에 효과가 둘 이상인데 컬럼이 하나뿐인 행 {len(report.note_partial)}건")
        print("       효과 슬롯이 늘어나야 나머지 절반이 데이터로 들어온다 (기획 요청)")
        for passive_id, name, desc in report.note_partial:
            print(f"       {passive_id} {name}: {desc}")

    if report.status_check_skipped:
        print("\n[확인] 상태이상 ID 교차 검증을 건너뛰었다 (상태이상 CSV를 못 읽었다)")

    if report.immunity_check_skipped:
        print(f"[확인] 면역 짝 검증을 건너뛰었다 ('{STATUS_SHEET}' 시트가 없다)")

    if report.effect_checks_skipped:
        print("\n[확인] 값 칸(EffectType·Trigger·Effect_Value…)이 시트에 없어 그 칸을 보는")
        print("       검사를 건너뛰었다 — 패시브 효과는 PassiveSkillEffectTable 이 들고 있고")
        print("       ConvertSkillEffectTables.py 가 본다")

    if report.ignored_columns:
        print(f"\n[확인] 시트에 있지만 읽지 않는 컬럼: {', '.join(report.ignored_columns)}")


def main():
    # 한국어 Windows 콘솔에서도 리포트 출력이 실패하지 않게 한다.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    script_dir = Path(__file__).parent

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="기획 패시브 CSV 또는 xlsx 경로")
    parser.add_argument(
        "--out",
        type=Path,
        default=script_dir / "Data" / "PassiveTable.csv",
        help="패시브 CSV 출력 경로 (기본: Scripts/Data/PassiveTable.csv)",
    )
    parser.add_argument(
        "--status-csv",
        type=Path,
        default=script_dir / "Data" / "StatusEffectTable.csv",
        help="Status_ID 교차 검증에 쓸 상태이상 CSV 경로",
    )
    parser.add_argument(
        "--sheet",
        default=DEFAULT_SHEET,
        help=f"읽을 워크시트 이름 (기본: {DEFAULT_SHEET}). 종합본에서 시트를 고르는 데 쓴다",
    )
    parser.add_argument(
        "--allow-incomplete",
        action="store_true",
        help="필수 값 누락·범위 위반이 있어도 CSV를 쓴다 (데이터를 채우는 중일 때)",
    )
    args = parser.parse_args()

    if not args.source.is_file():
        print(f"[실패] 입력 파일을 찾을 수 없다: {args.source}", file=sys.stderr)
        return 1

    try:
        if args.source.resolve() == args.out.resolve():
            print("[실패] 입력 파일과 출력 파일은 같을 수 없다.", file=sys.stderr)
            return 1
    except OSError as error:
        print(f"[실패] 경로를 확인할 수 없다: {error}", file=sys.stderr)
        return 1

    return convert(args.source, args.out, args.status_csv, args.allow_incomplete, args.sheet)


if __name__ == "__main__":
    sys.exit(main())
