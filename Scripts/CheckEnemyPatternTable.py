"""적 행동 패턴 CSV 네 장을 검사한다.

패턴이 데이터에셋(UNUEnemyPatternAsset)에 있던 동안은 에디터가 대신 막아주던 것이 있었다.

    EditCondition   조건 종류에 따라 Value / Tag 중 하나를 감췄다
    ClampMin        Weight·CooldownTurns·MaxUsesPerBattle 의 음수를 막았다
    태그 피커        존재하지 않는 게임플레이 태그를 애초에 고를 수 없었다

셋 다 디테일 패널 위젯의 기능이라 CSV 임포트에는 적용되지 않는다. 그래서 이 검사기가 그
자리를 메운다. 여기서 잡는 것은 전부 "임포트는 되는데 조용히 무동작·오동작" 이 되는 조건이다.

가장 위험한 것은 체력 비율을 퍼센트로 적는 실수다.

    Type=SelfHPRatioBelow, Value=50     <- "체력 50%" 라고 쓴 것

셀렉터는 Ratio <= Value 로 보므로 이 규칙은 만피에서부터 발동한다. 임포트도 되고 에러도 없다.

검사 대상 (Scripts/Data/):
    EnemyPatternMapTable.csv         NaiaId -> PatternId, 대체 행동
    EnemyPatternRuleTable.csv        규칙
    EnemyPatternConditionTable.csv   규칙·페이즈가 같이 쓰는 조건 (OwnerType 으로 가름)
    EnemyPatternPhaseTable.csv       보스 페이즈 (지금은 헤더만)

나중에 기획이 이 표를 가져가면 시트 -> 변환기 -> CSV 경로가 붙는다. 그때 이 검사기가
변환기의 꼬리가 되므로 검사 로직은 입력 형식과 분리해 뒀다.

사용법:
    python Scripts/CheckEnemyPatternTable.py
"""

import argparse
import csv
import re
import sys
from pathlib import Path


# 조건 종류별로 Value 의 단위가 다르다. 같은 컬럼인데 뜻이 바뀌는 자리라 검사가 필요하다.
HP_RATIO_TYPES = {"SelfHPRatioBelow", "SelfHPRatioAbove"}
TURN_TYPES = {"TurnNumberAfter", "TurnNumberBefore"}
TAG_TYPES = {"SelfHasTag", "AnyOpponentHasTag"}
CONDITION_TYPES = HP_RATIO_TYPES | TURN_TYPES | TAG_TYPES

# 조건 표를 규칙과 페이즈가 같이 쓴다. 두 구조체가 같은 모양이라 표를 쪼개면 파서도 둘이 된다.
OWNER_TYPES = {"Rule", "Phase"}

# UE DataTable 은 enum 을 짧은 이름으로 임포트한다 (다른 표들도 그렇게 쓴다).
ENUM_PREFIX = "ENUEnemyConditionType::"

TRUE_MARKS = {"true", "1"}
FALSE_MARKS = {"false", "0"}
EMPTY_MARKS = {"", "none", "-"}

REQUIRED_COLUMNS = {
    "map": ("Name", "NaiaId", "PatternId", "FallbackAbilityId"),
    "rule": ("Name", "PatternId", "AbilityId", "SkillId", "Priority", "Weight",
             "CooldownTurns", "MaxUsesPerBattle", "PhaseTag",
             "bOverrideParryTiming", "ChallengeDuration", "SuccessAnchor", "SuccessWindow"),
    "condition": ("Name", "OwnerType", "OwnerId", "Type", "Value", "Tag", "bInvert"),
    "phase": ("Name", "PatternId", "PhaseIndex", "PhaseTag", "OnEnterAbilityId"),
}

# 없어도 되는 컬럼. 있으면 값을 검사하고, 없으면 C++ 기본값(0 · 빈 경로)으로 임포트된다
OPTIONAL_COLUMNS = {
    "map": ("ConcedeHealthRatio", "ConcedeAnimation", "ConcedeLoopAnimation"),
}

# 액티브 스킬을 실행할 때 쓰는 공용 아키타입 태그. SkillId 유무와 이 태그가 어긋나면
# 실행 시점에 이벤트 태그와 스킬 데이터가 서로 다른 행동을 가리키게 된다.
ACTIVE_SKILL_ABILITY_TAG = "NU.Ability.Battle.ActiveSkill"

TABLE_FILES = {
    "map": "EnemyPatternMapTable.csv",
    "rule": "EnemyPatternRuleTable.csv",
    "condition": "EnemyPatternConditionTable.csv",
    "phase": "EnemyPatternPhaseTable.csv",
}


def is_empty(value):
    return value is None or str(value).strip().lower() in EMPTY_MARKS


class Report:
    """사람이 확인해야 할 항목을 모은다. 막는 것과 경고를 나눠 담는다."""

    def __init__(self):
        # 막는 것 — 임포트는 되지만 규칙이 조용히 무동작·오동작이 된다
        self.missing_columns = []
        self.duplicate_names = []
        self.empty_names = []
        self.bad_enums = []
        self.bad_bools = []
        self.bad_numbers = []
        self.hp_ratio_out_of_range = []
        self.turn_not_integer = []
        self.tag_on_numeric_condition = []
        self.missing_condition_tag = []
        self.unknown_tags = []
        self.orphan_conditions = []
        self.orphan_rules = []
        self.patterns_without_rules = []
        self.unknown_phase_tags = []
        self.duplicate_phase_index = []
        self.unknown_naia_ids = []
        self.naia_id_mismatch = []
        self.unknown_skill_ids = []
        self.skill_ability_mismatch = []
        self.bad_parry_override = []

        # 경고 — 의도일 수 있으므로 막지 않는다
        self.prefixed_enums = []
        self.zero_weight = []
        self.first_phase_has_conditions = []
        self.shared_cooldown = []
        self.empty_fallback = []
        self.ignored_columns = []
        self.parry_values_without_override = []

        # 건너뛴 검사
        self.tag_check_skipped = False
        self.naia_check_skipped = False
        self.skill_check_skipped = False


def read_registered_tags(tags_cpp_path):
    """NUGameplayTags.cpp 에 등록된 태그를 모은다.

    파일을 못 읽으면 None 을 돌려주고 검사를 건너뛴다. 어빌리티 태그와 상태 태그를 같이
    담는다 — 조건의 Tag 컬럼은 상태 태그를 가리킬 수 있다.
    """
    try:
        text = tags_cpp_path.read_text(encoding="utf-8")
    except OSError:
        return None

    return set(re.findall(r'"(NU\.[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)*)"', text))


def read_naia_ids(naia_csv_path):
    """NaiaTable.csv 의 RowName 을 모은다. 매핑 표의 키가 실제 나이아인지 교차 검증한다."""
    try:
        with naia_csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
            return {row["Name"].strip() for row in csv.DictReader(handle) if row.get("Name")}
    except (OSError, KeyError):
        return None


def read_skill_ids(skill_csv_path):
    """ActiveSkillTable.csv 의 SkillId 를 모은다. 규칙 표의 SkillId 가 실제 스킬인지 검증한다."""
    try:
        with skill_csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
            return {int(row["SkillId"]) for row in csv.DictReader(handle) if row.get("SkillId")}
    except (OSError, KeyError, ValueError):
        return None


def read_table(path, kind, report):
    """CSV 를 읽어 행 목록을 돌려준다. 필수 컬럼이 없으면 None."""
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            field_names = reader.fieldnames or []
            rows = [row for row in reader if any(not is_empty(v) for v in row.values())]
    except OSError as error:
        report.missing_columns.append((kind, f"파일을 읽을 수 없다: {error}"))
        return None

    required = REQUIRED_COLUMNS[kind]
    missing = [name for name in required if name not in field_names]
    if missing:
        report.missing_columns.append((kind, f"컬럼 누락: {', '.join(missing)}"))
        return None

    for name in field_names:
        if name not in required and name not in OPTIONAL_COLUMNS.get(kind, ()):
            report.ignored_columns.append((kind, name))

    return rows


def check_row_names(rows, kind, report):
    """행 이름이 비었거나 겹치는지 본다. 겹치면 DataTable 에서 뒤 행이 앞 행을 조용히 덮는다."""
    seen = set()
    for index, row in enumerate(rows):
        name = (row.get("Name") or "").strip()
        if not name:
            report.empty_names.append((kind, index + 2))
            continue
        if name in seen:
            report.duplicate_names.append((kind, name))
        seen.add(name)


def to_number(value, kind, row_name, column, report):
    """수치로 못 읽으면 막고 None 을 돌려준다."""
    if is_empty(value):
        return None
    try:
        return float(str(value).strip())
    except ValueError:
        report.bad_numbers.append((kind, row_name, column, str(value).strip()))
        return None


def check_int_min(value, minimum, kind, row_name, column, report):
    """ClampMin 이 CSV 에는 없으므로 여기서 본다."""
    number = to_number(value, kind, row_name, column, report)
    if number is None:
        return
    if number != int(number):
        report.bad_numbers.append((kind, row_name, column, f"{value} (정수여야 한다)"))
        return
    if number < minimum:
        report.bad_numbers.append((kind, row_name, column, f"{value} ({minimum} 이상이어야 한다)"))


def check_bool(value, kind, row_name, column, report):
    """시트에 Flase 오타가 44칸 있었던 선례가 있다. 조용히 false 로 먹으면 안 된다."""
    if is_empty(value):
        return
    text = str(value).strip().lower()
    if text not in TRUE_MARKS and text not in FALSE_MARKS:
        report.bad_bools.append((kind, row_name, column, str(value).strip()))


def check_tag(value, kind, row_name, column, registered, report, required=True):
    """등록되지 않은 태그는 임포트 뒤 빈 태그가 되고, 그 규칙은 영원히 안 뽑힌다.

    ImportSingleGameplayTag 의 ValidateTagCreation 이 실패하면 TagName 이 NAME_None 으로
    남고 행은 그대로 임포트된다. 그러면 IsRuleAvailable 의 AbilityId.IsValid() 에서 걸린다.
    """
    if is_empty(value):
        if required:
            report.unknown_tags.append((kind, row_name, column, "(비어 있음)"))
        return
    if registered is None:
        return
    tag = str(value).strip()
    if tag not in registered:
        report.unknown_tags.append((kind, row_name, column, tag))


def normalize_enum(value, report, kind, row_name):
    """짧은 이름으로 정규화한다. 접두어를 붙여도 임포트는 되지만 다른 표와 규약이 어긋난다."""
    text = str(value).strip()
    if text.startswith(ENUM_PREFIX):
        report.prefixed_enums.append((kind, row_name, text))
        return text[len(ENUM_PREFIX):]
    return text


def check_map_table(rows, registered, naia_ids, report):
    check_row_names(rows, "map", report)

    for row in rows:
        name = (row.get("Name") or "").strip()

        if naia_ids is not None and name and name not in naia_ids:
            report.unknown_naia_ids.append(name)

        # NaiaId 는 Name(RowName) 을 그대로 복제한 컬럼이다 (다른 표들의 관례).
        # 둘이 다르면 서버가 CSV 를 직접 파싱할 때와 UE 가 RowName 으로 찾을 때
        # 서로 다른 나이아를 가리키게 된다
        naia_id = (row.get("NaiaId") or "").strip()
        if not is_empty(naia_id) and name and naia_id != name:
            report.naia_id_mismatch.append((name, naia_id))

        if is_empty(row.get("PatternId")):
            report.bad_numbers.append(("map", name, "PatternId", "(비어 있음)"))

        # 비어 있으면 인카운터가 기본 공격으로 채운다. 의도일 수 있어 경고만 한다
        if is_empty(row.get("FallbackAbilityId")):
            report.empty_fallback.append(name)
        else:
            check_tag(row.get("FallbackAbilityId"), "map", name,
                      "FallbackAbilityId", registered, report)

        # 인정 임계치 — 남은 체력 비율이라 0 이상 1 미만. 1 이면 시작하자마자 인정한다
        ratio = to_number(row.get("ConcedeHealthRatio"), "map", name, "ConcedeHealthRatio", report)
        if ratio is not None and not (0.0 <= ratio < 1.0):
            report.bad_numbers.append(("map", name, "ConcedeHealthRatio", f"{ratio} (0 이상 1 미만이어야 한다)"))

        # 인정 시퀀스는 애셋 경로. 경로가 아니면 임포트가 조용히 빈 값으로 넣는다
        for column in ("ConcedeAnimation", "ConcedeLoopAnimation"):
            value = (row.get(column) or "").strip()
            if not is_empty(value) and not value.startswith("/Game/"):
                report.bad_numbers.append(("map", name, column, f"{value} (/Game/ 애셋 경로여야 한다)"))


def check_skill_id(row, row_name, skill_ids, report):
    """SkillId 와 AbilityId 조합, 그리고 SkillId 가 실제 스킬인지를 본다.

    SkillId=0 은 AbilityId 태그로 도는 행동(기본공격 등)이고, SkillId>0 은
    ActiveSkillTable 의 그 행을 쓴다. 이때 AbilityId 는 반드시 공용 아키타입
    태그(NU.Ability.Battle.ActiveSkill)여야 한다 — Command.AbilityId 가 게임플레이
    이벤트 태그로 그대로 실리므로, 둘이 어긋나면 실행 시점에 이벤트가 트리거하는
    행동과 실제로 먹이는 스킬 데이터가 서로 달라진다.
    """
    skill_id_raw = row.get("SkillId")
    if is_empty(skill_id_raw):
        return

    try:
        skill_id = int(float(skill_id_raw))
    except ValueError:
        # bad_numbers 는 check_int_min 계열이 이미 잡으므로 여기서는 조용히 넘어간다
        return

    ability_id = (row.get("AbilityId") or "").strip()

    if skill_id > 0 and ability_id != ACTIVE_SKILL_ABILITY_TAG:
        report.skill_ability_mismatch.append(
            (row_name, skill_id, ability_id, f"'{ACTIVE_SKILL_ABILITY_TAG}' 이어야 한다"))
    elif skill_id == 0 and ability_id == ACTIVE_SKILL_ABILITY_TAG:
        report.skill_ability_mismatch.append(
            (row_name, skill_id, ability_id, "SkillId 가 0 이라 구체적인 태그가 필요하다"))

    if skill_id != 0 and skill_ids is not None and skill_id not in skill_ids:
        report.unknown_skill_ids.append((row_name, skill_id))


def check_parry_override(row, row_name, report):
    """bOverrideParryTiming 이 켜졌을 때만 세 값을 읽으므로, 켜진 채로 값이
    비정상이면 조용히 잘못된 반응 창이 열린다.
    """
    override_raw = str(row.get("bOverrideParryTiming") or "").strip().upper()
    if override_raw not in ("TRUE", "FALSE"):
        return  # bad_bools 가 이미 잡음

    duration = to_number(row.get("ChallengeDuration"), "rule", row_name, "ChallengeDuration", Report())
    anchor = to_number(row.get("SuccessAnchor"), "rule", row_name, "SuccessAnchor", Report())
    window = to_number(row.get("SuccessWindow"), "rule", row_name, "SuccessWindow", Report())

    if override_raw == "TRUE":
        if duration is not None and duration < 0.1:
            report.bad_parry_override.append(
                (row_name, "ChallengeDuration", duration, "0.1 이상이어야 한다"))
        if anchor is not None and not 0.0 <= anchor <= 1.0:
            report.bad_parry_override.append(
                (row_name, "SuccessAnchor", anchor, "0.0~1.0 사이여야 한다"))
        if window is not None and window <= 0.0:
            report.bad_parry_override.append(
                (row_name, "SuccessWindow", window, "0보다 커야 한다"))
    else:
        # 꺼져 있으면 읽지 않으니 해가 없지만, 값이 남아 있으면 나중에 헷갈린다
        if any(v not in (None, 0.0) for v in (duration, anchor, window)):
            report.parry_values_without_override.append(row_name)


def check_rule_table(rows, pattern_ids, phase_tags_by_pattern, registered, skill_ids, report):
    check_row_names(rows, "rule", report)

    for row in rows:
        name = (row.get("Name") or "").strip()
        pattern_id = (row.get("PatternId") or "").strip()

        # 매핑 표가 가리키지 않는 패턴의 규칙은 아무도 쓰지 않는다
        if pattern_id and pattern_id not in pattern_ids:
            report.orphan_rules.append((name, pattern_id))

        check_tag(row.get("AbilityId"), "rule", name, "AbilityId", registered, report)

        check_int_min(row.get("Priority"), -(2 ** 31), "rule", name, "Priority", report)
        check_int_min(row.get("CooldownTurns"), 0, "rule", name, "CooldownTurns", report)
        check_int_min(row.get("MaxUsesPerBattle"), 0, "rule", name, "MaxUsesPerBattle", report)

        weight = to_number(row.get("Weight"), "rule", name, "Weight", report)
        if weight is not None:
            if weight < 0:
                report.bad_numbers.append(("rule", name, "Weight", f"{weight} (0 이상이어야 한다)"))
            elif weight == 0:
                # 구조체 주석대로 0 은 "뽑히지 않음" 이라 의도적 비활성일 수 있다
                report.zero_weight.append(name)

        phase_tag = (row.get("PhaseTag") or "").strip()
        if not is_empty(phase_tag):
            known = phase_tags_by_pattern.get(pattern_id, set())
            if phase_tag not in known:
                report.unknown_phase_tags.append((name, pattern_id, phase_tag))

        check_skill_id(row, name, skill_ids, report)
        check_parry_override(row, name, report)

    # 쿨타임과 사용 횟수는 규칙이 아니라 어빌리티+스킬 단위로 센다 (명세 10 의 코드전용 항목).
    # 액티브 스킬은 전부 AbilityId 가 같은 태그(NU.Ability.Battle.ActiveSkill)를 공유하므로
    # SkillId 까지 같이 묶어야 함 — 태그만 묶으면 서로 다른 스킬을 오탐한다
    by_pattern_ability_skill = {}
    for row in rows:
        key = (
            (row.get("PatternId") or "").strip(),
            (row.get("AbilityId") or "").strip(),
            (row.get("SkillId") or "").strip(),
        )
        by_pattern_ability_skill.setdefault(key, []).append(row)

    for (pattern_id, ability_id, skill_id), group in by_pattern_ability_skill.items():
        if len(group) < 2 or not ability_id:
            continue
        limited = [
            (r.get("Name") or "").strip()
            for r in group
            if (to_number(r.get("CooldownTurns"), "rule", "", "", Report()) or 0) > 0
            or (to_number(r.get("MaxUsesPerBattle"), "rule", "", "", Report()) or 0) > 0
        ]
        if limited:
            report.shared_cooldown.append((pattern_id, ability_id, skill_id, limited))


def check_condition_table(rows, owner_names, registered, report):
    check_row_names(rows, "condition", report)

    for row in rows:
        name = (row.get("Name") or "").strip()
        owner_type = (row.get("OwnerType") or "").strip()
        owner_id = (row.get("OwnerId") or "").strip()

        if owner_type not in OWNER_TYPES:
            report.bad_enums.append(("condition", name, "OwnerType", owner_type,
                                     " / ".join(sorted(OWNER_TYPES))))
        # 부모를 못 찾으면 조건이 통째로 사라져 그 규칙이 무조건 후보가 된다. 방향이 가장 나쁘다
        elif owner_id not in owner_names.get(owner_type, set()):
            report.orphan_conditions.append((name, owner_type, owner_id))

        condition_type = normalize_enum(row.get("Type") or "", report, "condition", name)
        if condition_type not in CONDITION_TYPES:
            report.bad_enums.append(("condition", name, "Type", condition_type,
                                     " / ".join(sorted(CONDITION_TYPES))))
            check_bool(row.get("bInvert"), "condition", name, "bInvert", report)
            continue

        check_bool(row.get("bInvert"), "condition", name, "bInvert", report)

        raw_value = row.get("Value")
        raw_tag = row.get("Tag")

        if condition_type in HP_RATIO_TYPES:
            value = to_number(raw_value, "condition", name, "Value", report)
            if value is not None and not 0.0 <= value <= 1.0:
                report.hp_ratio_out_of_range.append((name, condition_type, raw_value))
            if not is_empty(raw_tag):
                report.tag_on_numeric_condition.append((name, condition_type, raw_tag))

        elif condition_type in TURN_TYPES:
            value = to_number(raw_value, "condition", name, "Value", report)
            if value is not None and (value != int(value) or value < 1):
                report.turn_not_integer.append((name, condition_type, raw_value))
            if not is_empty(raw_tag):
                report.tag_on_numeric_condition.append((name, condition_type, raw_tag))

        else:
            # 태그 조건은 Value 를 보지 않는다. 비어 있어야 읽는 사람이 헷갈리지 않는다
            if is_empty(raw_tag):
                report.missing_condition_tag.append((name, condition_type))
            else:
                check_tag(raw_tag, "condition", name, "Tag", registered, report)


def check_phase_table(rows, pattern_ids, registered, report):
    check_row_names(rows, "phase", report)

    by_pattern = {}
    for row in rows:
        name = (row.get("Name") or "").strip()
        pattern_id = (row.get("PatternId") or "").strip()

        if pattern_id and pattern_id not in pattern_ids:
            report.orphan_rules.append((name, pattern_id))

        check_int_min(row.get("PhaseIndex"), 0, "phase", name, "PhaseIndex", report)
        check_tag(row.get("PhaseTag"), "phase", name, "PhaseTag", registered, report)

        # 비우면 그 턴도 규칙에서 고르므로 필수가 아니다
        if not is_empty(row.get("OnEnterAbilityId")):
            check_tag(row.get("OnEnterAbilityId"), "phase", name,
                      "OnEnterAbilityId", registered, report)

        by_pattern.setdefault(pattern_id, []).append(row)

    for pattern_id, group in by_pattern.items():
        seen = {}
        for row in group:
            index = (row.get("PhaseIndex") or "").strip()
            if index in seen:
                report.duplicate_phase_index.append((pattern_id, index))
            seen[index] = row

    return by_pattern


def check_first_phase(phase_rows_by_pattern, condition_rows, report):
    """첫 페이즈에 진입 조건이 있으면 실패 시 페이즈가 INDEX_NONE 이 된다.

    그러면 CurrentPhaseTag 가 비어서 페이즈 태그 없는 규칙만 후보가 된다. 구조체 주석이
    "목록의 첫 페이즈를 기본 페이즈로 두는 데 사용" 이라고 한 것이 이 뜻이다.
    """
    owners_with_conditions = {
        (row.get("OwnerId") or "").strip()
        for row in condition_rows
        if (row.get("OwnerType") or "").strip() == "Phase"
    }

    for pattern_id, group in phase_rows_by_pattern.items():
        def sort_key(row):
            try:
                return int(float((row.get("PhaseIndex") or "0").strip()))
            except ValueError:
                return 0

        first = sorted(group, key=sort_key)[0]
        name = (first.get("Name") or "").strip()
        if name in owners_with_conditions:
            report.first_phase_has_conditions.append((pattern_id, name))


def collect_blocking(report):
    """임포트는 되지만 규칙이 조용히 무동작·오동작이 되는 문제들을 모은다."""
    blocking = []

    for label, items in (
        ("표 읽기 실패·컬럼 누락", report.missing_columns),
        ("행 이름 중복", report.duplicate_names),
        ("행 이름 비어 있음", report.empty_names),
        ("enum 으로 읽을 수 없는 값", report.bad_enums),
        ("bool 로 읽을 수 없는 값", report.bad_bools),
        ("수치 오류", report.bad_numbers),
        ("체력 비율이 0.0~1.0 밖", report.hp_ratio_out_of_range),
        ("턴 수가 정수가 아님", report.turn_not_integer),
        ("수치 조건에 태그가 채워짐", report.tag_on_numeric_condition),
        ("태그 조건에 태그가 없음", report.missing_condition_tag),
        ("등록되지 않은 태그", report.unknown_tags),
        ("부모를 찾을 수 없는 조건", report.orphan_conditions),
        ("매핑되지 않은 패턴의 규칙·페이즈", report.orphan_rules),
        ("규칙이 하나도 없는 패턴", report.patterns_without_rules),
        ("페이즈에 없는 PhaseTag", report.unknown_phase_tags),
        ("같은 패턴에서 PhaseIndex 중복", report.duplicate_phase_index),
        ("NaiaTable 에 없는 NaiaId", report.unknown_naia_ids),
        ("NaiaId 가 Name 과 다름", report.naia_id_mismatch),
        ("액티브 · 보스 스킬 표에 없는 SkillId", report.unknown_skill_ids),
        ("SkillId 와 AbilityId 조합 오류", report.skill_ability_mismatch),
        ("패링 오버라이드 값 범위 오류", report.bad_parry_override),
    ):
        if items:
            blocking.append(f"{label} {len(items)}건")

    return blocking


def print_report(counts, report):
    for kind, count in counts.items():
        print(f"[읽음] {TABLE_FILES[kind]} {count}행")

    if report.missing_columns:
        print(f"\n[실패] 표를 검사할 수 없음 {len(report.missing_columns)}건")
        for kind, message in report.missing_columns:
            print(f"       {TABLE_FILES[kind]}: {message}")

    if report.empty_names:
        print(f"\n[실패] 행 이름이 비어 있음 {len(report.empty_names)}건")
        for kind, line in report.empty_names:
            print(f"       {TABLE_FILES[kind]} {line}행")

    if report.duplicate_names:
        print(f"\n[실패] 행 이름 중복 {len(report.duplicate_names)}건 — 뒤 행이 앞 행을 조용히 덮는다")
        for kind, name in report.duplicate_names:
            print(f"       {TABLE_FILES[kind]}: {name}")

    if report.hp_ratio_out_of_range:
        print(f"\n[실패] 체력 비율이 0.0~1.0 밖 {len(report.hp_ratio_out_of_range)}건")
        print("       퍼센트로 적은 것일 가능성이 높다 (50 -> 0.5). 그대로 두면 만피부터 발동한다")
        for name, condition_type, value in report.hp_ratio_out_of_range:
            print(f"       {name}: {condition_type} Value = {value}")

    if report.turn_not_integer:
        print(f"\n[실패] 턴 수가 1 이상의 정수가 아님 {len(report.turn_not_integer)}건")
        print("       셀렉터가 RoundToInt 로 반올림하므로 의도가 조용히 바뀐다")
        for name, condition_type, value in report.turn_not_integer:
            print(f"       {name}: {condition_type} Value = {value}")

    if report.tag_on_numeric_condition:
        print(f"\n[실패] 수치 조건에 Tag 가 채워짐 {len(report.tag_on_numeric_condition)}건")
        print("       에셋에서는 EditCondition 이 감췄던 칸이다. 조건 종류를 잘못 골랐을 수 있다")
        for name, condition_type, value in report.tag_on_numeric_condition:
            print(f"       {name}: {condition_type} Tag = {value}")

    if report.missing_condition_tag:
        print(f"\n[실패] 태그 조건에 Tag 가 없음 {len(report.missing_condition_tag)}건 — 조건이 무조건 실패한다")
        for name, condition_type in report.missing_condition_tag:
            print(f"       {name}: {condition_type}")

    if report.unknown_tags:
        print(f"\n[실패] 등록되지 않은 태그 {len(report.unknown_tags)}건")
        print("       임포트 뒤 빈 태그가 되고 그 규칙은 영원히 후보에서 빠진다")
        for kind, row_name, column, tag in report.unknown_tags:
            print(f"       {TABLE_FILES[kind]} {row_name}: {column} = {tag}")

    if report.orphan_conditions:
        print(f"\n[실패] 부모를 찾을 수 없는 조건 {len(report.orphan_conditions)}건")
        print("       조건이 통째로 사라져 그 규칙이 무조건 후보가 된다")
        for name, owner_type, owner_id in report.orphan_conditions:
            print(f"       {name}: OwnerType={owner_type} OwnerId={owner_id}")

    if report.orphan_rules:
        print(f"\n[실패] 매핑 표가 가리키지 않는 패턴 {len(report.orphan_rules)}건 — 아무도 쓰지 않는다")
        for name, pattern_id in report.orphan_rules:
            print(f"       {name}: PatternId = {pattern_id}")

    if report.patterns_without_rules:
        print(f"\n[실패] 규칙이 하나도 없는 패턴 {len(report.patterns_without_rules)}건")
        print("       항상 대체 행동만 나온다. 매핑에서 빼거나 규칙을 넣어야 한다")
        for pattern_id, naia_ids in report.patterns_without_rules:
            print(f"       {pattern_id} (NaiaId: {', '.join(naia_ids)})")

    if report.unknown_phase_tags:
        print(f"\n[실패] 그 패턴의 페이즈에 없는 PhaseTag {len(report.unknown_phase_tags)}건")
        for name, pattern_id, phase_tag in report.unknown_phase_tags:
            print(f"       {name}: {pattern_id} 에 {phase_tag} 페이즈가 없다")

    if report.duplicate_phase_index:
        print(f"\n[실패] 같은 패턴에서 PhaseIndex 중복 {len(report.duplicate_phase_index)}건")
        print("       페이즈 순서가 의미이므로 인덱스가 겹치면 재현이 안 된다")
        for pattern_id, index in report.duplicate_phase_index:
            print(f"       {pattern_id}: PhaseIndex = {index}")

    if report.bad_enums:
        print(f"\n[실패] enum 으로 읽을 수 없는 값 {len(report.bad_enums)}건")
        for kind, row_name, column, value, expected in report.bad_enums:
            print(f"       {TABLE_FILES[kind]} {row_name}: {column} = '{value}'")
            print(f"       {expected} 중 하나여야 한다")

    if report.bad_bools:
        print(f"\n[실패] bool 로 읽을 수 없는 값 {len(report.bad_bools)}건 — 오타일 가능성이 높다")
        for kind, row_name, column, value in report.bad_bools:
            print(f"       {TABLE_FILES[kind]} {row_name}: {column} = '{value}' (TRUE 또는 FALSE)")

    if report.bad_numbers:
        print(f"\n[실패] 수치 오류 {len(report.bad_numbers)}건")
        print("       에셋에서는 ClampMin 이 막던 것이다. CSV 임포트는 검사하지 않는다")
        for kind, row_name, column, value in report.bad_numbers:
            print(f"       {TABLE_FILES[kind]} {row_name}: {column} = {value}")

    if report.unknown_naia_ids:
        print(f"\n[실패] NaiaTable 에 없는 NaiaId {len(report.unknown_naia_ids)}건")
        print("       매핑의 Name 은 DT_NaiaDefinition 의 RowName 이어야 한다")
        print(f"       {', '.join(report.unknown_naia_ids)}")

    if report.naia_id_mismatch:
        print(f"\n[실패] NaiaId 가 Name 과 다름 {len(report.naia_id_mismatch)}건")
        print("       CSV 를 직접 읽는 서버와 RowName 으로 찾는 UE 가 서로 다른 나이아를 가리키게 된다")
        for name, naia_id in report.naia_id_mismatch:
            print(f"       Name={name}, NaiaId={naia_id}")

    if report.unknown_skill_ids:
        print(f"\n[실패] ActiveSkillTable 에 없는 SkillId {len(report.unknown_skill_ids)}건")
        for row_name, skill_id in report.unknown_skill_ids:
            print(f"       {row_name}: SkillId={skill_id}")

    if report.skill_ability_mismatch:
        print(f"\n[실패] SkillId 와 AbilityId 조합 오류 {len(report.skill_ability_mismatch)}건")
        print("       실행 시점에 이벤트 태그와 스킬 데이터가 서로 다른 행동을 가리킨다")
        for row_name, skill_id, ability_id, why in report.skill_ability_mismatch:
            print(f"       {row_name}: SkillId={skill_id}, AbilityId={ability_id} — {why}")

    if report.bad_parry_override:
        print(f"\n[실패] 패링 오버라이드 값 범위 오류 {len(report.bad_parry_override)}건")
        print("       bOverrideParryTiming=TRUE 인데 값이 비정상이면 잘못된 반응 창이 열린다")
        for row_name, column, value, why in report.bad_parry_override:
            print(f"       {row_name}: {column} = {value} — {why}")

    if report.prefixed_enums:
        print(f"\n[경고] enum 에 접두어가 붙음 {len(report.prefixed_enums)}건 — 짧은 이름을 쓰는 것이 다른 표와 같은 규약이다")
        for kind, row_name, value in report.prefixed_enums:
            print(f"       {TABLE_FILES[kind]} {row_name}: {value}")

    if report.zero_weight:
        print(f"\n[경고] Weight 가 0 인 규칙 {len(report.zero_weight)}건 — 뽑히지 않는다 (의도적 비활성일 수 있음)")
        print(f"       {', '.join(report.zero_weight)}")

    if report.empty_fallback:
        print(f"\n[경고] 대체 행동이 빈 매핑 {len(report.empty_fallback)}건 — 인카운터가 기본 공격으로 채운다")
        print(f"       {', '.join(report.empty_fallback)}")

    if report.first_phase_has_conditions:
        print(f"\n[경고] 첫 페이즈에 진입 조건이 있음 {len(report.first_phase_has_conditions)}건")
        print("       조건이 실패하면 페이즈가 정해지지 않아 페이즈 태그 없는 규칙만 후보가 된다")
        for pattern_id, name in report.first_phase_has_conditions:
            print(f"       {pattern_id}: {name}")

    if report.shared_cooldown:
        print(f"\n[경고] 같은 스킬을 쓰는 규칙이 쿨타임·사용 횟수를 공유함 {len(report.shared_cooldown)}건")
        print("       쿨타임은 규칙이 아니라 어빌리티+스킬 단위로 센다. 한쪽이 쓰면 다른 쪽도 막힌다")
        for pattern_id, ability_id, skill_id, names in report.shared_cooldown:
            print(f"       {pattern_id} / {ability_id} / SkillId={skill_id}: {', '.join(names)}")

    if report.parry_values_without_override:
        print(f"\n[경고] bOverrideParryTiming=FALSE 인데 값이 남아 있음 {len(report.parry_values_without_override)}건")
        print("       읽히진 않지만 나중에 켰다고 착각하기 쉽다")
        print(f"       {', '.join(report.parry_values_without_override)}")

    if report.ignored_columns:
        print(f"\n[경고] 검사하지 않은 컬럼 {len(report.ignored_columns)}건 — DataTable 임포트도 무시한다")
        for kind, column in report.ignored_columns:
            print(f"       {TABLE_FILES[kind]}: {column}")

    if report.tag_check_skipped:
        print("\n[확인] 태그 등록 검사를 건너뛰었다 (NUGameplayTags.cpp 를 읽지 못했다)")

    if report.naia_check_skipped:
        print("\n[확인] NaiaId 교차 검증을 건너뛰었다 (NaiaTable.csv 를 읽지 못했다)")

    if report.skill_check_skipped:
        print("\n[확인] SkillId 교차 검증을 건너뛰었다 (ActiveSkillTable.csv 를 읽지 못했다)")


def check(data_dir, tags_cpp_path, naia_csv_path, skill_csv_path, skip_tag_check, boss_skill_csv_path=None):
    report = Report()

    tables = {}
    for kind, file_name in TABLE_FILES.items():
        tables[kind] = read_table(data_dir / file_name, kind, report)

    counts = {kind: len(rows) for kind, rows in tables.items() if rows is not None}

    if any(rows is None for rows in tables.values()):
        print_report(counts, report)
        print("\n[실패] 표를 읽지 못해 검사를 중단했다")
        return 1

    registered = None if skip_tag_check else read_registered_tags(tags_cpp_path)
    if registered is None:
        report.tag_check_skipped = not skip_tag_check

    naia_ids = read_naia_ids(naia_csv_path)
    if naia_ids is None:
        report.naia_check_skipped = True

    skill_ids = read_skill_ids(skill_csv_path)
    if skill_ids is None:
        report.skill_check_skipped = True
    elif boss_skill_csv_path is not None:
        # 보스 전용 스킬 표 — 행 구조체가 액티브와 같고 게임도 이어서 찾는다. 없는 프로젝트도 있어 못 읽으면 액티브만 본다
        boss_skill_ids = read_skill_ids(boss_skill_csv_path)
        if boss_skill_ids:
            skill_ids |= boss_skill_ids

    pattern_ids = {
        (row.get("PatternId") or "").strip()
        for row in tables["map"]
        if not is_empty(row.get("PatternId"))
    }

    check_map_table(tables["map"], registered, naia_ids, report)

    phase_rows_by_pattern = check_phase_table(tables["phase"], pattern_ids, registered, report)

    phase_tags_by_pattern = {
        pattern_id: {
            (row.get("PhaseTag") or "").strip()
            for row in group
            if not is_empty(row.get("PhaseTag"))
        }
        for pattern_id, group in phase_rows_by_pattern.items()
    }

    check_rule_table(tables["rule"], pattern_ids, phase_tags_by_pattern, registered, skill_ids, report)

    owner_names = {
        "Rule": {(row.get("Name") or "").strip() for row in tables["rule"]},
        "Phase": {(row.get("Name") or "").strip() for row in tables["phase"]},
    }
    check_condition_table(tables["condition"], owner_names, registered, report)

    check_first_phase(phase_rows_by_pattern, tables["condition"], report)

    # 규칙이 하나도 안 잡히는 패턴은 항상 대체 행동만 낸다
    rules_by_pattern = {}
    for row in tables["rule"]:
        rules_by_pattern.setdefault((row.get("PatternId") or "").strip(), []).append(row)

    for pattern_id in sorted(pattern_ids):
        if pattern_id not in rules_by_pattern:
            naia_ids_for_pattern = [
                (row.get("Name") or "").strip()
                for row in tables["map"]
                if (row.get("PatternId") or "").strip() == pattern_id
            ]
            report.patterns_without_rules.append((pattern_id, naia_ids_for_pattern))

    print_report(counts, report)

    blocking = collect_blocking(report)
    if blocking:
        print(f"\n[실패] 막는 문제 {len(blocking)}종")
        for item in blocking:
            print(f"       {item}")
        return 1

    print(f"\n[완료] 패턴 {len(pattern_ids)}개 · 규칙 {len(tables['rule'])}개 ·"
          f" 조건 {len(tables['condition'])}개 · 페이즈 {len(tables['phase'])}개, 막는 문제 없음")
    return 0


def main():
    # 한국어 Windows 콘솔에서도 리포트 출력이 실패하지 않게 한다.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    script_dir = Path(__file__).parent

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=script_dir / "Data",
        help="적 패턴 CSV 네 장이 있는 폴더 (기본: Scripts/Data)",
    )
    parser.add_argument(
        "--tags",
        type=Path,
        default=script_dir.parent / "Source" / "ProjectM" / "NUGAS" / "NUGameplayTags.cpp",
        help="태그 등록 검사에 사용할 NUGameplayTags.cpp 경로",
    )
    parser.add_argument(
        "--naia",
        type=Path,
        default=script_dir / "Data" / "NaiaTable.csv",
        help="NaiaId 교차 검증에 사용할 NaiaTable.csv 경로",
    )
    parser.add_argument(
        "--skills",
        type=Path,
        default=script_dir / "Data" / "ActiveSkillTable.csv",
        help="SkillId 교차 검증에 사용할 ActiveSkillTable.csv 경로",
    )
    parser.add_argument(
        "--boss-skills",
        type=Path,
        default=script_dir / "Data" / "BossSkillTable.csv",
        help="SkillId 교차 검증에 같이 쓸 보스 전용 스킬 표 BossSkillTable.csv 경로",
    )
    parser.add_argument(
        "--skip-tag-check",
        action="store_true",
        help="태그 등록 검사를 건너뛴다",
    )
    args = parser.parse_args()

    if not args.data_dir.is_dir():
        print(f"[실패] 폴더를 찾을 수 없다: {args.data_dir}", file=sys.stderr)
        return 1

    return check(args.data_dir, args.tags, args.naia, args.skills, args.skip_tag_check, args.boss_skills)


if __name__ == "__main__":
    sys.exit(main())
