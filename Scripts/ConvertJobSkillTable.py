"""기획 직업 스킬 테이블을 UE DataTable 임포트용 CSV로 변환한다.

기획 원본은 CSV 또는 xlsx로 받을 수 있다. 두 형식 모두 4행의 컬럼명을 기준으로 읽고,
6행부터 Skill_ID가 있는 행만 변환한다.

★ 한글 값을 영문 enum으로 바꾸지 않는다 - 단 TargetType 은 예외다.
   FNUJobSkillDefinition 의 대부분이 FName 이고, 성장 창이 SkillType 을 그대로 화면에 찍는다
   (NUPlayerSkillWindowWidget.cpp - FText::FromName). 영문으로 바꾸면 UI에 "Passive" 가 뜬다.
   그래서 이 변환기가 하는 일은 대체로 번역이 아니라 검증이다.

   TargetType 만 다른 이유는 두 가지다 - 화면에 찍히는 칸이 아니고, 코드가 FName 이 아니라
   ENUTargetPolicy 로 읽는다. 액티브 스킬 표도 같은 다섯 값을 여기서 바꿔 내보낸다
   (ConvertActiveSkillTable.py 의 TARGET_POLICY). 런타임에 옮기면 표기가 하나 바뀔 때마다
   코드를 고쳐야 하고, 모르는 값이 들어와도 임포트는 성공해 전투에서야 드러난다.

★ 계수와 그 기준은 이제 이 표에 없다. 기획이 Coefficient·CalcType·Resource·ResourceGain
   네 칸을 걷어내고 "직업 스킬 효과 테이블" 로 옮겼다 — 넷 다 스킬당 한 칸이었는데
   실제로는 줄마다 달랐다. CalcType 은 세 행이 "합연산+곱연산" 이라고 두 값을 한 칸에
   적고 있었고, ResourceGain 은 숫자와 "전량 소모" 가 섞여 있었다.

   ★ 직업별 계수 기준(드루이드만 최대 HP)을 코드가 들고 있던 것도 같이 없앴다.
     효과표의 AffectedStat 이 행마다 기준을 들고 오므로 직업으로 가를 이유가 없다 —
     실제로 한 직업 안에서 공격력·마법 공격력·최대 HP 가 섞인다.

   계수가 정수인지 보는 검사는 ConvertSkillEffectTables.py 로 옮겼다.

노드 보상 표(ProgressionNodeReward.csv)와 양방향으로 교차 검증한다.
어느 노드에서도 안 나오는 스킬, 없는 스킬을 가리키는 노드, SkillType과 RewardType의
불일치는 전부 "임포트는 되는데 조용히 안 나오는" 조건이라 막는다.

사용법:
    python Scripts/ConvertJobSkillTable.py "경로/데이터 테이블 종합본.xlsx"

출력: Scripts/Data/JobSkillDefinition.csv
"""

import argparse
import csv
import sys
from collections import Counter, defaultdict
from decimal import Decimal, InvalidOperation
from pathlib import Path


# 기획 문서 구조: 1~3행은 그룹 헤더, 4행은 컬럼명, 5행은 타입 표기, 6행부터 데이터
DEFAULT_SHEET = "직업 스킬 테이블"

HEADER_ROW = 4
FIRST_DATA_ROW = 6

# 기획 컬럼 -> FNUJobSkillDefinition 프로퍼티. 이름이 같아도 순서를 고정하려고 다 적는다
#   kind: int / float / power / name / text
COLUMN_MAP = [
    ("Skill_ID", "Skill_ID", "int"),
    ("Skill_Key", "Skill_Key", "name"),
    ("Name_KR", "Name_KR", "text"),
    ("Class", "Class", "name"),
    ("Branch", "Branch", "name"),
    ("Tree", "Tree", "name"),
    ("NodeType", "NodeType", "name"),
    ("SkillType", "SkillType", "name"),
    ("Choice", "Choice", "name"),
    ("UnlockLv", "UnlockLv", "int"),
    ("MainStat", "MainStat", "name"),
    ("SP_Cost", "SP_Cost", "int"),
    ("AG_Cost", "AG_Cost", "int"),
    ("HP_Cost", "HP_Cost", "float"),
    ("CooldownTurns", "CooldownTurns", "int"),
    ("TargetType", "TargetPolicy", "target_policy"),
    ("TargetCount", "TargetCount", "int"),
    ("HitCount", "HitCount", "int"),
    ("DealContrib", "DealContrib", "float"),
    ("Description", "Description", "text"),
]

# 기획 컬럼명이 바뀔 수 있는 것들. 왼쪽이 이 스크립트가 쓰는 이름이다
COLUMN_ALIASES = {
    "Skill_ID": ("Skill_Id", "SkillID"),
    "Name_KR": ("Skill_KR", "이름"),
    "Description": ("Desc", "Desc_KR", "설명"),
}

# 직업별 ID 대역. 대역과 Class 가 어긋나면 막는다
#   새 직업이 생기면 여기에 한 줄을 더한다
CLASS_ID_BANDS = {
    "NaiaKnight": (3001, 3099),
    "Tracker": (3101, 3199),
    "Elementalist": (3201, 3299),
    "Gambler": (3301, 3399),
    "Druid": (3401, 3499),
}

# ★ 여기 적힌 이름은 JobDefinition.csv 의 JobKey 와 같아야 한다 —
#   목록이 두 벌이라 한쪽만 고쳐지면 조용히 갈린다. convert() 가 매번 맞춰본다

# 직업 스킬 전체가 들어갈 대역. 나이아 액티브(2001~2510)·패시브(28xx)와 겹치면 안 된다
JOB_SKILL_BAND = (3000, 3999)


# SkillType -> 노드 보상의 RewardType. 필살기도 액티브로 나간다
REWARD_TYPE_BY_SKILL_TYPE = {
    "액티브": "ActiveSkill",
    "필살기": "ActiveSkill",
    "패시브": "PassiveSkill",
    "시너지": "PassiveSkill",
}

# 처음 보는 값이면 알린다. 막지는 않는다 - 기획이 칸을 늘리는 중일 수 있다
KNOWN_VALUES = {
    "Class": set(CLASS_ID_BANDS),
    "NodeType": {"시작스킬", "전직", "스킬노드", "메인노드", "최종특성", "필살기"},
    "SkillType": set(REWARD_TYPE_BY_SKILL_TYPE),
    "Choice": {"", "A", "B", "A·B"},
    "MainStat": {"", "STR", "DEX", "INT", "LUK", "VIT"},
}

# 값이 비었다는 뜻으로 기획에서 사용하는 표기
EMPTY_MARKS = {"", "-", "none", "null"}


def is_empty(value):
    return value is None or str(value).strip().lower() in EMPTY_MARKS


class Report:
    """변환 중 사람이 확인해야 할 항목을 모은다."""

    def __init__(self):
        self.unknown_values = []
        self.job_key_mismatch = []
        self.job_key_check_skipped = False
        self.band_mismatch = []
        self.duplicate_rows = []
        self.orphan_skills = []
        self.unknown_reward_ids = []
        self.reward_type_mismatch = []
        self.reward_key_mismatch = []
        self.multi_granted = []
        self.id_collision = []
        self.ignored_columns = []
        self.reward_check_skipped = False
        self.collision_check_skipped = False


def read_job_keys(job_csv_path):
    """JobDefinition.csv 의 JobKey 를 읽는다. 직업 이름의 진짜 목록이다.

    ★ 직업 이름이 세 표에 각자 적혀 있다 — 성장 표(JobKey) · 직업 스킬 표(Class) ·
      자원 표(JobClass). 오타가 나면 게임에서야 드러난다(그 직업의 자원이 통째로
      안 쌓이는데 아무 소리도 안 난다). 그래서 임포트 전에 여기서 맞춰본다.

      enum 으로 바꾸지 않은 이유이기도 하다 — 직업은 늘 수 있고, UE 임포터는 모르는
      enum 값을 경고 한 줄 내고 기본값으로 넣어 오타의 증거를 지운다.
    """
    if not job_csv_path.exists():
        return None

    try:
        with job_csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
            keys = {(row.get("JobKey") or "").strip()
                    for row in csv.DictReader(handle)}
    except (OSError, csv.Error):
        return None

    keys.discard("")

    return keys or None


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


# TargetType -> ENUTargetPolicy. 액티브 스킬 표와 같은 열거형에 같은 표기를 쓴다
TARGET_POLICY = {
    "자신": "Self",
    "단일적": "SingleEnemy",
    "전체적": "AllEnemies",
    "단일아군": "SingleAlly",
    "전체아군": "AllAllies",
}


def to_target_policy(value):
    """대상 범위는 빈 칸도 모르는 값도 통과시키지 않는다.

    ENUTargetPolicy 에는 "미정" 이 없어서 빈 칸으로 흘리면 DT 가 기본값으로 채운다.
    전체 공격이 단일 공격으로 조용히 바뀌는 자리라 임포트가 아니라 여기서 막아야 한다.
    """
    key = "" if value is None else str(value).strip()
    if key in TARGET_POLICY:
        return TARGET_POLICY[key]
    known = " / ".join(TARGET_POLICY)
    raise ValueError(f"모르는 대상 범위: '{key}' — {known} 중 하나여야 한다")




def convert_cell(kind, value):
    if kind == "int":
        return to_int(value)
    if kind == "float":
        return to_float(value)
    if kind == "target_policy":
        return to_target_policy(value)
    if kind in ("name", "text"):
        # 기획이 "해당 없음" 으로 쓰는 표기는 빈 값으로 떨어뜨린다
        return "" if is_empty(value) else str(value).strip()
    raise ValueError(f"알 수 없는 변환 종류: {kind}")


def cell_of(source_row, header, column_name):
    index = header.get(column_name)
    if index is None or index >= len(source_row):
        return None
    return source_row[index]


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


def read_node_rewards(reward_csv_path):
    """성장 노드 보상에서 스킬 보상만 뽑는다. 못 읽으면 None."""
    try:
        with reward_csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
            rows = list(csv.DictReader(handle))
    except OSError:
        return None

    rewards = []
    for row in rows:
        reward_type = (row.get("RewardType") or "").strip()
        if reward_type not in ("ActiveSkill", "PassiveSkill"):
            continue
        raw_id = (row.get("RewardId") or "").strip()
        if not raw_id.isdigit():
            continue
        rewards.append(
            {
                "NodeId": (row.get("NodeId") or "").strip(),
                "RewardType": reward_type,
                "RewardId": int(raw_id),
                "RewardKey": (row.get("RewardKey") or "").strip(),
            }
        )
    return rewards or None


def read_existing_ids(csv_path, id_column):
    """다른 표의 ID 집합을 읽는다. 대역이 겹치는지 보려는 것이라 못 읽으면 None."""
    try:
        with csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
            rows = list(csv.DictReader(handle))
    except OSError:
        return None

    ids = set()
    for row in rows:
        raw = row.get(id_column)
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
    # Name 은 DataTable 의 행 이름이다. 기존 에셋이 Skill_Key 를 쓰고 있어 그대로 맞춘다
    return ["Name"] + [prop for _, prop, _ in COLUMN_MAP]


def write_csv(path, field_names, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    # UE CSV 임포터용 파일은 BOM 붙은 UTF-8과 CRLF로 기록한다 (다른 테이블과 동일)
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=field_names, lineterminator="\r\n")
        writer.writeheader()
        writer.writerows(rows)


def check_record(record, report):
    """행 하나가 조용히 잘못 들어가는 조건을 본다."""
    skill_id = int(record["Skill_ID"])
    job_class = record["Class"]
    skill_type = record["SkillType"]

    # 처음 보는 값 - 막지는 않되 매번 알린다
    for column, known in KNOWN_VALUES.items():
        value = record.get(column, "")
        if value not in known:
            report.unknown_values.append((skill_id, column, value))

    # ID 대역과 직업이 맞는지
    band = CLASS_ID_BANDS.get(job_class)
    if band is None:
        report.band_mismatch.append((skill_id, job_class, "대역이 정의되지 않은 직업"))
    elif not band[0] <= skill_id <= band[1]:
        report.band_mismatch.append((skill_id, job_class, f"{band[0]}~{band[1]} 밖"))


    # ★ 비용 검사는 여기서 사라졌다. 이 표만 봐서는 자원 비용을 못 본다 —
    #   3205 명상은 SP·HP 가 0이지만 효과표에 "마력 응축 -3" 줄이 있고,
    #   3203 나이아 개화는 "남은 SP 전량 소모" 라 전용 구현이 비용을 들고 있다.
    #   둘 다 멀쩡한데 비용이 없다고 잡히던 자리라 ConvertSkillEffectTables.py 의
    #   check_job_costs 로 옮겼다 — 거기서는 두 표를 같이 본다.


def cross_check_rewards(records, rewards, report):
    """노드 보상 표와 양방향으로 맞는지 본다.

    한쪽만 맞춰도 화면에는 아무 일이 안 일어나므로 둘 다 본다.
    - 노드가 없는 스킬은 영원히 못 얻는다
    - 없는 스킬을 가리키는 노드는 보상이 조용히 사라진다
    """
    by_id = {int(r["Skill_ID"]): r for r in records}
    granted = defaultdict(list)

    for reward in rewards:
        skill_id = reward["RewardId"]
        record = by_id.get(skill_id)
        if record is None:
            report.unknown_reward_ids.append((reward["NodeId"], skill_id))
            continue

        granted[skill_id].append(reward)

        expected = REWARD_TYPE_BY_SKILL_TYPE.get(record["SkillType"])
        if expected is not None and expected != reward["RewardType"]:
            report.reward_type_mismatch.append(
                (skill_id, record["SkillType"], reward["RewardType"], expected)
            )

        if reward["RewardKey"] and reward["RewardKey"] != record["Skill_Key"]:
            report.reward_key_mismatch.append(
                (skill_id, reward["RewardKey"], record["Skill_Key"])
            )

    for skill_id, record in by_id.items():
        if skill_id not in granted:
            report.orphan_skills.append((skill_id, record["Name_KR"], record["NodeType"]))
        elif len(granted[skill_id]) > 1:
            # 최종 노드 A·B 가 같은 필살기를 주는 경우가 실제로 있다. 막지 않고 알리기만 한다
            report.multi_granted.append(
                (skill_id, record["Name_KR"], len(granted[skill_id]), record["Choice"])
            )


def convert(source_path, csv_path, reward_csv_path, active_csv_path, passive_csv_path,
            job_csv_path, allow_incomplete, sheet_name=DEFAULT_SHEET):
    try:
        source_rows = read_source_rows(source_path, sheet_name)
        header = make_header(source_rows)
    except (OSError, ValueError, RuntimeError) as error:
        sys.stdout.flush()
        print(f"[실패] 입력 파일을 읽을 수 없다: {error}", file=sys.stderr)
        return 1

    missing = sorted({sheet_col for sheet_col, _, _ in COLUMN_MAP if sheet_col not in header})
    if missing:
        print(f"[실패] 기획 문서에 없는 컬럼: {', '.join(missing)}", file=sys.stderr)
        print(
            "       기획 컬럼명이 바뀌었다면 COLUMN_MAP 또는 COLUMN_ALIASES를 갱신해야 한다.",
            file=sys.stderr,
        )
        return 1

    report = Report()

    # 직업 이름의 진짜 목록과 코드가 든 대역표를 맞춰본다
    # 직업이 늘었는데 대역표가 낡으면 그 직업 스킬의 ID 검사가 통째로 빠진다
    job_keys = read_job_keys(job_csv_path)
    report.job_key_check_skipped = job_keys is None

    if job_keys is not None:
        for name in sorted(job_keys - set(CLASS_ID_BANDS)):
            report.job_key_mismatch.append((name, "직업 표에 있는데 대역표에 없다"))

        for name in sorted(set(CLASS_ID_BANDS) - job_keys):
            report.job_key_mismatch.append((name, "대역표에 있는데 직업 표에 없다"))
    known = {sheet_col for sheet_col, _, _ in COLUMN_MAP}
    report.ignored_columns = sorted(set(header.keys()) - known)

    records = []
    seen_ids = set()
    seen_keys = set()

    for row_index, source_row in enumerate(
        source_rows[FIRST_DATA_ROW - 1 :], start=FIRST_DATA_ROW
    ):
        raw_id = cell_of(source_row, header, "Skill_ID")
        if is_empty(raw_id):
            continue

        try:
            skill_id = to_int(raw_id)
        except ValueError as error:
            print(f"[실패] {row_index}행 Skill_ID: {error}", file=sys.stderr)
            return 1

        if skill_id in seen_ids:
            report.duplicate_rows.append((skill_id, "Skill_ID"))
            continue
        seen_ids.add(skill_id)

        record = {}
        for sheet_col, prop, kind in COLUMN_MAP:
            raw = cell_of(source_row, header, sheet_col)
            try:
                record[prop] = convert_cell(kind, raw)
            except ValueError as error:
                print(f"[실패] {row_index}행 {sheet_col}: {error}", file=sys.stderr)
                return 1


        key = record["Skill_Key"]
        if key:
            if key in seen_keys:
                report.duplicate_rows.append((skill_id, f"Skill_Key '{key}'"))
            seen_keys.add(key)

        # 행 이름은 기존 에셋과 같게 Skill_Key 를 쓴다. 비어 있으면 ID 로 떨어뜨린다
        record["Name"] = key or skill_id

        check_record(record, report)
        records.append(record)

    records.sort(key=lambda row: int(row["Skill_ID"]))

    # 다른 표와 ID 가 겹치면 노드 보상의 RewardId 가 어느 표를 가리키는지 알 수 없어진다
    other_ids = set()
    active_ids = read_existing_ids(active_csv_path, "SkillId")
    passive_ids = read_existing_ids(passive_csv_path, "PassiveId")
    if active_ids is None and passive_ids is None:
        report.collision_check_skipped = True
    else:
        other_ids = (active_ids or set()) | (passive_ids or set())

    for record in records:
        skill_id = int(record["Skill_ID"])
        if skill_id in other_ids:
            report.id_collision.append((skill_id, record["Name_KR"]))
        if not JOB_SKILL_BAND[0] <= skill_id <= JOB_SKILL_BAND[1]:
            report.band_mismatch.append(
                (
                    skill_id,
                    record["Class"],
                    f"직업 스킬 대역 {JOB_SKILL_BAND[0]}~{JOB_SKILL_BAND[1]} 밖",
                )
            )

    rewards = read_node_rewards(reward_csv_path)
    if rewards is None:
        report.reward_check_skipped = True
    else:
        cross_check_rewards(records, rewards, report)

    # 아래는 임포트는 되는데 조용히 틀리게 만든다
    blocking = []
    if report.band_mismatch:
        blocking.append(f"ID 대역 불일치 {len(report.band_mismatch)}건")
    if report.job_key_mismatch:
        blocking.append(f"직업 이름 불일치 {len(report.job_key_mismatch)}건")
    if report.duplicate_rows:
        blocking.append(f"중복 행 {len(report.duplicate_rows)}건")
    if report.id_collision:
        blocking.append(f"다른 표와 ID 충돌 {len(report.id_collision)}건")
    if report.orphan_skills:
        blocking.append(f"노드에서 안 나오는 스킬 {len(report.orphan_skills)}건")
    if report.unknown_reward_ids:
        blocking.append(f"없는 스킬을 가리키는 노드 {len(report.unknown_reward_ids)}건")
    if report.reward_type_mismatch:
        blocking.append(f"보상 종류 불일치 {len(report.reward_type_mismatch)}건")
    if report.reward_key_mismatch:
        blocking.append(f"보상 키 불일치 {len(report.reward_key_mismatch)}건")

    if blocking and not allow_incomplete:
        print_report(records, csv_path, report, written=False)
        sys.stdout.flush()
        print(
            f"\n[실패] {' / '.join(blocking)} — CSV를 쓰지 않았다."
            " 채우는 중이라면 --allow-incomplete 를 붙인다.",
            file=sys.stderr,
        )
        return 1

    write_csv(csv_path, output_properties(), records)
    print_report(records, csv_path, report, written=True)
    return 0


# 리포트 한 항목에 찍을 최대 줄 수. 시트를 반만 채운 상태로 돌리면 수백 건이 나온다
REPORT_LINE_LIMIT = 15


def print_lines(items, formatter):
    for item in items[:REPORT_LINE_LIMIT]:
        print(f"       {formatter(item)}")
    remaining = len(items) - REPORT_LINE_LIMIT
    if remaining > 0:
        print(f"       ... 외 {remaining}건")


def print_report(rows, csv_path, report, written):
    if written:
        print(f"[완료] 직업 스킬 {len(rows)}행 -> {csv_path.resolve()}")

        by_class = Counter(r["Class"] for r in rows)
        by_node = Counter(r["NodeType"] for r in rows)
        print("       직업별:   " + ", ".join(f"{k}={v}" for k, v in by_class.most_common()))
        print("       노드별:   " + ", ".join(f"{k}={v}" for k, v in by_node.most_common()))

    if report.band_mismatch:
        print(f"\n[실패] ID 대역과 직업이 어긋난 행 {len(report.band_mismatch)}건")
        print("       대역이 어긋나면 어느 직업의 스킬인지 ID 로 판별할 수 없다")
        print_lines(report.band_mismatch, lambda x: f"{x[0]} ({x[1]}): {x[2]}")

    if report.duplicate_rows:
        print(f"\n[실패] 중복된 행 {len(report.duplicate_rows)}건")
        print_lines(report.duplicate_rows, lambda x: f"{x[0]}: {x[1]}")

    if report.id_collision:
        print(f"\n[실패] 나이아 액티브·패시브 표와 ID 가 겹치는 행 {len(report.id_collision)}건")
        print("       노드 보상의 RewardId 가 어느 표를 가리키는지 알 수 없어진다")
        print_lines(report.id_collision, lambda x: f"{x[0]} {x[1]}")

    if report.orphan_skills:
        print(f"\n[실패] 어느 노드에서도 나오지 않는 스킬 {len(report.orphan_skills)}건")
        print("       표에는 있지만 플레이어가 영원히 얻을 수 없다")
        print_lines(report.orphan_skills, lambda x: f"{x[0]} {x[1]} ({x[2]})")

    if report.unknown_reward_ids:
        print(f"\n[실패] 없는 스킬을 가리키는 노드 보상 {len(report.unknown_reward_ids)}건")
        print_lines(report.unknown_reward_ids, lambda x: f"노드 {x[0]} -> 스킬 {x[1]}")

    if report.reward_type_mismatch:
        print(f"\n[실패] SkillType 과 노드 보상 종류가 다름 {len(report.reward_type_mismatch)}건")
        print("       보상 종류가 틀리면 보유 목록의 액티브·패시브 칸이 뒤바뀐다")
        print_lines(
            report.reward_type_mismatch,
            lambda x: f"{x[0]}: {x[1]} 인데 보상이 {x[2]} 다 ({x[3]} 여야 한다)",
        )

    if report.reward_key_mismatch:
        print(f"\n[실패] 노드 보상의 RewardKey 가 Skill_Key 와 다름 {len(report.reward_key_mismatch)}건")
        print_lines(report.reward_key_mismatch, lambda x: f"{x[0]}: 보상 '{x[1]}' vs 스킬 '{x[2]}'")

    if report.job_key_mismatch:
        print(f"\n[실패] 직업 이름이 JobDefinition.csv 와 다른 것 {len(report.job_key_mismatch)}건")
        print("       CLASS_ID_BANDS 를 직업 표에 맞춰야 한다 (ID 대역도 같이 정해야 한다)")
        print_lines(report.job_key_mismatch, lambda x: f"{x[0]}: {x[1]}")

    if report.job_key_check_skipped:
        print("\n[확인] 직업 이름 대조를 건너뛰었다 (JobDefinition.csv 를 못 읽었다)")

    if report.unknown_values:
        print(f"\n[경고] 처음 보는 값 {len(report.unknown_values)}건 — 그대로 내보냈다")
        print("       기획이 칸을 늘린 것이면 KNOWN_VALUES 에 더한다")
        print_lines(report.unknown_values, lambda x: f"{x[0]}: {x[1]} = '{x[2]}'")

    if report.multi_granted:
        print(f"\n[확인] 노드 두 곳 이상에서 나오는 스킬 {len(report.multi_granted)}건")
        print("       최종 노드 A·B 공통 필살기가 여기 해당한다")
        print_lines(
            report.multi_granted, lambda x: f"{x[0]} {x[1]}: {x[2]}곳 (Choice={x[3] or '없음'})"
        )

    if report.reward_check_skipped:
        print("\n[확인] 노드 보상 교차 검증을 건너뛰었다 (보상 CSV를 못 읽었다)")

    if report.collision_check_skipped:
        print("[확인] ID 충돌 검사를 건너뛰었다 (액티브·패시브 CSV를 못 읽었다)")

    if report.ignored_columns:
        print(f"\n[확인] 시트에 있지만 읽지 않는 컬럼: {', '.join(report.ignored_columns)}")


def main():
    # 한국어 Windows 콘솔에서도 리포트 출력이 실패하지 않게 한다.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    script_dir = Path(__file__).parent

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="기획 직업 스킬 CSV 또는 xlsx 경로")
    parser.add_argument(
        "--out",
        type=Path,
        default=script_dir / "Data" / "JobSkillDefinition.csv",
        help="직업 스킬 CSV 출력 경로 (기본: Scripts/Data/JobSkillDefinition.csv)",
    )
    parser.add_argument(
        "--reward-csv",
        type=Path,
        default=script_dir / "Data" / "CharacterProgression" / "ProgressionNodeReward.csv",
        help="교차 검증에 쓸 성장 노드 보상 CSV 경로",
    )
    parser.add_argument(
        "--job-csv",
        type=Path,
        default=script_dir / "Data" / "CharacterProgression" / "JobDefinition.csv",
        help="직업 이름(JobKey)의 진짜 목록으로 쓸 직업 정의 CSV 경로",
    )
    parser.add_argument(
        "--active-csv",
        type=Path,
        default=script_dir / "Data" / "ActiveSkillTable.csv",
        help="ID 충돌 검사에 쓸 나이아 액티브 스킬 CSV 경로",
    )
    parser.add_argument(
        "--passive-csv",
        type=Path,
        default=script_dir / "Data" / "PassiveTable.csv",
        help="ID 충돌 검사에 쓸 나이아 패시브 CSV 경로",
    )
    parser.add_argument(
        "--sheet",
        default=DEFAULT_SHEET,
        help=f"읽을 워크시트 이름 (기본: {DEFAULT_SHEET}). 종합본에서 시트를 고르는 데 쓴다",
    )
    parser.add_argument(
        "--allow-incomplete",
        action="store_true",
        help="검증에 걸려도 CSV를 쓴다 (데이터를 채우는 중일 때)",
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

    return convert(
        args.source,
        args.out,
        args.reward_csv,
        args.active_csv,
        args.passive_csv,
        args.job_csv,
        args.allow_incomplete,
        args.sheet,
    )


if __name__ == "__main__":
    sys.exit(main())
