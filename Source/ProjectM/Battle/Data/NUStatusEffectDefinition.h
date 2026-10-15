#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "NUStatusEffectDefinition.generated.h"

// 상태이상의 기획 분류, 표시와 검증에 사용함
UENUM(BlueprintType)
enum class ENUStatusCategory : uint8
{
	None,
	DamageOverTime,		// 지속피해
	Stack,				// 스택
	ActionControl,		// 행동제어
	ResourceDrain,		// 자원소모
	DamageAmplify		// 피해증폭
};

// 효과가 언제 발동하는지
// 화상 중독은 턴종료, 폭발 감전은 스택충족
UENUM(BlueprintType)
enum class ENUStatusTickTiming : uint8
{
	None,
	Immediate,			// 즉시
	TurnStart,			// 턴시작
	TurnEnd,			// 턴종료
	OnAction,			// 행동시
	OnSkillUse,			// 스킬사용시
	OnHit,				// 피격시
	OnStackFull			// 스택충족
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUStatusEffectDefinition : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Identity")
	int32 StatusEffectId = INDEX_NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Identity")
	FName StatusEffectKey = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Identity")
	FText KoreanName;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Identity")
	FText EnglishName;
	
	// 상태이상 태그, NU.State.Ailment.XX
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Identity")
	FGameplayTag StatusEffectTag;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Classification")
	ENUStatusCategory StatusCategory = ENUStatusCategory::None;
	
	
	// 0이면 전투가 끝날때까지 유지, 스택형인 폭발과 감전이 해당함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Duration")
	int32 DurationTurns = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Duration")
	int32 MaxStack = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Duration")
	ENUStatusTickTiming TickTiming = ENUStatusTickTiming::None;
	
	
	/* 상태이상 효과, 0 또는 false 면 사용하지 않음 */
	
	// 최대HP 비율 피해, 화상 중독 폭발 감전
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Effect")
	float DamagePerTick = 0.f;
	
	// 위 피해가 대상의 아군 전체에 번지는 비율, 감전
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Effect")
	float SplashRate = 0.f;
	
	// 받는 피해 증가율, 곱연산 항으로 적용함, 균열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Effect")
	float DamageAmp = 0.f;
	
	// 모든 속성을 약점으로 강제함, 치명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Effect")
	bool bForceWeakness = false;
	
	// 행동이 실패할 확률, 환각 마비
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Effect")
	float ActionFailRate = 0.f;
	
	// 행동 자체가 불가능함, 기절
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Effect")
	bool bActionBlock = false;
	
	// 액티브 스킬만 불가능함, 기본 공격은 가능, 실명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Effect")
	bool bBlockActiveSkill = false;
	
	// 행동 순서를 항상 마지막으로 보냄, 둔화
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Effect")
	bool bForceLastAction = false;
	
	// SP 소모량 증가율, 젖음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Effect")
	float SPCostRate = 0.f;
	
	
	// 포획 성공률 보정 배율, 보정이 없으면 1.0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Capture")
	float CaptureBonus = 1.f;
	
	
	// 이 속성을 가진 대상은 해당 상태이상에 면역, 비어있으면 면역 속성 X
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Immunity")
	FGameplayTag ImmuneElement;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Status|Profile")
	FText Description;
};


// 상태이상 아이콘 한 칸을 그리는데 필요한 값만 모은 표시용 스냅샷
// 정의는 표에서, 스택과 남은 턴은 지금 걸려있는 효과에서 읽어서 합침
USTRUCT(BlueprintType)
struct PROJECTM_API FNUStatusEffectView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Status|View")
	int32 StatusEffectId = INDEX_NONE;

	// 아이콘 텍스처를 찾는 용도, 태그의 마지막이 텍스처 이름이 됨
	UPROPERTY(BlueprintReadOnly, Category = "NU|Status|View")
	FGameplayTag StatusEffectTag;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Status|View")
	FText KoreanName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Status|View")
	FText Description;

	// 아이콘 테두리 색처럼 표시를 가르는데 사용함
	UPROPERTY(BlueprintReadOnly, Category = "NU|Status|View")
	ENUStatusCategory StatusCategory = ENUStatusCategory::None;

	// 아이콘 위에 겹쳐 띄울 숫자, 0이면 표시하지 않음
	// 스택 수인지 남은 턴인지는 C++ 이 이미 갈랐으므로 위젯은 그리기만 하면 됨
	UPROPERTY(BlueprintReadOnly, Category = "NU|Status|View")
	int32 BadgeNumber = 0;

	// 위 숫자가 스택 수라면 참, 3/5 처럼 상한과 같이 쓰고 싶으면 MaxStack 을 보면 됨
	UPROPERTY(BlueprintReadOnly, Category = "NU|Status|View")
	bool bBadgeIsStack = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Status|View")
	int32 MaxStack = 1;
};


// 상태이상과 다른 테이블을 잇는 관계의 종류
UENUM(BlueprintType)
enum class ENUStatusRelationType : uint8
{
	None,
	CureItem,			// 이 상태이상을 해제하는 아이템
	ImmunePassive		// 이 상태이상에 면역이 되는 패시브
};

// 상태이상 하나를 해제하는 방법 여러 개가 붙는 관계를 행으로 담음
// 아이템과 패시브를 컬럼으로 두면 시트가 Item3_ID 를 늘릴 때마다 구조체가 바뀌므로
// 액티브 스킬과 스킬 효과처럼 서브 테이블로 분리함
USTRUCT(BlueprintType)
struct PROJECTM_API FNUStatusRelationRow : public FTableRowBase
{
	GENERATED_BODY()
	
	// 상태이상 Id, StatusEffectTable 의 RowName
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|StatusRelation")
	int32 StatusEffectId = INDEX_NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|StatusRelation")
	ENUStatusRelationType RelationType = ENUStatusRelationType::None;
	
	// RelationType 에 따라 아이템 또는 패시브 테이블의 RowName
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|StatusRelation")
	int32 ReferenceId = 0;
};
