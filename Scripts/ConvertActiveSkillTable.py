"""기획 액티브 스킬 테이블을 UE DataTable 임포트용 CSV 두 개로 변환한다.

기획 원본은 CSV 또는 xlsx로 받을 수 있다. 두 형식 모두 4행의 컬럼명을 기준으로 읽고,
6행부터 Active_ID가 있는 행만 변환한다. 기획 컬럼명과 FNUActiveSkillDefinition 프로퍼티명,
한글 enum 값의 차이는 이 스크립트가 매핑한다.

출력이 두 개인 이유:
    스킬 대부분의 컬럼은 스킬 1개당 값 1개(1:1)지만 "이 스킬이 무엇을 부여하는가"는
    1개당 여러 개(1:N)다. 시트는 슬롯 컬럼(EffectType/Effect_*, Status1_*, Status2_*)으로
    적고 기획은 한 줄에서 다 보는 편이 낫지만, 코드는 슬롯 수가 상한으로 박히면
    슬롯이 늘 때마다 구조체와 어빌리티까지 고쳐야 한다.
    그래서 시트는 그대로 두고 변환 시점에 행으로 펴서 자식 테이블로 내보낸다.
    슬롯이 늘면 EFFECT_SLOTS 나 STATUS_SLOTS 에 한 줄 추가하면 되고 C++ 은 건드리지 않는다.

Desc(효과 요약) 는 컬럼을 부정하는 값이 아니다. 효과 슬롯이 스킬당 하나뿐이라
담기지 못한 추가 효과나 아직 축이 없는 효과가 산문으로 남은 것이다.
그래서 컬럼 값은 그대로 내보내고, 산문은 해석하지 않는다.
대신 설명이 어디까지 컬럼에 반영됐는지만 세 갈래로 나눠 리포트에 올린다.

설명은 기획이 쓰는 구분자로 조각을 내서 조각마다 본다. Desc 는 스킬이 하는 일을
전부 적으므로 통째로 보면 평범한 공격 스킬까지 '설명만' 으로 떨어진다.

    반영됨   - 조각이 전부 컬럼에 담겼다
    일부     - 컬럼에서 효과가 나왔고 담기지 못한 조각이 남았다
    설명만   - 컬럼에서 나온 효과가 없고 남은 조각이 있다 (전용 구현 필요)

Status1/2_ID는 상태이상 테이블에 실제로 있는 ID인지 교차 검증한다.

사용법:
    python Scripts/ConvertActiveSkillTable.py "경로/액티브 데이터 테이블.xlsx"

출력: Scripts/Data/ActiveSkillTable.csv
      Scripts/Data/SkillParryTable.csv

★ 효과 줄은 더 이상 이 시트에 없다. 기획이 슬롯 컬럼(EffectType/Effect_*, Status*_*)을
  걷어내고 "액티브 스킬 효과 테이블" 시트로 옮겼고, 그쪽은 ConvertSkillEffectTables.py
  가 만든다. 슬롯이 하나도 없으면 아래 슬롯 경로는 통째로 쉰다.
"""

import argparse
import csv
import sys
from decimal import Decimal, InvalidOperation
from pathlib import Path


# 기획 문서 구조: 1~3행은 그룹 헤더, 4행은 컬럼명, 5행은 타입 표기, 6행부터 데이터
DEFAULT_SHEET = "액티브 스킬 테이블"

HEADER_ROW = 4
FIRST_DATA_ROW = 6

# 기획 컬럼 -> FNUActiveSkillDefinition 프로퍼티
# 여기 남은 것은 전부 스킬 1개당 값 1개다.
# ★ 계수·방어력 무시·흡혈은 여기 없다. 기획이 정의표에서 걷어내고 효과표 줄로 옮겼다 —
#   계수의 기준 스탯과 조건이 행마다 달라질 수 있어 스킬당 한 칸으로는 표현이 안 된다.
#   게임도 효과표에서 읽는다 (NUActiveSkill::GatherDamageParams).
COLUMN_MAP = [
    ("Active_ID", "SkillId", "int"),
    ("Name_Key", "SkillKey", "name"),
    ("Name_KR", "KoreanName", "text"),
    ("Name_EN", "EnglishName", "text"),
    ("SkillType", "SkillType", "skill_type"),
    ("DamageType", "DamageType", "damage_type"),
    ("Element", "AttackElement", "element"),
    ("SPCost", "SPCost", "int"),
    ("TargetType", "TargetPolicy", "target_policy"),
    # 대상 수. TargetType 이 갈래를 정하지만 "적 4체" 로 적힌 스킬이 둘 있어
    # 99(전체)와 구분해 싣는다 — 적이 넷을 넘는 전투가 오면 이 칸이 필요해진다
    ("TargetCount", "TargetCount", "int"),
    ("HitCount", "HitCount", "int"),
    # Desc 는 스킬의 효과 요약, Description 은 스킬의 설명. 성격이 달라 둘 다 남긴다
    ("Desc", "EffectNote", "text"),
    ("Description", "Description", "text"),
    ("Desc_Key", "DescriptionKey", "name"),
]

# 효과 슬롯. 지금 시트에는 하나뿐이다
#   종류, 대상 스탯, 값, 비율 여부, 지속 턴, 타이밍(선택)
EFFECT_SLOTS = (
    ("EffectType", "Effect_Stat", "Effect_Value", "IsPercent", "Duration", "Effect_Timing"),
)

# 상태이상 슬롯
#   ID, 확률, 지속 턴, 타이밍(선택)
STATUS_SLOTS = (
    ("Status1_ID", "Status1_Chance", "Status1_Duration", "Status1_Timing"),
    ("Status2_ID", "Status2_Chance", "Status2_Duration", "Status2_Timing"),
)

# 자식 CSV의 컬럼. FNUSkillEffectRow 와 1:1로 맞춘다
# 종류에 따라 쓰는 컬럼이 다른 판별 유니온이다
EFFECT_OUTPUT_FIELDS = [
    "Name",
    "SkillId",
    "EffectType",
    "ApplyTo",
    "Timing",
    "Chance",
    "DurationTurns",
    "StatusEffectId",
    "AffectedStat",
    "Value",
    "bIsPercent",
]

# 자식 CSV의 컬럼. FNUSkillParryDefinition 와 1:1로 맞춘다.
# Parryable=TRUE 인 스킬만 행을 만든다 — 회피는 이 값을 안 읽으므로
# 나머지 스킬은 시트에 값이 채워져 있어도 무시한다 (자리표시자로 흔히 채워짐)
PARRY_OUTPUT_FIELDS = [
    "Name",
    "SkillId",
    "ChallengeDuration",
    "SuccessAnchor",
    "SuccessWindow",
]

# 패링 자식 행을 만들 때 읽는 시트 컬럼. COLUMN_MAP 에는 안 넣는다 —
# ActiveSkillTable.csv 의 컬럼이 아니라 자식 표로만 나가는 값이라서다.
# 여기 없으면 build_parry_row 가 매 행을 조용히 건너뛰므로 별도로 존재를 확인한다.
PARRY_SHEET_COLUMNS = ("Parryable", "ChallengeDuration", "SuccessAnchor", "SuccessWindow")

# 기획 컬럼명이 바뀔 수 있는 것들. 왼쪽이 이 스크립트가 쓰는 이름이다
# 단위가 다른 컬럼을 서로의 별칭으로 넣지 않는다 (예: 옛 계수(Coefficient, 0.6) 컬럼을
# Power 의 별칭으로 이으면 안 된다) --- to_power() 가 정수가 아닌 값은 즉시 실패시키므로
# 지금은 그런 값이 들어와도 조용히 통과하지 않지만, 애초에 다른 단위를 같은 컬럼으로
# 취급하지 않는 것이 원칙이다. 컬럼을 못 찾아 실패하는 쪽이 안전한 결과다
COLUMN_ALIASES = {
    "Active_ID": ("Skill_ID", "ActiveSkill_ID"),
    "Desc": ("SpecialNote", "Effect_KR", "효과"),
    "Description": ("Desc_KR", "설명"),
}

# 기획 문서에 아직 없어도 실패하지 않는 컬럼
OPTIONAL_SHEET_COLUMNS = {
    "Desc",
    "Desc_Key",
}

# 시트에 있어도 코드가 읽지 않는 컬럼. 남아 있어도 무해하지만 한 번 알려준다
IGNORED_SHEET_COLUMNS = {
    "Rating",       # 습득·분배용, 전투와 무관하고 지금 전부 비어 있다
}

# SkillType -> ENUSkillType. 모션 유도와 피해 항목 생성에 쓴다
SKILL_TYPE = {
    "공격": "Attack",
    "버프": "Buff",
    "디버프": "Debuff",
    "회복": "Heal",
    "특수": "Special",
}

# 효과 슬롯의 종류 -> ENUSkillEffectType
# 스탯증가와 스탯감소를 StatModifier 하나로 합친다. 방향은 Value 의 부호가 갖는다
EFFECT_TYPE = {
    "스탯증가": "StatModifier",
    "스탯감소": "StatModifier",
    "회복": "Heal",
    "상태해제": "StatusCure",
    "특수": "Special",
}

# 효과 슬롯에 적혀 있지만 다른 슬롯이 이미 담고 있어 행을 만들지 않는 종류
# 상태부여는 Status1_* 슬롯이 ID·확률·지속을 모두 갖고 있어 효과 슬롯은 중복 선언이다
EFFECT_TYPES_HANDLED_ELSEWHERE = {"상태부여"}

# 값을 음수로 뒤집어야 하는 기획 분류
NEGATIVE_EFFECT_TYPES = {"스탯감소"}

# 회복으로 읽는 기획 분류
HEAL_EFFECT_TYPES = {"회복"}

# 회복은 현재치를 올리는 것이므로 최대치 스탯을 현재치 스탯으로 바꾼다
# 최대HP 20% 회복은 "MaxHealth 를 20% 올린다"가 아니라 "Health 를 최대치의 20%만큼 올린다"다.
# 비율 기준은 bIsPercent 가 갖는다
HEAL_STAT_OVERRIDE = {
    "MaxHealth": "Health",
    "MaxSP": "SP",
}

# TargetType -> ENUTargetPolicy. 기존 열거형을 그대로 쓴다
TARGET_POLICY = {
    "자신": "Self",
    "단일적": "SingleEnemy",
    "전체적": "AllEnemies",
    "단일아군": "SingleAlly",
    "전체아군": "AllAllies",
}

DAMAGE_TYPE = {
    "물리": "NU.Damage.Type.Physical",
    "마법": "NU.Damage.Type.Magical",
}

ELEMENT = {
    "불": "NU.Element.Fire",
    "물": "NU.Element.Water",
    "풀": "NU.Element.Grass",
    "번개": "NU.Element.Lightning",
    "땅": "NU.Element.Earth",
    "바람": "NU.Element.Wind",
}

# 타이밍 컬럼 -> ENUEffectTiming. 시트에 컬럼이 없으면 기본값을 쓴다
APPLY_TIMING = {
    "BeforeHit": "BeforeHit",
    "OnHit": "OnHit",
    "AfterHit": "AfterHit",
    "판정전": "BeforeHit",
    "판정시": "OnHit",
    "판정후": "AfterHit",
}

# 상태이상은 피해가 들어간 뒤에 걸어야 한다
# 젖음 <-> 화상·폭발 상호배타처럼 먼저 붙은 쪽이 이기는 규칙에서 순서가 결과를 바꾼다
DEFAULT_STATUS_TIMING = "AfterHit"

# 버프·디버프·회복은 판정 전에 걸어야 같은 행동의 피해 계산에 반영된다
DEFAULT_EFFECT_TIMING = "BeforeHit"

# 자식 테이블 RowName 접두. 같은 스킬에 같은 축을 두 번 적으면 중복으로 걸린다
ROW_KEY_PREFIX = {
    "StatusApply": "S",
    "StatusCure": "C",
    "StatModifier": "M",
    "Heal": "H",
    "Special": "X",
}

# 축을 특정할 수 없어 순번으로 RowName 을 만드는 종류
# 같은 종류가 여러 번 오는 것이 정상이라 중복으로 보지 않는다
ORDINAL_KEYED_TYPES = {"Special", "StatusCure"}

# Effect_Stat -> ENUStatusAffectedStat. 상태이상 테이블과 같은 열거형을 쓴다
# 시트의 HP·SP 는 모호하다 — 스탯 변경이면 최대치를, 회복이면 현재치를 건드린다.
# 그 모호함은 종류로 변환 시점에 해소하고 CSV에는 확정된 값만 내보낸다.
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

# 효과 종류별로 반드시 있어야 하는 자식 컬럼
#   prop, 사람이 읽을 이름
REQUIRED_BY_EFFECT_TYPE = {
    "StatusApply": (("StatusEffectId", "상태이상 ID"),),
    "StatModifier": (("AffectedStat", "대상 스탯"), ("Value", "증감치")),
    "Heal": (("AffectedStat", "회복 대상"), ("Value", "회복량")),
}

# ★ SkillType 이 요구하는 부모 칸 검사는 여기서 사라졌다.
#   "공격이면 위력이 있어야 한다"·"버프면 효과 행이 있어야 한다" 는 둘 다 이 시트에
#   값이 있을 때의 이야기였다. 지금은 위력도 효과 줄도 효과표에 있어서
#   ConvertSkillEffectTables.py 가 본다 (check_coverage · check_damage_coefficients).
REQUIRED_BY_SKILL_TYPE = {}
NEEDS_EFFECT_ROW = set()

# 설명이 이미 다른 컬럼으로 반영된 경우를 걸러낸다
# 산문을 해석해서 데이터를 만들지는 않는다. 리포트 분류에만 쓴다
NOTE_COVERED_BY_COLUMN = (
    (("방어", "무시"), "DefenseIgnore"),
    (("흡혈",), "Lifesteal"),
)

# 기획이 효과를 여러 개 나열할 때 쓰는 구분자
# 이게 있으면 위 키워드가 걸려도 "전부 반영됨"으로 볼 수 없다
# (1310 개화의 "행동불가 후 발동·풀 위력x2·흡혈" 은 흡혈만 컬럼에 있다)
NOTE_SEPARATORS = ("·", "／", "/")

# ★ 위력 단위 검사는 여기서 사라졌다. 계수가 효과표로 갔으므로 "정수여야 한다"·"자릿수를
#   잘못 적었다" 를 보는 자리도 같이 옮겼다 — ConvertSkillEffectTables.py 의
#   check_damage_coefficients 가 이제 그 일을 한다.

# 값의 허용 범위. 벗어나면 데이터 오류로 본다
VALUE_RANGES = {
    "Power": (0, 2000),
    "DefenseIgnore": (0.0, 1.0),
    "Lifesteal": (0.0, 1.0),
}

# 자식 테이블 값의 허용 범위
EFFECT_VALUE_RANGES = {
    "Chance": (0.0, 1.0),
}

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
        self.slot_order = []
        self.duplicate_rows = []
        self.flipped_signs = []
        self.heal_stats = []
        self.handled_elsewhere = []
        self.default_chance = []
        self.default_cure_count = []
        self.note_covered = []
        self.note_partial = []
        self.note_only = []
        self.ignored_columns = []
        self.missing_slots = []
        self.status_check_skipped = False
        self.bad_parryable = []
        self.bad_parry_values = []


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
    # Decimal 문자열로 유효 숫자를 보존하고 불필요한 소수점 이하 0만 제거한다
    return format(number.normalize(), "f")


def to_bool(value):
    return "TRUE" if str(value).strip().lower() == "true" else "FALSE"


def lookup(table, value, skill_id, prop, report):
    key = "" if value is None else str(value).strip()
    if key in table:
        return table[key]

    report.unknown_enums.append((skill_id, prop, key))
    return ""


def convert_cell(kind, value, skill_id, prop, report):
    if kind == "int":
        return to_int(value)

    if kind == "float":
        return to_float(value)

    if kind == "bool":
        return to_bool(value)

    if kind in ("name", "text"):
        return "" if is_empty(value) else str(value).strip()

    if kind == "skill_type":
        return lookup(SKILL_TYPE, value, skill_id, "SkillType", report)

    if kind == "target_policy":
        return lookup(TARGET_POLICY, value, skill_id, "TargetPolicy", report)

    if kind == "damage_type":
        return lookup(DAMAGE_TYPE, value, skill_id, "DamageType", report)

    if kind == "element":
        # 무속성 스킬은 비어 있다
        if is_empty(value):
            return ""
        return lookup(ELEMENT, value, skill_id, "AttackElement", report)

    raise ValueError(f"알 수 없는 변환 종류: {kind}")


def cell_of(source_row, header, column_name):
    index = header.get(column_name)
    if index is None or index >= len(source_row):
        return None
    return source_row[index]


def resolve_timing(value, default, skill_id, report):
    if is_empty(value):
        return default
    return lookup(APPLY_TIMING, value, skill_id, "Timing", report)


def make_effect_row(skill_id, effect_type, **overrides):
    """자식 행의 기본값. 종류가 쓰지 않는 컬럼은 그대로 비어 있다."""
    row = {
        "Name": "",
        "SkillId": str(skill_id),
        "EffectType": effect_type,
        # 스킬의 TargetPolicy 를 따라간다는 뜻이다.
        # "적에게 피해 + 자신에게 버프" 조합이 생기면 그 행만 Self 로 적으면 된다
        "ApplyTo": "Target",
        "Timing": DEFAULT_EFFECT_TIMING,
        "Chance": "1",
        "DurationTurns": "0",
        "StatusEffectId": "0",
        "AffectedStat": "None",
        "Value": "0",
        "bIsPercent": "FALSE",
    }
    row.update(overrides)
    return row


def build_effect_slot_row(source_row, header, slot, skill_id, report):
    """효과 슬롯 하나를 자식 행으로 만든다. 만들 게 없으면 None."""
    type_col, stat_col, value_col, percent_col, duration_col, timing_col = slot

    raw_type = cell_of(source_row, header, type_col)
    raw_key = "" if raw_type is None else str(raw_type).strip()

    if is_empty(raw_key):
        return None

    # 다른 슬롯이 이미 담고 있는 종류는 여기서 행을 만들지 않는다
    if raw_key in EFFECT_TYPES_HANDLED_ELSEWHERE:
        report.handled_elsewhere.append((skill_id, raw_key))
        return None

    effect_type = lookup(EFFECT_TYPE, raw_key, skill_id, "EffectType", report)
    if not effect_type:
        return None

    stat = "None"
    raw_stat = cell_of(source_row, header, stat_col)
    if not is_empty(raw_stat):
        stat = lookup(AFFECTED_STAT, raw_stat, skill_id, "AffectedStat", report)

    value = to_float(cell_of(source_row, header, value_col))

    # 스탯감소는 시트가 양수로 적으므로 음수로 뒤집어 상태이상 테이블과 규약을 맞춘다
    if raw_key in NEGATIVE_EFFECT_TYPES and value != "0":
        number = Decimal(value)
        if number > 0:
            value = format(-number, "f")
            report.flipped_signs.append((skill_id, raw_key, format(number, "f")))

    # 회복은 최대치가 아니라 현재치를 올린다
    if raw_key in HEAL_EFFECT_TYPES:
        resolved = HEAL_STAT_OVERRIDE.get(stat)
        if resolved:
            report.heal_stats.append((skill_id, stat, resolved))
            stat = resolved

    # 해제 개수를 비워두면 아무것도 해제하지 않는다. 1개로 본다
    if effect_type == "StatusCure" and value == "0":
        value = "1"
        report.default_cure_count.append(skill_id)

    return make_effect_row(
        skill_id,
        effect_type,
        Timing=resolve_timing(
            cell_of(source_row, header, timing_col), DEFAULT_EFFECT_TIMING, skill_id, report
        ),
        DurationTurns=to_int(cell_of(source_row, header, duration_col)),
        AffectedStat=stat,
        Value=value,
        bIsPercent=to_bool(cell_of(source_row, header, percent_col)),
    )


def build_status_slot_rows(source_row, header, slots, skill_id, report):
    """상태이상 슬롯을 자식 행으로 펴낸다."""
    rows = []
    saw_empty_slot = False

    for id_col, chance_col, duration_col, timing_col in slots:
        status_id = to_int(cell_of(source_row, header, id_col))

        if status_id == "0":
            saw_empty_slot = True
            continue

        # 앞 슬롯을 비우고 뒤 슬롯만 채운 시트는 사람이 실수한 것이다.
        # 행으로 펴면 그냥 통과해버리므로 시트 형태가 남아 있는 지금 잡아야 한다
        if saw_empty_slot:
            report.slot_order.append((skill_id, id_col))

        # 확률을 비워두면 확정 부여로 본다. 0을 그대로 내보내면 절대 안 걸린다
        chance = to_float(cell_of(source_row, header, chance_col))
        if chance == "0":
            chance = "1"
            report.default_chance.append((skill_id, status_id))

        rows.append(
            make_effect_row(
                skill_id,
                "StatusApply",
                Timing=resolve_timing(
                    cell_of(source_row, header, timing_col),
                    DEFAULT_STATUS_TIMING,
                    skill_id,
                    report,
                ),
                Chance=chance,
                # 0이면 상태이상 정의의 지속 턴을 그대로 쓴다
                DurationTurns=to_int(cell_of(source_row, header, duration_col)),
                StatusEffectId=status_id,
            )
        )

    return rows


def build_parry_row(source_row, header, skill_id, report):
    """Parryable=TRUE 인 스킬만 자식 행을 만든다.

    Duration/Anchor/Window 는 시트에서 모든 행에 자리표시자로 채워지곤 하므로
    (예: 1.5 / 0.5 / 0.15) Parryable 자체가 게이트다 — 값이 있다고 만들지 않는다.
    """
    raw_parryable = cell_of(source_row, header, "Parryable")
    if is_empty(raw_parryable):
        return None

    text = str(raw_parryable).strip().lower()
    if text not in ("true", "false"):
        report.bad_parryable.append((skill_id, str(raw_parryable).strip()))
        return None
    if text != "true":
        return None

    duration = to_float(cell_of(source_row, header, "ChallengeDuration"))
    anchor = to_float(cell_of(source_row, header, "SuccessAnchor"))
    window = to_float(cell_of(source_row, header, "SuccessWindow"))

    if float(duration) < 0.1:
        report.bad_parry_values.append(
            (skill_id, "ChallengeDuration", duration, "0.1 이상이어야 한다"))
    if not 0.0 <= float(anchor) <= 1.0:
        report.bad_parry_values.append(
            (skill_id, "SuccessAnchor", anchor, "0.0~1.0 사이여야 한다"))
    if float(window) <= 0.0:
        report.bad_parry_values.append(
            (skill_id, "SuccessWindow", window, "0보다 커야 한다"))

    return {
        "Name": str(skill_id),
        "SkillId": str(skill_id),
        "ChallengeDuration": duration,
        "SuccessAnchor": anchor,
        "SuccessWindow": window,
    }


def assign_row_names(rows, skill_id):
    """자식 행에 RowName 을 붙인다. 축을 알 수 있는 종류는 축을, 아니면 순번을 쓴다."""
    ordinals = {}

    for row in rows:
        effect_type = row["EffectType"]
        prefix = ROW_KEY_PREFIX.get(effect_type, "E")

        if effect_type == "StatusApply" and row["StatusEffectId"] != "0":
            suffix = row["StatusEffectId"]
        elif effect_type not in ORDINAL_KEYED_TYPES and row["AffectedStat"] != "None":
            suffix = row["AffectedStat"]
        else:
            ordinals[effect_type] = ordinals.get(effect_type, 0) + 1
            suffix = str(ordinals[effect_type])

        row["Name"] = f"{skill_id}_{prefix}{suffix}"


def check_effect_rows(rows, valid_status_ids, skill_id, report):
    for row in rows:
        effect_type = row["EffectType"]

        for prop, label in REQUIRED_BY_EFFECT_TYPE.get(effect_type, ()):
            if row[prop] in ("", "0", "None"):
                report.missing_required.append(
                    (skill_id, effect_type, f"{prop} ({label})")
                )

        for prop, (low, high) in EFFECT_VALUE_RANGES.items():
            value = float(row[prop])
            if value < low or value > high:
                report.out_of_range.append(
                    (skill_id, f"{prop}[{row['Name']}]", row[prop], f"{low} ~ {high}")
                )

        # 상태이상 ID가 실제로 있는 행인지
        if valid_status_ids is not None and row["StatusEffectId"] != "0":
            status_id = int(row["StatusEffectId"])
            if status_id not in valid_status_ids:
                report.unknown_status_ids.append((skill_id, status_id))


def split_note(note):
    """설명을 기획이 쓰는 구분자로 조각낸다."""
    parts = [note]
    for mark in NOTE_SEPARATORS:
        parts = [piece for part in parts for piece in part.split(mark)]
    return [part.strip(" .") for part in parts if part.strip(" .")]


def note_part_covered(part, record, effect_rows):
    """조각이 이미 컬럼에 담겼는지 값싼 신호로만 본다. 산문을 해석하지는 않는다.

    느슨한 판정이라 놓치는 쪽보다 덮어쓰는 쪽으로 틀릴 수 있다. 리포트 분류에만 쓰고
    출력 데이터에는 영향을 주지 않으므로 이 정도로 둔다.
    """
    # "위력 75의 마법 피해" --- Power 와 DamageType 이 담고 있다
    if "위력" in part and "피해" in part and record.get("Power", "0") != "0":
        return True

    # "10% 확률로 [화상] 부여" --- 상태이상 슬롯이 담고 있다
    # 대괄호가 상태이상 이름 표기라서 슬롯이 하나라도 나왔으면 담긴 것으로 본다
    if "[" in part and any(row["EffectType"] == "StatusApply" for row in effect_rows):
        return True

    for keywords, prop in NOTE_COVERED_BY_COLUMN:
        if all(word in part for word in keywords) and record.get(prop, "0") != "0":
            return True

    return False


def classify_note(record, effect_rows, report):
    """Desc 중 컬럼이 담지 못한 조각만 남겨 세 갈래로 나눈다."""
    note = record.get("EffectNote", "")
    if is_empty(note):
        return

    skill_id = record["SkillId"]

    leftover = [
        part
        for part in split_note(note)
        if not note_part_covered(part, record, effect_rows)
    ]

    if not leftover:
        report.note_covered.append(skill_id)
        return

    # 컬럼에서 나온 효과가 있으면 남은 조각은 "그 외에 더 있는 것"이다
    described = [
        row for row in effect_rows if row["EffectType"] not in ("Special", "StatusApply")
    ]
    if described:
        summary = " / ".join(
            f"{row['EffectType']} {row['AffectedStat']} {row['Value']}"
            f"{'(비율)' if row['bIsPercent'] == 'TRUE' else ''}"
            f"{' ' + row['DurationTurns'] + '턴' if row['DurationTurns'] != '0' else ''}"
            for row in described
        )
        report.note_partial.append(
            (skill_id, record["KoreanName"], summary, " · ".join(leftover))
        )
        return

    report.note_only.append((skill_id, record["KoreanName"], " · ".join(leftover)))


def check_record(record, effect_rows, report):
    """SkillType이 요구하는 부모 값과 범위를 확인한다."""
    skill_id = record["SkillId"]
    skill_type = record["SkillType"]

    for prop, label in REQUIRED_BY_SKILL_TYPE.get(skill_type, ()):
        value = record.get(prop, "")
        if is_empty(value) or value in ("0", "None"):
            report.missing_required.append((skill_id, skill_type, f"{prop} ({label})"))

    # 공격이 아닌 스킬은 뭐라도 해야 한다
    if skill_type in NEEDS_EFFECT_ROW and not effect_rows:
        report.missing_required.append((skill_id, skill_type, "효과 행"))

    for prop, (low, high) in VALUE_RANGES.items():
        text = record.get(prop, "0")
        if text in ("", "0"):
            continue
        value = float(text)
        if value < low or value > high:
            report.out_of_range.append((skill_id, prop, text, f"{low} ~ {high}"))


def read_valid_status_ids(status_csv_path):
    """상태이상 CSV에서 유효한 ID 집합을 읽는다. 못 읽으면 None."""
    try:
        with status_csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
            rows = list(csv.DictReader(handle))
    except OSError:
        return None

    ids = set()
    for row in rows:
        raw = row.get("StatusEffectId") or row.get("StatusId") or row.get("Name")
        if raw and str(raw).strip().isdigit():
            ids.add(int(str(raw).strip()))
    return ids or None


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


def resolve_slots(header, slots, report):
    """시트에 실제로 있는 슬롯만 남긴다. 슬롯 수가 늘거나 줄어도 되게 한다."""
    resolved = []
    for slot in slots:
        if slot[0] in header:
            resolved.append(slot)
        else:
            report.missing_slots.append(slot[0])
    return resolved


def output_properties():
    return [prop for _, prop, _ in COLUMN_MAP]


def write_csv(path, field_names, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    # UE CSV 임포터용 파일은 BOM 붙은 UTF-8과 CRLF로 기록한다 (다른 테이블과 동일)
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=field_names, lineterminator="\r\n")
        writer.writeheader()
        writer.writerows(rows)


def convert(source_path, csv_path, effect_out_path, parry_out_path, status_csv_path,
            allow_incomplete, sheet_name=DEFAULT_SHEET):
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

    missing_parry = sorted(col for col in PARRY_SHEET_COLUMNS if col not in header)
    if missing_parry:
        print(f"[실패] 기획 문서에 없는 패링 컬럼: {', '.join(missing_parry)}", file=sys.stderr)
        print(
            "       빠지면 SkillParryTable 이 조용히 빈 채로 만들어진다.",
            file=sys.stderr,
        )
        return 1

    report = Report()
    report.ignored_columns = sorted(IGNORED_SHEET_COLUMNS & header.keys())

    effect_slots = resolve_slots(header, EFFECT_SLOTS, report)
    status_slots = resolve_slots(header, STATUS_SLOTS, report)

    # 슬롯 칸이 시트에서 통째로 빠졌으면 효과 줄을 만들 재료가 없다.
    # 그대로 두면 설명 대조가 60행을 전부 "설명만" 으로 찍어 리포트가 쓸모없어지고,
    # 읽지도 않는 빈 CSV 를 하나 남긴다. 그래서 이 경로만 쉰다
    has_slots = bool(effect_slots or status_slots)

    valid_status_ids = read_valid_status_ids(status_csv_path)
    report.status_check_skipped = valid_status_ids is None

    output_rows = []
    effect_output_rows = []
    parry_output_rows = []
    seen_ids = set()
    seen_keys = set()
    seen_row_names = set()

    for row_index, source_row in enumerate(
        source_rows[FIRST_DATA_ROW - 1 :], start=FIRST_DATA_ROW
    ):
        raw_id = cell_of(source_row, header, "Active_ID")
        if is_empty(raw_id):
            continue

        try:
            skill_id = int(to_int(raw_id))
        except (TypeError, ValueError):
            print(
                f"[실패] Active_ID가 숫자가 아니다: '{raw_id}' ({row_index}행)",
                file=sys.stderr,
            )
            return 1

        if skill_id in seen_ids:
            print(f"[실패] Active_ID 중복: {skill_id} ({row_index}행)", file=sys.stderr)
            return 1
        seen_ids.add(skill_id)

        # UE DataTable의 RowName은 Active_ID 문자열을 사용한다 (다른 테이블과 같은 규약)
        record = {"Name": str(skill_id)}

        try:
            for sheet_col, prop, kind in COLUMN_MAP:
                value = cell_of(source_row, header, sheet_col)
                record[prop] = convert_cell(kind, value, skill_id, prop, report)

            # 타수는 최소 1이어야 한다. 비어 있으면 단타로 본다
            if int(record["HitCount"]) < 1:
                record["HitCount"] = "1"

            effect_rows = []
            for slot in effect_slots:
                built = build_effect_slot_row(source_row, header, slot, skill_id, report)
                if built:
                    effect_rows.append(built)

            effect_rows.extend(
                build_status_slot_rows(source_row, header, status_slots, skill_id, report)
            )
            assign_row_names(effect_rows, skill_id)

            parry_row = build_parry_row(source_row, header, skill_id, report)
        except (TypeError, ValueError) as error:
            print(
                f"[실패] {skill_id} 변환 중 오류 ({row_index}행): {error}",
                file=sys.stderr,
            )
            return 1

        if not record["SkillKey"]:
            print(f"[실패] Name_Key가 비어 있다: {skill_id} ({row_index}행)", file=sys.stderr)
            return 1
        if record["SkillKey"] in seen_keys:
            print(
                f"[실패] Name_Key 중복: {record['SkillKey']} ({row_index}행)",
                file=sys.stderr,
            )
            return 1
        seen_keys.add(record["SkillKey"])

        for effect_row in effect_rows:
            if effect_row["Name"] in seen_row_names:
                report.duplicate_rows.append(effect_row["Name"])
                continue
            seen_row_names.add(effect_row["Name"])
            effect_output_rows.append(effect_row)

        check_record(record, effect_rows, report)
        check_effect_rows(effect_rows, valid_status_ids, skill_id, report)
        if has_slots:
            classify_note(record, effect_rows, report)

        if parry_row:
            parry_output_rows.append(parry_row)

        output_rows.append(record)

    if not output_rows:
        print(
            "[실패] 변환할 행이 없다. Active_ID 컬럼과 데이터 시작 행을 확인해야 한다.",
            file=sys.stderr,
        )
        return 1

    # 자식 테이블은 읽는 쪽이 로드 시 색인하므로 CSV 순서가 결과를 정하지 않는다.
    # 그래도 정렬해두면 diff 가 안정적이고 사람이 눈으로 훑기 쉽다
    effect_output_rows.sort(key=lambda row: (int(row["SkillId"]), row["Name"]))
    parry_output_rows.sort(key=lambda row: int(row["SkillId"]))

    # 아래는 임포트는 되지만 스킬이 조용히 잘못 동작하게 만든다
    blocking = []
    empty_types = [
        r["SkillId"] for r in output_rows if not r["SkillType"] or not r["TargetPolicy"]
    ]
    if empty_types:
        blocking.append(f"SkillType·TargetPolicy 미정 {len(empty_types)}행")
    if report.missing_required:
        blocking.append(f"필수 값 누락 {len(report.missing_required)}건")
    if report.out_of_range:
        blocking.append(f"범위 위반 {len(report.out_of_range)}건")
    if report.unknown_status_ids:
        blocking.append(f"없는 상태이상 ID {len(report.unknown_status_ids)}건")
    if report.slot_order:
        blocking.append(f"슬롯 순서 오류 {len(report.slot_order)}건")
    if report.duplicate_rows:
        blocking.append(f"효과 행 중복 {len(report.duplicate_rows)}건")
    if report.bad_parryable:
        blocking.append(f"Parryable 을 읽을 수 없는 값 {len(report.bad_parryable)}건")
    if report.bad_parry_values:
        blocking.append(f"패링 값 범위 오류 {len(report.bad_parry_values)}건")

    if blocking and not allow_incomplete:
        print_report(
            output_rows, effect_output_rows, parry_output_rows,
            csv_path, effect_out_path, parry_out_path, report, written=False
        )
        sys.stdout.flush()
        print(
            f"\n[실패] {' / '.join(blocking)} — CSV를 쓰지 않았다."
            " 채우는 중이라면 --allow-incomplete 를 붙인다.",
            file=sys.stderr,
        )
        return 1

    write_csv(csv_path, ["Name"] + output_properties(), output_rows)
    if has_slots:
        write_csv(effect_out_path, EFFECT_OUTPUT_FIELDS, effect_output_rows)
    write_csv(parry_out_path, PARRY_OUTPUT_FIELDS, parry_output_rows)

    print_report(
        output_rows, effect_output_rows, parry_output_rows,
        csv_path, effect_out_path, parry_out_path, report, written=True
    )
    return 0


def print_report(rows, effect_rows, parry_rows, csv_path, effect_out_path, parry_out_path,
                  report, written):
    if written:
        print(f"[완료] 스킬 {len(rows)}행 -> {csv_path}")
        if effect_rows:
            print(f"[완료] 효과 {len(effect_rows)}행 -> {effect_out_path}")
        print(f"[완료] 패링 {len(parry_rows)}행 -> {parry_out_path}")
    else:
        print(
            f"[중단] 스킬 {len(rows)}행 / 효과 {len(effect_rows)}행 / 패링 {len(parry_rows)}행을"
            " 읽었지만 CSV를 쓰지 않았다"
        )

    kinds = {}
    per_skill = {}
    for effect_row in effect_rows:
        kinds[effect_row["EffectType"]] = kinds.get(effect_row["EffectType"], 0) + 1
        per_skill[effect_row["SkillId"]] = per_skill.get(effect_row["SkillId"], 0) + 1

    if kinds:
        print("       종류별: " + ", ".join(f"{k}={v}" for k, v in sorted(kinds.items())))
    if per_skill:
        counts = {}
        for count in per_skill.values():
            counts[count] = counts.get(count, 0) + 1
        summary = ", ".join(f"{n}개={c}행" for n, c in sorted(counts.items()))
        print(f"       효과를 가진 스킬 {len(per_skill)}행 ({summary})")

    if parry_rows:
        print(f"       패링 가능 스킬 {len(parry_rows)} / {len(rows)}")

    if report.unknown_enums:
        print(f"\n[실패] 매핑에 없는 값 {len(report.unknown_enums)}건")
        for skill_id, prop, value in report.unknown_enums[:15]:
            print(f"       {skill_id}: {prop} = '{value}'")

    if report.missing_required:
        print(f"\n[실패] 필요한 값 누락 {len(report.missing_required)}건")
        for skill_id, kind, label in report.missing_required:
            print(f"       {skill_id}: {kind} 인데 {label} 가 비었다")

    if report.out_of_range:
        print(f"\n[실패] 값이 범위를 벗어남 {len(report.out_of_range)}건")
        for skill_id, prop, value, expected in report.out_of_range:
            print(f"       {skill_id}: {prop} = {value}, {expected} 여야 한다")

    if report.unknown_status_ids:
        print(f"\n[실패] 상태이상 테이블에 없는 ID {len(report.unknown_status_ids)}건")
        for skill_id, status_id in report.unknown_status_ids:
            print(f"       {skill_id}: {status_id}")

    if report.slot_order:
        print(f"\n[실패] 앞 슬롯을 비우고 뒤 슬롯만 채운 행 {len(report.slot_order)}건")
        for skill_id, column in report.slot_order:
            print(f"       {skill_id}: {column} 만 채워져 있다")

    if report.duplicate_rows:
        print(f"\n[실패] 같은 스킬에 같은 축이 두 번 {len(report.duplicate_rows)}건")
        for name in report.duplicate_rows:
            print(f"       {name}")

    if report.bad_parryable:
        print(f"\n[실패] Parryable 을 읽을 수 없는 값 {len(report.bad_parryable)}건 — 오타일 가능성이 높다")
        for skill_id, value in report.bad_parryable:
            print(f"       {skill_id}: Parryable = '{value}' (True 또는 False 여야 한다)")

    if report.bad_parry_values:
        print(f"\n[실패] Parryable=TRUE 인데 패링 값 범위 오류 {len(report.bad_parry_values)}건")
        for skill_id, prop, value, expected in report.bad_parry_values:
            print(f"       {skill_id}: {prop} = {value}, {expected}")

    if report.flipped_signs:
        print(f"\n[확인] 스탯감소 {len(report.flipped_signs)}건을 음수로 뒤집었다")
        for skill_id, raw_type, value in report.flipped_signs:
            print(f"       {skill_id}: {raw_type} {value} -> -{value}")

    if report.heal_stats:
        print(f"\n[확인] 회복 대상을 현재치로 바꿨다 {len(report.heal_stats)}건")
        for skill_id, before, after in report.heal_stats:
            print(f"       {skill_id}: {before} -> {after}")

    if report.handled_elsewhere:
        print(f"\n[확인] 효과 슬롯이 상태이상 슬롯과 중복 선언인 행 {len(report.handled_elsewhere)}건")
        print("       상태이상 슬롯이 ID·확률·지속을 다 갖고 있어 효과 슬롯에서는 행을 만들지 않았다")
        for skill_id, raw_type in report.handled_elsewhere:
            print(f"       {skill_id}: '{raw_type}'")

    if report.default_chance:
        print(f"\n[확인] 확률이 비어 있어 1.0으로 본 상태이상 {len(report.default_chance)}건")
        for skill_id, status_id in report.default_chance:
            print(f"       {skill_id}: {status_id}")

    if report.default_cure_count:
        print(f"\n[확인] 해제 개수가 비어 있어 1개로 본 행 {len(report.default_cure_count)}건")
        print(f"       {', '.join(str(skill_id) for skill_id in report.default_cure_count)}")

    if report.note_covered:
        print(f"\n[확인] 효과 설명이 전부 컬럼에 담긴 행 {len(report.note_covered)}건")
        print(f"       {', '.join(str(skill_id) for skill_id in report.note_covered)}")

    if report.note_partial:
        print(f"\n[확인] 컬럼 효과 외에 설명에만 있는 효과가 더 있는 행 {len(report.note_partial)}건")
        print("       효과 슬롯이 하나뿐이라 담기지 못한 부분이다. 슬롯이 늘면 행으로 들어온다")
        print("       조각 판정이 느슨해서 이미 컬럼에 담긴 것이 섞여 보일 수 있다")
        for skill_id, name, summary, note in report.note_partial:
            print(f"       {skill_id} {name}")
            print(f"            반영 [{summary}]")
            print(f"            잔여 [{note}]")

    if report.note_only:
        print(f"\n[확인] 효과 전체가 설명에만 있는 행 {len(report.note_only)}건 — 전용 구현이 필요하다")
        for skill_id, name, note in report.note_only:
            print(f"       {skill_id} {name}: {note}")

    if report.missing_slots:
        print(f"\n[확인] 시트에 없는 슬롯: {', '.join(report.missing_slots)}")

    if report.ignored_columns:
        print(f"\n[확인] 시트에 있지만 읽지 않는 컬럼: {', '.join(report.ignored_columns)}")

    if report.status_check_skipped:
        print("\n[확인] 상태이상 ID 교차 검증을 건너뛰었다 (상태이상 CSV를 찾지 못했다)")


def main():
    # 한국어 Windows 콘솔에서도 리포트 출력이 실패하지 않게 한다
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    script_dir = Path(__file__).parent

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="기획 액티브 스킬 CSV 또는 xlsx 경로")
    parser.add_argument(
        "--out",
        type=Path,
        default=script_dir / "Data" / "ActiveSkillTable.csv",
        help="스킬 CSV 출력 경로 (기본: Scripts/Data/ActiveSkillTable.csv)",
    )
    parser.add_argument(
        "--effect-out",
        type=Path,
        default=script_dir / "Data" / "SkillEffectTable.csv",
        help="스킬 효과 CSV 출력 경로 (기본: Scripts/Data/SkillEffectTable.csv)",
    )
    parser.add_argument(
        "--parry-out",
        type=Path,
        default=script_dir / "Data" / "SkillParryTable.csv",
        help="패링 정보 CSV 출력 경로 (기본: Scripts/Data/SkillParryTable.csv)",
    )
    parser.add_argument(
        "--status-table",
        type=Path,
        default=script_dir / "Data" / "StatusEffectTable.csv",
        help="상태이상 ID 교차 검증에 사용할 CSV 경로",
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
        source = args.source.resolve()
        outputs = (args.out.resolve(), args.effect_out.resolve(), args.parry_out.resolve())
        if source in outputs:
            print("[실패] 입력 파일과 출력 파일은 같을 수 없다.", file=sys.stderr)
            return 1
        if len(set(outputs)) != len(outputs):
            print("[실패] 출력 파일 경로가 서로 겹친다.", file=sys.stderr)
            return 1
    except OSError as error:
        print(f"[실패] 경로를 확인할 수 없다: {error}", file=sys.stderr)
        return 1

    return convert(
        args.source, args.out, args.effect_out, args.parry_out, args.status_table,
        args.allow_incomplete, args.sheet
    )


if __name__ == "__main__":
    sys.exit(main())
