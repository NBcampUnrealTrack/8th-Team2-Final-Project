#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "Engine/DataTable.h"
#include "NUJobSkillDefinition.generated.h"


// 직업 스킬이 무엇을 하는지
// 겹치는 이름은 ENUSkillEffectType · ENUPassiveEffectType 과 같은 뜻으로 맞춰둠
UENUM(BlueprintType)
enum class ENUJobEffectType : uint8
{
	None,
	Damage,				 // 피해, 값이 곧 계수 — 100 = 1배
	DamageDealtMod,		 // 주는 피해 증감
	DamageTakenMod,		 // 받는 피해 증감
	StatModifier,		 // 스탯 증감
	Heal,				 // 회복
	Shield,				 // 보호막
	ResourceGain,		 // 자원 획득, 직업별 자원 10종은 아직 표가 없음
	SelfCost,			 // 자기 체력 소모
	DebuffStack,		 // 대상에게 쌓이는 중첩 (약점 · 마크)

	// 전부 전용 구현이 필요해 표도 ImplementBy 를 Code_* 로 적어 보냄
	ResourceGainMod,	 // 자원 획득량 증감 (3127 각성 촉진 — AG 턴당 20 -> 29)
	ResourceGainCap,	 // 자원 획득량을 고정 (3230 초과 부하 — 마력 응축 턴당 1)
	ResourceClear,		 // 자원 전량 소멸 (3049 정적)
	ResourceDecayImmune, // 자원이 줄지 않음 (3035 영력 고정 · 3251 마나 타이드)
	DeathPrevent,		 // 죽을 피해를 한 번 버팀 (3027 · 3044 · 3448)
	ShieldMod,			 // 보호막 증감 (3444 · 3447 — 아군 보호막 2배)
	HealReceivedMod,	 // 받는 회복량 증감 (3034 극점 — 30% 감소)
	ForceCritical,		 // 반드시 치명타로 적중 (3121 나이아의 유도)
	StatusDurationMod,	 // 건 상태의 지속 턴 증감 (3323 계시 확산)
	CardFloor,			 // 뽑는 카드의 하한 (3349 아르카나 순환)
	SkillCostMod,		 // 스킬 비용 증감 (3301 운명 읽기 — 뽑은 카드에 따라 SP -40% / +30%)
 
	Special,			 // 전용 구현, ImplementBy 가 갈래를 적어줌
	CoefficientAdd,		 // 기본 계수에 %p 가산, 자원에 비례 (3029 영력 1당 +3%p)

	CoefficientMul,		 // 기본 계수에 배수를 곱함, 값이 곧 배수 (3013 반전 x1.5 · 3017 쌍격 x1.25)
	DefenseIgnore,		 // 대상 방어력을 일부 무시, 값은 백분율 (3025 · 3110 · 3210 · 3410 — 50%)
	HPCost,				 // 상태가 자기 최대 HP 를 소모함 (3007 공진 중 매 턴 10.8%)
	ResourceTimerReset,	 // 자원의 자연 소멸 대기를 처음부터 다시 셈 (3137 — 마크 3턴)
	Taunt				 // 적의 공격 대상을 자신으로 고정 (3402 야수 위압)
};

// 효과가 누구에게 가는지
UENUM(BlueprintType)
enum class ENUJobTargetScope : uint8
{
	None,
	Self,
	Ally,				// 자신과 나이아를 뺀 아군 캐릭터, 액티브 줄과 아군 보호막 배수만 이 뜻으로 거름
	Enemy,
	AllyAndNaia,		// 자신을 뺀 같은 편 전원, 나이아 포함 (3423 고양 확산)
	Party				// 자신 · 아군 · 나이아 전원 (3308 성배 방출 · 3201 · 3303 시너지)
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUJobSkillDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	int32 Skill_ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	FName Skill_Key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	FText Name_KR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	FName Class;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	FName Branch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	FName Tree;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	FName NodeType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	FName SkillType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	FName Choice;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	int32 UnlockLv = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	FName MainStat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	int32 SP_Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	int32 AG_Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	float HP_Cost = 0.f;

	// 쿨타임 턴, 0이면 없음 (3228 · 3402)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	int32 CooldownTurns = 0;

	// 대상 범위, 자신 / 단일적 / 전체적 / 단일아군 / 전체아군 다섯 가지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	ENUTargetPolicy TargetPolicy = ENUTargetPolicy::SingleEnemy;

	// 스킬 하나에 대상이 되는 타겟 수, 전체 대상 스킬이면 상한
	// 지금은 TargetPolicy 만으로 갈리지만 적 2체 같은 스킬이 오면 이 변수가 필요함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	int32 TargetCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	int32 HitCount = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source")
	float DealContrib = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job Skill|Source", meta = (MultiLine = "true"))
	FText Description;
};


// 직업 스킬 효과, JobSkillEffectTable 의 RowName 이 SkillEffectId 가 됨
// 한 스킬이 여러 줄을 가지며 EffectIndex 로 갈림
USTRUCT(BlueprintType)
struct PROJECTM_API FNUJobSkillEffectRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Identity")
	int32 SkillEffectId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Identity")
	int32 JobSkillId = INDEX_NONE;

	// 한 스킬 안에서의 순번, 1부터
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Identity")
	int32 EffectIndex = 0;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Effect")
	ENUJobEffectType EffectType = ENUJobEffectType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Effect")
	ENUJobTargetScope TargetScope = ENUJobTargetScope::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Effect")
	ENUEffectStackType StackType = ENUEffectStackType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Effect")
	ENUStatusAffectedStat AffectedStat = ENUStatusAffectedStat::None;

	// bIsPercent 면 백분율(30 = 30%)이지 비율이 아님
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Effect")
	float Value = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Effect")
	bool bIsPercent = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Effect")
	ENUEffectDurationScope DurationScope = ENUEffectDurationScope::Instant;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Effect")
	int32 DurationTurns = 0;

	// 전투당 횟수 제한, 0이면 제한 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Effect")
	int32 MaxPerBattle = 0;


	// "무엇 1당" 인지, 비어 있으면 고정값
	// 잃은HP · 최대HP 만 지금 계산할 수 있고 자원 이름은 자원표가 와야 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Scaling")
	FName ValuePer = NAME_None;

	// 그 단위 하나의 크기, 0이나 1이면 1당 (ex) 최대HP 250당 이면 250)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Scaling")
	float ValuePerUnit = 0.f;

	// 쌓인 값의 상한, 0이면 무제한. Value 와 같은 단위라 백분율이면 같이 나눔
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Scaling")
	float ValueCap = 0.f;

	// 비례해서 쓴 자원을 소모하는지, 같은 "1당" 이라도 3233 응축 파열은 쓰고 3229 마력 방출은 안 씀
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Scaling")
	bool bConsumeResource = false;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Condition")
	ENUEffectCondition ConditionType = ENUEffectCondition::None;

	// 조건이 값을 요구할 때 쓰는 칸, "약점:5" · "자원:50%" 처럼 이름과 수가 같이 옴
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Condition")
	FString ConditionValue;

	// 자원 이름, 직업별 자원 표가 오면 그 표의 이름 칸과 이어짐
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Condition")
	FName ResourceName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Condition")
	int32 StatusEffectId = 0;

	// Data 가 아니면 전용 구현이 있어야 하고, Pending 은 표가 아직 덜 온 행임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobEffect|Condition")
	ENUEffectImplementBy ImplementBy = ENUEffectImplementBy::Data;


	// 계산에 쓸 값, 백분율이면 여기서 한 번만 나눔
	float GetScaledValue() const
	{
		return bIsPercent ? Value / 100.f : Value;
	}

	// 상한도 Value 와 같은 단위
	float GetScaledCap() const
	{
		return bIsPercent ? ValueCap / 100.f : ValueCap;
	}

	// 단위 크기, 표가 비워 두면 1당 x증가라는 뜻
	float GetPerUnit() const
	{
		return ValuePerUnit > 0.f ? ValuePerUnit : 1.f;
	}

	// 조건 칸의 이름, "약점:5" 의 약점 · "성배+검:8" 의 성배+검 · "영력" 의 영력
	// 숫자만 적힌 칸(체력 조건 등)은 이름이 아니므로 비워서 돌려줌
	FName GetConditionResourceName() const
	{
		FString Head = ConditionValue;
		
		int32 Colon = INDEX_NONE;
		if (Head.FindLastChar(TEXT(':'), Colon))
		{
			Head = Head.Left(Colon);
		}
		
		Head = Head.TrimStartAndEnd();
		
		return Head.IsEmpty() || Head.IsNumeric() ? NAME_None : FName(*Head);
	}

	// 조건 칸의 수치, "약점:5" 의 5 · "70" 의 70 · "자원:50%" 의 50. 없으면 0
	// 꼬리의 % 와 턴 같은 글자는 떼고 읽음, 표가 사람이 읽을 말로 적어 보냄
	float GetConditionNumber() const
	{
		FString Tail = ConditionValue;

		int32 Colon = INDEX_NONE;
		if (Tail.FindLastChar(TEXT(':'), Colon))
		{
			Tail = Tail.Mid(Colon + 1);
		}

		Tail = Tail.TrimStartAndEnd();

		// 숫자로 읽히는 앞부분만 남김
		int32 Length = 0;
		while (Length < Tail.Len()
			&& (FChar::IsDigit(Tail[Length]) || Tail[Length] == TEXT('.') || Tail[Length] == TEXT('-')))
		{
			++Length;
		}

		return Length > 0 ? FCString::Atof(*Tail.Left(Length)) : 0.f;
	}

	// 조건 칸의 이름, "약점:5" 의 약점 · "공진" 의 공진. 숫자뿐이면 빈 값
	FName GetConditionName() const
	{
		int32 Colon = INDEX_NONE;
		if (ConditionValue.FindLastChar(TEXT(':'), Colon))
		{
			return FName(*ConditionValue.Left(Colon).TrimStartAndEnd());
		}

		const FString Trimmed = ConditionValue.TrimStartAndEnd();

		return (Trimmed.IsEmpty() || Trimmed.IsNumeric()) ? NAME_None : FName(*Trimmed);
	}

	// 이 줄을 거르는 조건이 있는지, 조건이 있어도 사용 제한(턴당 1회 · 첫 행동)은 뺌
	// 한 턴에 명령이 하나라 사용 제한은 늘 지켜지므로 조건 없는 줄과 같이 다룸 (2026-09-29 확정)
	bool HasGatingCondition() const
	{
		return ConditionType != ENUEffectCondition::None
			&& ConditionType != ENUEffectCondition::OncePerTurn
			&& ConditionType != ENUEffectCondition::OncePerTurnFirstAction;
	}
};

// 보유 직업 스킬id와 효과 묶음
struct FNUOwnedJobEffect
{
	int32 JobSkillId = 0;
	FNUJobSkillEffectRow Row;
};
