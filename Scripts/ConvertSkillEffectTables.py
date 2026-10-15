"""기획이 준 새 효과표 셋(액티브·패시브·직업)을 UE DataTable 임포트용 CSV로 변환한다.

기존 변환기들과 달리 입력이 xlsx 가 아니라 이미 평평한 CSV 다. 그래서 하는 일은 셋뿐이다.

  1. enum 이름을 C++ 값과 대조한다 — 매핑에 없는 값은 실패시킨다
  2. 두 표가 서로·정의표와 어긋나지 않는지 본다
  3. 값의 단위가 의심스러우면 알린다 (값은 건드리지 않는다)

★ 표 둘을 한 스크립트가 다루는 이유
    액티브와 패시브 효과표는 칸 이름이 거의 같고 단위 규칙이 완전히 같다. 스크립트를
    가르면 그 규칙이 두 벌이 되고, 한쪽만 고쳐져 조용히 어긋난다. 기존 변환기들이
    표마다 하나인 것은 입력이 시트마다 전혀 달랐기 때문이고 여기는 그렇지 않다.

★ 단위를 바꾸지 않는다 — 이 표는 서버와 공유한다
    한때 여기서 `30` 을 `0.3` 으로 나눴었다. 그러면 **클라와 서버가 같은 표를 놓고 다른
    숫자를 보게 된다.** 지금은 기획이 적은 값을 그대로 내보내고, 나누는 일은 C++ 쪽
    `FNUPassiveEffectRow::GetScaledValue()` 한 곳에서만 한다.

사용법:
    python Scripts/ConvertSkillEffectTables.py            (기본 경로)
    python Scripts/ConvertSkillEffectTables.py --active 경로.csv --passive 경로.csv

출력: Scripts/Data/ActiveSkillEffectTable.csv
      Scripts/Data/PassiveSkillEffectTable.csv
      Scripts/Data/JobSkillEffectTable.csv
"""

import argparse
import csv
import sys
from decimal import Decimal, InvalidOperation
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "Scripts" / "Data"
SOURCE = DATA / "Source"


# ---------------------------------------------------------------- enum 매핑
# 오른쪽이 C++ enum 값 이름이다. 여기에 없는 값이 오면 실패시킨다 —
# 빈 값으로 흘려보내면 그 행만 조용히 안 도는데, 그게 제일 찾기 어렵다

ACTIVE_EFFECT = {
    "Damage": "Damage",
    "Heal": "Heal",
    "StatModifier": "StatModifier",
    "StatusApply": "StatusApply",
    "StatusCure": "StatusCure",
    "Shield": "Shield",
    "Reflect": "Reflect",
    "ElementPowerMod": "ElementPowerMod",
    "ElementDamageTakenMod": "ElementDamageTakenMod",
    "ElementSkillBlock": "ElementSkillBlock",
    "DefenseIgnore": "DefenseIgnore",
    "ResourceGain": "ResourceGain",
    "ActionBlock": "ActionBlock",
    "Lifesteal": "Lifesteal",
    "DamageDealtMod": "DamageDealtMod",
}

PASSIVE_EFFECT = {
    "StatModifier": "StatModifier",
    "StatusApply": "StatusApply",
    "StatusImmune": "StatusImmune",
    "Heal": "Heal",
    "Evade": "Evade",
    "ElementPowerMod": "ElementPowerMod",
    "DamageDealtMod": "DamageDealtMod",
    "DamageTakenMod": "DamageTakenMod",
    "StatusDurationMod": "StatusDurationMod",
    "ReflectStatus": "ReflectStatus",
    "CopyPassive": "CopyPassive",
    "ResourceGain": "ResourceGain",
    "DeathPrevent": "DeathPrevent",
    "TurnOrderPriority": "TurnOrderPriority",
}

TARGET_SCOPE = {
    "Self": "Self",
    "Ally": "Ally",
    "Enemy": "Enemy",
    "AllEnemies": "AllEnemies",
    "Attacker": "Attacker",
    "RandomEnemy": "RandomEnemy",
}

STACK_TYPE = {"None": "None", "Additive": "Additive", "Multiplicative": "Multiplicative"}

DURATION_SCOPE = {
    "Instant": "Instant",
    "Turns": "Turns",
    "Battle": "Battle",
    "Permanent": "Permanent",
}

AFFECTED_STAT = {
    "None": "None",
    "Speed": "Speed",
    "Defense": "Defense",
    "AttackPower": "AttackPower",
    "MagicAttackPower": "MagicAttackPower",
    "CriticalRate": "CriticalRate",
    "CriticalDamage": "CriticalDamage",
    "MaxHealth": "MaxHealth",
    "MaxSP": "MaxSP",
    "Health": "Health",
    "SP": "SP",
    # 피해 배율 행이 스탯 칸을 빌려 쓴다, 실제 어트리뷰트가 아니라 표식이다
    "DamageDealt": "None",
    "DamageTaken": "None",
}

# 속성은 FGameplayTag 라 전체 태그 문자열로 내보낸다 (PassiveTable 과 같은 형식)
# 없음은 빈 칸이다 — "None" 이라고 쓰면 그런 이름의 태그를 찾다가 실패한다
ELEMENT = {
    "": "", "None": "",
    "Fire": "NU.Element.Fire",
    "Water": "NU.Element.Water",
    "Grass": "NU.Element.Grass",
    "Earth": "NU.Element.Earth",
    "Lightning": "NU.Element.Lightning",
    "Wind": "NU.Element.Wind",
}

TRIGGER = {
    "Always": "Always",
    "OnAttack": "OnAttack",
    "OnHitTaken": "OnHitTaken",
    "OnLowHealth": "OnLowHealth",
    "OnKill": "OnKill",
    "OnDefeated": "OnDefeated",
    "OnStatusEffect": "OnStatusEffect",
    "OnBattleStart": "OnBattleStart",
    "OnTurnStart": "OnTurnStart",
    "OnCriticalTaken": "OnCriticalTaken",
}

THRESHOLD_COMPARE = {"": "None", "None": "None", "Below": "Below", "AtLeast": "AtLeast"}

# 조건은 아직 코드가 다 안 받는다. 매핑만 해두고 미구현은 리포트로 알린다
CONDITION = {
    "": "None", "None": "None",
    "OnAttack": "OnAttack",
    "OnHitTaken": "OnHitTaken",
    "AnyStatus": "AnyStatus",
    "TargetHasStatus": "TargetHasStatus",
    "OnStatusApplied": "OnStatusApplied",
    "OnStateEnd": "OnStateEnd",
    "DuringState": "DuringState",
    "AfterElementUse": "AfterElementUse",
    "SourceIsSkill": "SourceIsSkill",
    "AttackerSpeedLower": "AttackerSpeedLower",
    "FirstTurnOnly": "FirstTurnOnly",
    "RandomTarget": "RandomTarget",
    "NextTurn": "NextTurn",
    "NoChain": "NoChain",

    # 직업 효과표가 쓰는 것들
    "HPBelow": "HPBelow",
    "HPAbove": "HPAbove",
    "SpeedDiffAtLeast": "SpeedDiffAtLeast",
    "SpeedDiffAtMost": "SpeedDiffAtMost",
    # 속도가 기준선을 넘은 만큼을 세는 줄, ConditionValue 가 그 기준선이다 (3146 초속 = 20)
    "SpeedAbove": "SpeedAbove",
    "ResourceAtMax": "ResourceAtMax",
    "ResourceAbove": "ResourceAbove",
    "StackAtCount": "StackAtCount",
    "AfterState": "AfterState",
    "OnFirstStrike": "OnFirstStrike",
    "OnFirstStrikeFail": "OnFirstStrikeFail",
    "AfterSkill": "AfterSkill",
    "OnCardDrawn": "OnCardDrawn",

    # 2차 요청서로 채워진 것들
    "TargetHPBelow": "TargetHPBelow",
    "SingleHit": "SingleHit",
    "HitCountAtLeast": "HitCountAtLeast",
    "OncePerTurn": "OncePerTurn",
    "OncePerTurnFirstAction": "OncePerTurnFirstAction",
    "OnActFirst": "OnActFirst",
    "OnActSecond": "OnActSecond",
    "HandHasCard": "HandHasCard",
    "AfterResourceSpend": "AfterResourceSpend",
    "ResourceSumAbove": "ResourceSumAbove",
    "LowestHPAlly": "LowestHPAlly",
    "AllyHasShield": "AllyHasShield",
    "WhileAllyShielded": "WhileAllyShielded",

    # 7차 요청서로 채워진 것들
    "SPBelow": "SPBelow",
    "OnTurnEnd": "OnTurnEnd",
    "OnStateEnter": "OnStateEnter",
    "LowestStat": "LowestStat",

    	# 8차 표에서 3105 연사가 HitCountAtLeast 에서 바꿔 쓴 것, 피해를 주는 스킬을 쓸 때마다 한 번
    	"OnAttackSkillUse": "OnAttackSkillUse",
}

# 구간표를 행으로 편 조건들, 같은 스킬 안에서 여럿이 동시에 참이 된다
# 가장 좁은 구간 하나만 적용하는 것이 기획 확인을 받은 해석이다
# 중첩 수도 구간이다 (3202 원소 지배 — 약점 1 · 3 · 5중첩), C++ 의 IsBandCondition 과 같은 목록이어야 한다
BAND_CONDITIONS = {"HPBelow", "HPAbove", "SpeedDiffAtLeast", "SpeedDiffAtMost", "StackAtCount"}

# 직업 효과표만 쓰는 것들
JOB_EFFECT = {
    "Damage": "Damage",
    "DamageDealtMod": "DamageDealtMod",
    "DamageTakenMod": "DamageTakenMod",
    "StatModifier": "StatModifier",
    "Heal": "Heal",
    "Shield": "Shield",
    "ResourceGain": "ResourceGain",
    "SelfCost": "SelfCost",
    "DebuffStack": "DebuffStack",
    "Special": "Special",

    # 요청서 4번으로 채워진 것들, 절반만 오던 노드의 나머지 절반이다
    "ResourceGainMod": "ResourceGainMod",
    "ResourceGainCap": "ResourceGainCap",
    "ResourceClear": "ResourceClear",
    "ResourceDecayImmune": "ResourceDecayImmune",
    "DeathPrevent": "DeathPrevent",
    "ShieldMod": "ShieldMod",
    "HealReceivedMod": "HealReceivedMod",
    "ForceCritical": "ForceCritical",
    "StatusDurationMod": "StatusDurationMod",
    "CardFloor": "CardFloor",
    "SkillCostMod": "SkillCostMod",

    # 기본 계수에 %p 를 가산하는 축, 합연산 풀과 자리가 다르다 (요청서 6차 1-A)
    "CoefficientAdd": "CoefficientAdd",

    # 7차 요청서로 온 것들, 전부 ImplementBy 가 Code_* 라 전용 구현이 붙어야 돈다
    "CoefficientMul": "CoefficientMul",
    "DefenseIgnore": "DefenseIgnore",
    "HPCost": "HPCost",
    "ResourceTimerReset": "ResourceTimerReset",
    "Taunt": "Taunt",
}

JOB_TARGET_SCOPE = {
    "": "None", "None": "None",
    "Self": "Self",
    "Ally": "Ally",
    "Enemy": "Enemy",
    # 7차 3번 — Ally 는 자신 · 나이아를 뺀 아군 캐릭터, 아래 둘이 나머지 범위다
    "AllyAndNaia": "AllyAndNaia",
    "Party": "Party",
}

IMPLEMENT_BY = {
    "Data": "Data",
    "Code": "Code",
    "Code_Override": "CodeOverride",
    "Code_Resource": "CodeResource",
    "Code_NewMechanic": "CodeNewMechanic",
    	# 기획이 "표가 덜 왔다" 고 직접 적은 줄 (3124 #2), 런타임은 건너뛴다
    	"Pending": "Pending",
}

# 직업별 자원표 (17행) 의 enum 칸들
RESOURCE_OWNER = {"Self": "Self", "Target": "Target"}

RESOURCE_SCALE_STAT = {
    "": "None", "None": "None",
    "Speed": "Speed",
    "INT": "Intelligence",
    "SP": "SP",
}

RESOURCE_GAIN_TRIGGER = {
    "": "None", "None": "None",
    "OnSelfHPCost": "OnSelfHPCost",
    "OnAttackHit": "OnAttackHit",
    "OnSingleHit": "OnSingleHit",
    "OnMagicDamage": "OnMagicDamage",
    "OnSkillUse": "OnSkillUse",
    "OnCardDrawn": "OnCardDrawn",
    "OnHitTaken": "OnHitTaken",
    "OnTurnStart": "OnTurnStart",
    "OnTurnEnd": "OnTurnEnd",
    "OnParrySuccess": "OnParrySuccess",
    "ByJobNode": "ByJobNode",
    "Derived": "Derived",
}

RESOURCE_DECAY = {
    "": "None", "None": "None",
    "PerTurn": "PerTurn",
    "TimeoutAll": "TimeoutAll",
    "TimeoutEach": "TimeoutEach",
    "Redraw": "Redraw",
    	# 마법을 쓰면 0 이 된다 — 필살기 · 기본 공격은 제외 (17018 임계)
    	"OnMagicUse": "OnMagicUse",
    "ByJobNode": "ByJobNode",
    "Derived": "Derived",
}

RESOURCE_ON_MAX = {
    "": "Keep", "Keep": "Keep",
    "EnterState": "EnterState",
    "BurnOut": "BurnOut",
    "PushOldest": "PushOldest",
    "UnlockUltimate": "UnlockUltimate",
    "ByJobNode": "ByJobNode",
}

RESOURCE_ON_BATTLE_END = {"": "Reset", "Keep": "Keep", "Reset": "Reset"}

# 자원이 켜는 상태와 자기 편 줄이 세는 방식 (7차 요청서), 빈 칸은 상태 없음 · 판정의 대상 기준이다
RESOURCE_STATE_ENTER_RULE = {
    "": "None", "None": "None",
    "AtValue": "AtValue",
    "AtMax": "AtMax",
    "DropToZero": "DropToZero",
}

RESOURCE_STATE_START_TIMING = {
    "": "None", "None": "None",
    "Immediate": "Immediate",
    "NextTurn": "NextTurn",
}

RESOURCE_STATE_ON_END = {"": "Keep", "Keep": "Keep", "Reset": "Reset"}

RESOURCE_SELF_SIDE_COUNT = {"": "Target", "Target": "Target", "MostStacked": "MostStacked"}

RESOURCE_FIELDS = [
    "Name", "ResourceId", "ResourceKey", "KoreanName", "EnglishName", "JobClass",
    "JobNodeId", "OwnerScope", "InitialValue", "bStartAtMax", "MaxValue", "MaxBase",
    "MaxScaleStat", "MaxScaleDivisor", "MaxFormula", "GainTrigger", "GainAmount",
    "bGainIsPercent", "GainTrigger2", "GainAmount2", "DecayType", "DecayAmount",
    "DecayInterval", "OnReachMax", "UsePerBattle", "bResetOnUse", "OnBattleEnd",
    "StateName", "StateEnterRule", "StateEnterValue", "StateDurationTurns",
    "StateStartTiming", "StateOnEnd", "SelfSideCountFrom", "Description",
]


# ValuePer 어휘 중 자원표를 안 보고도 세는 것들 (JobValueSteps 와 같은 목록)
# 나머지는 자원 이름이라 자원표를 봐야 셀 수 있는지 가려진다
VALUE_PER_SELF_COMPUTABLE = {"잃은HP", "최대HP", "속도초과분"}


ACTIVE_FIELDS = [
    "Name", "SkillEffectId", "SkillId", "EffectIndex", "EffectType", "TargetScope",
    "StackType", "AffectedStat", "Value", "bIsPercent", "DurationTurns", "DurationScope",
    "MaxPerBattle", "Element", "ConditionType", "ConditionValue", "StatusEffectId",
    "Chance", "ImplementBy",
]

PASSIVE_FIELDS = [
    "Name", "SkillEffectId", "PassiveId", "EffectIndex", "EffectType", "TargetScope",
    "StackType", "AffectedStat", "Value", "bIsPercent", "DurationScope", "DurationTurns",
    "Trigger", "TriggerChance", "ThresholdCompare", "TriggerThreshold", "ConditionType",
    "ConditionValue", "MaxPerBattle", "Element", "StatusEffectId", "ImplementBy",
]

JOB_FIELDS = [
    "Name", "SkillEffectId", "JobSkillId", "EffectIndex", "EffectType", "TargetScope",
    "StackType", "AffectedStat", "Value", "bIsPercent", "DurationScope", "DurationTurns",
    "MaxPerBattle", "ValuePer", "ValuePerUnit", "ValueCap", "bConsumeResource",
    "ConditionType", "ConditionValue", "ResourceName", "StatusEffectId", "ImplementBy",
]


# ---------------------------------------------------------------- 아는 표 오류
# `효과표_수정요청.md` 로 이미 보낸 것들이다. 실패로 두면 회신이 올 때까지 파이프라인이
# 통째로 멈추므로 여기서만 지나가게 한다.
#
# ★ 고쳐지면 "이제 필요 없다" 고 알려준다 — 예외가 낡은 채로 남는 것이 원래 오류보다 나쁘다
KNOWN_SHEET_ERRORS = {
    # 지금은 비어 있다. 아는 오류를 넣을 때는 열쇠를 "어디|무엇" 으로 적고
    # 고쳐지면 아래 [정리] 가 스스로 알려준다
}


# ---------------------------------------------------------------- 아직 못 돌리는 직업 행
# `직업스킬_효과표_수정요청.md` 로 보낸 것들이다. 이 행들은 표에 조건이 없어서
# **그대로 읽으면 조건 없는 상시 효과가 된다** — 설계보다 강한 노드가 조용히 생긴다.
# 그래서 ImplementBy 를 Pending 으로 돌려두고 런타임이 건너뛰게 한다.
#
# ★ 조건이 채워지면 스스로 풀린다 — 아래 둘 다 "아직 조건이 없을 때만" 걸린다.
#   낡으면 [정리] 로 알려주므로 그때 지우면 된다.
# 3301 운명 읽기는 7차 표에서 카드별 조건(OnCardDrawn)이 채워져 예외를 지웠다
# 조건 판정이 없는 줄은 IsJobConditionMet 이 거짓으로 돌려 그 줄만 빠진다
JOB_PENDING_NO_CONDITION = {}

# 덮어쓰기인데 Data 로 오던 7행은 2차 요청서로 전부 Code_Override 가 됐다
# 같은 일이 또 생기면 여기에 다시 적는다 — 열쇠는 (스킬 번호, EffectType) 이다
#
# 7차 회신은 아래 한 줄을 Data 로 보내지만 코드가 아직 바르게 못 돌린다, 조건이 없어도 돌리지 않는다
# 코드가 바르게 돌게 되면 표가 바뀐 것처럼 [정리] 로 알려주므로 그때 지우면 된다
# 3111 · 3411 적 대상 스탯 줄은 대상에게 걸고 다시 걸면 대체하게 되어, 3425 · 3428 아군 보호막은 아군 범위를 세워 풀었다
# 3124 #2 는 8차 표에서 기획이 ImplementBy=Pending 으로 직접 적어 여기서 거를 필요가 없어졌다
JOB_PENDING_OVERRIDE = {}


class Report:
    def __init__(self):
        self.failures = []
        self.notes = []
        self.code_rows = []
        self.empty_values = []

        self.known = []
        self.stale_known = []
        self.pending = []
        self.uncomputable = []

    def fail(self, where, message):
        key = f"{where}|{message}"
        if key in KNOWN_SHEET_ERRORS:
            self.known.append((where, message, KNOWN_SHEET_ERRORS[key]))
            return

        self.failures.append((where, message))

    def note(self, message):
        self.notes.append(message)


# 종합본 시트 이름과, 시트 컬럼명 -> 평평한 CSV 컬럼명
SHEETS = {
    "active": ("액티브 스킬 효과 테이블", "ActiveSkillEffectTable_Origin.csv"),
    "passive": ("패시브 스킬 효과 테이블", "PassiveSkillEffectTable_Origin.csv"),
    "job": ("직업 스킬 효과 테이블", "JobSkillEffectTable_Origin.csv"),
    "resource": ("직업별 자원 테이블", "JobResourceTable_Origin.csv"),
    	"fate": ("운명 읽기 확률 테이블", "FateCardRateTable_Origin.csv"),
}

SHEET_COLUMN_ALIASES = {
    "SkillEffect_ID": "SkillEffectId",
    "Active_ID": "ActiveId",
    "Passive_ID": "PassiveId",
    "Skill_ID": "JobSkillId",
    "StatusEffect_ID": "StatusEffectId",
    "Resource_ID": "ResourceId",
    "Resource_Key": "ResourceKey",
    	"CardRate_ID": "CardRateId",
}


def read_sheet(book_path, sheet_name, key_column="SkillEffectId"):
    """종합본에서 시트 하나를 평평한 dict 목록으로 읽는다.

    ★ 여기가 CSV 내보내기를 대신한다. 엑셀에서 CSV 로 내보내는 단계가
      `"5007, 5008"` 을 `"50,075,008"` 로 만든 적이 있다 — 쉼표가 든 칸을 숫자로 보고
      천단위 서식을 붙인 것이다. 셀 값을 직접 읽으면 그 단계가 아예 없다.

    머리는 4행째다(위 세 줄은 제목과 묶음 이름). 첫 칸은 비어 있다.
    """
    try:
        import openpyxl
    except ImportError as error:
        raise RuntimeError("종합본을 읽으려면 openpyxl 이 필요하다") from error

    workbook = openpyxl.load_workbook(book_path, data_only=True, read_only=True)

    if sheet_name not in workbook.sheetnames:
        raise RuntimeError(f"종합본에 '{sheet_name}' 시트가 없다 : "
                           f"{', '.join(workbook.sheetnames)}")

    rows = [list(r) for r in workbook[sheet_name].iter_rows(values_only=True)]
    def alias(cell):
    	return SHEET_COLUMN_ALIASES.get(str(cell).strip(), str(cell).strip()) if cell else ""

    # 머리 줄은 시트마다 다르다 — 묶음 이름 줄이 있으면 4행째, 없으면 3행째
    # 열쇠 칸 이름이 있는 첫 줄을 머리로 본다
    header_index = next((i for i, r in enumerate(rows[:8]) if key_column in [alias(c) for c in r]), 3)
    header = [alias(c) for c in rows[header_index]]

    def cell(value):
        if value is None:
            return ""
        if isinstance(value, bool):
            return "TRUE" if value else "FALSE"
        # 엑셀은 정수도 실수로 준다, 1.0 을 "1" 로 되돌린다
        if isinstance(value, float) and value == int(value):
            return str(int(value))
        return str(value).strip()

    out = []
    for row in rows[header_index + 1:]:
        record = {h: cell(v) for h, v in zip(header, row) if h}

        # 맨 아래 타입 안내 줄(int / enum ...)을 거른다, 번호가 숫자가 아니다
        # ★ 열쇠 칸은 시트마다 다르다 — 효과표는 SkillEffectId, 자원표는 ResourceId
        if not record.get(key_column, "").isdigit():
            continue

        record.setdefault("Name", record[key_column])
        out.append(record)

    print(f"[확인] 시트 '{sheet_name}' {len(out)}행")

    return out


def write_origin(path, rows):
    """읽은 것을 그대로 남겨둔다, 다음에 무엇이 바뀌었는지 볼 수 있어야 한다."""
    if not rows:
        return

    with open(path, "w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0].keys()), lineterminator="\r\n")
        writer.writeheader()
        writer.writerows(rows)


def load(path):
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return [row for row in csv.DictReader(handle) if (row.get("Name") or "").strip()]


def write(path, field_names, rows):
    # UE CSV 임포터용 파일은 BOM 붙은 UTF-8 과 CRLF 로 기록한다 (다른 테이블과 동일)
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=field_names, lineterminator="\r\n")
        writer.writeheader()
        writer.writerows(rows)


def pick(mapping, raw, where, prop, report):
    key = (raw or "").strip()
    if key in mapping:
        return mapping[key]
    report.fail(where, f"{prop} 에 모르는 값 '{key}'")
    return None


def to_bool(raw):
    return (raw or "").strip().upper() in ("TRUE", "1", "Y", "YES")


def to_int(raw, default=0):
    text = (raw or "").strip()
    if not text:
        return default
    try:
        return int(Decimal(text))
    except (InvalidOperation, ValueError):
        return default


def scale_value(raw, is_percent, stack_type, where, report):
    """값을 적힌 그대로 내보낸다. 숫자인지와 단위가 그럴듯한지만 본다.

    ★ 나누지 않는다 — 이 표는 서버와 공유하므로 양쪽이 같은 숫자를 봐야 한다.
      `30`(=30%) 을 `0.3` 으로 만드는 일은 C++ 의 GetScaledValue() 가 한다.
    """
    text = (raw or "").strip()
    if not text:
        return "0"

    try:
        value = Decimal(text)
    except (InvalidOperation, ValueError):
        report.fail(where, f"Value 를 숫자로 읽을 수 없다 '{text}'")
        return "0"

    if stack_type == "Multiplicative" and is_percent:
        # 곱연산이면서 백분율이면 `2` 가 x2 인지 2% 인지 사람이 정해야 한다
        report.fail(where, f"곱연산인데 bIsPercent 가 TRUE 다 (Value={text})")

    # 백분율 칸에 1 미만이 오면 누군가 이미 나눠둔 것일 수 있다
    # 0.5% 같은 값이 진짜로 필요해지면 이 검사를 빼면 된다
    if is_percent and value != 0 and abs(value) < 1:
        report.note(f"{where} bIsPercent 인데 값이 {text} 다 — 이미 나눠진 값인지 확인할 것")

    return format(value.normalize(), "f")


# 피해 줄이 쓰는 공격력 축, 정의표의 DamageType 과 같은 말이어야 한다
DAMAGE_STAT_BY_TYPE = {
    "NU.Damage.Type.Physical": "AttackPower",
    "NU.Damage.Type.Magical": "MagicAttackPower",
}


def convert_active(rows, report, definition_by_skill):
    out = []
    seen_damage = set()

    for row in rows:
        skill_id = to_int(row["ActiveId"])
        index = to_int(row["EffectIndex"])
        where = f"액티브 {skill_id} #{index}"

        effect = pick(ACTIVE_EFFECT, row["EffectType"], where, "EffectType", report)
        scope = pick(TARGET_SCOPE, row["TargetScope"], where, "TargetScope", report)
        stack = pick(STACK_TYPE, row["StackType"], where, "StackType", report)
        stat = pick(AFFECTED_STAT, row["AffectedStat"], where, "AffectedStat", report)
        duration = pick(DURATION_SCOPE, row["DurationScope"], where, "DurationScope", report)
        element = pick(ELEMENT, row["Element"], where, "Element", report)
        condition = pick(CONDITION, row["ConditionType"], where, "ConditionType", report)
        implement = pick(IMPLEMENT_BY, row["ImplementBy"], where, "ImplementBy", report)

        is_percent = to_bool(row["bIsPercent"])
        value = scale_value(row["Value"], is_percent, stack, where, report)

        if implement == "Code":
            report.code_rows.append((where, effect, row["SourceText"][:50]))

        # 반사는 이제 스킬마다 비율이 다르다, 값이 곧 되받는 비율이다
        #
        # ★ 0은 실패가 아니라 "미정" 이다 — 코드가 100% 로 받아준다. 그래도 알려는 준다,
        #   비율을 적어 보내기 시작한 표에서 0이 남아 있으면 십중팔구 빠뜨린 것이다
        if effect == "Reflect":
            # ★ 반사는 "가한 피해의 몇 할" 이라 고정값이라는 갈래가 없다.
            #   그래서 bIsPercent 가 FALSE 면 넘기지 않고 막는다 — 코드는 다른 줄과 같은
            #   규칙으로 읽으므로, FALSE 로 들어오면 50 이 50% 가 아니라 5000% 가 된다
            if not is_percent:
                report.fail(where, f"반사인데 bIsPercent 가 FALSE 다 "
                                   f"(값 {value} 가 {value}00% 로 읽힌다)")
            else:
                ratio = Decimal(value or 0) / 100

                if ratio == 0:
                    report.fail(where, "반사 비율이 비어 있다 (그 줄이 아무 일도 안 한다)")
                elif ratio > 1:
                    report.fail(where, f"반사 비율이 {ratio:%} 다 (가한 피해보다 크게 되받는다)")
        elif effect != "Damage" and Decimal(value or 0) == 0:
            report.empty_values.append((where, effect, row["SourceText"][:50]))

        # 자기 자신에게 피해를 넣는 행은 거의 언제나 실수다
        # 2506 바람의 장막이 이렇게 들어와 있었다 — 정의표까지 Self 라 실제로 자기를 때린다
        if effect == "Damage" and scope == "Self":
            report.fail(where, "피해를 자기 자신에게 넣는다 (TargetScope=Self)")

        # 상태이상 줄의 확률이 0이면 아무 것도 안 걸린다
        # 다른 줄의 0은 정상이라(안 읽는 칸) 상태 줄만 본다
        if effect == "StatusApply" and Decimal((row["Chance"] or "0").strip()) <= 0:
            report.fail(where, f"상태이상 줄인데 확률이 {row['Chance'] or '(빈칸)'} 이다 (걸리지 않는다)")

        # 정의표와 대조 — 같은 사실이 두 표에 있어 한쪽만 고쳐지면 조용히 어긋난다
        #
        # ★ 계수는 여기(효과표)가 주인이 됐다. 맞춰볼 짝이 없어진 대신 단위를 본다 —
        #   check_damage_coefficients 가 "정수인가" 를 보는데, 정의표 대조가 예전에
        #   잡던 것(150 을 1.5 로 적는 실수)과 같은 일이다
        if effect == "Damage":
            seen_damage.add(skill_id)
            definition = definition_by_skill.get(skill_id)

            if definition is None:
                report.fail(where, "정의표(ActiveSkillTable)에 없는 SkillId")
            else:

                want_stat = DAMAGE_STAT_BY_TYPE.get(definition["DamageType"])
                if want_stat and want_stat != stat:
                    report.fail(where, f"정의표 {definition['DamageType']} 인데 효과표 축은 {stat}")

                if definition["AttackElement"] != (element or ""):
                    report.fail(where, f"정의표 속성={definition['AttackElement'] or '(무속성)'} "
                                       f"인데 효과표 속성={element or '(무속성)'}")

        out.append({
            "Name": row["Name"],
            "SkillEffectId": to_int(row["SkillEffectId"]),
            "SkillId": skill_id,
            "EffectIndex": index,
            "EffectType": effect or "None",
            "TargetScope": scope or "None",
            "StackType": stack or "None",
            "AffectedStat": stat or "None",
            "Value": value,
            "bIsPercent": "TRUE" if is_percent else "FALSE",
            "DurationTurns": to_int(row["DurationTurns"]),
            "DurationScope": duration or "Instant",
            "MaxPerBattle": to_int(row["MaxPerBattle"]),
            "Element": element if element is not None else "",
            "ConditionType": condition or "None",
            "ConditionValue": (row["ConditionValue"] or "").strip(),
            "StatusEffectId": to_int(row["StatusEffectId"]),
            "Chance": (row["Chance"] or "0").strip(),
            "ImplementBy": implement or "Data",
        })

    # 공격인데 때리는 줄이 없는 스킬 — 때리지 않는 공격이 조용히 생긴다
    # 예전에는 "정의표 Power > 0 인데" 로 봤다. 위력 칸이 사라져서 종류로 본다
    for skill_id, definition in sorted(definition_by_skill.items()):
        if definition["SkillType"] == "Attack" and skill_id not in seen_damage:
            report.fail(f"액티브 {skill_id}", "SkillType 이 공격인데 효과표에 Damage 줄이 없다")

    return out


def convert_passive(rows, report, known_passives):
    out = []

    for row in rows:
        passive_id = to_int(row["PassiveId"])
        index = to_int(row["EffectIndex"])
        where = f"패시브 {passive_id} #{index}"

        effect = pick(PASSIVE_EFFECT, row["EffectType"], where, "EffectType", report)
        scope = pick(TARGET_SCOPE, row["TargetScope"], where, "TargetScope", report)
        stack = pick(STACK_TYPE, row["StackType"], where, "StackType", report)
        stat = pick(AFFECTED_STAT, row["AffectedStat"], where, "AffectedStat", report)
        duration = pick(DURATION_SCOPE, row["DurationScope"], where, "DurationScope", report)
        element = pick(ELEMENT, row["Element"], where, "Element", report)
        trigger = pick(TRIGGER, row["TriggerType"], where, "TriggerType", report)
        compare = pick(THRESHOLD_COMPARE, row["ThresholdCompare"], where,
                       "ThresholdCompare", report)
        condition = pick(CONDITION, row["ConditionType"], where, "ConditionType", report)
        implement = pick(IMPLEMENT_BY, row["ImplementBy"], where, "ImplementBy", report)

        if passive_id not in known_passives:
            report.fail(where, "정의표(PassiveTable)에 없는 PassiveId")

        is_percent = to_bool(row["bIsPercent"])
        value = scale_value(row["Value"], is_percent, stack, where, report)

        if implement == "Code":
            report.code_rows.append((where, effect, row["SourceText"][:50]))

        if Decimal(value or 0) == 0:
            report.empty_values.append((where, effect, row["SourceText"][:50]))

        # 기준치를 쓰는 행은 비교 방향이 있어야 한다 — 없으면 "이상/이하" 를 코드가 정하게 된다
        threshold = (row["TriggerThreshold"] or "0").strip()
        if Decimal(threshold or 0) != 0 and compare == "None":
            report.fail(where, f"TriggerThreshold={threshold} 인데 ThresholdCompare 가 비었다")

        out.append({
            "Name": row["Name"],
            "SkillEffectId": to_int(row["SkillEffectId"]),
            "PassiveId": passive_id,
            "EffectIndex": index,
            "EffectType": effect or "None",
            "TargetScope": scope or "None",
            "StackType": stack or "None",
            "AffectedStat": stat or "None",
            "Value": value,
            "bIsPercent": "TRUE" if is_percent else "FALSE",
            "DurationScope": duration or "Instant",
            "DurationTurns": to_int(row["DurationTurns"]),
            "Trigger": trigger or "Always",
            "TriggerChance": (row["TriggerChance"] or "0").strip(),
            "ThresholdCompare": compare or "None",
            "TriggerThreshold": threshold,
            "ConditionType": condition or "None",
            "ConditionValue": (row["ConditionValue"] or "").strip(),
            "MaxPerBattle": to_int(row["MaxPerBattle"]),
            "Element": element if element is not None else "",
            "StatusEffectId": to_int(row["StatusEffectId"]),
            "ImplementBy": implement or "Data",
        })

    return out


def convert_job(rows, report, job_definition_by_skill):
    """직업 스킬 효과표 353행.

    ★ 액티브·패시브와 단위 규칙이 같아서 같은 헬퍼를 그대로 쓴다. 다른 것은 셋뿐이다 —
      ValuePer(무엇 1당) · ValueCap(상한) · ImplementBy 가 갈래까지 적혀 온다는 것.
    """
    out = []

    for row in rows:
        skill_id = to_int(row["JobSkillId"])
        index = to_int(row["EffectIndex"])
        where = f"직업 {skill_id} #{index}"

        effect = pick(JOB_EFFECT, row["EffectType"], where, "EffectType", report)
        scope = pick(JOB_TARGET_SCOPE, row["TargetScope"], where, "TargetScope", report)
        stack = pick(STACK_TYPE, row["StackType"], where, "StackType", report)
        duration = pick(DURATION_SCOPE, row["DurationScope"], where, "DurationScope", report)
        condition = pick(CONDITION, row["ConditionType"], where, "ConditionType", report)
        implement = pick(IMPLEMENT_BY, row["ImplementBy"], where, "ImplementBy", report)

        definition = job_definition_by_skill.get(skill_id)
        if definition is None:
            report.fail(where, "정의표(JobSkillDefinition)에 없는 Skill_ID")

        is_percent = to_bool(row["bIsPercent"])

        # ★ 곱연산 + 백분율은 액티브에서 실패였지만 직업표에서는 뜻이 정해져 있다
        #   StatModifier 의 곱연산 행이 없고, 피해 증감의 곱연산은 전부 bIsPercent=FALSE 다
        value = scale_value(row["Value"], is_percent, stack, where, report)

        # AffectedStat 은 피해 증감 줄에서 EffectType 을 되풀이할 뿐이다
        # 같은 사실이 두 칸에 있으면 한쪽만 고쳐지므로, 읽지 않는 쪽을 검증에 쓰고 None 으로 내보낸다
        raw_stat = (row["AffectedStat"] or "").strip()
        expected_stat = {"DamageDealtMod": "DamageDealt", "DamageTakenMod": "DamageTaken"}.get(effect)

        if expected_stat:
            if raw_stat not in ("", "None", expected_stat):
                report.fail(where, f"{effect} 인데 AffectedStat 이 '{raw_stat}' 이다")
            stat = "None"
        else:
            stat = pick(AFFECTED_STAT, raw_stat, where, "AffectedStat", report)

        # 자원에 비례하는 피해 줄이 그 자원을 쓰는지, 요청서 3-1 로 받은 칸
        consume = to_bool(row.get("bConsumeResource"))

        value_per = (row["ValuePer"] or "").strip()
        value_per_unit = (row["ValuePerUnit"] or "0").strip() or "0"
        value_cap = (row["ValueCap"] or "0").strip() or "0"

        if consume and not value_per and not (row["ResourceName"] or "").strip():
            report.fail(where, "bConsumeResource 가 TRUE 인데 ValuePer 도 ResourceName 도 없다")

        # 상한만 있고 비례가 없으면 무엇에 대한 상한인지 알 수 없다
        if Decimal(value_cap or 0) != 0 and not value_per and condition not in BAND_CONDITIONS:
            report.note(f"{where} ValueCap={value_cap} 인데 ValuePer 가 비었다 — 무엇의 상한인지 불분명")

        # 조건이 수치를 요구하는데 칸이 비면 판정을 코드가 지어내게 된다
        condition_value = (row["ConditionValue"] or "").strip()
        if condition in BAND_CONDITIONS and not condition_value:
            report.fail(where, f"{condition} 인데 ConditionValue 가 비었다")

        # ★ 액티브·필살기인데 상시로 적힌 행
        #   상시 층은 DurationScope=Permanent 로 가르는데, 액티브의 "다음 턴" 이 여기 섞이면
        #   한 턴짜리 버프가 전투 내내 켜진 채로 남는다
        if (definition and definition["SkillType"] in ("액티브", "필살기")
                and duration == "Permanent" and implement == "Data"):
            report.pending.append((where, effect, "액티브인데 DurationScope 가 Permanent 다"))
            implement = "Pending"

        # ★ 요청서로 보낸 행을 Pending 으로 돌린다 — 그대로 읽으면 조건 없는 상시 효과가 된다
        #   조건이 채워지거나 갈래가 바뀌면 스스로 풀린다
        key = (skill_id, effect)
        if implement == "Data":
            if condition in ("None", None) and key in JOB_PENDING_NO_CONDITION:
                report.pending.append((where, effect, JOB_PENDING_NO_CONDITION[key]))
                implement = "Pending"
            elif key in JOB_PENDING_OVERRIDE:
                report.pending.append((where, effect, JOB_PENDING_OVERRIDE[key]))
                implement = "Pending"

        if implement and implement.startswith("Code"):
            report.code_rows.append((where, effect, (row["SourceText"] or "")[:50]))

        out.append({
            "Name": row["Name"],
            "SkillEffectId": to_int(row["SkillEffectId"]),
            "JobSkillId": skill_id,
            "EffectIndex": index,
            "EffectType": effect or "None",
            "TargetScope": scope or "None",
            "StackType": stack or "None",
            "AffectedStat": stat or "None",
            "Value": value,
            "bIsPercent": "TRUE" if is_percent else "FALSE",
            "DurationScope": duration or "Instant",
            "DurationTurns": to_int(row["DurationTurns"]),
            "MaxPerBattle": to_int(row["MaxPerBattle"]),
            "ValuePer": value_per,
            "ValuePerUnit": value_per_unit,
            "ValueCap": value_cap,
            "bConsumeResource": "TRUE" if consume else "FALSE",
            "ConditionType": condition or "None",
            "ConditionValue": condition_value,
            "ResourceName": (row["ResourceName"] or "").strip(),
            "StatusEffectId": to_int(row["StatusEffectId"]),
            "ImplementBy": implement or "Data",
        })

    return out


def convert_resource(rows, report, job_definition_by_skill):
    """직업별 자원 17종.

    ★ 값을 해석하지 않는다 — 상한식(`속도 / 2`)은 `MaxFormula` 에 사람이 읽는 말로 남기고,
      코드가 쓰는 것은 `MaxBase` + `MaxScaleStat` / `MaxScaleDivisor` 세 칸이다.
      산문을 파싱하지 않으려고 기획이 둘 다 준 것이므로 둘이 어긋나면 여기서 잡는다.
    """
    out = []
    seen_names = {}

    for row in rows:
        resource_id = to_int(row["ResourceId"])
        where = f"자원 {resource_id} {row.get('Name_KR', '')}"

        owner = pick(RESOURCE_OWNER, row["OwnerScope"], where, "OwnerScope", report)
        scale = pick(RESOURCE_SCALE_STAT, row["MaxScaleStat"], where, "MaxScaleStat", report)
        gain = pick(RESOURCE_GAIN_TRIGGER, row["GainTrigger"], where, "GainTrigger", report)
        gain2 = pick(RESOURCE_GAIN_TRIGGER, row["GainTrigger2"], where, "GainTrigger2", report)
        decay = pick(RESOURCE_DECAY, row["DecayType"], where, "DecayType", report)
        on_max = pick(RESOURCE_ON_MAX, row["OnReachMax"], where, "OnReachMax", report)
        on_end = pick(RESOURCE_ON_BATTLE_END, row.get("OnBattleEnd"), where, "OnBattleEnd", report)
        enter_rule = pick(RESOURCE_STATE_ENTER_RULE, row.get("StateEnterRule"), where, "StateEnterRule", report)
        start_timing = pick(RESOURCE_STATE_START_TIMING, row.get("StateStartTiming"), where,
                            "StateStartTiming", report)
        state_on_end = pick(RESOURCE_STATE_ON_END, row.get("StateOnEnd"), where, "StateOnEnd", report)
        side_count = pick(RESOURCE_SELF_SIDE_COUNT, row.get("SelfSideCountFrom"), where,
                          "SelfSideCountFrom", report)

        korean = (row.get("Name_KR") or "").strip()

        # ★ 효과표의 ResourceName 이 이 이름으로 이어진다, 겹치면 어느 자원인지 갈리지 않는다
        if korean in seen_names:
            report.fail(where, f"자원 이름 '{korean}' 이 {seen_names[korean]} 과 겹친다")
        seen_names[korean] = resource_id

        # 전직 노드를 가리키면 그 노드가 실제로 있어야 한다
        node_id = to_int(row["JobNodeId"])
        if node_id > 0 and node_id not in job_definition_by_skill:
            report.fail(where, f"JobNodeId {node_id} 가 직업 스킬 정의표에 없다")

        # 상한이 스탯 비례면 나눌 수가 있어야 한다
        divisor = (row["MaxScaleDivisor"] or "0").strip()
        if scale != "None" and Decimal(divisor or 0) == 0:
            report.fail(where, f"MaxScaleStat 이 {scale} 인데 MaxScaleDivisor 가 0 이다")

        # 감쇠가 시간 기준이면 간격이 있어야 한다
        if decay in ("PerTurn", "TimeoutAll", "TimeoutEach") \
                and to_int(row["DecayInterval"]) <= 0:
            report.fail(where, f"DecayType 이 {decay} 인데 DecayInterval 이 0 이다")

        # 상태 이름이 있으면 언제 들어가는지도 있어야 한다, 없으면 이름만 있는 상태가 영영 안 켜진다
        state_name = (row.get("StateName") or "").strip()
        state_value = to_int(row.get("StateEnterValue"))

        if state_name and enter_rule == "None":
            report.fail(where, f"상태 '{state_name}' 이 있는데 StateEnterRule 이 비었다 (영영 들어가지 않는다)")

        if enter_rule == "AtValue" and state_value <= 0:
            report.fail(where, f"StateEnterRule 이 AtValue 인데 StateEnterValue 가 {state_value} 이다")

        # 자기 편 줄의 셈 방식은 대상에게 붙는 자원에서만 뜻이 있다, 자기 자원은 늘 자기 값이다
        if side_count == "MostStacked" and owner == "Self":
            report.fail(where, "SelfSideCountFrom 이 MostStacked 인데 OwnerScope 가 Self 다 "
                               "(대상에게 붙는 자원만 상대 중 가장 많은 쪽을 셀 수 있다)")

        out.append({
            "Name": row.get("Name") or str(resource_id),
            "ResourceId": resource_id,
            "ResourceKey": (row.get("ResourceKey") or "").strip(),
            "KoreanName": korean,
            "EnglishName": (row.get("Name_EN") or "").strip(),
            "JobClass": (row.get("Class") or "").strip(),
            "JobNodeId": node_id,
            "OwnerScope": owner or "Self",
            "InitialValue": to_int(row["InitialValue"]),
            "bStartAtMax": "TRUE" if to_bool(row["bStartAtMax"]) else "FALSE",
            "MaxValue": to_int(row["MaxValue"]),
            "MaxBase": to_int(row["MaxBase"]),
            "MaxScaleStat": scale or "None",
            "MaxScaleDivisor": divisor,
            "MaxFormula": (row.get("MaxFormula") or "").strip(),
            "GainTrigger": gain or "None",
            "GainAmount": (row["GainAmount"] or "0").strip(),
            "bGainIsPercent": "TRUE" if to_bool(row["bGainIsPercent"]) else "FALSE",
            "GainTrigger2": gain2 or "None",
            "GainAmount2": (row["GainAmount2"] or "0").strip(),
            "DecayType": decay or "None",
            "DecayAmount": (row["DecayAmount"] or "0").strip(),
            "DecayInterval": to_int(row["DecayInterval"]),
            "OnReachMax": on_max or "Keep",
            "UsePerBattle": to_int(row["UsePerBattle"]),
            "bResetOnUse": "TRUE" if to_bool(row["bResetOnUse"]) else "FALSE",
            "OnBattleEnd": on_end or "Reset",
            "StateName": state_name,
            "StateEnterRule": enter_rule or "None",
            "StateEnterValue": state_value,
            "StateDurationTurns": to_int(row.get("StateDurationTurns")),
            "StateStartTiming": start_timing or "None",
            "StateOnEnd": state_on_end or "Keep",
            "SelfSideCountFrom": side_count or "Target",
            "Description": (row.get("Description") or "").strip(),
        })

    return out


def check_resource_names(job_rows, resource_rows, report):
    """효과표가 부르는 자원 이름이 자원표에 실제로 있는지.

    ★ 두 표를 잇는 것이 이름뿐이라 한 글자만 달라도 조용히 안 걸린다.
      `ValuePer` 는 자원 이름과 파생값(잃은HP · 최대HP)이 같은 칸에 섞여 있으므로
      자원표에 없는 것은 파생값으로 본다 — 그것도 아니면 여기서 잡힌다.
    """
    known = {row["KoreanName"] for row in resource_rows}
    derived = {"잃은HP", "최대HP", "준 피해", "소모한 영력", "소모한 성배", "패의 대길", "패의 길 이상", "속도초과분",
    		   "보호막 소모량"}

    missing = {}

    for row in job_rows:
        for column in ("ResourceName", "ValuePer"):
            name = (row.get(column) or "").strip()
            if not name or name == "None" or name in known or name in derived:
                continue

            # 성배·검 처럼 한 칸에 둘을 적은 것은 갈라서 본다
            if all(part.strip() in known for part in name.replace("·", ",").split(",") if part.strip()):
                continue

            missing.setdefault(name, []).append(f"{row['JobSkillId']}#{row['EffectIndex']}")

    for name, wheres in sorted(missing.items()):
        report.fail("직업 효과표",
                    f"자원표에 없는 이름 '{name}' ({len(wheres)}행 : {', '.join(wheres[:4])})")


def check_value_per_computable(job_rows, resource_rows, job_definition_by_skill, report):
    """`무엇 1당` 의 기준을 런타임이 실제로 셀 수 있는지.

    ★ 이 목록은 "자원표를 기다리는 행" 이 아니다 — 자원표는 같은 실행에서 읽었다.
      런타임이 그 기준을 못 세는 행이고, 못 세는 것은 0 이 아니라 그 줄만 빠진다
      (UNUAbilitySystemComponent::TryGetJobResource 가 거짓을 준다).
      0 으로 세면 "영력 1당 3%" 가 조용히 0% 가 되어 설계보다 약한 스킬이 생긴다.

      자원표가 Derived(파생값) · ByJobNode(전직 갈래가 정함) 라고 적은 자원과,
      아예 자원표에 없는 행동 문맥 축(준 피해 · 소모한 영력 …)이 여기 남는다.
    """
    countable = set(VALUE_PER_SELF_COMPUTABLE)

    for row in resource_rows:
        if row["GainTrigger"] not in ("Derived", "ByJobNode"):
            countable.add(row["KoreanName"])

    for row in job_rows:
        value_per = (row.get("ValuePer") or "").strip()

        if not value_per or value_per in countable:
            continue

        definition = job_definition_by_skill.get(row["JobSkillId"])
        where = f"직업 {row['JobSkillId']} #{row['EffectIndex']}"

        report.uncomputable.append(
            (where, value_per, definition["Name_KR"] if definition else ""))


def check_job_against_definition(job_rows, job_definition_by_skill, report):
    """자기 체력 소모는 정의표에도 같은 값이 있다.

    ★ 없앨 수 없는 중복이라 읽지 않는 쪽을 검증에 쓴다 — 게임은 정의표의 HP_Cost 를 읽고,
      효과표의 SelfCost 줄은 여기서 맞춰만 본다. 한쪽만 고쳐지면 여기서 걸린다.

      계수는 이 대조에서 빠졌다. 정의표 Coefficient 칸이 없어졌기 때문이다.
    """
    for row in job_rows:
        definition = job_definition_by_skill.get(row["JobSkillId"])
        if not definition:
            continue

        where = f"직업 {row['JobSkillId']} #{row['EffectIndex']}"


        # 정의표는 비율(0.1), 효과표는 백분율(10) 이라 100 을 곱해서 맞춘다
        if row["EffectType"] == "SelfCost":
            expected = Decimal(definition["HP_Cost"] or 0) * 100
            if Decimal(row["Value"] or 0) != expected:
                report.fail(where, f"자기 체력 소모가 정의표와 다르다 "
                                   f"(효과표 {row['Value']}% / 정의표 {definition['HP_Cost']})")


def check_damage_coefficients(rows, key, label, report):
    """계수는 정수다. 100 = 1배이고 두 표가 같은 규약을 쓴다.

    ★ 이 검사가 예전에는 정의표와의 대조였다 — 계수가 두 곳에 있어 서로 맞춰볼 수 있었고,
      그 대조가 실제로 1.5(150% 를 이미 나눈 값)를 잡았다. 기획이 계수를 효과표 한 곳으로
      모으면서 짝이 없어졌으므로, 맞춰보는 대신 단위 자체를 본다.
    """
    for row in rows:
        if row["EffectType"] != "Damage":
            continue

        value = Decimal(row["Value"] or 0)

        # ★ 순번을 빼고 적는다 — 줄 순서가 바뀌어도 아는 오류의 열쇠가 안 어긋난다
        where = f"{label} {row[key]}"

        if value != value.to_integral_value():
            report.fail(where, f"피해 계수가 정수가 아니다 : {row['Value']}")

        elif value <= 0:
            report.fail(where, f"피해 줄인데 계수가 {row['Value']} 다 (때려도 0이다)")


def check_resource_job_class(resource_rows, job_keys, report):
    """자원표의 JobClass 가 직업 표에 실제로 있는 이름인지.

    ★ 여기가 조용한 자리다 — 오타가 나면 그 자원의 기본 축적이 통째로 멈추는데
      오류도 경고도 안 난다. 런타임이 이름으로 직업을 가리기 때문이다
      (UNUAbilitySystemComponent::TriggerJobResourceGain).
    """
    if job_keys is None:
        return

    # 직업을 가리지 않는 자원은 Common 이라고 적는다 (자원 · AG · SP)
    known = job_keys | {"Common"}

    for row in resource_rows:
        name = (row.get("JobClass") or "").strip()

        if name and name not in known:
            report.fail(f"자원 {row['ResourceId']} {row['KoreanName']}",
                        f"직업 표에 없는 JobClass : {name}")


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


def check_job_costs(job_rows, job_definition_by_skill, report):
    """대가 없이 나가는 스킬을 잡는다.

    ★ 정의표만 보면 안 된다 — 비용이 SP·AG·HP 세 칸에만 있는 것이 아니다.
      자원으로 내는 스킬이 있고(3205 명상은 마력 응축 3), 전량 소모처럼 값이
      고정이 아니라 전용 구현이 들고 있는 것도 있다(3203 나이아 개화).
      그래서 효과표를 같이 보는 이 자리로 옮겼다.
    """
    paid = set()

    for row in job_rows:
        # 자원을 깎는 줄. 획득 줄에 음수를 적는 것이 표의 관례다 (마력 응축 -3)
        if row["bConsumeResource"] == "TRUE" or row["EffectType"] == "ResourceClear":
            paid.add(row["JobSkillId"])

        elif row["EffectType"] == "ResourceGain" and Decimal(row["Value"] or 0) < 0:
            paid.add(row["JobSkillId"])

        # 전용 구현이 붙는 줄은 비용도 그 구현이 들고 있을 수 있어 여기서 따지지 않는다
        elif row["ImplementBy"] != "Data":
            paid.add(row["JobSkillId"])

    for skill_id, definition in sorted(job_definition_by_skill.items()):
        kind = definition["SkillType"]
        where = f"직업 {skill_id} {definition['Name_KR']}"

        if kind == "필살기":
            if definition["AG_Cost"] == "0":
                report.note(f"{where} : 필살기인데 AG_Cost 가 없다")
            continue

        if kind != "액티브":
            continue

        if definition["SP_Cost"] != "0" or definition["HP_Cost"] != "0":
            continue

        if skill_id in paid:
            continue

        report.note(f"{where} : 액티브인데 SP·HP·자원 어느 것으로도 내는 것이 없다")


FATE_FIELDS = [
	"Name", "CardRateId", "ArcanaType", "LUK", "Rate_GreatFortune", "Rate_Fortune",
	"Rate_Misfortune", "Rate_GreatMisfortune", "Description",
]
FATE_ARCANA = {"Major", "Minor"}
FATE_RATE_COLUMNS = ("Rate_GreatFortune", "Rate_Fortune", "Rate_Misfortune", "Rate_GreatMisfortune")


def convert_fate(rows, report):
	"""갬블러 [운명 읽기] 카드 확률표.

	★ LUK 구간마다 한 줄이고 사이는 런타임이 선형으로 잇는다. 네 확률의 합이 100 이 아니면
	  뽑을 때 마지막 카드(대흉 · 흉)에 몰려 조용히 어긋나므로 여기서 잡는다.
	  메이저는 대흉이 없고, LUK 는 갈래 안에서 낮은 쪽부터 늘어야 한다.
	"""
	out = []
	last_luk = {}

	for row in rows:
		card_id = to_int(row["CardRateId"])
		where = f"확률표 {card_id}"

		arcana = (row.get("ArcanaType") or "").strip()
		if arcana not in FATE_ARCANA:
			report.fail(where, f"ArcanaType 에 모르는 값 '{arcana}' ({', '.join(sorted(FATE_ARCANA))})")
			continue

		luk = to_int(row["LUK"])
		if luk <= last_luk.get(arcana, -1):
			report.fail(where, f"{arcana} 의 LUK {luk} 가 앞 줄보다 크지 않다 (낮은 쪽부터 적는다)")
		last_luk[arcana] = luk

		rates = [Decimal((row.get(column) or "0").strip() or 0) for column in FATE_RATE_COLUMNS]
		if any(rate < 0 for rate in rates):
			report.fail(where, "확률이 음수다")

		total = sum(rates)
		if total != 100:
			report.fail(where, f"네 확률의 합이 {total} 이다 (100 이어야 한다)")

		if arcana == "Major" and rates[3] != 0:
			report.fail(where, "메이저 아르카나에는 대흉이 없다 (Rate_GreatMisfortune 이 0 이 아니다)")

		out.append({
			"Name": row.get("Name") or str(card_id),
			"CardRateId": card_id,
			"ArcanaType": arcana,
			"LUK": luk,
			"Rate_GreatFortune": (row.get("Rate_GreatFortune") or "0").strip(),
			"Rate_Fortune": (row.get("Rate_Fortune") or "0").strip(),
			"Rate_Misfortune": (row.get("Rate_Misfortune") or "0").strip(),
			"Rate_GreatMisfortune": (row.get("Rate_GreatMisfortune") or "0").strip(),
			"Description": (row.get("Description") or "").strip(),
		})

	for arcana in sorted(FATE_ARCANA):
		if arcana not in last_luk:
			report.fail("확률표", f"{arcana} 줄이 하나도 없다")

	return out


STATE_CONDITIONS = {"DuringState", "AfterState", "OnStateEnter", "OnStateEnd"}


def check_job_states(job_rows, resource_rows, report):
    """상태 조건 줄이 부르는 상태 이름이 자원표에 있는지, 기간이 자원표와 같은지.

    ★ 상태는 자원표의 StateName 이 정하고 효과표는 이름으로 부르므로 한 글자만 달라도 조용히 안 켜진다.
      `AfterState 번아웃:1턴` 의 1턴은 런타임이 안 읽고(상태가 켜져 있는 동안으로 읽는다) 자원표 기간과
      어긋나면 어느 쪽이 맞는지 알 수 없어 여기서 잡는다.
      상태 중 줄이 자기 기간(Turns)을 들고 있으면 상시 층이 못 읽어 안 돈다 — 실패가 아니라 알림이다
      (표 전체 임포트를 막을 일이 아니다).
    """
    states = {row["StateName"]: row for row in resource_rows if row["StateName"]}
    long_lived = []

    for row in job_rows:
        condition = row["ConditionType"]
        if condition not in STATE_CONDITIONS:
            continue

        where = f"직업 {row['JobSkillId']} #{row['EffectIndex']}"
        name, _, tail = (row["ConditionValue"] or "").partition(":")
        name = name.strip()
        state = states.get(name)

        if state is None:
            report.fail(where, f"자원표 StateName 에 없는 상태 '{name}' ({condition})")
            continue

        if condition == "AfterState":
            digits = "".join(ch for ch in tail if ch.isdigit())
            duration = int(str(state["StateDurationTurns"] or 0))

            # 기간 0 은 턴 제한이 없고 자원이 문턱 아래로 내려갈 때 끝나는 상태다 (17018 임계)
            # 이 상태의 AfterState 는 "끝난 뒤 N턴" 이라 N 이 자원표 기간과 같을 이유가 없다
            if digits and duration > 0 and int(digits) != duration:
            	report.fail(where, f"상태 '{name}' 기간 {digits}턴이 자원표 기간 {duration}턴과 다르다")

        if condition == "DuringState" and row["DurationScope"] == "Turns":
            long_lived.append(f"{row['JobSkillId']}#{row['EffectIndex']}")

    if long_lived:
        report.note(f"상태 중 줄 {len(long_lived)}행이 자기 기간(Turns)을 들고 있다 — 상태 기간은 자원표가 정하고 "
                    f"상시 층은 Permanent 만 읽어 안 돈다 ({', '.join(long_lived)})")


def check_job_bands(job_rows, report):
    """한 스킬 안에서 구간 조건이 겹치면 어느 구간인지 갈리지 않는다.

    ★ 가장 좁은 것 하나만 적용하므로, 같은 묶음에 같은 임계가 둘이면 어느 값을 쓸지 모른다.
    """
    seen = {}

    for row in job_rows:
        if row["ConditionType"] not in BAND_CONDITIONS or row["ImplementBy"] != "Data":
            continue

        key = (row["JobSkillId"], row["EffectType"], row["StackType"],
               row["ConditionType"], row["ConditionValue"])

        where = f"직업 {row['JobSkillId']} #{row['EffectIndex']}"
        if key in seen:
            report.fail(where, f"같은 구간 조건이 #{seen[key]} 와 겹친다 "
                               f"({row['ConditionType']} {row['ConditionValue']})")
            continue

        seen[key] = row["EffectIndex"]


def check_lifesteal_once(active_rows, report):
    """흡혈이 한 스킬에 두 갈래로 적히면 두 번 얹힌다.

    ★ 조건 없는 흡혈 줄은 그 타격의 스펙에 실리고(NUActiveSkill::GatherDamageParams),
      조건이 붙은 줄은 ASC 가 상태와 함께 들고 있다가 SumLifestealBonus 로 더한다.
      한 스킬에 둘 다 있으면 합쳐져서 두 배가 된다.

      예전에는 정의표 Lifesteal 칸과 맞춰봤다. 그 칸이 사라져서 효과표 안에서 본다.
    """
    kinds = defaultdict(set)

    for row in active_rows:
        if row["EffectType"] == "Lifesteal":
            kinds[row["SkillId"]].add(row["ConditionType"] == "None")

    for skill_id, seen in sorted(kinds.items()):
        if len(seen) > 1:
            # ★ 순번을 빼고 적는다 — 줄 순서가 바뀌어도 열쇠가 안 어긋난다
            report.fail(f"액티브 {skill_id}",
                        "흡혈 줄이 조건 있는 것과 없는 것으로 둘 다 있다 (두 번 얹힌다)")


def check_coverage(active_rows, passive_rows, report):
    """정의표에 있는데 효과표에 줄이 하나도 없는 행을 잡는다.

    ★ 이게 제일 조용한 고장이다 — 줄이 없으면 그 스킬·패시브는 **아무 일도 안 한다.**
      오류도 경고도 없이 그냥 안 돈다. 예전에 면역을 정의표와 맞춰보던 자리를 이걸로 바꿨다:
      정의표의 값 칸이 사라져서 맞출 짝이 없어졌고, 덮였는지 보는 쪽이 값어치가 더 크다.
    """
    checks = (
        ("액티브", DATA / "ActiveSkillTable.csv", "SkillId",
         {row["SkillId"] for row in active_rows}),
        ("패시브", DATA / "PassiveTable.csv", "PassiveId",
         {row["PassiveId"] for row in passive_rows}),
    )

    for label, path, id_column, covered in checks:
        if not path.exists():
            report.note(f"{label} 정의표가 없어 덮임 검사를 건너뛴다")
            continue

        for row in load(path):
            row_id = to_int(row[id_column])
            if row_id <= 0 or row_id in covered:
                continue

            name = (row.get("KoreanName") or "").strip()
            report.fail(f"{label} {row_id} {name}", "효과표에 줄이 하나도 없다 (아무 일도 안 한다)")


def check_status_ids(active_rows, passive_rows, report):
    status_path = DATA / "StatusEffectTable.csv"
    if not status_path.exists():
        report.note("상태이상 표가 없어 StatusEffectId 대조를 건너뛴다")
        return

    known = {to_int(r["StatusEffectId"]) for r in load(status_path)}

    for rows, label, key in ((active_rows, "액티브", "SkillId"),
                             (passive_rows, "패시브", "PassiveId")):
        for row in rows:
            sid = row["StatusEffectId"]
            if sid and sid not in known:
                report.fail(f"{label} {row[key]} #{row['EffectIndex']}",
                            f"상태이상 표에 없는 StatusEffectId {sid}")
            # 조건값이 상태 목록인 행도 같이 본다 (2410 의 "5007, 5008")
            #
            # ★ 비어 있는 것이 제일 위험하다. 코드는 목록이 비면 "조건 없음" 으로 읽어서,
            #   "감전된 적에게만 2배" 가 "아무에게나 2배" 가 된다. 오류도 안 난다
            if row["ConditionType"] == "TargetHasStatus":
                parts = [p.strip() for p in row["ConditionValue"].split(",") if p.strip()]

                if not parts:
                    report.fail(f"{label} {row[key]} #{row['EffectIndex']}",
                                "TargetHasStatus 인데 조건값이 비어 있다 (조건 없이 항상 켜진다)")

                for part in parts:
                    if to_int(part) not in known:
                        report.fail(f"{label} {row[key]} #{row['EffectIndex']}",
                                    f"조건값에 없는 상태이상 {part}")

            # 공격시 조건은 "어떤 공격일 때" 를 조건값에서 읽는다
            #
            # ★ 속성 칸에는 하나만 담기는데 조건은 "불 속성 또는 일반 공격" 처럼 여럿일 수 있어
            #   코드가 조건값을 기준으로 읽는다. 그래서 속성 칸은 대조에만 쓴다 —
            #   두 칸이 어긋나면 어느 쪽이 맞는지 표를 보는 사람이 알 수 없다
            if row["ConditionType"] == "OnAttack" and row["EffectType"] == "StatusApply":
                kinds = [p.strip() for p in row["ConditionValue"].split(",") if p.strip()]

                if not kinds:
                    report.fail(f"{label} {row[key]} #{row['EffectIndex']}",
                                "OnAttack 인데 조건값이 비어 있다 (어떤 공격인지 알 수 없다)")

                elements = [k for k in kinds if k.lower() != "normal"]
                element_tags = {ELEMENT.get(k) for k in elements}

                if None in element_tags:
                    report.fail(f"{label} {row[key]} #{row['EffectIndex']}",
                                f"조건값에 모르는 공격 종류가 있다 : {row['ConditionValue']}")

                elif row["Element"] and row["Element"] not in element_tags:
                    report.fail(f"{label} {row[key]} #{row['EffectIndex']}",
                                f"속성 칸은 {row['Element']} 인데 조건값은 {row['ConditionValue']} 다")

            # 속성 사용 조건은 속성 칸을 읽는다, 비어 있으면 켜질 공격이 없어 영영 안 돈다
            if row["ConditionType"] == "AfterElementUse" and not row["Element"]:
                report.fail(f"{label} {row[key]} #{row['EffectIndex']}",
                            "AfterElementUse 인데 속성 칸이 비어 있다 (영영 켜지지 않는다)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--book", default=None,
                        help="기획 종합본 xlsx, 주면 두 시트를 여기서 읽고 Origin CSV 도 갱신한다")
    parser.add_argument("--active", default=str(SOURCE / "ActiveSkillEffectTable_Origin.csv"))
    parser.add_argument("--passive", default=str(SOURCE / "PassiveSkillEffectTable_Origin.csv"))
    parser.add_argument("--job", default=str(SOURCE / "JobSkillEffectTable_Origin.csv"))
    args = parser.parse_args()

    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8")
        except AttributeError:
            pass

    report = Report()

    # 계수는 이제 효과표가 주인이라 맞춰볼 짝이 없다. 정의표에 남은 것은 축·속성·종류 셋뿐
    definition_by_skill = {
        to_int(r["SkillId"]): {
            "SkillType": (r["SkillType"] or "").strip(),
            "DamageType": (r["DamageType"] or "").strip(),
            "AttackElement": (r["AttackElement"] or "").strip(),
        }
        for r in load(DATA / "ActiveSkillTable.csv")
    }
    known_passives = {to_int(r["PassiveId"]) for r in load(DATA / "PassiveTable.csv")}

    # 직업 스킬은 자기 체력 소모가 정의표에도 있다, 맞춰만 본다
    job_definition_by_skill = {
        to_int(r["Skill_ID"]): {
            "HP_Cost": (r["HP_Cost"] or "0").strip(),
            "SP_Cost": (r["SP_Cost"] or "0").strip(),
            "AG_Cost": (r["AG_Cost"] or "0").strip(),
            "SkillType": (r["SkillType"] or "").strip(),
            "Name_KR": (r["Name_KR"] or "").strip(),
        }
        for r in load(DATA / "JobSkillDefinition.csv")
    }

    # 종합본을 주면 CSV 내보내기 단계를 건너뛰고 시트에서 바로 읽는다
    if args.book:
        book_path = Path(args.book)
        active_source = read_sheet(book_path, SHEETS["active"][0])
        passive_source = read_sheet(book_path, SHEETS["passive"][0])

        job_source = read_sheet(book_path, SHEETS["job"][0])
        resource_source = read_sheet(book_path, SHEETS["resource"][0], "ResourceId")
        fate_source = read_sheet(book_path, SHEETS["fate"][0], "CardRateId")

        write_origin(SOURCE / SHEETS["active"][1], active_source)
        write_origin(SOURCE / SHEETS["passive"][1], passive_source)
        write_origin(SOURCE / SHEETS["job"][1], job_source)
        write_origin(SOURCE / SHEETS["resource"][1], resource_source)
        write_origin(SOURCE / SHEETS["fate"][1], fate_source)
    else:
        active_source = load(Path(args.active))
        passive_source = load(Path(args.passive))
        job_path = Path(args.job)
        job_source = load(job_path) if job_path.exists() else []
        resource_path = SOURCE / SHEETS["resource"][1]
        resource_source = load(resource_path) if resource_path.exists() else []
        fate_path = SOURCE / SHEETS["fate"][1]
        fate_source = load(fate_path) if fate_path.exists() else []

    active_rows = convert_active(active_source, report, definition_by_skill)
    passive_rows = convert_passive(passive_source, report, known_passives)
    job_rows = convert_job(job_source, report, job_definition_by_skill)
    resource_rows = convert_resource(resource_source, report, job_definition_by_skill)
    fate_rows = convert_fate(fate_source, report)

    check_status_ids(active_rows, passive_rows, report)
    check_coverage(active_rows, passive_rows, report)
    check_lifesteal_once(active_rows, report)
    check_damage_coefficients(active_rows, "SkillId", "액티브", report)
    check_damage_coefficients(job_rows, "JobSkillId", "직업", report)
    check_job_against_definition(job_rows, job_definition_by_skill, report)
    check_job_costs(job_rows, job_definition_by_skill, report)
    check_resource_job_class(resource_rows,
                             read_job_keys(DATA / "CharacterProgression" / "JobDefinition.csv"),
                             report)
    check_job_bands(job_rows, report)
    check_job_states(job_rows, resource_rows, report)
    check_resource_names(job_rows, resource_rows, report)
    check_value_per_computable(job_rows, resource_rows, job_definition_by_skill, report)

    # 정의표에 있는데 효과 줄이 하나도 없는 직업 스킬
    job_covered = {row["JobSkillId"] for row in job_rows}
    job_missing = sorted(set(job_definition_by_skill) - job_covered) if job_rows else []
    if job_missing:
        report.fail("직업", f"효과 줄이 없는 스킬 {len(job_missing)}개 : "
                            f"{', '.join(str(i) for i in job_missing[:10])}")

    if report.code_rows:
        # ★ "안 돈다" 가 아니라 "전용 구현이 필요하다" 다. 2026-09-17 기준 아래 행은 전부 붙어 있고,
        #   이 목록은 표가 늘어날 때 "이건 코드가 있어야 한다" 를 다시 알려주는 자리로 남긴다
        print(f"[알림] ImplementBy=Code 인 행 {len(report.code_rows)}건 — 전용 구현이 붙어 있어야 도는 줄들")
        for where, effect, text in report.code_rows:
            text = text or "(설명 칸 비어 있음)"
            print(f"       {where} {effect} : {text}")

    if report.pending:
        # ★ 표가 조건을 안 적어서 지금은 돌리지 않는 행들이다 (ImplementBy=Pending)
        #   그대로 읽으면 조건 없는 상시 효과가 되어 설계보다 강한 노드가 생긴다
        print(f"\n[보류] 표가 덜 온 직업 행 {len(report.pending)}건 — 런타임이 건너뛴다")
        for where, effect, why in report.pending:
            print(f"       {where} {effect} : {why}")

    # 보류 줄이 하나도 안 걸려도 낡은 예외는 알려야 한다 — 예외가 스스로 만료되게 하는 자리
    stale_pending = (set(JOB_PENDING_NO_CONDITION) | set(JOB_PENDING_OVERRIDE)) - {
        (int(w.split()[1]), e) for w, e, _ in report.pending}
    if stale_pending:
        print(f"       [정리] 보류 목록 {len(stale_pending)}건이 이제 안 걸린다 — 지울 것")
        for skill_id, effect in sorted(stale_pending):
            print(f"              {skill_id} {effect}")

    if report.uncomputable:
        # ★ 자원표를 기다리는 행이 아니다 — 런타임이 그 기준을 못 세는 행이다
        kinds = {}
        for where, per, name in report.uncomputable:
            kinds.setdefault(per, []).append(where)
        print(f"\n[대기] 런타임이 아직 못 세는 기준 {len(report.uncomputable)}건 — 그 줄만 빠진다")
        for per in sorted(kinds):
            print(f"       {per} : {len(kinds[per])}행  ({', '.join(kinds[per][:4])}…)")

    if report.empty_values:
        print(f"\n[알림] 값이 0인 행 {len(report.empty_values)}건 — 켜져도 아무 일이 없다")
        for where, effect, text in report.empty_values:
            print(f"       {where} {effect} : {text}")

    for message in report.notes:
        print(f"[참고] {message}")

    if report.known:
        print(f"\n[아는 오류] {len(report.known)}건 — 요청서로 보냈고 지나간다")
        for where, message, why in report.known:
            print(f"       {where} : {message}")
            print(f"           -> {why}")

    stale = set(KNOWN_SHEET_ERRORS) - {f"{w}|{m}" for w, m, _ in report.known}
    if stale:
        print(f"\n[정리] 아는 오류 {len(stale)}건이 표에서 사라졌다 — 예외를 지울 것")
        for key in sorted(stale):
            print(f"       {key.split('|')[0]} : {key.split('|')[1]}")

    if report.failures:
        print(f"\n[실패] {len(report.failures)}건 — 쓰지 않았다", file=sys.stderr)
        for where, message in report.failures:
            print(f"       {where} : {message}", file=sys.stderr)
        return 1

    write(DATA / "ActiveSkillEffectTable.csv", ACTIVE_FIELDS, active_rows)
    write(DATA / "PassiveSkillEffectTable.csv", PASSIVE_FIELDS, passive_rows)

    print(f"\n[완료] 액티브 {len(active_rows)}행 -> {DATA / 'ActiveSkillEffectTable.csv'}")
    print(f"[완료] 패시브 {len(passive_rows)}행 -> {DATA / 'PassiveSkillEffectTable.csv'}")

    if job_rows:
        write(DATA / "JobSkillEffectTable.csv", JOB_FIELDS, job_rows)
        runnable = sum(1 for r in job_rows if r["ImplementBy"] == "Data")
        print(f"[완료] 직업 {len(job_rows)}행 (Data {runnable}) -> {DATA / 'JobSkillEffectTable.csv'}")

    if resource_rows:
    	write(DATA / "JobResourceTable.csv", RESOURCE_FIELDS, resource_rows)
    	print(f"[완료] 직업 자원 {len(resource_rows)}행 -> {DATA / 'JobResourceTable.csv'}")

    if fate_rows:
    	write(DATA / "FateCardRateTable.csv", FATE_FIELDS, fate_rows)
    	print(f"[완료] 운명 읽기 확률 {len(fate_rows)}행 -> {DATA / 'FateCardRateTable.csv'}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
