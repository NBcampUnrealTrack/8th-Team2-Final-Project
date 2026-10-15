// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "NUEnemyPatternTypes.generated.h"

class UPaperZDAnimSequence;

// 규칙을 후보에 넣을지 판단하는 조건 종류
UENUM(BlueprintType)
enum class ENUEnemyConditionType : uint8
{
	// Value = 0.0 ~ 1.0
	SelfHPRatioBelow	UMETA(DisplayName = "자신 체력 비율 이하"),
	SelfHPRatioAbove	UMETA(DisplayName = "자신 체력 비율 이상"),

	// Value = 턴 수
	TurnNumberAfter		UMETA(DisplayName = "N턴 이후"),
	TurnNumberBefore	UMETA(DisplayName = "N턴 이전"),

	// 내가 태그를 보유하고 있는가
	SelfHasTag			UMETA(DisplayName = "자신이 태그 보유"),

	// 상대 진영에 이 태그를 가진 전투원이 하나라도 있는가
	// 대상은 어빌리티를 고른 뒤에 정해지므로 "지정 대상"이 아니라 진영 전체를 확인함
	AnyOpponentHasTag	UMETA(DisplayName = "상대 진영에 태그 보유자 존재")
};

// 조건이 페이즈 전환 조건인지, 패턴 발동 조건인지 구분 
UENUM(BlueprintType)
enum class ENUEnemyPatternOwnerType : uint8
{
	None,
	Rule,		// 패턴 전용 조건
	Phase		// 페이즈 전환 조건
};

// 공격 패턴을 발동하기 위한 조건 하나
USTRUCT(BlueprintType)
struct PROJECTM_API FNUEnemyPatternCondition : public FTableRowBase
{
	GENERATED_BODY()

	// 이 조건이 붙는 대상
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ENUEnemyPatternOwnerType OwnerType = ENUEnemyPatternOwnerType::None;
	
	// 규칙 또는 페이즈 표의 RowName
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName OwnerId;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ENUEnemyConditionType Type = ENUEnemyConditionType::SelfHPRatioBelow;

	// 체력 비율(0~1) 또는 턴 수, 태그 관련일땐 비활성화
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition =
		"Type != ENUEnemyConditionType::SelfHasTag && Type != ENUEnemyConditionType::AnyOpponentHasTag"))
	float Value = 0.f;

	// 필요한 태그, 태그 관련일때만 활성화
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition =
		"Type == ENUEnemyConditionType::SelfHasTag || Type == ENUEnemyConditionType::AnyOpponentHasTag"))
	FGameplayTag Tag;

	// 결과를 뒤집음, 태그가 없을 때 같은 상황에 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInvert = false;
};

// 공격 패턴을 발동할 규칙 -> 조건이 모두 맞으면 이 어빌리티를 후보에 넣음
USTRUCT(BlueprintType)
struct FNUEnemyPatternRule
{
	GENERATED_BODY()

	// 사용할 어빌리티 게임플레이 태그
	// 전투원이 실제로 보유하지 않은 어빌리티면 후보에서 빠짐
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "NU.Ability.Battle"))
	FGameplayTag AbilityId;

	// 조건을 비우면 항상 후보
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FNUEnemyPatternCondition> Conditions;

	// 이 규칙을 만족하면 사용할 스킬
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 SkillId = 0;	
	
	// 높은 우선순위를 먼저 선택
	// 기획서의 "HP 50% 이하 시 점착 우선" 이 예시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 Priority = 0;

	// 같은 우선 순위라면 가중치 이용, 0 이하면 뽑히지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	// 0이면 쿨타임 없음, 3이면 사용한 턴으로부터 3턴 뒤에 다시 쓸 수 있음
	// 규칙이 아니라 어빌리티+스킬 단위로 세므로 같은 스킬을 쓰는 규칙끼리는 쿨타임을 공유함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 CooldownTurns = 0;

	// 한 전투에서 사용 가능한 횟수, 0이면 무제한, 쿨타임과 같이 어빌리티+스킬 단위로 셈
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 MaxUsesPerBattle = 0;

	// 비우면 모든 페이즈에서 후보, 페이즈 전환이 있는 보스에서만 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag PhaseTag;
	
	
	/* 패링 타이밍 별도로 지정할 때 사용 */
	
	// false면 액티브 스킬 테이블의 기본 패링 타이밍 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOverrideParryTiming = false;
	
	// OverrideParryTiming이 true일 때 사용, 패링 도전 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ChallengeDuration = 0.f;
	
	// OverrideParryTiming이 true일 때 사용, 패링 판정 위치 (0~1.0 정규화, 0.5 = 중앙)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SuccessAnchor = 0.f;
	
	// OverrideParryTiming이 true일 때 사용, 패링 판정 범위, 비율이 아니라 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SuccessWindow = 0.f;
};

// 보스 페이즈, 일반 몬스터는 비워두면 됨
USTRUCT(BlueprintType)
struct FNUEnemyPhase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag PhaseTag;

	// 페이즈 진입 조건, 비우면 항상 만족, 목록의 첫 페이즈를 기본 페이즈로 두는 데 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FNUEnemyPatternCondition> EnterConditions;

	// 진입한 턴에 강제로 제출할 행동(연출기, 광역기), 1회용 페이즈 전환 스킬
	// 비우면 그 턴도 규칙에서 고름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "NU.Ability.Battle"))
	FGameplayTag OnEnterAbilityId;
};

struct FNUEnemyActionKey
{
	FGameplayTag AbilityId;
	
	// 0이면 태그 기반 행동 (기본 공격, 페이즈 진입 등)
	int32 SkillId = 0;
	
	FNUEnemyActionKey() = default;
	FNUEnemyActionKey(const FGameplayTag& InAbilityId, int32 InSkillId)
		: AbilityId(InAbilityId), SkillId(InSkillId)
	{
	}
	
	bool operator==(const FNUEnemyActionKey& Other) const
	{
		return AbilityId == Other.AbilityId && SkillId == Other.SkillId;
	}
	
	friend uint32 GetTypeHash(const FNUEnemyActionKey& Key)
	{
		return HashCombine(GetTypeHash(Key.AbilityId), GetTypeHash(Key.SkillId));
	}
};

// 전투원 하나의 패턴 진행 상태
// 서버에서만 쓰고 클라가 알 필요가 없어 복제하지 않음, USTRUCT일 필요도 없음
struct FNUEnemyPatternRuntime
{
	// 어빌리티별로 마지막에 사용한 턴, 쿨타임 계산용
	TMap<FNUEnemyActionKey, int32> LastUsedTurn;

	// 어빌리티별 이번 전투 사용 횟수, 한 전투당 사용 횟수 계산용
	TMap<FNUEnemyActionKey, int32> UseCount;

	// 현재 페이즈, 진입 턴을 판정하려면 이전 값이 필요함
	int32 PhaseIndex = INDEX_NONE;
};

USTRUCT()
struct FNUEnemyPattern
{
	GENERATED_BODY()
	
	TArray<FNUEnemyPhase> Phases;
	
	TArray<FNUEnemyPatternRule> Rules;
	
	FGameplayTag FallbackAbilityId;

	// 인정 임계치와 그때 틀 시퀀스, 매핑 행(FNUEnemyPatternMapRow)에서 그대로 옮김
	float ConcedeHealthRatio = 0.f;
	TSoftObjectPtr<UPaperZDAnimSequence> ConcedeAnimation;
	TSoftObjectPtr<UPaperZDAnimSequence> ConcedeLoopAnimation;
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUEnemyPatternMapRow : public FTableRowBase
{
	GENERATED_BODY()
	
	// 이 패턴을 쓰는 몬스터, 조회 키임
	// 행 이름은 안 보므로 알아보기 쉬운 이름으로 지어도 되고, 비워두면 그 행은 무시됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 NaiaId = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PatternId;
	
	// 패턴 후보가 없을때 사용할 기본 어빌리티, 비우면 인카운터가 기본 공격으로 채움
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "NU.Ability.Battle"))
	FGameplayTag FallbackAbilityId;

	// 인정 임계치, 남은 체력이 최대 체력의 이 비율에 닿으면 쓰러뜨리지 않고 인정 연출 뒤 승리로 끝냄 (0 = 끝까지 싸움)
	// 대련 보스처럼 쓰러뜨리는 것이 서사와 맞지 않는 적에 씀, 이 아래로는 피해를 받아도 체력이 내려가지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ConcedeHealthRatio = 0.f;

	// 인정할 때 한 번 틀 시퀀스(몸을 낮추는 동작), 비우면 건너뜀
	// 나이아 애님 BP 와 같은 애님 소스여야 재생됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UPaperZDAnimSequence> ConcedeAnimation;

	// 그 뒤 전투가 끝날 때까지 반복할 시퀀스(낮춘 자세), 비우면 대기 그림으로 돌아감
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UPaperZDAnimSequence> ConcedeLoopAnimation;
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUEnemyPatternRuleRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PatternId;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "NU.Ability.Battle"))
	FGameplayTag AbilityId;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SkillId = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Priority = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Weight = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 CooldownTurns = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxUsesPerBattle = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag PhaseTag;
	
	
	/* 패링 타이밍 별도로 지정할 때 사용 */
	
	// false면 액티브 스킬 테이블의 기본 패링 타이밍 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOverrideParryTiming = false;
	
	// OverrideParryTiming이 true일 때 사용, 패링 도전 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ChallengeDuration = 0.f;
	
	// OverrideParryTiming이 true일 때 사용, 패링 판정 위치 (0~1.0 정규화, 0.5 = 중앙)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SuccessAnchor = 0.f;
	
	// OverrideParryTiming이 true일 때 사용, 패링 판정 범위, 비율이 아니라 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SuccessWindow = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUEnemyPhaseRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PatternId;
	
	// 위에서부터 확인해서 마지막으로 만족한 것이 현재 페이즈
	// 인덱스 순서로 확인함
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 PhaseIndex = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag PhaseTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "NU.Ability.Battle"))
	FGameplayTag OnEnterAbilityId;
};
