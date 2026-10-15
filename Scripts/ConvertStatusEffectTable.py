"""기획 상태이상 데이터 테이블을 UE DataTable 임포트용 CSV로 변환한다.

기획 원본은 CSV 또는 xlsx로 받을 수 있다. 두 형식 모두 4행의 컬럼명을 기준으로 읽고,
6행부터 Status_ID가 있는 행만 변환한다. 기획 컬럼명과 구조체 프로퍼티명, 한글 enum 값의
차이는 이 스크립트가 매핑한다.

기획이 표를 종합본 하나로 합쳤으므로 --sheet 로 시트를 골라야 한다. 첫 시트를 읽으면
나이아 표를 읽게 된다.

출력이 둘이다.
    Scripts/Data/StatusEffectTable.csv     FNUStatusEffectDefinition   12행
    Scripts/Data/StatusRelationTable.csv   FNUStatusRelationRow        26행

상태이상 하나에 여러 개가 붙는 역참조(해제 아이템 둘, 면역 패시브)는 컬럼이 아니라 행으로
펼친다. 시트가 Item3_ID 를 늘려도 RELATION_COLUMNS 에 한 줄만 더하면 되고 구조체는 그대로다.

StatusEffectTag는 시트에서 받지 않고 Status_EN으로 만든다 (NU.State.Ailment.<Status_EN>).
태그 이름을 손으로 두 곳에 적으면 어긋나므로, 만든 태그가 NUGameplayTags.cpp에 실제로
등록돼 있는지도 함께 검사한다.

효과는 판별자 enum 하나가 아니라 축 여러 개다. 감전처럼 한 상태이상이 스택·틱피해·광역을
동시에 켜기 때문이다. 0 또는 FALSE 가 "이 축을 쓰지 않는다" 는 뜻이고, 그래서 축을 하나도
켜지 않은 행은 조용히 무동작이 되므로 실패시킨다.

사용법:
    python Scripts/ConvertStatusEffectTable.py "경로/데이터 테이블 종합본.xlsx"
"""

import argparse
import csv
import re
import sys
from decimal import Decimal, InvalidOperation
from pathlib import Path


# 종합본에서 이 표가 들어 있는 시트
DEFAULT_SHEET = "상태이상 테이블"

# 면역 패시브 교차 검증에 쓰는 시트, 같은 워크북에 있을 때만 검사한다
PASSIVE_SHEET = "패시브 스킬 테이블"

# 기획 문서 구조: 1~3행은 그룹 헤더, 4행은 컬럼명, 5행은 타입 표기, 6행부터 데이터
HEADER_ROW = 4
FIRST_DATA_ROW = 6

# 상태이상 태그의 공통 접두어. NUGameplayTags.cpp의 NU::State::Ailment와 짝이다.
AILMENT_TAG_PREFIX = "NU.State.Ailment"

# 기획 컬럼 -> FNUStatusEffectDefinition 프로퍼티
# 시트에 없는 컬럼(StatusEffectTag)은 Status_EN에서 파생시키므로 여기 넣지 않는다.
# 역참조 3컬럼은 서브 테이블로 가므로 RELATION_COLUMNS 가 따로 든다.
COLUMN_MAP = [
    ("Status_ID",        "StatusEffectId",    "int"),
    ("Status_Key",       "StatusEffectKey",   "name"),
    ("Status_KR",        "KoreanName",        "text"),
    ("Status_EN",        "EnglishName",       "text"),
    ("StatusType",       "StatusCategory",    "category"),
    ("Duration",         "DurationTurns",     "int"),
    ("MaxStack",         "MaxStack",          "int"),
    ("TickTiming",       "TickTiming",        "tick_timing"),
    ("DamagePerTick",    "DamagePerTick",     "float"),
    ("SplashRate",       "SplashRate",        "float"),
    ("DamageAmp",        "DamageAmp",         "float"),
    ("ForceWeakness",    "bForceWeakness",    "bool"),
    ("ActionFailRate",   "ActionFailRate",    "float"),
    ("ActionBlock",      "bActionBlock",      "bool"),
    ("BlockActiveSkill", "bBlockActiveSkill", "bool"),
    ("ForceLastAction",  "bForceLastAction",  "bool"),
    ("SPCostRate",       "SPCostRate",        "float"),
    ("CaptureBonus",     "CaptureBonus",      "ratio"),
    ("ImmuneElements",   "ImmuneElement",     "element"),
    ("Description",      "Description",       "text"),
]

# 기획 컬럼명이 바뀌는 중이라 옛 이름도 받아준다. 왼쪽이 새 이름이다.
COLUMN_ALIASES = {
    "Status_ID": ("StatusEffect_ID",),
    "Status_Key": ("StatusEffect_Key",),
    "Status_KR": ("StatusEffect_KR",),
    "Status_EN": ("StatusEffect_EN",),
    "StatusType": ("StatusEffectType",),
    "ImmuneElements": ("ImmuneElement",),
    "Description": ("Desc", "Desc_KR", "설명"),
}

# 서브 테이블로 펼치는 역참조 컬럼. (시트 컬럼, ENUStatusRelationType)
# 시트가 Item3_ID 를 늘리면 여기에 한 줄만 더한다.
#
# ★ Passive_ID(면역)는 일부러 빠져 있다.
#   패시브가 새 효과표(PassiveSkillEffectTable)의 StatusImmune 줄로 옮겨가서 게임 코드가
#   관계표의 면역 행을 더는 읽지 않는다. 아무도 안 읽는 행을 표에 남겨두면 누군가
#   그걸 고치고 효과가 없는 이유를 찾게 되므로 아예 만들지 않는다.
#   시트의 Passive_ID 컬럼은 그대로 두고, 아래 check_passive_immunities 가 시트끼리 대조한다.
RELATION_COLUMNS = [
    ("Item1_ID", "CureItem"),
    ("Item2_ID", "CureItem"),
]

# 관계 행의 RowName 을 만들 때 쓰는 약자. SkillEffectTable 의 1103_S5001 과 같은 규약이다.
RELATION_ROWNAME_TAG = {"CureItem": "I", "ImmunePassive": "P"}

# StatusType -> ENUStatusCategory. 동작을 고르지 않고 검증과 표시에만 쓰인다.
STATUS_CATEGORY = {
    "지속피해": "DamageOverTime",
    "스택": "Stack",
    "행동제어": "ActionControl",
    "자원소모": "ResourceDrain",
    "피해증폭": "DamageAmplify",
}

# 옛 분류 이름. 그대로 두면 조용히 잘못 분류되므로 실패시키고 어디로 갈지 알려준다.
RETIRED_STATUS_CATEGORY = {
    "스탯변경": "행동제어(둔화) 또는 피해증폭(균열) — 수치는 축 컬럼으로 옮겨졌다",
    "취약": "자원소모(젖음) · 스택(감전) · 행동제어(마비) · 피해증폭(치명)",
    "행동제한": "행동제어",
    "행동방해": "행동제어",
    "디버프": "행동제어 또는 피해증폭",
}

# TickTiming -> ENUStatusTickTiming. 효과를 어느 훅에 걸지 정한다.
# 분류로는 훅을 못 가른다 — 행동제어 5종의 훅이 4가지로 갈린다.
TICK_TIMING = {
    "즉시": "Immediate",
    "턴시작": "TurnStart",
    "턴종료": "TurnEnd",
    "행동시": "OnAction",
    "스킬사용시": "OnSkillUse",
    "피격시": "OnHit",
    "스택충족": "OnStackFull",
}

ELEMENT = {
    "불": "NU.Element.Fire",
    "물": "NU.Element.Water",
    "풀": "NU.Element.Grass",
    "번개": "NU.Element.Lightning",
    "땅": "NU.Element.Earth",
    "바람": "NU.Element.Wind",
}

# DamageType 은 구조체에 담지 않는다. 피해가 최대HP 비율로 고정이라는 전제가 코드에 박혀
# 있어서, 시트가 다른 값을 쓰기 시작하면 조용히 틀리므로 여기서 막는다.
ALLOWED_DAMAGE_TYPE = {"최대hp비율"}

# 효과 축. 0 또는 FALSE 면 그 축을 쓰지 않는다는 뜻이다.
FLOAT_AXES = ("DamagePerTick", "SplashRate", "DamageAmp", "ActionFailRate", "SPCostRate")
BOOL_AXES = ("bForceWeakness", "bActionBlock", "bBlockActiveSkill", "bForceLastAction")
ALL_AXES = FLOAT_AXES + BOOL_AXES

# 축의 허용 범위. None 이면 그쪽 끝을 검사하지 않는다.
AXIS_RANGE = {
    "DamagePerTick": (0.0, 1.0),
    "SplashRate": (0.0, 1.0),
    "DamageAmp": (0.0, None),
    "ActionFailRate": (0.0, 1.0),
    "SPCostRate": (0.0, None),
    "CaptureBonus": (0.0, None),
}

# 분류가 요구하는 축. 하나도 안 켜져 있으면 그 분류의 상태이상이 아무 일도 하지 않는다.
CATEGORY_REQUIRED_AXES = {
    "DamageOverTime": ("DamagePerTick",),
    "Stack": ("DamagePerTick",),
    "ActionControl": ("ActionFailRate", "bActionBlock", "bBlockActiveSkill", "bForceLastAction"),
    "ResourceDrain": ("SPCostRate",),
    "DamageAmplify": ("DamageAmp", "bForceWeakness"),
}

# DamagePerTick 을 실제로 읽는 훅. 그 밖의 훅이면 피해를 줄 자리가 없어 조용히 무동작이 된다.
DAMAGE_TICK_TIMINGS = ("TurnStart", "TurnEnd", "OnStackFull")

# 값이 비었다는 뜻으로 기획에서 사용하는 표기
EMPTY_MARKS = {"", "-", "none", "null"}

# bool 로 받는 문자열. 시트에 오타가 들어오면 조용히 FALSE 로 먹지 않고 실패해야 한다.
# 한 번 흡수하면 나중에 Ture 가 들어왔을 때 그 상태이상이 아무 일도 하지 않는다.
BOOL_WORDS = {"true": "TRUE", "false": "FALSE"}


def is_empty(value):
    return value is None or str(value).strip().lower() in EMPTY_MARKS


class Report:
    """변환 중 사람이 확인해야 할 항목을 모은다."""

    def __init__(self):
        self.unknown_enums = []
        self.retired_types = []
        self.bad_bools = []
        self.bad_damage_types = []
        self.no_axis = []
        self.missing_category_axis = []
        self.out_of_range = []
        self.timing_mismatch = []
        self.orphan_splash = []
        self.unlimited_duration = []
        self.missing_tags = []
        self.duplicate_relations = []
        self.passive_mismatch = []
        self.passive_check_skipped = False
        self.ignored_columns = []


def to_int(value):
    if is_empty(value):
        return 0
    try:
        number = Decimal(str(value).strip())
    except InvalidOperation as error:
        raise ValueError(f"정수로 변환할 수 없는 값: {value}") from error
    if not number.is_finite() or number != number.to_integral_value():
        raise ValueError(f"정수 컬럼에 정수가 아닌 값이 있다: {value}")
    return int(number)


def to_float(value, default=0.0):
    if is_empty(value):
        return default
    try:
        number = Decimal(str(value).strip())
    except InvalidOperation as error:
        raise ValueError(f"실수로 변환할 수 없는 값: {value}") from error
    if not number.is_finite():
        raise ValueError(f"유한한 실수가 아닌 값: {value}")
    return float(number)


def format_float(number):
    """유효 숫자를 보존하고 불필요한 소수점 이하 0만 제거한다."""
    if number == 0:
        return "0"
    return format(Decimal(repr(number)).normalize(), "f")


def to_bool(value, status_effect_id, prop, report):
    # 엑셀이 진짜 불리언으로 저장한 칸과 문자열 "True" 로 적은 칸이 섞여 있다
    if isinstance(value, bool):
        return "TRUE" if value else "FALSE"

    if is_empty(value):
        # 축을 쓰지 않는다는 뜻으로 본다, FALSE 가 중립값이다
        return "FALSE"

    if isinstance(value, (int, float)) and float(value) in (0.0, 1.0):
        return "TRUE" if float(value) == 1.0 else "FALSE"

    text = str(value).strip()
    word = BOOL_WORDS.get(text.lower())
    if word is None:
        report.bad_bools.append((status_effect_id, prop, text))
        return "FALSE"

    return word


def lookup(table, value, status_effect_id, prop, report):
    key = "" if value is None else str(value).strip()
    if key in table:
        return table[key]

    report.unknown_enums.append((status_effect_id, prop, key))
    return ""


def convert_category(value, status_effect_id, report):
    key = "" if value is None else str(value).strip()
    if key in RETIRED_STATUS_CATEGORY:
        # 한 행에서 멈추지 않고 전부 모아서 알려준다, 시트를 한 번에 고칠 수 있게
        report.retired_types.append((status_effect_id, key, RETIRED_STATUS_CATEGORY[key]))
        return ""
    return lookup(STATUS_CATEGORY, value, status_effect_id, "StatusCategory", report)


def convert_element(value, status_effect_id, report):
    if is_empty(value):
        return ""
    text = str(value).strip()
    if "," in text:
        raise ValueError(
            f"면역 속성이 둘 이상이다: '{text}' — 지금 구조는 속성 하나만 담는다"
        )
    return lookup(ELEMENT, text, status_effect_id, "ImmuneElement", report)


def convert_cell(kind, value, status_effect_id, prop, report):
    if kind == "int":
        return to_int(value)

    if kind == "float":
        return to_float(value)

    if kind == "ratio":
        # 보정이 없으면 1배다. 0 으로 들어가면 포획률이 0이 되므로 기본값을 1로 둔다
        return to_float(value, default=1.0)

    if kind == "bool":
        return to_bool(value, status_effect_id, prop, report)

    if kind in ("name", "text"):
        return "" if is_empty(value) else str(value).strip()

    if kind == "category":
        return convert_category(value, status_effect_id, report)

    if kind == "tick_timing":
        if is_empty(value):
            return "None"
        return lookup(TICK_TIMING, value, status_effect_id, "TickTiming", report)

    if kind == "element":
        return convert_element(value, status_effect_id, report)

    raise ValueError(f"알 수 없는 변환 종류: {kind}")


def make_tag(english_name, status_effect_id):
    """Status_EN으로 상태이상 태그를 만든다."""
    name = "" if english_name is None else str(english_name).strip()
    if not name:
        raise ValueError("Status_EN이 비어 있어 태그를 만들 수 없다")
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9]*", name):
        raise ValueError(
            f"Status_EN이 태그 이름으로 쓸 수 없는 형식이다: '{name}'"
            " — 영문자로 시작하고 영문·숫자만 사용해야 한다"
        )
    return f"{AILMENT_TAG_PREFIX}.{name}"


def read_registered_tags(tags_cpp_path):
    """NUGameplayTags.cpp에 등록된 상태이상 태그를 모은다.

    파일을 못 읽으면 None을 돌려주고 검사를 건너뛴다.
    """
    try:
        text = tags_cpp_path.read_text(encoding="utf-8")
    except OSError:
        return None

    pattern = re.escape(AILMENT_TAG_PREFIX) + r"\.[A-Za-z][A-Za-z0-9]*"
    return set(re.findall(pattern, text))


def read_csv_rows(source_path):
    with source_path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.reader(handle))


def read_xlsx_rows(source_path, sheet_name=None):
    """워크북에서 시트를 이름으로 골라 읽는다.

    기획이 표를 종합본 하나로 합쳐서, 첫 시트를 읽으면 다른 표를 읽게 된다.
    이름을 못 찾으면 첫 시트로 폴백하되 무엇을 읽었는지 반드시 알린다 —
    엉뚱한 시트를 읽으면 컬럼 누락으로 실패하지만 원인이 안 보인다.
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


def read_passive_immunities(source_path, sheet_name):
    """패시브 시트에서 {Passive_ID: Status_ID} 를 모은다.

    같은 워크북에 패시브 시트가 없으면 None 을 돌려주고 교차 검증을 건너뛴다.
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

    if "Passive_ID" not in header or "Status_ID" not in header:
        return None

    immunities = {}
    for row in rows[FIRST_DATA_ROW - 1 :]:
        passive_raw = row[header["Passive_ID"]] if header["Passive_ID"] < len(row) else None
        if is_empty(passive_raw):
            continue

        status_raw = row[header["Status_ID"]] if header["Status_ID"] < len(row) else None

        try:
            passive_id = to_int(passive_raw)
            status_id = to_int(status_raw)
        except ValueError:
            continue

        immunities[passive_id] = status_id

    return immunities


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

    # 옛 컬럼명을 새 이름으로 이어준다
    for new_name, old_names in COLUMN_ALIASES.items():
        if new_name in header:
            continue
        for old_name in old_names:
            if old_name in header:
                header[new_name] = header[old_name]
                break

    return header


def cell(source_row, header, column_name):
    index = header.get(column_name)
    if index is None or index >= len(source_row):
        return None
    return source_row[index]


def axis_is_on(record, axis):
    if axis in BOOL_AXES:
        return record[axis] == "TRUE"
    return record[axis] != 0.0


def check_axes(record, report):
    """축과 훅과 분류가 서로 맞는지 본다.

    여기서 걸리는 것들은 임포트는 되지만 상태이상이 조용히 아무 일도 하지 않게 만든다.
    """
    status_effect_id = record["StatusEffectId"]
    category = record["StatusCategory"]

    # 축을 하나도 켜지 않은 행
    if not any(axis_is_on(record, axis) for axis in ALL_AXES):
        report.no_axis.append(status_effect_id)

    # 분류가 요구하는 축이 하나도 없는 행
    required = CATEGORY_REQUIRED_AXES.get(category)
    if required and not any(axis_is_on(record, axis) for axis in required):
        report.missing_category_axis.append((status_effect_id, category, required))

    # 범위 검사
    for axis, (low, high) in AXIS_RANGE.items():
        value = record[axis]
        if low is not None and value < low:
            report.out_of_range.append((status_effect_id, axis, value, f"{low} 이상"))
        if high is not None and value > high:
            report.out_of_range.append((status_effect_id, axis, value, f"{high} 이하"))

    timing = record["TickTiming"]

    # 피해 축이 켜졌는데 그 피해를 읽는 훅이 아니면 아무 일도 일어나지 않는다
    if record["DamagePerTick"] != 0.0 and timing not in DAMAGE_TICK_TIMINGS:
        report.timing_mismatch.append(
            (status_effect_id, "DamagePerTick", timing, " / ".join(DAMAGE_TICK_TIMINGS))
        )

    # 스택이 차야 터지는 훅인데 상한이 1이면 영원히 터지지 않는다
    if timing == "OnStackFull" and record["MaxStack"] <= 1:
        report.timing_mismatch.append(
            (status_effect_id, "MaxStack", str(record["MaxStack"]), "OnStackFull 이면 2 이상")
        )

    # 번질 피해가 없는 광역 비율
    if record["SplashRate"] != 0.0 and record["DamagePerTick"] == 0.0:
        report.orphan_splash.append(status_effect_id)

    # 스택은 전투가 끝날 때까지 유지하므로 0이 정상이다
    if record["DurationTurns"] == 0 and category != "Stack":
        report.unlimited_duration.append((status_effect_id, category))


def output_fields():
    fields = ["Name"] + [prop for _, prop, _ in COLUMN_MAP]
    # StatusEffectTag는 파생 컬럼이라 EnglishName 뒤에 끼워 넣는다
    fields.insert(fields.index("EnglishName") + 1, "StatusEffectTag")
    return fields


def format_record(record):
    """검사에 쓴 파이썬 값을 CSV 문자열로 바꾼다."""
    out = {"Name": record["Name"], "StatusEffectTag": record["StatusEffectTag"]}
    for _, prop, kind in COLUMN_MAP:
        value = record[prop]
        if kind in ("float", "ratio"):
            out[prop] = format_float(value)
        elif kind == "int":
            out[prop] = str(value)
        else:
            out[prop] = value
    return out


def build_relations(source_row, header, status_effect_id, report):
    """역참조 컬럼을 서브 테이블 행으로 펼친다."""
    relations = []
    seen = set()

    for column_name, relation_type in RELATION_COLUMNS:
        raw = cell(source_row, header, column_name)
        if is_empty(raw):
            continue

        reference_id = to_int(raw)
        if reference_id <= 0:
            continue

        key = (relation_type, reference_id)
        if key in seen:
            # 같은 대상을 두 칸에 적은 것. 런타임은 흡수하지만 시트가 틀린 것이므로 막는다
            report.duplicate_relations.append((status_effect_id, column_name, reference_id))
            continue
        seen.add(key)

        relations.append(
            {
                "Name": f"{status_effect_id}_{RELATION_ROWNAME_TAG[relation_type]}{reference_id}",
                "StatusEffectId": str(status_effect_id),
                "RelationType": relation_type,
                "ReferenceId": str(reference_id),
            }
        )

    return relations


def check_passive_immunities(sheet_immunities, passive_immunities, report):
    """상태이상의 Passive_ID 와 패시브 표의 Status_ID 가 서로를 가리키는지 본다.

    어긋나면 면역이 엉뚱한 상태이상에 걸린다. 어느 쪽이 맞는지는 기획이 정해야 하므로
    실패시키지 않고 짝만 보여준다.

    ★ 예전에는 생성된 관계 행을 훑었다. 그 행을 더는 만들지 않으므로 시트 값에서 바로 본다 —
      검사가 원래 보려던 것이 시트 둘의 짝이지 출력 행이 아니었다.
    """
    if passive_immunities is None:
        report.passive_check_skipped = True
        return

    for status_id, passive_id in sorted(sheet_immunities.items()):
        if passive_immunities.get(passive_id) != status_id:
            report.passive_mismatch.append(
                (status_id, passive_id, passive_immunities.get(passive_id))
            )


def write_csv(path, field_names, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    # UE CSV 임포터용 파일은 BOM 붙은 UTF-8과 CRLF로 기록한다 (나이아 테이블과 동일)
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=field_names, lineterminator="\r\n")
        writer.writeheader()
        writer.writerows(rows)


def collect_blocking(output_rows, report):
    """임포트는 되지만 상태이상이 조용히 무동작이 되는 문제들을 모은다."""
    blocking = []

    if any(not r["StatusCategory"] for r in output_rows):
        ids = [str(r["StatusEffectId"]) for r in output_rows if not r["StatusCategory"]]
        blocking.append(f"StatusCategory 미정 {len(ids)}행 ({', '.join(ids)})")

    for label, items in (
        ("폐기된 분류", report.retired_types),
        ("bool 로 읽을 수 없는 값", report.bad_bools),
        ("DamageType 전제 위반", report.bad_damage_types),
        ("효과 축이 하나도 없는 행", report.no_axis),
        ("분류가 요구하는 축 누락", report.missing_category_axis),
        ("범위 위반", report.out_of_range),
        ("축과 훅 불일치", report.timing_mismatch),
        ("번질 피해가 없는 SplashRate", report.orphan_splash),
        ("역참조 중복", report.duplicate_relations),
    ):
        if items:
            blocking.append(f"{label} {len(items)}건")

    return blocking


def convert(source_path, csv_path, relation_csv_path, tags_cpp_path,
            skip_tag_check, allow_incomplete, sheet_name=DEFAULT_SHEET):
    try:
        source_rows = read_source_rows(source_path, sheet_name)
        header = make_header(source_rows)
    except (OSError, ValueError, RuntimeError) as error:
        sys.stdout.flush()
        print(f"[실패] 입력 파일을 읽을 수 없다: {error}", file=sys.stderr)
        return 1

    # 어느 시트를 읽었는지가 아래 실패 메시지보다 먼저 보여야 한다
    # 엉뚱한 시트를 읽으면 원인은 폴백 경고인데 결과는 컬럼 누락으로 나온다
    sys.stdout.flush()

    missing = sorted({sheet_col for sheet_col, _, _ in COLUMN_MAP if sheet_col not in header})
    if missing:
        print(f"[실패] 기획 문서에 없는 컬럼: {', '.join(missing)}", file=sys.stderr)
        print(
            "       기획 컬럼명이 바뀌었다면 COLUMN_MAP 또는 COLUMN_ALIASES를 갱신해야 한다.",
            file=sys.stderr,
        )
        return 1

    report = Report()

    # 코드가 읽지 않는 컬럼. 남아 있어도 무해하지만 한 번 알려준다.
    known = {sheet_col for sheet_col, _, _ in COLUMN_MAP}
    known |= {column_name for column_name, _ in RELATION_COLUMNS}
    known |= {"DamageType"}
    report.ignored_columns = sorted(set(header.keys()) - known)

    registered_tags = None if skip_tag_check else read_registered_tags(tags_cpp_path)
    passive_immunities = read_passive_immunities(source_path, PASSIVE_SHEET)

    output_rows = []
    relation_rows = []
    sheet_immunities = {}
    seen_ids = set()
    seen_keys = set()

    for row_index, source_row in enumerate(
        source_rows[FIRST_DATA_ROW - 1 :], start=FIRST_DATA_ROW
    ):
        raw_id = cell(source_row, header, "Status_ID")
        if is_empty(raw_id):
            continue

        try:
            status_effect_id = to_int(raw_id)
        except (TypeError, ValueError):
            print(
                f"[실패] Status_ID가 숫자가 아니다: '{raw_id}' ({row_index}행)",
                file=sys.stderr,
            )
            return 1

        if status_effect_id in seen_ids:
            print(f"[실패] Status_ID 중복: {status_effect_id} ({row_index}행)", file=sys.stderr)
            return 1
        seen_ids.add(status_effect_id)

        # UE DataTable의 RowName은 Status_ID 문자열을 사용한다 (나이아 테이블과 같은 규약)
        record = {"Name": str(status_effect_id)}

        try:
            for sheet_col, prop, kind in COLUMN_MAP:
                record[prop] = convert_cell(
                    kind, cell(source_row, header, sheet_col), status_effect_id, prop, report
                )

            record["StatusEffectTag"] = make_tag(record["EnglishName"], status_effect_id)
            relations = build_relations(source_row, header, status_effect_id, report)
        except (TypeError, ValueError) as error:
            print(
                f"[실패] {status_effect_id} 변환 중 오류 ({row_index}행): {error}",
                file=sys.stderr,
            )
            return 1

        # DamageType 은 담지 않지만 전제가 깨지면 알아야 한다
        damage_type = cell(source_row, header, "DamageType")
        if not is_empty(damage_type):
            normalized = str(damage_type).strip().lower().replace(" ", "")
            if normalized not in ALLOWED_DAMAGE_TYPE:
                report.bad_damage_types.append((status_effect_id, str(damage_type).strip()))

        if not record["StatusEffectKey"]:
            print(
                f"[실패] Status_Key가 비어 있다: {status_effect_id} ({row_index}행)",
                file=sys.stderr,
            )
            return 1
        if record["StatusEffectKey"] in seen_keys:
            print(
                f"[실패] Status_Key 중복: {record['StatusEffectKey']} ({row_index}행)",
                file=sys.stderr,
            )
            return 1
        seen_keys.add(record["StatusEffectKey"])

        if registered_tags is not None and record["StatusEffectTag"] not in registered_tags:
            report.missing_tags.append((status_effect_id, record["StatusEffectTag"]))

        check_axes(record, report)

        output_rows.append(record)
        relation_rows.extend(relations)

        # 면역 대조용. 관계 행으로는 더 만들지 않으므로 시트 값에서 직접 모은다
        immune_passive = cell(source_row, header, "Passive_ID")
        if not is_empty(immune_passive) and to_int(immune_passive) > 0:
            sheet_immunities[status_effect_id] = to_int(immune_passive)

    if not output_rows:
        print(
            "[실패] 변환할 행이 없다. 시트 이름과 Status_ID 컬럼을 확인해야 한다.",
            file=sys.stderr,
        )
        return 1

    check_passive_immunities(sheet_immunities, passive_immunities, report)

    blocking = collect_blocking(output_rows, report)
    if blocking and not allow_incomplete:
        print_report(output_rows, relation_rows, csv_path, relation_csv_path, report,
                     registered_tags is None, written=False)
        sys.stdout.flush()
        print(
            f"\n[실패] {' / '.join(blocking)} — CSV를 쓰지 않았다."
            " 채우는 중이라면 --allow-incomplete 를 붙인다.",
            file=sys.stderr,
        )
        return 1

    write_csv(csv_path, output_fields(), [format_record(r) for r in output_rows])
    write_csv(relation_csv_path,
              ["Name", "StatusEffectId", "RelationType", "ReferenceId"], relation_rows)

    print_report(output_rows, relation_rows, csv_path, relation_csv_path, report,
                 registered_tags is None)
    return 0


def print_report(rows, relation_rows, csv_path, relation_csv_path, report,
                 tag_check_skipped, written=True):
    if written:
        print(f"[완료] 상태이상 {len(rows)}행 -> {csv_path}")
        print(f"[완료] 관계 {len(relation_rows)}행 -> {relation_csv_path}")
    else:
        print(
            f"[중단] 상태이상 {len(rows)}행 · 관계 {len(relation_rows)}행을 읽었지만"
            " CSV를 쓰지 않았다"
        )

    if report.retired_types:
        print(f"\n[실패] 더 이상 쓰지 않는 분류 {len(report.retired_types)}건")
        for status_effect_id, old, guide in report.retired_types:
            print(f"       {status_effect_id}: '{old}' -> {guide}")

    if report.unknown_enums:
        print(f"\n[경고] 매핑에 없는 값 {len(report.unknown_enums)}건 — 빈 값으로 출력했다")
        for status_effect_id, prop, value in report.unknown_enums[:10]:
            print(f"       {status_effect_id}: {prop} = '{value}'")
        print(f"       StatusType 은 {' / '.join(STATUS_CATEGORY)} 중 하나여야 한다")
        print(f"       TickTiming 은 {' / '.join(TICK_TIMING)} 중 하나여야 한다")

    if report.bad_bools:
        print(f"\n[실패] bool 로 읽을 수 없는 값 {len(report.bad_bools)}건 — 시트 오타일 가능성이 높다")
        for status_effect_id, prop, value in report.bad_bools:
            print(f"       {status_effect_id}: {prop} = '{value}' (True 또는 False 여야 한다)")

    if report.bad_damage_types:
        print(f"\n[실패] DamageType 전제 위반 {len(report.bad_damage_types)}건")
        for status_effect_id, value in report.bad_damage_types:
            print(f"       {status_effect_id}: '{value}' — 코드는 최대HP 비율만 처리한다")

    if report.no_axis:
        print(f"\n[실패] 효과 축이 하나도 켜지지 않은 행 {len(report.no_axis)}건 — 무동작이 된다")
        print(f"       {', '.join(str(i) for i in report.no_axis)}")

    if report.missing_category_axis:
        print(f"\n[실패] 분류가 요구하는 축 누락 {len(report.missing_category_axis)}건")
        for status_effect_id, category, required in report.missing_category_axis:
            print(f"       {status_effect_id}: {category} 인데 {' / '.join(required)} 가 모두 비었다")

    if report.out_of_range:
        print(f"\n[실패] 수치가 범위를 벗어남 {len(report.out_of_range)}건")
        for status_effect_id, axis, value, expected in report.out_of_range:
            print(f"       {status_effect_id}: {axis} = {value}, {expected}여야 한다")

    if report.timing_mismatch:
        print(f"\n[실패] 축과 훅이 맞지 않음 {len(report.timing_mismatch)}건 — 발동할 자리가 없다")
        for status_effect_id, axis, actual, expected in report.timing_mismatch:
            print(f"       {status_effect_id}: {axis} 인데 {actual}, {expected} 여야 한다")

    if report.orphan_splash:
        print(f"\n[실패] DamagePerTick 없이 SplashRate 만 있는 행 {len(report.orphan_splash)}건")
        print(f"       {', '.join(str(i) for i in report.orphan_splash)} — 번질 피해가 없다")

    if report.duplicate_relations:
        print(f"\n[실패] 역참조 중복 {len(report.duplicate_relations)}건 — 같은 대상을 두 칸에 적었다")
        for status_effect_id, column_name, reference_id in report.duplicate_relations:
            print(f"       {status_effect_id}: {column_name} = {reference_id}")

    if report.missing_tags:
        print(f"\n[경고] NUGameplayTags.cpp에 없는 태그 {len(report.missing_tags)}건")
        for status_effect_id, tag in report.missing_tags:
            print(f"       {status_effect_id}: {tag}")
        print("       C++에 태그를 추가하거나 Status_EN을 태그 이름과 맞춰야 한다")

    if report.passive_mismatch:
        print(f"\n[경고] 면역 패시브 짝이 어긋남 {len(report.passive_mismatch)}건")
        for status_id, passive_id, pointed in report.passive_mismatch:
            target = "없음" if pointed is None else str(pointed)
            print(f"       상태이상 {status_id} -> 패시브 {passive_id} 인데,"
                  f" 패시브 {passive_id} 의 Status_ID 는 {target}")
        print("       어느 쪽이 맞는지 기획이 정해야 한다")

    if report.unlimited_duration:
        print(f"\n[확인] Duration 0 {len(report.unlimited_duration)}건 — 전투 종료까지 유지로 해석된다")
        for status_effect_id, category in report.unlimited_duration:
            print(f"       {status_effect_id}: {category}")

    if report.ignored_columns:
        print(f"\n[확인] 시트에 있지만 읽지 않는 컬럼: {', '.join(report.ignored_columns)}")

    if tag_check_skipped:
        print("\n[확인] 태그 등록 검사를 건너뛰었다 (NUGameplayTags.cpp를 찾지 못했거나 --skip-tag-check)")

    if report.passive_check_skipped:
        print(f"[확인] 면역 패시브 교차 검증을 건너뛰었다 ('{PASSIVE_SHEET}' 시트가 없다)")


def main():
    # 한국어 Windows 콘솔에서도 리포트 출력이 실패하지 않게 한다.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    script_dir = Path(__file__).parent

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="기획 상태이상 CSV 또는 xlsx 경로")
    parser.add_argument(
        "--out",
        type=Path,
        default=script_dir / "Data" / "StatusEffectTable.csv",
        help="상태이상 CSV 출력 경로 (기본: Scripts/Data/StatusEffectTable.csv)",
    )
    parser.add_argument(
        "--relation-out",
        type=Path,
        default=script_dir / "Data" / "StatusRelationTable.csv",
        help="관계 CSV 출력 경로 (기본: Scripts/Data/StatusRelationTable.csv)",
    )
    parser.add_argument(
        "--sheet",
        default=DEFAULT_SHEET,
        help=f"읽을 워크시트 이름 (기본: {DEFAULT_SHEET}). 종합본에서 시트를 고르는 데 쓴다",
    )
    parser.add_argument(
        "--tags",
        type=Path,
        default=script_dir.parent / "Source" / "ProjectM" / "NUGAS" / "NUGameplayTags.cpp",
        help="태그 등록 검사에 사용할 NUGameplayTags.cpp 경로",
    )
    parser.add_argument(
        "--skip-tag-check",
        action="store_true",
        help="태그 등록 검사를 건너뛴다",
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
        source = args.source.resolve()
        if source in (args.out.resolve(), args.relation_out.resolve()):
            print("[실패] 입력 파일과 출력 파일은 같을 수 없다.", file=sys.stderr)
            return 1
        if args.out.resolve() == args.relation_out.resolve():
            print("[실패] 두 출력 경로가 같을 수 없다.", file=sys.stderr)
            return 1
    except OSError as error:
        print(f"[실패] 경로를 확인할 수 없다: {error}", file=sys.stderr)
        return 1

    return convert(
        args.source, args.out, args.relation_out, args.tags,
        args.skip_tag_check, args.allow_incomplete, args.sheet
    )


if __name__ == "__main__":
    sys.exit(main())
