"""기획 적 패턴 데이터 테이블을 UE DataTable 임포트용 CSV 네 장으로 변환한다.

기획 원본은 하나의 xlsx 안에 시트 다섯 개(정의·매핑·규칙·조건·페이즈)와 참조 스냅샷
두 개(참조_액티브스킬·참조_나이아)로 구성된다. 이 중 정의·참조 셋은 사람이 찾아보기 위한
탭이라 CSV로 내보내지 않는다 (`Docs/적 패턴 테이블 작성 가이드.md` 참고).

행 이름(Name) 규칙이 표마다 다르다.
    매핑    NaiaId 를 문자열로 그대로 씀 (다른 표들과 같은 관례)
    규칙·조건·페이즈  기획이 의미를 알 수 있게 손으로 적은 키를 그대로 씀
                      (예: P_SelfHealAtHalf_Heal). 손으로 저작하므로 변환기가
                      키를 새로 만들지 않는다 — 순번을 새로 매기면 조건의
                      OwnerId 참조가 전부 어긋난다

행동 식별은 AbilityId(태그) + SkillId(정수) 두 컬럼을 함께 쓴다.
    SkillId = 0   AbilityId 태그로 실행하는 행동 (기본공격 등)
    SkillId > 0   액티브 스킬 테이블의 그 행을 실행, AbilityId 는 공용 태그
                  (NU.Ability.Battle.ActiveSkill) 여야 함
둘의 조합이 어긋나면(SkillId>0인데 공용 태그가 아니거나, SkillId=0인데 공용 태그인 경우)
실행 시점에 이벤트 태그와 데이터가 서로 다른 행동을 가리키게 되므로 여기서 막는다.

SkillId 는 같은 워크북의 "참조_액티브스킬" 시트(0 = 태그 기반 행동 포함)로 교차검증한다.
PatternId 는 "적 패턴 정의" 시트 목록과 대조하되, 이건 기획 전용 문서라 안 맞아도 경고만 한다.

이 스크립트가 하는 일은 시트 -> CSV 추출과 셀 단위 검증까지다. Owner 참조가 실제로
존재하는지, 조건 없는 규칙이 의도인지 같은 표 사이 그래프 검증은 CheckEnemyPatternTable.py
가 하므로 변환이 끝나면 그 검사를 이어서 돌린다 (건너뛰려면 --skip-check).

사용법:
    python Scripts/ConvertEnemyPatternTable.py "경로/적 패턴 테이블.xlsx"

출력 (Scripts/Data/):
    EnemyPatternMapTable.csv
    EnemyPatternRuleTable.csv
    EnemyPatternConditionTable.csv
    EnemyPatternPhaseTable.csv
"""

import argparse
import csv
import importlib.util
import sys
from decimal import Decimal, InvalidOperation
from pathlib import Path


SHEET_DEFINITION = "적 패턴 정의"
SHEET_MAP = "적 패턴 매핑"
SHEET_RULE = "적 패턴 규칙"
SHEET_CONDITION = "적 패턴 조건"
SHEET_PHASE = "적 패턴 페이즈"
SHEET_SKILL_REF = "참조_액티브스킬"

# 기획 문서 구조: 1~3행은 제목·그룹 헤더, 4행은 컬럼명, 5행은 타입 표기, 6행부터 데이터
HEADER_ROW = 4
FIRST_DATA_ROW = 6

# 액티브 스킬을 실행할 때 쓰는 공용 아키타입 태그. NUGameplayTags.cpp 의
# NU::Ability::Battle::ActiveSkill 과 짝이다. Command.AbilityId 로 그대로 실려
# 게임플레이 이벤트를 트리거하므로, SkillId 유무와 이 태그가 어긋나면 안 된다.
ACTIVE_SKILL_ABILITY_TAG = "NU.Ability.Battle.ActiveSkill"

EMPTY_MARKS = {"", "-", "none", "null", "비움"}
BOOL_WORDS = {"true": "TRUE", "false": "FALSE"}

OWNER_TYPES = {"Rule", "Phase"}
CONDITION_TYPES = {
    "SelfHPRatioBelow",
    "SelfHPRatioAbove",
    "TurnNumberAfter",
    "TurnNumberBefore",
    "SelfHasTag",
    "AnyOpponentHasTag",
}

# 기획 컬럼 -> 출력 프로퍼티. 이름이 같아도 표마다 따로 두어 표 하나만 바뀌어도
# 다른 표의 COLUMN_MAP을 안 건드리게 한다.
MAP_COLUMNS = [
    ("NaiaId", "NaiaId", "int"),
    ("PatternId", "PatternId", "fname"),
    ("FallbackAbilityId", "FallbackAbilityId", "tag"),
]

# 없어도 되는 컬럼 — 시트에 없으면 기본값(0 · 빈 경로)으로 나간다
# 인정 임계치는 대련 보스에만 쓰므로 기획 시트가 컬럼을 안 만들어도 변환이 멈추면 안 된다
MAP_OPTIONAL_COLUMNS = [
    ("ConcedeHealthRatio", "ConcedeHealthRatio", "float"),
    ("ConcedeAnimation", "ConcedeAnimation", "path"),
    ("ConcedeLoopAnimation", "ConcedeLoopAnimation", "path"),
]

RULE_COLUMNS = [
    ("PatternId", "PatternId", "fname"),
    ("AbilityId", "AbilityId", "tag"),
    ("SkillId", "SkillId", "int"),
    ("Priority", "Priority", "int"),
    ("Weight", "Weight", "float"),
    ("CooldownTurns", "CooldownTurns", "int"),
    ("MaxUsesPerBattle", "MaxUsesPerBattle", "int"),
    ("PhaseTag", "PhaseTag", "tag"),
    ("bOverrideParryTiming", "bOverrideParryTiming", "bool"),
    ("ChallengeDuration", "ChallengeDuration", "float"),
    ("SuccessAnchor", "SuccessAnchor", "float"),
    ("SuccessWindow", "SuccessWindow", "float"),
]

CONDITION_COLUMNS = [
    ("OwnerType", "OwnerType", "owner_type"),
    ("OwnerId", "OwnerId", "fname"),
    ("Type", "Type", "condition_type"),
    ("Value", "Value", "float"),
    ("Tag", "Tag", "tag"),
    ("bInvert", "bInvert", "bool"),
]

PHASE_COLUMNS = [
    ("PatternId", "PatternId", "fname"),
    ("PhaseIndex", "PhaseIndex", "int"),
    ("PhaseTag", "PhaseTag", "tag"),
    ("OnEnterAbilityId", "OnEnterAbilityId", "tag"),
]


def is_empty(value):
    return value is None or str(value).strip().lower() in EMPTY_MARKS


class Report:
    """변환 중 사람이 확인해야 할 항목을 모은다. sheet 이름을 같이 담아 표를 구분한다."""

    def __init__(self):
        self.bad_bools = []
        self.unknown_enums = []
        self.skill_ability_mismatch = []
        self.unknown_skill_ids = []
        self.unknown_pattern_ids = []
        self.skill_ref_missing = False
        self.definition_missing = False


def to_int(value, label):
    if is_empty(value):
        return "0"
    try:
        number = Decimal(str(value).strip())
    except InvalidOperation as error:
        raise ValueError(f"{label}: 정수로 변환할 수 없는 값 '{value}'") from error
    if not number.is_finite() or number != number.to_integral_value():
        raise ValueError(f"{label}: 정수 컬럼에 정수가 아닌 값 '{value}'")
    return str(int(number))


def to_float(value, label):
    if is_empty(value):
        return "0"
    try:
        number = Decimal(str(value).strip())
    except InvalidOperation as error:
        raise ValueError(f"{label}: 실수로 변환할 수 없는 값 '{value}'") from error
    if not number.is_finite():
        raise ValueError(f"{label}: 유한한 실수가 아닌 값 '{value}'")
    if number == 0:
        return "0"
    return format(number.normalize(), "f")


def to_bool(value, row_name, prop, report):
    # 엑셀이 진짜 불리언 칸으로 저장한 것과 문자열 "True" 로 적은 것이 섞여 있다
    if isinstance(value, bool):
        return "TRUE" if value else "FALSE"

    if is_empty(value):
        return "FALSE"

    text = str(value).strip()
    word = BOOL_WORDS.get(text.lower())
    if word is None:
        report.bad_bools.append((row_name, prop, text))
        return "FALSE"
    return word


def to_enum(value, valid_values, row_name, prop, report):
    text = "" if value is None else str(value).strip()
    if text not in valid_values:
        report.unknown_enums.append((row_name, prop, text, sorted(valid_values)))
        return ""
    return text


def to_plain(value):
    return "" if is_empty(value) else str(value).strip()


def convert_cell(kind, value, row_name, prop, report):
    if kind == "int":
        return to_int(value, f"{row_name}.{prop}")
    if kind == "float":
        return to_float(value, f"{row_name}.{prop}")
    if kind == "bool":
        return to_bool(value, row_name, prop, report)
    if kind in ("fname", "tag", "path"):
        return to_plain(value)
    if kind == "owner_type":
        return to_enum(value, OWNER_TYPES, row_name, prop, report)
    if kind == "condition_type":
        return to_enum(value, CONDITION_TYPES, row_name, prop, report)
    raise ValueError(f"알 수 없는 변환 종류: {kind}")


def read_xlsx_sheet(workbook, sheet_name):
    for candidate in workbook.worksheets:
        if candidate.title.strip() == sheet_name.strip():
            return [list(row) for row in candidate.iter_rows(values_only=True)]
    return None


def make_header(rows, sheet_name):
    if len(rows) < HEADER_ROW:
        raise ValueError(f"'{sheet_name}' 시트에 {HEADER_ROW}행 헤더가 없다.")

    header = {}
    for column_index, value in enumerate(rows[HEADER_ROW - 1]):
        if is_empty(value):
            continue
        name = str(value).strip()
        if name in header:
            raise ValueError(
                f"'{sheet_name}' 시트에 중복된 컬럼명: {name}"
                f" ({HEADER_ROW}행 {header[name] + 1}번째 칸과 {column_index + 1}번째 칸)"
            )
        header[name] = column_index
    return header


def check_required_columns(header, columns, sheet_name):
    missing = sorted(
        {sheet_col for sheet_col, _, _ in columns if sheet_col not in header}
    )
    if missing:
        raise ValueError(f"'{sheet_name}' 시트에 없는 컬럼: {', '.join(missing)}")


def cell(source_row, header, column_name):
    index = header.get(column_name)
    if index is None or index >= len(source_row):
        return None
    return source_row[index]


def convert_map_sheet(rows, report):
    """NaiaId 로 행 이름을 만든다. 다른 표들의 'Name = ID 문자열' 관례와 같다."""
    header = make_header(rows, SHEET_MAP)
    check_required_columns(header, MAP_COLUMNS, SHEET_MAP)

    output_rows = []
    seen = set()

    for row_index, source_row in enumerate(rows[FIRST_DATA_ROW - 1 :], start=FIRST_DATA_ROW):
        raw_id = cell(source_row, header, "NaiaId")
        if is_empty(raw_id):
            continue

        naia_id = to_int(raw_id, f"{SHEET_MAP} {row_index}행 NaiaId")
        if naia_id in seen:
            raise ValueError(f"'{SHEET_MAP}' NaiaId 중복: {naia_id} ({row_index}행)")
        seen.add(naia_id)

        record = {"Name": naia_id}
        for sheet_col, prop, kind in MAP_COLUMNS + MAP_OPTIONAL_COLUMNS:
            value = cell(source_row, header, sheet_col)
            record[prop] = convert_cell(kind, value, naia_id, prop, report)

        output_rows.append(record)

    field_names = ["Name"] + [prop for _, prop, _ in MAP_COLUMNS + MAP_OPTIONAL_COLUMNS]
    return output_rows, field_names


def convert_named_sheet(rows, sheet_name, columns, report):
    """규칙·조건·페이즈 공용. Name 컬럼을 시트에서 그대로 읽는다 (기획이 손으로 적은 키)."""
    header = make_header(rows, sheet_name)
    if "Name" not in header:
        raise ValueError(f"'{sheet_name}' 시트에 Name 컬럼이 없다.")
    check_required_columns(header, columns, sheet_name)

    output_rows = []
    seen = set()

    for row_index, source_row in enumerate(rows[FIRST_DATA_ROW - 1 :], start=FIRST_DATA_ROW):
        raw_name = cell(source_row, header, "Name")
        if is_empty(raw_name):
            continue

        name = str(raw_name).strip()
        if name in seen:
            raise ValueError(f"'{sheet_name}' Name 중복: {name} ({row_index}행)")
        seen.add(name)

        record = {"Name": name}
        for sheet_col, prop, kind in columns:
            value = cell(source_row, header, sheet_col)
            record[prop] = convert_cell(kind, value, name, prop, report)

        output_rows.append(record)

    field_names = ["Name"] + [prop for _, prop, _ in columns]
    return output_rows, field_names


def check_rule_skill_ability_pairing(rule_rows, report):
    """SkillId 유무와 AbilityId 가 서로 다른 행동을 가리키면 실행 시점에 어긋난다."""
    for row in rule_rows:
        skill_id = int(row["SkillId"])
        ability_id = row["AbilityId"]

        if skill_id > 0 and ability_id != ACTIVE_SKILL_ABILITY_TAG:
            report.skill_ability_mismatch.append(
                (row["Name"], skill_id, ability_id, f"'{ACTIVE_SKILL_ABILITY_TAG}' 이어야 함")
            )
        elif skill_id == 0 and ability_id == ACTIVE_SKILL_ABILITY_TAG:
            report.skill_ability_mismatch.append(
                (row["Name"], skill_id, ability_id, "SkillId 가 0 이라 구체적인 태그가 필요함")
            )


def read_reference_skill_ids(workbook):
    """참조_액티브스킬 시트에서 유효한 SkillId 집합을 모은다. 0(태그 기반)도 포함된다."""
    rows = read_xlsx_sheet(workbook, SHEET_SKILL_REF)
    if rows is None:
        return None

    header = make_header(rows, SHEET_SKILL_REF)
    if "Active_ID" not in header:
        return None

    ids = set()
    for source_row in rows[FIRST_DATA_ROW - 1 :]:
        raw_id = cell(source_row, header, "Active_ID")
        if is_empty(raw_id):
            continue
        ids.add(int(to_int(raw_id, "참조_액티브스킬.Active_ID")))
    return ids


def read_definition_pattern_ids(workbook):
    """적 패턴 정의 시트에서 PatternId 목록을 모은다. 기획 전용 문서라 없어도 경고만 한다."""
    rows = read_xlsx_sheet(workbook, SHEET_DEFINITION)
    if rows is None:
        return None

    header = make_header(rows, SHEET_DEFINITION)
    if "PatternId" not in header:
        return None

    ids = set()
    for source_row in rows[FIRST_DATA_ROW - 1 :]:
        raw_id = cell(source_row, header, "PatternId")
        if not is_empty(raw_id):
            ids.add(str(raw_id).strip())
    return ids


def write_csv(path, field_names, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=field_names, lineterminator="\r\n")
        writer.writeheader()
        writer.writerows(rows)


def convert(source_path, out_dir):
    try:
        import openpyxl
    except ImportError as error:
        print(f"[실패] xlsx 입력에는 openpyxl이 필요하다: {error}", file=sys.stderr)
        return 1

    try:
        workbook = openpyxl.load_workbook(source_path, data_only=True, read_only=True)
    except OSError as error:
        print(f"[실패] 입력 파일을 열 수 없다: {error}", file=sys.stderr)
        return 1

    report = Report()

    try:
        map_rows_raw = read_xlsx_sheet(workbook, SHEET_MAP)
        rule_rows_raw = read_xlsx_sheet(workbook, SHEET_RULE)
        condition_rows_raw = read_xlsx_sheet(workbook, SHEET_CONDITION)
        phase_rows_raw = read_xlsx_sheet(workbook, SHEET_PHASE)

        for sheet_name, rows in (
            (SHEET_MAP, map_rows_raw),
            (SHEET_RULE, rule_rows_raw),
            (SHEET_CONDITION, condition_rows_raw),
            (SHEET_PHASE, phase_rows_raw),
        ):
            if rows is None:
                raise ValueError(f"'{sheet_name}' 시트를 찾을 수 없다.")

        map_out, map_fields = convert_map_sheet(map_rows_raw, report)
        rule_out, rule_fields = convert_named_sheet(rule_rows_raw, SHEET_RULE, RULE_COLUMNS, report)
        condition_out, condition_fields = convert_named_sheet(
            condition_rows_raw, SHEET_CONDITION, CONDITION_COLUMNS, report
        )
        phase_out, phase_fields = convert_named_sheet(phase_rows_raw, SHEET_PHASE, PHASE_COLUMNS, report)
    except ValueError as error:
        print(f"[실패] {error}", file=sys.stderr)
        return 1

    check_rule_skill_ability_pairing(rule_out, report)

    # SkillId 교차검증 — 참조 시트가 없으면 건너뛰고 알린다
    skill_ids = read_reference_skill_ids(workbook)
    if skill_ids is None:
        report.skill_ref_missing = True
    else:
        for row in rule_out:
            skill_id = int(row["SkillId"])
            if skill_id not in skill_ids:
                report.unknown_skill_ids.append((row["Name"], skill_id))

    # PatternId 교차검증 — 정의 탭은 기획 전용이라 경고만
    pattern_ids = read_definition_pattern_ids(workbook)
    if pattern_ids is None:
        report.definition_missing = True
    else:
        referenced = (
            {(row["PatternId"], "Map", row["Name"]) for row in map_out}
            | {(row["PatternId"], "Rule", row["Name"]) for row in rule_out}
            | {(row["PatternId"], "Phase", row["Name"]) for row in phase_out}
        )
        for pattern_id, source, row_name in sorted(referenced):
            if pattern_id and pattern_id not in pattern_ids:
                report.unknown_pattern_ids.append((row_name, source, pattern_id))

    out_dir.mkdir(parents=True, exist_ok=True)
    write_csv(out_dir / "EnemyPatternMapTable.csv", map_fields, map_out)
    write_csv(out_dir / "EnemyPatternRuleTable.csv", rule_fields, rule_out)
    write_csv(out_dir / "EnemyPatternConditionTable.csv", condition_fields, condition_out)
    write_csv(out_dir / "EnemyPatternPhaseTable.csv", phase_fields, phase_out)

    blocking = print_report(map_out, rule_out, condition_out, phase_out, out_dir, report)
    return 1 if blocking else 0


def print_report(map_rows, rule_rows, condition_rows, phase_rows, out_dir, report):
    print(f"[완료] 매핑 {len(map_rows)}행 -> {out_dir / 'EnemyPatternMapTable.csv'}")
    print(f"[완료] 규칙 {len(rule_rows)}행 -> {out_dir / 'EnemyPatternRuleTable.csv'}")
    print(f"[완료] 조건 {len(condition_rows)}행 -> {out_dir / 'EnemyPatternConditionTable.csv'}")
    print(f"[완료] 페이즈 {len(phase_rows)}행 -> {out_dir / 'EnemyPatternPhaseTable.csv'}")

    if report.bad_bools:
        print(f"\n[실패] bool 로 읽을 수 없는 값 {len(report.bad_bools)}건 — 오타일 가능성이 높다")
        for row_name, prop, value in report.bad_bools:
            print(f"       {row_name}: {prop} = '{value}' (True 또는 False 여야 한다)")

    if report.unknown_enums:
        print(f"\n[실패] enum 으로 읽을 수 없는 값 {len(report.unknown_enums)}건")
        for row_name, prop, value, valid in report.unknown_enums:
            print(f"       {row_name}: {prop} = '{value}' — {'/'.join(valid)} 중 하나여야 한다")

    if report.skill_ability_mismatch:
        print(f"\n[실패] SkillId 와 AbilityId 조합 오류 {len(report.skill_ability_mismatch)}건")
        print("       실행 시점에 이벤트 태그와 스킬 데이터가 서로 다른 행동을 가리킨다")
        for row_name, skill_id, ability_id, why in report.skill_ability_mismatch:
            print(f"       {row_name}: SkillId={skill_id}, AbilityId={ability_id} — {why}")

    if report.unknown_skill_ids:
        print(f"\n[실패] 참조_액티브스킬에 없는 SkillId {len(report.unknown_skill_ids)}건")
        for row_name, skill_id in report.unknown_skill_ids:
            print(f"       {row_name}: SkillId={skill_id}")

    if report.skill_ref_missing:
        print(f"\n[확인] '{SHEET_SKILL_REF}' 시트가 없어 SkillId 교차검증을 건너뛰었다")

    if report.unknown_pattern_ids:
        print(f"\n[경고] '{SHEET_DEFINITION}' 목록에 없는 PatternId {len(report.unknown_pattern_ids)}건")
        print("       기획 전용 문서라 막지는 않는다. 정의 탭에 추가를 누락했을 수 있다")
        for row_name, source, pattern_id in report.unknown_pattern_ids:
            print(f"       {source} {row_name}: PatternId={pattern_id}")

    if report.definition_missing:
        print(f"\n[확인] '{SHEET_DEFINITION}' 시트가 없어 PatternId 교차검증을 건너뛰었다")

    blocking = (
        len(report.bad_bools)
        + len(report.unknown_enums)
        + len(report.skill_ability_mismatch)
        + len(report.unknown_skill_ids)
    )
    if blocking:
        print(f"\n[실패] 막는 문제 {blocking}건 — CSV 는 썼지만 임포트 전에 고쳐야 한다")
    else:
        print("\n[완료] 셀 단위 검증 통과, 막는 문제 없음")

    return blocking


def run_checker(data_dir):
    """CheckEnemyPatternTable.py 를 이어서 돌린다 — 표 사이 그래프 검증은 그쪽 담당이다."""
    checker_path = Path(__file__).parent / "CheckEnemyPatternTable.py"
    if not checker_path.is_file():
        print(f"\n[확인] {checker_path.name} 을 찾을 수 없어 표 사이 교차검증을 건너뛰었다")
        return 0

    spec = importlib.util.spec_from_file_location("CheckEnemyPatternTable", checker_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    print("\n" + "=" * 60)
    print("표 사이 교차검증 (CheckEnemyPatternTable.py)")
    print("=" * 60)

    tags_cpp_path = Path(__file__).parent.parent / "Source" / "ProjectM" / "NUGAS" / "NUGameplayTags.cpp"
    naia_csv_path = data_dir / "NaiaTable.csv"
    skill_csv_path = data_dir / "ActiveSkillTable.csv"
    return module.check(data_dir, tags_cpp_path, naia_csv_path, skill_csv_path, False)


def main():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    script_dir = Path(__file__).parent

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path, help="기획 적 패턴 xlsx 경로")
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=script_dir / "Data",
        help="CSV 네 장을 쓸 폴더 (기본: Scripts/Data)",
    )
    parser.add_argument(
        "--skip-check",
        action="store_true",
        help="변환 뒤 CheckEnemyPatternTable.py 자동 실행을 건너뛴다",
    )
    args = parser.parse_args()

    if not args.source.is_file():
        print(f"[실패] 입력 파일을 찾을 수 없다: {args.source}", file=sys.stderr)
        return 1

    result = convert(args.source, args.out_dir)
    if result != 0 or args.skip_check:
        return result

    return run_checker(args.out_dir)


if __name__ == "__main__":
    sys.exit(main())
