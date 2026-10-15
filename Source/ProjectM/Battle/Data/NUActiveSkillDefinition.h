#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NUStatusEffectDefinition.h"
#include "Battle/NUBattleTypes.h"
#include "Engine/DataTable.h"
#include "NUActiveSkillDefinition.generated.h"


// 스킬 종류
UENUM(BlueprintType)
enum class ENUSkillType : uint8
{
	Attack,		// 공격
	Buff,		// 버프
	Debuff,		// 디버프
	Heal,		// 회복
	Special		// 특수 (불바다, 개화)
};

// 공격 외 부여할 효과
// 스탯증가/감소는 StatModifier로 통합
// 겹치는 이름은 ENUPassiveEffectType 과 같은 뜻으로 맞춰둠
UENUM(BlueprintType)
enum class ENUSkillEffectType : uint8
{
	None,
	StatModifier,
	Heal,
	StatusApply,
	StatusCure,
	Special,				// 옛날 표 전용, 새 표는 아래 이름들로 갈림
	Shield,
	Reflect,				// 반사 피해, 되받는 양은 언제나 가한 피해 그대로
	Damage,					// 피해, 값이 곧 계수 — 100 = 1배
	ElementPowerMod,		// 속성 위력 증감 (2005 · 2204 · 2405, 2008 은 적의 것을 깎음)
	ResourceGain,			// 자원 획득 (2007 SP)
	Lifesteal,				// 가한 피해만큼 회복 (2210)
	DamageDealtMod,			// 주는 피해 증감 (2410)
	ActionBlock,			// 그동안 행동하지 못함 (2210 개화)
	ElementDamageTakenMod,	// 그 속성으로 받는 피해 증감 (2109)
	ElementSkillBlock,		// 그 속성 스킬 봉인 (2109)
	DefenseIgnore			// 방어력 무시 비율 (2004 · 2409 · 2507)
};
	
// 효과가 어느 편으로 가는지
UENUM(BlueprintType)
enum class ENUSkillTargetScope : uint8
{
	None,
	Self,			// 시전자 자신, 대상 수와 무관하게 한 번
	Ally,			// 아군 쪽
	Enemy,			// 적 쪽
	AllEnemies		// 적 전체, 실제 범위는 TargetPolicy 가 정함
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUActiveSkillDefinition : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Identity")
	int32 SkillId = INDEX_NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Identity")
	FName SkillKey = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Identity")
	FText KoreanName;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Identity")
	FText EnglishName;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Classification")
	ENUSkillType SkillType = ENUSkillType::Attack;
	
	// NU.Damage.Type.Physical / NU.Damage.Type.Magical
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Classification")
	FGameplayTag DamageType;
	
	// NU.Element.*,  무속성이면 비어있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Classification")
	FGameplayTag AttackElement;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Cost")
	int32 SPCost = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Cost")
	ENUTargetPolicy TargetPolicy = ENUTargetPolicy::SingleEnemy;
	
	// 스킬 하나에 대상이 되는 수, 전체 대상이면 상한
	// 직업 스킬 정의표와 같은 칸이고 같은 뜻 (99 = 전체)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Cost")
	int32 TargetCount = 1;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Damage")
	int32 HitCount = 1;
	
	
	// 스킬의 효과
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Profile")
	FText EffectNote;
	
	// 스킬 설명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Profile")
	FText Description;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Skill|Profile")
	FName DescriptionKey = NAME_None;
};

// 액티브 스킬 효과, ActiveSkillEffectTable 의 행
USTRUCT(BlueprintType)
struct PROJECTM_API FNUActiveEffectRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Identity")
	int32 SkillEffectId = 0;
	
	// 이 효과를 부여하는 스킬, ActiveSkillTable 의 RowName
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Identity")
	int32 SkillId = INDEX_NONE;
	
	// 한 스킬 안에서의 순번, 1부터
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Identity")
	int32 EffectIndex = 0;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	ENUSkillEffectType EffectType = ENUSkillEffectType::None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	ENUSkillTargetScope TargetScope = ENUSkillTargetScope::None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	ENUEffectStackType StackType = ENUEffectStackType::None;
	
	// StatModifier 면 변경 대상, Heal·ResourceGain 이면 회복 대상(Health, SP)
	// Damage 면 어느 공격력을 쓰는지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	ENUStatusAffectedStat AffectedStat = ENUStatusAffectedStat::None;
	
	// bIsPercent 면 백분율(30 = 30%)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	float Value = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	bool bIsPercent = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	ENUEffectDurationScope DurationScope = ENUEffectDurationScope::Instant;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	int32 DurationTurns = 0;
	
	// 전투당 횟수 제한, 0이면 제한 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	int32 MaxPerBattle = 0;
	
	// 속성을 가리는 효과가 쓰는 칸
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Effect")
	FGameplayTag Element;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Condition")
	ENUEffectCondition ConditionType = ENUEffectCondition::None;
	
	// 조건이 값을 요구할 때 쓰는 칸, 상태이상 목록이면 쉼표로 여럿이 올 수 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Condition")
	FString ConditionValue;
	
	// StatusApply 가 사용, StatusEffectTable 의 RowName
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Condition")
	int32 StatusEffectId = 0;
	
	// 상태이상이 걸릴 확률
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Condition", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 0.f;
	
	// Code 면 전용 구현이 있어야 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActiveEffect|Condition")
	ENUEffectImplementBy ImplementBy = ENUEffectImplementBy::Data;
	
	
	// 계산에 쓸 값, 백분율이면 여기서 한 번만 나눔
	float GetScaledValue() const
	{
		return bIsPercent ? Value / 100.f : Value;
	}
	
	// 조건이 요구하는 횟수, 표가 "번개:1" 처럼 적어 보내므로 뒤의 숫자만 읽음
	// 숫자가 없으면 1로 봄 - 조건이 붙었는데 0이면 조건이 없는 것과 같음
	int32 GetRequiredUses() const
	{
		int32 Colon = INDEX_NONE;
		
		if (ConditionValue.FindLastChar(TEXT(':'), Colon))
		{
			const FString Tail = ConditionValue.Mid(Colon + 1).TrimStartAndEnd();
			
			if (Tail.IsNumeric())
			{
				return FCString::Atoi(*Tail);
			}
		}
		
		return 1;
	}
	
	// 조건이 요구하는 상태이상 번호들, 표가 "5007, 5008" 처럼 쉼표로 여럿을 적어 보냄
	void GetConditionStatusIds(TArray<int32>& OutIds) const
	{
		OutIds.Reset();
		
		TArray<FString> Tokens;
		ConditionValue.ParseIntoArray(Tokens, TEXT(","), true);
		
		for (const FString& Token : Tokens)
		{
			const FString Trimmed = Token.TrimStartAndEnd();
			
			if (Trimmed.IsNumeric())
			{
				OutIds.Add(FCString::Atoi(*Trimmed));
			}
		}
	}
	
	// 조건이 요구하는 공격 종류, 표가 "Fire,Normal" 처럼 적어 보냄
	void GetConditionAttackKinds(FGameplayTagContainer& OutElements, bool& bOutIncludeBasicAttack) const
	{
		OutElements.Reset();
		bOutIncludeBasicAttack = false;
		
		TArray<FString> Tokens;
		ConditionValue.ParseIntoArray(Tokens, TEXT(","), true);
		
		for (const FString& Token : Tokens)
		{
			const FString Trimmed = Token.TrimStartAndEnd();
			
			if (Trimmed.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
			{
				bOutIncludeBasicAttack = true;
				continue;
			}
			
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(
				FName(*FString::Printf(TEXT("NU.Element.%s"), *Trimmed)), false);
			
			if (Tag.IsValid())
			{
				OutElements.AddTag(Tag);
			}
		}
	}
	
	// 실제로 유지할 턴 수
	// 턴 범위일 때만 턴 수를 쓰고 나머지는 0이다. 0을 받는 쪽들이 그것을
	// "전투가 끝날 때까지" 로 읽으므로 Battle 범위도 같은 0으로 떨어진다
	int32 GetDurationTurns() const
	{
		return DurationScope == ENUEffectDurationScope::Turns ? DurationTurns : 0;
	}
};

// 패링 가능한 스킬들의 패링 정보 행, 없으면 회피
USTRUCT(BlueprintType)
struct PROJECTM_API FNUSkillParryDefinition : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Parry")
	int32 SkillId = INDEX_NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Parry")
	float ChallengeDuration = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Parry")
	float SuccessAnchor = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Parry")
	float SuccessWindow = 0.15f;
};

