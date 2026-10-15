#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NUJobResourceDefinition.generated.h"


// 자원이 누구에게 쌓이는지
// 마크/약점/문양은 적에게 붙고 나머지(동조율, 템포, 성배, 검 등)는 자신에게 쌓임
UENUM(BlueprintType)
enum class ENUJobResourceOwner : uint8
{
	Self,
	Target
};

// 상한이 스탯에 비례할 때 쓰는 축
// 표는 `속도 / 2` 처럼 사람이 읽는 식도 같이 주지만, 코드는 이 셋만 읽음
UENUM(BlueprintType)
enum class ENUJobResourceScaleStat : uint8
{
	None,
	Speed,			// 템포 — 번아웃 한계치가 속도 / 2
	Intelligence,	// 문양 — 상한이 3 + INT / 10
	SP
};

// 자원이 언제 쌓이는지
UENUM(BlueprintType)
enum class ENUJobResourceGainTrigger : uint8
{
	None,
	OnSelfHPCost,	// 자기 최대 HP 를 소모했을 때 (영력 · 동조율)
	OnAttackHit,	// 공격이 맞았을 때 (템포)
	OnSingleHit,	// 단발 공격이 맞았을 때 (마크)
	OnMagicDamage,	// 마법 피해를 줬을 때 (마력 응축 · 약점)
	OnSkillUse,		// 스킬을 썼을 때 (문양)
	OnCardDrawn,	// 카드를 뽑았을 때 (패 · 성배 · 검)
	OnHitTaken,		// 맞았을 때 (응전)
	OnTurnStart,	// 턴이 열릴 때 (카드 · SP)
	OnTurnEnd,		// 턴이 닫힐 때 (동화율 · AG)
	OnParrySuccess,	// 패링에 성공했을 때 (AG 의 둘째 통로)

	// 표가 값을 못 적는 둘, 코드가 정함
	ByJobNode,		// 취득한 전직 노드를 보고 결정 (공용 줄기의 자원)
	Derived			// 쌓이지 않고 매번 계산됨 (속도차이)
};

// 자원이 언제 줄어드는지
UENUM(BlueprintType)
enum class ENUJobResourceDecay : uint8
{
	None,			// 줄지 않음, 전투 끝까지 유지
	PerTurn,		// 매 턴 DecayAmount 만큼 (문양)
	TimeoutAll,		// DecayInterval 턴 동안 안 쌓이면 전량 소멸 (마크)
	TimeoutEach,	// 중첩마다 따로 만료 (약점)
	Redraw,			// 다시 뽑으면 갈린다 (카드)
	ByJobNode,
	Derived,
	OnMagicUse		// 마법을 쓰면 0 이 된다, 필살기 · 기본 공격은 제외 (임계)
	};

	// 상한에 도달하면 어떻게 되는지
UENUM(BlueprintType)
enum class ENUJobResourceOnMax : uint8
{
	Keep,			// 그대로 멈춤
	EnterState,		// 특정 상태로 전환됨 (동조율 7 -> [공진])
	BurnOut,		// 전량 소멸하고 페널티 (템포 -> [번아웃])
	PushOldest,		// 오래된 것부터 밀어냄 (패 3장)
	UnlockUltimate,	// 필살기가 열림 (AG 100)
	ByJobNode
};

// 전투가 끝나면
UENUM(BlueprintType)
enum class ENUJobResourceOnBattleEnd : uint8
{
	Reset,
	Keep
};

// 자원이 상태에 들어가는 조건 (동조율 -> [공진] · 템포 -> [번아웃] · 문양 -> [지맥 붕괴])
UENUM(BlueprintType)
enum class ENUJobStateEnterRule : uint8
{
	None,
	AtValue,		// StateEnterValue 에 닿을 때 (동조율 7)
	AtMax,			// 상한에 닿을 때 (템포)
	DropToZero		// 1 이상이었다가 0 이 될 때, 전투 시작의 0 에서는 안 켜짐 (문양)
};

// 상태가 언제부터 시작되는지
UENUM(BlueprintType)
enum class ENUJobStateStartTiming : uint8
{
	None,
	Immediate,		// 들어간 그 자리에서 (공진 · 지맥 붕괴)
	NextTurn		// 다음 턴부터 (번아웃)
};

// 상태가 끝날 때 자원을 어떻게 할지
UENUM(BlueprintType)
enum class ENUJobStateOnEnd : uint8
{
	Keep,			// 그대로 둠
	Reset			// 전량 소멸 (공진)
};

// 자기 편 줄이 대상에게 붙는 자원을 셀 때 누구의 값을 보는지
UENUM(BlueprintType)
enum class ENUJobSelfSideCount : uint8
{
	Target,			// 판정의 대상 — 주는 피해는 맞는 적, 받는 피해는 때린 적 (마크 · 약점)
	MostStacked		// 살아 있는 상대 중 그 자원이 가장 많은 전투원 (문양)
};


// 직업별 자원, JobResourceTable 의 RowName 이 ResourceId 가 됨
USTRUCT(BlueprintType)
struct PROJECTM_API FNUJobResourceDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Identity")
	int32 ResourceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Identity")
	FName ResourceKey = NAME_None;

	// 효과표가 부르는 이름, 두 표를 잇는 열쇠다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Identity")
	FName KoreanName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Identity")
	FName EnglishName = NAME_None;


	// 어느 직업의 것인지, `Common` 이면 직업을 가리지 않음 (자원 · AG · SP)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Owner")
	FName JobClass = NAME_None;

	// 이 자원을 여는 전직 노드, 0이면 전직과 무관
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Owner")
	int32 JobNodeId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Owner")
	ENUJobResourceOwner OwnerScope = ENUJobResourceOwner::Self;

	// 자기 편 줄이 이 자원을 셀 때 누구의 값을 볼지, 대상에게 붙는 자원(OwnerScope = Target)에서만 뜻이 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Owner")
	ENUJobSelfSideCount SelfSideCountFrom = ENUJobSelfSideCount::Target;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Limit")
	int32 InitialValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Limit")
	bool bStartAtMax = false;

	// 고정 상한, 스탯 비례면 0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Limit")
	int32 MaxValue = 0;

	// 스탯 비례 상한의 바닥값 (문양 = 3 + INT / 10 의 3)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Limit")
	int32 MaxBase = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Limit")
	ENUJobResourceScaleStat MaxScaleStat = ENUJobResourceScaleStat::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Limit")
	float MaxScaleDivisor = 0.f;

	// 사람이 읽는 상한식, 코드는 처리 X — 위 세 칸과 어긋나면 변환기가 잡는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Limit")
	FText MaxFormula;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Change")
	ENUJobResourceGainTrigger GainTrigger = ENUJobResourceGainTrigger::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Change")
	float GainAmount = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Change")
	bool bGainIsPercent = false;

	// 쌓이는 통로가 둘인 자원이 있음 (AG — 턴 종료 +20, 패링 성공 +20)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Change")
	ENUJobResourceGainTrigger GainTrigger2 = ENUJobResourceGainTrigger::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Change")
	float GainAmount2 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Change")
	ENUJobResourceDecay DecayType = ENUJobResourceDecay::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Change")
	float DecayAmount = 0.f;

	// 감쇠의 턴 간격, 0이면 감쇠 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Change")
	int32 DecayInterval = 0;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Special")
	ENUJobResourceOnMax OnReachMax = ENUJobResourceOnMax::Keep;

	// 전투당 사용 횟수 제한, 0이면 제한 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Special")
	int32 UsePerBattle = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Special")
	bool bResetOnUse = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Special")
	ENUJobResourceOnBattleEnd OnBattleEnd = ENUJobResourceOnBattleEnd::Reset;


	// 자원이 켜는 상태, 이름이 비면 상태 없음. 지금은 칸만 받고 상태가 들어가고 끝나는 로직은 아직 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|State")
	FName StateName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|State")
	ENUJobStateEnterRule StateEnterRule = ENUJobStateEnterRule::None;

	// AtValue 일 때 들어가는 값 (동조율 7)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|State")
	int32 StateEnterValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|State")
	int32 StateDurationTurns = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|State")
	ENUJobStateStartTiming StateStartTiming = ENUJobStateStartTiming::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|State")
	ENUJobStateOnEnd StateOnEnd = ENUJobStateOnEnd::Keep;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|JobResource|Text", meta = (MultiLine = "true"))
	FText Description;


	// 직업을 가리지 않는 자원인지, 표가 Common 이라고 적음 (자원 · AG · SP)
	bool IsCommonToAllJobs() const
	{
		static const FName CommonClass(TEXT("Common"));
		
		return JobClass == CommonClass;
	}
	
	// 이 전투원이 그 자원을 가지는지, 직업 이름으로 가림
	bool BelongsToJob(FName InJobClass) const
	{
		return IsCommonToAllJobs() || JobClass == InJobClass;
	}
	
	// 쌓이지 않고 매번 계산되는 파생값인지 (속도차이)
	// 자원 축에 칸을 잡으면 안 되고 판정 때마다 물어야 함
	bool IsDerived() const
	{
		return GainTrigger == ENUJobResourceGainTrigger::Derived;
	}

	// 취득한 전직 노드를 봐야 풀리는 자리인지 (공용 줄기의 `자원`)
	bool IsResolvedByJobNode() const
	{
		return GainTrigger == ENUJobResourceGainTrigger::ByJobNode;
	}

	// 이 전투원의 상한, 스탯 비례면 바닥값에 얹어 계산함
	// ScaleStatValue 는 부르는 쪽이 해당 어트리뷰트에서 읽어 넘김
	int32 ResolveMaxValue(float ScaleStatValue) const
	{
		if (MaxScaleStat == ENUJobResourceScaleStat::None)
		{
			return MaxValue;
		}

		if (MaxScaleDivisor <= 0.f)
		{
			return MaxBase;
		}

		return MaxBase + FMath::FloorToInt32(ScaleStatValue / MaxScaleDivisor);
	}
};

// 전투원 하나에 붙은 직업 자원 · 직업 상태를 화면에 그리기 위한 스냅샷 (UNUAbilitySystemComponent::GetJobResourceViews)
// 무엇을 띄울지 · 숫자를 무엇으로 할지는 C++ 이 정해서 넘기고 위젯은 그리기만 함 (상태이상 FNUStatusEffectView 와 같은 규약)
USTRUCT(BlueprintType)
struct PROJECTM_API FNUJobResourceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	int32 ResourceId = 0;

	// 효과표 · 자원표가 서로 부르는 이름 (영력 · 약점)
	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	FName ResourceName = NAME_None;

	// 아이콘 텍스처를 찾는 용도 (ST_RES_WEAKNESS -> T_Resource_Weakness)
	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	FName ResourceKey = NAME_None;

	// 자원이면 자원 이름, 상태면 상태 이름 (번아웃)
	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	int32 Amount = 0;

	// 0 이면 상한 없음
	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	int32 MaxAmount = 0;

	// 상태면 남은 턴(대기 중이면 전체 기간), 자원이면 가장 먼저 사라질 때까지 남은 턴(약점 · 마크), 셀 것이 없으면 0
	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	int32 RemainingTurns = 0;

	// 자원이 켠 상태인지 (번아웃 · 지맥 붕괴), 거짓이면 자원 수치
	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	bool bIsState = false;

	// 다음 턴부터 켜지는 상태 (번아웃)
	UPROPERTY(BlueprintReadOnly, Category = "NU|JobResource|View")
	bool bStatePending = false;
};
