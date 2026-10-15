// Fill out your copyright notice in the Description page of Project Settings.


#include "NUGameplayTags.h"

namespace  NU
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(None, "NU.None", "빈 태그 — 유효한 값이 없음을 나타냄");

    namespace Ability
    {
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dead, "NU.Ability.Dead", "어빌리티 — 사망 처리")

        namespace Field
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact, "NU.Ability.Field.Interact", "필드 어빌리티 — 상호작용")
        }

        namespace Battle
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(BasicAttack, "NU.Ability.Battle.BasicAttack", "전투 어빌리티 — 기본 공격")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(UseItem,     "NU.Ability.Battle.UseItem",     "전투 어빌리티 — 아이템 사용")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flee,        "NU.Ability.Battle.Flee",        "전투 어빌리티 — 도주")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(ActiveSkill, "NU.Ability.Battle.ActiveSkill", "전투 어빌리티 — 액티브 스킬 공용 태그")

            // 나이아 전용 Ability
            namespace Naia
            {
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(SelfHeal, "NU.Ability.Battle.Naia.SelfHeal", "나이아 전투 어빌리티 — 자기 회복")
            }
            
            // 적 전용 Ability
            namespace Enemy
            {
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(Telegraph, "NU.Ability.Battle.Enemy.Telegraph", "적 전투 어빌리티 — 큰 기술 앞 경고, 피해 없이 한 턴을 씀")
            }
        }
    }

    namespace Event
    {
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dead, "NU.Event.Dead", "이벤트 — 사망")

        namespace Battle
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(BasicAttack, "NU.Event.Battle.BasicAttack", "전투 이벤트 — 기본 공격 발동")
        }
    }

    namespace Context
    {
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Field,  "NU.Context.Field",  "컨텍스트 — 플레이어가 필드에 있음")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Battle, "NU.Context.Battle", "컨텍스트 — 플레이어가 전투 중")
    }

    namespace State
    {
        namespace Field
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interacting, "NU.State.Field.Interacting", "필드 상태 — 상호작용 중")
        }

        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dead,        "NU.State.Dead",        "상태 — 사망")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Parried,     "NU.State.Parried",     "상태 — 패링 성공")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dodging,     "NU.State.Dodging",     "상태 — 회피 성공, SP를 소모해 패링 불가 공격을 무효화함")
        
        namespace Ailment
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root, "NU.State.Ailment", "상태이상 루트 — 개별 상태를 모를 때 확인용")
            
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Burn,          "NU.State.Ailment.Burn",          "상태이상 — 화상, 5턴 턴종료마다 최대HP 1/15 피해")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Explosion,     "NU.State.Ailment.Explosion",     "상태이상 — 폭발 스택, 5스택에서 최대HP 1/5 피해")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slow,          "NU.State.Ailment.Slow",          "상태이상 — 둔화, 3턴 행동을 가장 마지막에 실행")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Wetness,       "NU.State.Ailment.Wetness",       "상태이상 — 젖음, 3턴 SP 소모량이 30% 증가")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Poison,        "NU.State.Ailment.Poison",        "상태이상 — 중독, 4턴 턴종료마다 최대HP 1/8 피해")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hallucination, "NU.State.Ailment.Hallucination", "상태이상 — 환각, 3턴 30% 확률로 아군 1명을 공격")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Electrocution, "NU.State.Ailment.Electrocution", "상태이상 — 감전 스택, 4스택에서 최대HP 1/6 피해, 아군에겐 피해의 40퍼 전이")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Paralysis,     "NU.State.Ailment.Paralysis",     "상태이상 — 마비, 5턴 40% 확률로 행동 실패")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stun,          "NU.State.Ailment.Stun",          "상태이상 — 기절, 1턴 행동 불가")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Fracture,      "NU.State.Ailment.Fracture",      "상태이상 — 균열, 3턴 데미지를 받을때 50퍼 추가로 받음")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Blind,         "NU.State.Ailment.Blind",         "상태이상 — 실명, 2턴 액티브 스킬 사용 금지")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Fatal,         "NU.State.Ailment.Fatal",         "상태이상 — 치명, 1턴 모든 속성에 약점 판정")
        }
        
    }

    namespace Damage
    {
        namespace Type
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Physical, "NU.Damage.Type.Physical", "데미지 타입 — 물리, 공격력을 기준으로 계산")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Magical,  "NU.Damage.Type.Magical",  "데미지 타입 — 마법, 마법 공격력을 기준으로 계산")
        }

        namespace Base
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(MaxHealth, "NU.Damage.Base.MaxHealth", "계수의 기준 — 시전자의 최대 HP 비율")
        }
    }

    namespace Element
    {
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root, "NU.Element", "속성 루트 — 속성 확인하는 용도")
        
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Fire,      "NU.Element.Fire",      "속성 — 불")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Water,     "NU.Element.Water",     "속성 — 물")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Grass,     "NU.Element.Grass",     "속성 — 풀")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Lightning, "NU.Element.Lightning", "속성 — 번개")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Earth,     "NU.Element.Earth",     "속성 — 땅")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Wind,      "NU.Element.Wind",      "속성 — 바람")
    }

    namespace Data
    {
        namespace Initial
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(MaxHealth, "NU.Data.Initial.MaxHealth", "초기화 GE SetByCaller — 최대 체력");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Health,    "NU.Data.Initial.Health",    "초기화 GE SetByCaller — 현재 체력");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(MaxSP,     "NU.Data.Initial.MaxSP",     "초기화 GE SetByCaller — 최대 SP");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(SP,        "NU.Data.Initial.SP",        "초기화 GE SetByCaller — 현재 SP");

            UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackPower,      "NU.Data.Initial.AttackPower",      "초기화 GE SetByCaller — 공격력");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(MagicAttackPower, "NU.Data.Initial.MagicAttackPower", "초기화 GE SetByCaller — 마법 공격력");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Defense,          "NU.Data.Initial.Defense",          "초기화 GE SetByCaller — 방어력");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Speed,            "NU.Data.Initial.Speed",            "초기화 GE SetByCaller — 속도");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(CriticalRate,     "NU.Data.Initial.CriticalRate",     "초기화 GE SetByCaller — 치명타 확률");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(CriticalDamage,   "NU.Data.Initial.CriticalDamage",   "초기화 GE SetByCaller — 치명타 피해");
        }

        namespace Damage
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Coefficient,   "NU.Data.Damage.Coefficient",   "데미지 GE SetByCaller — 데미지 계수");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(DefenseIgnore, "NU.Data.Damage.DefenseIgnore", "데미지 GE SetByCaller — 방어력 무시 비율(0~1)");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Lifesteal,     "NU.Data.Damage.Lifesteal",     "데미지 GE SetByCaller — 흡혈 비율, 가한 피해에 곱함");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(AdditiveBonus, "NU.Data.Damage.AdditiveBonus", "데미지 GE SetByCaller — 합연산 풀에 더할 이 행동만의 값(0.2 면 +20%)");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Multiplier,    "NU.Data.Damage.Multiplier",    "데미지 GE SetByCaller — 곱연산 항 하나, 1 이면 곱하지 않음");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(ForceCritical, "NU.Data.Damage.ForceCritical", "데미지 GE SetByCaller — 1 이면 반드시 치명타 (3121 나이아의 유도)");
        }

        namespace Skill
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Heal, "NU.Data.Skill.Heal", "스킬 회복 GE SetByCaller — 회복량");
        }

        namespace Recovery
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Health, "NU.Data.Recovery.Health", "회복 지점 GE SetByCaller — 최종 체력");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(SP, "NU.Data.Recovery.SP", "거점 부활 시 최종 SP");
        }

        namespace StatusEffect
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(TickDamage, "NU.Data.StatusEffect.TickDamage", "상태이상 GE SetByCaller — 턴 종료 피해량");
            
            // 스탯 배율, 0.7이면 30퍼 감소한 것
            // 사용하지 않는 스탯은 1을 채워야 함, 비우면 SetByCaller가 0으로 변경함
            namespace Multiplier
            {
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(Speed,            "NU.Data.StatusEffect.Multiplier.Speed",            "상태이상 GE SetByCaller — 속도 배율");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(Defense,          "NU.Data.StatusEffect.Multiplier.Defense",          "상태이상 GE SetByCaller — 방어력 배율");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(CriticalRate,     "NU.Data.StatusEffect.Multiplier.CriticalRate",     "상태이상 GE SetByCaller — 치명타 확률 배율");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(CriticalDamage,   "NU.Data.StatusEffect.Multiplier.CriticalDamage",   "상태이상 GE SetByCaller — 치명타 피해 배율");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackPower,      "NU.Data.StatusEffect.Multiplier.AttackPower",      "상태이상 GE SetByCaller — 공격력 배율");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(MagicAttackPower, "NU.Data.StatusEffect.Multiplier.MagicAttackPower", "상태이상 GE SetByCaller — 마법 공격력 배율");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(MaxHealth,        "NU.Data.StatusEffect.Multiplier.MaxHealth",        "상태이상 GE SetByCaller — 최대 체력 배율");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(MaxSP,            "NU.Data.StatusEffect.Multiplier.MaxSP",            "상태이상 GE SetByCaller — 최대 SP 배율");
            }

            // 스탯 고정값 증감, 0.2면 0.2만큼 더한 것
            // 사용하지 않는 스탯은 0을 채워야 함
            namespace Additive
            {
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(Speed,            "NU.Data.StatusEffect.Additive.Speed",            "상태이상 GE SetByCaller — 속도 증감");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(Defense,          "NU.Data.StatusEffect.Additive.Defense",          "상태이상 GE SetByCaller — 방어력 증감");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(CriticalRate,     "NU.Data.StatusEffect.Additive.CriticalRate",     "상태이상 GE SetByCaller — 치명타 확률 증감");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(CriticalDamage,   "NU.Data.StatusEffect.Additive.CriticalDamage",   "상태이상 GE SetByCaller — 치명타 피해 증감");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackPower,      "NU.Data.StatusEffect.Additive.AttackPower",      "상태이상 GE SetByCaller — 공격력 증감");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(MagicAttackPower, "NU.Data.StatusEffect.Additive.MagicAttackPower", "상태이상 GE SetByCaller — 마법 공격력 증감");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(MaxHealth,        "NU.Data.StatusEffect.Additive.MaxHealth",        "상태이상 GE SetByCaller — 최대 체력 증감");
                UE_DEFINE_GAMEPLAY_TAG_COMMENT(MaxSP,            "NU.Data.StatusEffect.Additive.MaxSP",            "상태이상 GE SetByCaller — 최대 SP 증감");
            }
        }
    }

    namespace Marker
    {
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(QuestOffer,      "NU.Marker.Quest.Offer",      "마커 — 퀘스트 수락 가능")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(QuestLocked,     "NU.Marker.Quest.Locked",     "마커 — 선행 조건 미달 퀘스트")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(QuestInProgress, "NU.Marker.Quest.InProgress", "마커 — 퀘스트 진행 중")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(QuestTurnIn,     "NU.Marker.Quest.TurnIn",     "마커 — 퀘스트 보고 가능")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(QuestTarget,     "NU.Marker.Quest.Target",     "마커 — 진행 중 퀘스트의 목표")
    }

    namespace Interaction
    {
        namespace Option
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root,      "NU.Interaction.Option",          "상호작용 선택지 루트")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dialogue,  "NU.Interaction.Option.Dialogue", "상호작용 선택지 — 대화")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Quest,     "NU.Interaction.Option.Quest",    "상호작용 선택지 — 퀘스트")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Job,       "NU.Interaction.Option.Job",      "상호작용 선택지 — 직업")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(NaiaContract, "NU.Interaction.Option.NaiaContract", "상호작용 선택지 — 스타팅 나이아 계약")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shop,      "NU.Interaction.Option.Shop",     "상호작용 선택지 — 상점")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Storage,   "NU.Interaction.Option.Storage",  "상호작용 선택지 — 창고")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Recovery,  "NU.Interaction.Option.Recovery", "상호작용 선택지 — 회복 지점")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Crafting,  "NU.Interaction.Option.Crafting", "상호작용 선택지 — 제작")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Encounter, "NU.Interaction.Option.Encounter", "상호작용 선택지 — 전투")
        }
    }

    namespace Quest
    {
        namespace Category
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root,     "NU.Quest.Category",          "퀘스트 분류 루트")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Main,     "NU.Quest.Category.Main",     "퀘스트 분류 — 메인")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Side,     "NU.Quest.Category.Side",     "퀘스트 분류 — 서브")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Daily,    "NU.Quest.Category.Daily",    "퀘스트 분류 — 일일")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tutorial, "NU.Quest.Category.Tutorial", "퀘스트 분류 — 튜토리얼")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Job,      "NU.Quest.Category.Job",      "퀘스트 분류 — 직업")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Craft,    "NU.Quest.Category.Craft",    "퀘스트 분류 — 생산")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hidden,   "NU.Quest.Category.Hidden",   "퀘스트 분류 — 히든")
        }

        namespace Requirement
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(PlayerLevel,              "NU.Quest.Requirement.PlayerLevel",              "선행 조건 — 최소 플레이어 레벨")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(QuestCompleted,           "NU.Quest.Requirement.QuestCompleted",           "선행 조건 — 특정 퀘스트 완료")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(QuestGroupCompletedCount, "NU.Quest.Requirement.QuestGroupCompletedCount", "선행 조건 — 퀘스트 그룹 완료 개수")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(FlagSet,                  "NU.Quest.Requirement.FlagSet",                  "선행 조건 — 특정 플래그 보유")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(FlagNotSet,               "NU.Quest.Requirement.FlagNotSet",               "선행 조건 — 특정 플래그 미보유")
        }

        namespace Objective
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root, "NU.Quest.Objective", "퀘스트 목표 루트")
            
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Talk,          "NU.Quest.Objective.Talk",          "목표 — 대상과 대화")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(UseObject,     "NU.Quest.Objective.UseObject",     "목표 — 대상과 상호작용")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(CollectItem,   "NU.Quest.Objective.CollectItem",   "목표 — 아이템 획득")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(PurchaseItem,  "NU.Quest.Objective.PurchaseItem",  "목표 — 상점에서 아이템 구매")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(UseStorage,    "NU.Quest.Objective.UseStorage",     "목표 — 창고 이용")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(EnemyDefeated, "NU.Quest.Objective.EnemyDefeated", "목표 — 적 1마리 처치")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(BattleWon,     "NU.Quest.Objective.BattleWon",     "목표 — 전투 승리")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(ReachLocation, "NU.Quest.Objective.ReachLocation", "목표 — 위치 도달")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(GrowNaia,      "NU.Quest.Objective.GrowNaia",      "목표 — 나이아 육성")
        }

        namespace Reward
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Experience,    "NU.Quest.Reward.Experience",    "보상 — 경험치 지급")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Gold, "NU.Quest.Reward.Gold", "보상 — 골드 지급")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item, "NU.Quest.Reward.Item", "보상 — 아이템 지급")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetFlag,       "NU.Quest.Reward.SetFlag",       "보상 — 플래그 설정")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(UnlockFeature, "NU.Quest.Reward.UnlockFeature", "보상 — 기능 해금")
        }
    }
    
    namespace Enemy
    {
        namespace Phase
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(First,  "NU.Enemy.Phase.First",  "보스 페이즈 — 첫 페이즈 (기본)")
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Second, "NU.Enemy.Phase.Second", "보스 페이즈 — 두 번째 페이즈")
        }
    }
}

namespace GameplayCue
{
    // 전투에서 사용할 GameplayCue
    namespace Battle
    {
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hit,       "GameplayCue.Battle.Hit",       "전투 큐 — 피격 시 대상 중심에 고리 이펙트")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Heal,      "GameplayCue.Battle.Heal",      "전투 큐 — HP 회복 시 대상에게 이펙트")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(SPRestore, "GameplayCue.Battle.SPRestore", "전투 큐 — SP 회복 시 대상에게 이펙트")
        UE_DEFINE_GAMEPLAY_TAG_COMMENT(Death,     "GameplayCue.Battle.Death",     "전투 큐 — 적이 죽어 가루가 될 때 대상 자리에 이펙트")
    }

}
