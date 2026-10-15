"""기획 퀘스트·퀘스트 목표 시트(구글 스프레드시트)를 UE DataTable 임포트용 CSV로 변환한다.

왜 시트에서 굽나: 서버가 읽는 정본이 시트로 보인다(10/8 기준, 백엔드 확인 전). 저장소 CSV를 손으로
고치면 시트와 값이 갈린다 --- 10/8 대조에서 Q9~Q13 선행·레벨, 10214 보상, 목표 선행 순서가 갈려 있었다.

출력 양식은 지금 저장소 CSV가 기준이다: 컬럼 이름·순서는 기존 CSV 헤더를 그대로 쓰고
(시트 열 순서가 바뀌어도 된다), UTF-8 BOM·CRLF·필요할 때만 따옴표, bool은 퀘스트 TRUE/FALSE·목표 True/False,
배열은 (a,b), 빈 정수는 0, 목표 선행 None은 0으로 쓴다.

사용법:
    python Scripts/ConvertQuestTable.py --check        (시트를 받아 기존 CSV와 칸 단위로 대조만, 파일은 안 씀)
    python Scripts/ConvertQuestTable.py                (시트를 받아 CSV 두 개를 굽는다)
    python Scripts/ConvertQuestTable.py --quest-csv a.csv --objective-csv b.csv   (로컬에 받아 둔 시트 CSV)

출력: Scripts/Data/QuestTable.csv          (Row Type = NUQuestDefinitionRow -> DT_QuestDefinitionRow)
      Scripts/Data/QuestObjectiveTable.csv (Row Type = NUQuestObjectiveRow  -> DT_QuestObjective)
굽고 나면 두 DT를 CSV로 다시 채워야 게임에 반영된다.

시트에 없고 CSV에만 있는 행(클라에서 먼저 만든 퀘스트 등)이 있으면 굽지 않고 멈춘다. 시트에 먼저 올리거나,
지워도 되면 --drop-csv-only-rows 로 굽는다.
"""

import argparse
import csv
import io
import sys
import urllib.request
from decimal import Decimal, InvalidOperation
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DATA_DIR = SCRIPT_DIR / "Data"

QUEST_SHEET_URL = "https://docs.google.com/spreadsheets/d/1dEWj2b-LXa3qadVt1C7FeBRwkBMJ_eU-WTj8_yVMtS4/export?format=csv"
OBJECTIVE_SHEET_URL = "https://docs.google.com/spreadsheets/d/1OozBF-2tAIlPrK4E3M-XFoNevFhsSNOg25c2o4GYnGU/export?format=csv"

# 표마다: 키 컬럼, 정수 컬럼, bool 컬럼과 표기, 배열 컬럼, 값 목록을 지키는 enum 컬럼
TABLES = {
    "quest": {
        "label": "퀘스트",
        "url": QUEST_SHEET_URL,
        "out": DATA_DIR / "QuestTable.csv",
        "key": "Quest_ID",
        "ints": {"Quest_ID", "RequiredLevel", "PrereqQuest_ID", "QuestGiver_ID", "Location_ID", "Target_ID",
                 "TargetCount", "TurnInNPC_ID", "Reward_EXP", "Reward_Gold", "RewardItem1_ID", "RewardItem1Count",
                 "RewardItem2_ID", "RewardItem2Count"},
        "bools": {"bUseObjectiveTable", "bAutoAccept"},
        "bool_words": ("TRUE", "FALSE"),
        "arrays": {"AdditionalPrereqQuest_IDs"},
        "enums": {"RepeatType", "ClaimType"},
        "tags": {"QuestCategory": "NU.Quest.Category.", "QuestType": "NU.Quest.Objective."},
    },
    "objective": {
        "label": "퀘스트 목표",
        "url": OBJECTIVE_SHEET_URL,
        "out": DATA_DIR / "QuestObjectiveTable.csv",
        "key": "ObjectiveId",
        "ints": {"ObjectiveId", "QuestId", "TargetId", "RequiredCount", "PrerequisiteObjectiveId"},
        "bools": {"bOptional", "bHideInObjectiveList"},
        "bool_words": ("True", "False"),
        "arrays": set(),
        "enums": set(),
        "tags": {"ObjectiveType": "NU.Quest.Objective."},
    },
}

TRUE_WORDS = {"1", "true", "yes", "y", "o"}
FALSE_WORDS = {"", "0", "false", "no", "n", "x"}


class ConvertError(Exception):
    pass


def read_text(path_or_url):
    if path_or_url.startswith("http"):
        print(f"[받기] {path_or_url}")
        with urllib.request.urlopen(path_or_url, timeout=60) as response:
            return response.read().decode("utf-8-sig")
    return Path(path_or_url).read_text(encoding="utf-8-sig")


def to_int(value, row_key, column):
    text = (value or "").strip()
    if text == "" or text.lower() == "none":
        return "0"
    try:
        number = Decimal(text.replace(",", ""))
    except InvalidOperation:
        raise ConvertError(f"{row_key} {column}: 정수가 아니다 ({value!r})")
    if number != number.to_integral_value():
        raise ConvertError(f"{row_key} {column}: 소수가 들어 있다 ({value!r})")
    return str(int(number))


def to_bool(value, words, row_key, column):
    text = (value or "").strip().lower()
    if text in TRUE_WORDS:
        return words[0]
    if text in FALSE_WORDS:
        return words[1]
    raise ConvertError(f"{row_key} {column}: bool이 아니다 ({value!r})")


def to_array(value, row_key, column):
    text = (value or "").strip().strip("()").strip()
    if text == "":
        return ""
    items = [to_int(item, row_key, column) for item in text.split(",") if item.strip()]
    return "(" + ",".join(items) + ")"


def read_sheet(text, spec):
    """컬럼명 줄을 키 컬럼 이름으로 찾고, 바로 다음 타입 줄을 건너뛴 뒤 키가 있는 행만 읽는다."""
    rows = list(csv.reader(io.StringIO(text)))
    header_index = next((i for i, row in enumerate(rows) if spec["key"] in [c.strip() for c in row]), None)
    if header_index is None:
        raise ConvertError(f"{spec['label']} 시트에서 {spec['key']} 컬럼명 줄을 못 찾았다")

    columns = {}
    for index, name in enumerate(rows[header_index]):
        name = name.strip()
        if not name:
            continue
        if name in columns:
            raise ConvertError(f"{spec['label']} 시트에 {name} 컬럼이 두 번 있다")
        columns[name] = index

    data = []
    for row in rows[header_index + 2:]:
        cell = lambda name: row[columns[name]] if columns[name] < len(row) else ""
        if cell(spec["key"]).strip() == "":
            continue
        data.append({name: cell(name) for name in columns})
    return list(columns), data


def read_csv_table(path):
    with open(path, encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        return reader.fieldnames, list(reader)


def convert(spec, sheet_columns, sheet_rows, out_columns, known_values, warnings):
    missing = [c for c in out_columns if c not in sheet_columns]
    if missing:
        raise ConvertError(f"{spec['label']} 시트에 CSV 컬럼이 없다: {', '.join(missing)}")
    extra = [c for c in sheet_columns if c not in out_columns]
    if extra:
        warnings.append(f"{spec['label']} 시트에만 있는 컬럼은 CSV에 안 넣는다: {', '.join(extra)}")

    converted = []
    seen = set()
    for row in sheet_rows:
        key = to_int(row[spec["key"]], "?", spec["key"])
        if key in seen:
            raise ConvertError(f"{spec['label']} {key}가 시트에 두 번 있다")
        seen.add(key)

        out = {}
        for column in out_columns:
            value = row[column]
            if column in spec["ints"]:
                out[column] = to_int(value, key, column)
            elif column in spec["bools"]:
                out[column] = to_bool(value, spec["bool_words"], key, column)
            elif column in spec["arrays"]:
                out[column] = to_array(value, key, column)
            else:
                out[column] = (value or "").strip()

            if column in spec["enums"] and out[column] not in known_values.get(column, set()):
                warnings.append(f"{spec['label']} {key} {column}: 기존 CSV에 없던 값 {out[column]!r}")
            prefix = spec["tags"].get(column)
            if prefix and out[column] and not out[column].startswith(prefix):
                warnings.append(f"{spec['label']} {key} {column}: {prefix} 태그가 아니다 ({out[column]!r})")
        converted.append(out)
    return converted


def validate(quests, objectives, warnings):
    """목표가 가리키는 퀘스트·선행 목표가 실제로 있는지 본다. 깨진 연결은 게임에서 조용히 막힌다."""
    quest_ids = {row["Quest_ID"] for row in quests}
    objective_quest = {row["ObjectiveId"]: row["QuestId"] for row in objectives}
    for row in objectives:
        if row["QuestId"] not in quest_ids:
            raise ConvertError(f"목표 {row['ObjectiveId']}의 QuestId {row['QuestId']}가 퀘스트 시트에 없다")
        prerequisite = row["PrerequisiteObjectiveId"]
        if prerequisite != "0":
            if prerequisite not in objective_quest:
                raise ConvertError(f"목표 {row['ObjectiveId']}의 선행 목표 {prerequisite}가 없다")
            if objective_quest[prerequisite] != row["QuestId"]:
                raise ConvertError(f"목표 {row['ObjectiveId']}의 선행 목표 {prerequisite}가 다른 퀘스트 것이다")

    with_objectives = {row["QuestId"] for row in objectives}
    for row in quests:
        quest_id = row["Quest_ID"]
        if row["bUseObjectiveTable"] == "TRUE" and quest_id not in with_objectives:
            warnings.append(f"퀘스트 {quest_id}: bUseObjectiveTable인데 목표 행이 없다")
        for column in ("PrereqQuest_ID",):
            if row[column] != "0" and row[column] not in quest_ids:
                raise ConvertError(f"퀘스트 {quest_id}의 {column} {row[column]}가 시트에 없다")
        for prerequisite in row["AdditionalPrereqQuest_IDs"].strip("()").split(","):
            if prerequisite and prerequisite not in quest_ids:
                raise ConvertError(f"퀘스트 {quest_id}의 추가 선행 {prerequisite}가 시트에 없다")


def compare(spec, columns, old_rows, new_rows):
    """기존 CSV와 칸 단위로 비교해 바뀌는 내용을 출력하고, CSV에만 있는 행 키 목록을 돌려준다."""
    key = spec["key"]
    old = {row[key]: row for row in old_rows}
    new = {row[key]: row for row in new_rows}
    added = [k for k in new if k not in old]
    csv_only = [k for k in old if k not in new]
    changed = 0

    print(f"\n== {spec['label']} ({spec['out'].name}) 시트 {len(new)}행 / CSV {len(old)}행")
    if added:
        print(f"  시트에서 새로 들어오는 행: {', '.join(added)}")
    if csv_only:
        print(f"  CSV에만 있는 행(시트에 없음): {', '.join(csv_only)}")
    for k, row in new.items():
        if k not in old:
            continue
        for column in columns:
            if (old[k].get(column) or "") != row[column]:
                changed += 1
                print(f"  {k} {column}: {old[k].get(column)!r} -> {row[column]!r}")
    if [k for k in old if k in new] != [k for k in new if k in old]:
        print("  행 순서가 시트 순서로 바뀐다")
    print(f"  바뀌는 칸 {changed}개")
    return csv_only


def write_csv(path, columns, rows):
    buffer = io.StringIO()
    writer = csv.DictWriter(buffer, fieldnames=columns, lineterminator="\r\n")
    writer.writeheader()
    writer.writerows(rows)
    path.write_text(buffer.getvalue(), encoding="utf-8-sig", newline="")


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description="기획 퀘스트·목표 시트 -> UE DataTable CSV")
    parser.add_argument("--quest-csv", help="로컬 퀘스트 시트 CSV (없으면 시트에서 받는다)")
    parser.add_argument("--objective-csv", help="로컬 퀘스트 목표 시트 CSV (없으면 시트에서 받는다)")
    parser.add_argument("--check", action="store_true", help="기존 CSV와 대조만 하고 쓰지 않는다")
    parser.add_argument("--drop-csv-only-rows", action="store_true", help="시트에 없는 CSV 행을 지우고 굽는다")
    parser.add_argument("--out-dir", help="출력 폴더 (기본 Scripts/Data, 시험용)")
    args = parser.parse_args()

    sources = {"quest": args.quest_csv, "objective": args.objective_csv}
    warnings = []
    results = {}
    try:
        for name, spec in TABLES.items():
            columns, old_rows = read_csv_table(spec["out"])
            known = {c: {row[c] for row in old_rows} for c in spec["enums"]}
            sheet_columns, sheet_rows = read_sheet(read_text(sources[name] or spec["url"]), spec)
            new_rows = convert(spec, sheet_columns, sheet_rows, columns, known, warnings)
            results[name] = (spec, columns, old_rows, new_rows)
        validate(results["quest"][3], results["objective"][3], warnings)
    except ConvertError as error:
        print(f"[오류] {error}")
        return 1

    csv_only = {}
    for name, (spec, columns, old_rows, new_rows) in results.items():
        csv_only[name] = compare(spec, columns, old_rows, new_rows)

    for warning in warnings:
        print(f"[경고] {warning}")

    if args.check:
        print("\n[대조만] 파일은 쓰지 않았다")
        return 0

    if any(csv_only.values()) and not args.drop_csv_only_rows:
        print("\n[중단] CSV에만 있는 행이 있다. 시트에 먼저 올리거나, 지워도 되면 --drop-csv-only-rows 로 다시 실행")
        return 1

    out_dir = Path(args.out_dir) if args.out_dir else DATA_DIR
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, (spec, columns, _, new_rows) in results.items():
        path = out_dir / spec["out"].name
        write_csv(path, columns, new_rows)
        print(f"[출력] {path}")
    print("DT_QuestDefinitionRow · DT_QuestObjective 를 이 CSV로 다시 채워야 게임에 반영된다")
    return 0


if __name__ == "__main__":
    sys.exit(main())
