#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Battle/Encounter/NUBattleEncounterTypes.h"
#include "Templates/SubclassOf.h"
#include "NUBattleTypes.generated.h"

class UNUAbilitySystemComponent;
class UNiagaraSystem;
// 전투 페이즈
UENUM(BlueprintType)
enum class ENUBattlePhase : uint8
{
	None,
	Preparing,
	CommandSelection,
	ActionExecution,	
	Ending,
	Ended
};

// 전투 팀 구분
UENUM(BlueprintType)
enum class ENUBattleTeam : uint8
{
	Ally, 
	Enemy
};

// 전투 결과
UENUM(BlueprintType)
enum class ENUBattleResult: uint8
{
	None,
	Victory,
	Defeat,
	Fled
};

// 커맨드 종류
UENUM(BlueprintType)
enum class ENUBattleCommandType : uint8
{
	Ability,
	Item,
	Flee,
	SpiritSwap
};

// 타겟팅 방식
UENUM(BlueprintType)
enum class ENUTargetPolicy : uint8
{
	Self, 
	SingleEnemy, 
	AllEnemies, 
	SingleAlly, 
	AllAllies
};

// 클라이언트에서 서버로 제출하는 전투 명령
USTRUCT(BlueprintType)
struct FNUBattleCommandRequest
{
	GENERATED_BODY()
	
	// 명령을 제출한 턴
	UPROPERTY()
	int32 TurnNumber = 0;
	
	// 명령을 제출한 전투원 ID
	UPROPERTY()
	int32 SourceCombatantId = INDEX_NONE;
	
	// 어떤 명령인지 구분
	UPROPERTY()
	ENUBattleCommandType CommandType = ENUBattleCommandType::Ability;
	
	// Ability Id
	// 어느 어빌리티 클래스로 실행할지, 액티브 스킬은 아키타입 공용 태그를 사용
	UPROPERTY()
	FGameplayTag AbilityId;

	// 액티브 스킬 행 번호, 0이면 AbilityId 태그로 도는 기존 행동
	UPROPERTY()
	int32 SkillId = 0;

	// 선택한 타겟 Id
	UPROPERTY()
	int32 SelectedTargetId = INDEX_NONE;
	
	// 아이템 or 정령 Id
	UPROPERTY()
	FGuid InstanceId;

	// 이 명령에 투입한 SP, 제출 시점에 차감 예약됨(SP 소모 선점)
	UPROPERTY()
	int32 SPSpend = 0;

	// 이 명령이 낸 SP로 확정된 타격 횟수
	// 예약 시점에 한 번 정하고 실행과 배너가 이 값을 그대로 읽는다
	// SPSpend 를 실행 시점에 다시 나누면 안 됨 - 그 사이 젖음이 걸리면 실제 타격 횟수와 표시가 어긋남
	UPROPERTY()
	int32 HitCount = 1;
	
	// 적 패턴 규칙이 패링 타이밍을 덮어쓰는지, 선택 시점에 한번만 확정함
	UPROPERTY()
	bool bOverrideParryTiming = false;
	
	UPROPERTY()
	float ParryChallengeDuration = 0.f;
	
	UPROPERTY()
	float ParrySuccessAnchor = 0.f;
	
	UPROPERTY()
	float ParrySuccessWindow = 0.f;
};

// 전투 참가자 정보
USTRUCT(BlueprintType)
struct FNUBattleParticipant
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	int32 CombatantId = INDEX_NONE;
	
	UPROPERTY(BlueprintReadOnly)
	ENUBattleTeam Team = ENUBattleTeam::Enemy;
	
	// PlayerState, SpiritState, EnemyActor
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> ASCOwner = nullptr;
	
	// 전투에서 실제로 보여지는 아바타 액터
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> AvatarActor = nullptr;
	
	// 캐릭터, 정령의 Owner 저장, 적이면 nullptr
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<class APlayerState> OwningPlayerState = nullptr;

	// 적을 생성할 때 사용한 원본 정보
	UPROPERTY(BlueprintReadOnly)
	FNUEnemySpawnSpec EnemySpec;
};

// 클라이언트에서 제출한 전투 명령의 결과
UENUM(BlueprintType)
enum class ENUBattleCommandSubmitResult : uint8
{
	Accepted,
	Rejected,			// Requester 유효하지 않음
	WrongEncounter,		// 잘못된 BattleEncounter, 오류 상황
	WrongPhase,			// 명령선택 Phase가 아닌 페이즈에 제출
	WrongTurn,			// 턴 번호 불일치 
	InvalidSource,		// Source 참가자 없음
	NotOwner,			// 조종할 수 없는 전투원
	SourceDead,			// Source 사망
	InvalidAbility,		// 유효하지 않은 어빌리티
	InvalidTarget,		// 타겟 해결 실패
	InvalidItem,		// 아이템 정의가 없거나 전투에서 쓸 수 없는 아이템
	NotEnoughSP,		// 투입하려는 SP가 현재 SP보다 많음
	CannotCommune,		// 교감(포획)은 플레이어만 가능, 정령은 불가능함
};

// 포획 시도 결과
UENUM(BlueprintType)
enum class ENUCaptureResult : uint8
{
	Success,
	InvalidTarget,		// 적 진영 참가자가 아니거나 이미 죽음
	NotCapturable,		// 포획 불가능한 나이아
	RosterFull,			// 파티와 보관함에 빈자리가 없음
	AlreadyCaptured,	// 같은 전투원을 두 번 포획하려 함
	ChanceFailed,		// 확률 판정에 실패, 교감석을 쓰고 턴만 소모함
};

// 전투 중 나이아 교체 결과
// 턴을 소비하지 않는 자유 행동이라 명령 제출과 결과 체계를 따로 둠
UENUM(BlueprintType)
enum class ENUNaiaSwapResult : uint8
{
	Success,
	WrongPhase,				// 명령 선택 페이즈가 아님
	InvalidRequester,		// 요청자의 로스터나 정령 전투원을 찾을 수 없음
	InvalidSlot,			// 파티 슬롯이 유효하지 않거나 이미 활성인 0번
	TargetDead,				// 교체해 넣을 나이아가 죽어 있음
	AlreadySwappedThisTurn,	// 이번 턴에 이미 교체함, 교체가 SP를 채우므로 반복을 막아야 함
};

// 행동이 어떤 움직임을 취하는지
UENUM(BlueprintType)
enum class ENUBattleActionMotion : uint8
{
	InPlace,		// 제자리 행동
	MoveToCenter,	// 중앙으로 이동
	MoveToTarget	// 타겟으로 이동
};

// 화면 전환 연출 큐
// 인카운터 시작, 끝을 나눔, 보스 연출 추가 가능
UENUM(BlueprintType)
enum class ENUTransitionCue : uint8
{
	None,
	FadeOut,	// 투명 -> 검정. 월드가 재배치되기 전
	FadeIn		// 검정 -> 투명. 재배치가 끝난 뒤
};

// 전투원 머리 위에 잠깐 떠오르는 글자
// 문구와 색은 WBP가 정하고, Amount가 필요 없는 타입은 0을 넘김
UENUM(BlueprintType)
enum class ENUCombatPopupType : uint8
{
	Damage,			// 피해량, Amount 사용
	Critical,		// 치명타 피해, Amount 사용
	Heal,			// 회복량, Amount 사용
	Parry,			// 패링 성공
	Dodge,			// 회피 성공, MISS
	DodgeFail,		// 회피 실패, 확률 판정에 떨어짐 (SP 는 소모됨)
	ParryFail,		// 패링 실패, 타이밍을 놓침
	CaptureSuccess,	// 포획 성공
	CaptureFail,	// 포획 실패, 사유는 로그로만 남김
	ActionFail,		// 기절·마비·환각으로 행동 실패
	NotEnoughSP,	// SP 가 부족해 패링·회피를 못 함, 확률 실패와는 다른 사유임
	ShieldAbsorb	// 보호막이 피해를 전부 먹음, Amount 는 흡수량
};

// 행동 실행마다 화면 상단에 잠깐 띄우는 행동 이름
USTRUCT(BlueprintType)
struct FNUBattleActionBanner
{
	GENERATED_BODY()

	// 아이템 행동이면 어빌리티 이름이 아니라 아이템 이름을 사용
	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	// 2 이상일 때만 "×N" 을 붙임
	UPROPERTY(BlueprintReadOnly)
	int32 HitCount = 1;

	// 2 이상일 때만 "Lv.N" 을 붙임
	// 레벨이 다른 스킬이 아직 없어 지금은 항상 1, 표기 자리만 열어둔 것
	UPROPERTY(BlueprintReadOnly)
	int32 Level = 1;
};

// 표에 이름이 없는 두 행동의 배너 문구
// 스킬은 액티브 스킬 CSV 가, 아이템은 아이템 표가 이름을 들고 있어서 예외가 이 둘뿐임
// 로컬과 넷이 같은 문구를 쓰도록 한 곳에 둠
namespace NUBattleActionName
{
	inline FText GetBasicAttack()
	{
		return NSLOCTEXT("NUBattle", "ActionBasicAttack", "기본 공격");
	}

	inline FText GetFlee()
	{
		return NSLOCTEXT("NUBattle", "ActionFlee", "도주");
	}
}

// 패링 타이밍 정보
USTRUCT(BlueprintType)
struct FNUParryTimingSpec
{
	GENERATED_BODY()
	
	// 패링 도전 시간 (초), 바 전체 길이 = 마커가 이동하는 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float ChallengeDuration = 1.5f;
	
	// 도전 구간 내 판정 기준점 (0 = 시작, 0.5 = 중앙, 0.25 = 1/4 지점,  0.75 = 3/4 지점)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float SuccessAnchor = 0.5f;
	
	// 성공 구간, 기준점으로부터 +- 초
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	float SuccessWindow = 0.15f;
	
	float GetSuccessRatio() const
	{
		return ChallengeDuration > KINDA_SMALL_NUMBER ? SuccessWindow / ChallengeDuration : 0.f;
	}
	
	// 판정 기준점 앞구간
	float GetLeadTime() const { return ChallengeDuration * SuccessAnchor; }
	
	// 판정 기준점 뒷구간
	float GetTailTime() const { return ChallengeDuration * (1.f - SuccessAnchor); }
	
	// true면 성공, false면 실패
	bool Judge(float PressOffset, float ImpactOffset) const
	{
		const float Delta = FMath::Abs(PressOffset - ImpactOffset);
		
		if (Delta <= SuccessWindow) return true;
		return false;
	}
};

// 행동 세부 설정
USTRUCT(BlueprintType)
struct FNUBattleActionMotionSpec
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	ENUBattleActionMotion Motion = ENUBattleActionMotion::MoveToCenter;
	
	UPROPERTY(BlueprintReadOnly)
	FVector ActionLocation = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<class UPaperZDAnimSequence> ActionAnimation = nullptr;
	
	UPROPERTY(BlueprintReadOnly)
	float ApproachTime = 0.25f;
	
	UPROPERTY(BlueprintReadOnly)
	float ActionTime = 0.6f;
	
	UPROPERTY(BlueprintReadOnly)
	float ReturnTime = 0.25f;
	
	
	// ----------- 반복 타격 정보 ----------- 
	// 아바타가 Acting 구간 길이와 애니메이션 재생 시점을 여기서 구함
	
	UPROPERTY(BlueprintReadOnly)
	int32 HitCount = 1;

	UPROPERTY(BlueprintReadOnly)
	float HitInterval = 0.f;
	
	
	// --------- 적 공격 전용 -----------

	// 접근 전에 두는 예고 시간, 이 동안 아바타는 제자리에서 대기함
	// 패링·회피 창은 이 구조체가 아니라 서버가 표적의 소유자에게 직접 보냄
	UPROPERTY(BlueprintReadOnly)
	float TelegraphTime = 0.f;

	// 공격 애니메이션 중 실제 타격 판정 지점 (0~1)
	// 탄환 비행 시간(ActionTime * HitTimeNormalized)을 계산하는 데 씀
	// 서버의 GetImpactDelay 와 같은 지점이지만,
	// Acting 페이즈 시작을 0으로 보는 클라 로컬 시계 기준이라 예고,접근 시간은 빼고 재야 함
	// 전투 모션에 Hit 표시가 있으면 ANUBattleAvatar::ResolveMotionTiming 이 그 시각으로 바꿈
	UPROPERTY(BlueprintReadOnly)
	float HitTimeNormalized = 0.5f;

	// 시전 연출(검기)이 뜨고 탄환이 출발하는 지점 (0~1), 타격 지점과 같은 Acting 시작 기준
	// 전투 모션의 Cast 표시에서만 채워짐, 표시가 없으면 0 이라 Acting 시작과 동시에 나감
	UPROPERTY(BlueprintReadOnly)
	float CastTimeNormalized = 0.f;


	// ----------- 연출 VFX -----------
	// DT_SkillVFX 조회 결과를 어빌리티에서 조회해서 저장함
	// 전부 비어 있으면(기본공격 등) 아바타 쪽 기본 프로필로 처리

	// 시전자에게, 없으면 안 뜸
	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UNiagaraSystem> CastVFX;

	// 있으면 시전자 -> 대상으로 비행하는 탄환 액터가 뜸, 없으면 CastVFX만으로 끝
	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UNiagaraSystem> ProjectileVFX;

	// ProjectileVFX 도착 지점에서 재생, 탄환이 없으면 쓰이지 않음
	// (근접 명중 이펙트는 기존 GameplayCue.Battle.Hit 가 그대로 담당)
	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

	// 탄환이 향할 대상 위치, 타겟 액터의 스폰 시점 위치를 어빌리티가 미리 구해둠
	UPROPERTY(BlueprintReadOnly)
	FVector ProjectileTargetLocation = FVector::ZeroVector;

	// 위 VFX들의 User Parameter 색상, 속성별 색상이나 프로필 고정색상을 어빌리티가 미리 계산해둠
	UPROPERTY(BlueprintReadOnly)
	FLinearColor EffectColor = FLinearColor::White;

	// 스킬의 공격 속성 (없으면 빈 태그), 탄환이 닿을 때 나는 소리를 속성별로 고르는 데 씀
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag AttackElement;

	// 위 VFX를 스킬 표(DT_SkillVFX) 행이 채웠는지
	// false 면 어빌리티 기본 연출이라, 플레이어 아바타가 직업의 기본 공격 연출로 바꿀 수 있음
	UPROPERTY(BlueprintReadOnly)
	bool bHasSkillVFXProfile = false;

	// 앞으로 나가지만 공격이 아닌 행동 (퀘스트 달래기 · 돌 옮기기)
	// 플레이어 아바타가 전투 공격 모션과 직업 기본 공격 연출을 붙이지 않음, 도착하면 대기 자세로 섬
	UPROPERTY(BlueprintReadOnly)
	bool bSkipAttackPresentation = false;
};

// GameplayEffect를 누구에게 시전할지
// 여러 GameplayEffect를 적용할때 각각의 Effect를 다른 대상에 적용하기 위함
UENUM(BlueprintType)
enum class ENUEffectApplyTo : uint8
{
	Target,		// Ability의 타겟팅 정책에 따라감
	Self,		// 타겟팅 정책과 무관하게 자기 자신에게 시전
};

// GameplayEffect를 부여하는 타이밍
UENUM(BlueprintType)
enum class ENUEffectTiming : uint8
{
	OnHit,		// 판정 시점, 보통 데미지 GE가 사용
	BeforeHit,	// 판정 직전, 버프/디버프 부여시 데미지 처리에 반영됨
	AfterHit	// 판정 직후, 상태이상 부여나 체력 감소(자해) 등
};

// 효과 값이 기존 값에 어떻게 적용되는지
UENUM(BlueprintType)
enum class ENUEffectStackType : uint8
{
	None,
	Additive,			// 더함, 값이 비율이면 0.3 = +30%
	Multiplicative		// 곱함, 값이 곧 배수 (2 = x2)
};

// 효과 지속시간
UENUM(BlueprintType)
enum class ENUEffectDurationScope : uint8
{
	Instant,			// 그 자리에서 한 번
	Turns,				// DurationTurns 만큼
	Battle,				// 전투가 끝날 때까지
	Permanent			// 조건이 붙어 있는 동안 (2210 개화)
};

// 기준치를 어느 방향으로 읽는지
UENUM(BlueprintType)
enum class ENUThresholdCompare : uint8
{
	None,
	Below,				// 기준치 미만일 때
	AtLeast				// 기준치 이상일 때
};

// 트리거만으로 못 가르는 추가 조건
UENUM(BlueprintType)
enum class ENUEffectCondition : uint8
{
	None,
	OnAttack,				// 공격할 때
	OnHitTaken,				// 맞을 때
	AnyStatus,				// 아무 상태이상이나
	TargetHasStatus,		// 대상이 ConditionValue 의 상태이상을 갖고 있을 때 (2803)
	OnStatusApplied,		// 상태이상을 걸었을 때 (2820)
	OnStateEnd,				// 그 상태가 끝날 때 (2210)
	DuringState,			// 그 상태인 동안 (2210)
	AfterElementUse,		// 그 속성 스킬을 쓴 뒤 (2407)
	SourceIsSkill,			// 기본공격이 아니라 스킬일 때 (2836)
	AttackerSpeedLower,		// 공격자 속도가 자신보다 낮을 때 (2838)
	FirstTurnOnly,			// 첫 턴에만 (2837)
	RandomTarget,			// 대상을 무작위로 (2833)
	NextTurn,				// 다음 턴에 (2834)
	NoChain,				// 되튕김 금지 (2832)

	// 아래는 직업 스킬 효과표가 쓰는 것들
	HPBelow,				// 자신 HP 가 ConditionValue % 이하 (3001 · 3034)
	HPAbove,				// 자신 HP 가 ConditionValue % 초과 (3001 의 0 구간)
	SpeedDiffAtLeast,		// 대상과의 속도 차이가 ConditionValue 이상 (3101)
	SpeedDiffAtMost,		// 대상과의 속도 차이가 ConditionValue 이하 (3101 의 0 구간)
	ResourceAtMax,			// 그 자원이 상한일 때 (3018 · 3046 · 3248)
	ResourceAbove,			// 그 자원이 기준 이상일 때, "자원:50%" 처럼 적힘 (3118)
	StackAtCount,			// 대상 중첩이 N 이상일 때, "약점:5" 처럼 적힘 (3148 · 3202)
	AfterState,				// 그 상태가 끝난 뒤, "번아웃:1턴" 처럼 적힘 (3104)
	OnFirstStrike,			// 선제 타격에 성공한 턴 (3139)
	OnFirstStrikeFail,		// 선제 타격에 실패한 턴 (3037 · 3129 의 낮은 쪽)
	AfterSkill,				// ConditionValue 의 스킬을 쓴 다음 턴 (3235)
	OnCardDrawn,			// 그 카드를 뽑은 턴, ConditionValue 가 대길·길·흉·대흉 (3303)

	TargetHPBelow,			// 대상 HP 가 ConditionValue % 이하 (3024 · 3120 계수 교체)
	SingleHit,				// 단발 공격일 때 (3114 · 3115)
	HitCountAtLeast,		// 이번 턴 이미 공격한 횟수가 ConditionValue 이상, 지금 타격은 빼고 셈 (3050 · 3105)
	OncePerTurn,			// 한 턴에 한 번 (3124 · 3243)
	OncePerTurnFirstAction,	// 한 턴에 한 번, 그 턴의 첫 행동으로만 (3012)
	OnActFirst,				// 선공 턴 (3010)
	OnActSecond,			// 후공 턴
	HandHasCard,			// 패에 그 카드가 있을 때 (3316 · 3317)
	AfterResourceSpend,		// 그 자원을 쓴 턴 (3338)
	ResourceSumAbove,		// 자원 둘의 합이 기준 이상, "성배+검:8" (3348)
	LowestHPAlly,			// 체력이 가장 낮은 아군에게 (3409)
	AllyHasShield,			// 보호막을 받은 아군에게만 (3431)
	WhileAllyShielded,		// 아군이 보호막을 유지하는 동안 (3446)
	SpeedAbove,				// 속도가 ConditionValue 를 초과할 때, 초과분을 세는 기준선 (3146)
	SPBelow,				// 자신 SP 가 ConditionValue 미만일 때 (3243 — 행동 후 SP 20 미만)
	OnTurnEnd,				// 시전자의 턴이 끝날 때, 감쇠보다 먼저 (3207 문양 지속 피해)
	OnStateEnter,			// 그 상태에 들어가는 순간 한 번, ConditionValue 가 상태 이름 (3038 공진)
	LowestStat,				// ConditionValue 의 능력치가 가장 낮은 대상에게, "Defense" 처럼 적힘 (2211)
	OnAttackSkillUse			// 피해를 주는 직업 스킬을 쓸 때마다, 행동당 한 번 (3105 연사 — 템포 +2)
};

// 이 행이 데이터만으로 도는지, 전용 구현이 필요한지
UENUM(BlueprintType)
enum class ENUEffectImplementBy : uint8
{
	Data,
	Code,
	CodeOverride,		// 다른 노드의 수치를 덮어씀 (3022 전율 증폭)
	CodeResource,		// 자원 규칙 자체 (전직 패시브)
	CodeNewMechanic,	// 새 메커니즘 (보호막 · SP 이월 · 카드 재추첨)

	// 표가 아직 조건이나 나머지 효과를 안 적은 행
	// 그대로 읽으면 조건 없는 상시 효과가 되므로 변환기가 여기로 돌려두고 런타임이 건너뜀
	Pending
};

// 스탯 변경 효과가 건드리는 스탯
UENUM(BlueprintType)
enum class ENUStatusAffectedStat : uint8
{
	None,
	Speed,
	Defense,
	AttackPower,
	MagicAttackPower,
	CriticalRate,
	MaxHealth,
	MaxSP,
	Health,
	SP,
	CriticalDamage
};

// 어빌리티가 적용할 GameplayEffect 구조체
USTRUCT(BlueprintType)
struct FNUAbilityEffectEntry
{
	GENERATED_BODY()
	
	// 적용할 GameplayEffect 클래스
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<class UGameplayEffect> EffectClass;
	
	// GameplayEffect가 적용할 태그와 수치
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "NU.Data"))
	TMap<FGameplayTag, float> Magnitudes;
	
	// 누구에게 적용?
	// Target이면 어빌리티의 타겟 정책 따라감
	UPROPERTY(EditDefaultsOnly)
	ENUEffectApplyTo ApplyTo = ENUEffectApplyTo::Target;
	
	// 언제 적용?
	// 공격 판정 전, 공격 판정 시점, 공격 판정 직후
	UPROPERTY(EditDefaultsOnly)
	ENUEffectTiming Timing = ENUEffectTiming::OnHit;
	
	// 지속 시간, 0이면 즉발, 1이상이면 수만큼 턴 유지
	UPROPERTY(EditDefaultsOnly)
	int32 DurationTurns = 0;

	// None 이 아니면 같은 시전자가 같은 스킬로 대상에게 이미 건 이 축의 스탯 변경을 지우고 새로 검
	// 다시 걸어도 겹치지 않고 지속만 새로 잡힘 (3111 견제 화살 · 3411 포효)
	UPROPERTY(EditDefaultsOnly)
	ENUStatusAffectedStat ReplaceStat = ENUStatusAffectedStat::None;
	
	// 부여할 상태이상 ID, 0이면 걸지않음
	UPROPERTY(EditDefaultsOnly)
	int32 StatusEffectId = 0;
	
	// 상태이상 확률
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float StatusEffectChance = 1.f;
	
	
	/* 대상 조건 - 대상마다 갈리는 것이라 대상을 아는 자리에서 판정함 (2410 뇌신의 심판) */
	
	// 대상이 이 중 하나에 걸려 있어야 조건을 만족함, 비어 있으면 조건 없음
	UPROPERTY(EditDefaultsOnly)
	TArray<int32> ConditionStatusIds;
	
	// 조건을 못 채운 대상에게 이 항목을 통째로 건너뛸지
	UPROPERTY(EditDefaultsOnly)
	bool bSkipWhenConditionFails = false;
	
	// 조건을 채운 대상에게만 피해에 곱할 값, 1이면 안 곱함
	UPROPERTY(EditDefaultsOnly)
	float ConditionalDamageMultiplier = 1.f;

	// 대상 체력 조건 - 때리는 순간 대상 체력이 이 비율(%) 이하면 계수를 갈아끼움, 0 이면 조건 없음
	// 5직업 처형기(3024 종결 등 - 35% 이하면 225% 대신 380%), 기본 계수와 배타라 곱하지 않고 바꿈
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "100"))
	float ConditionTargetHPBelow = 0.f;

	// 대상 중첩 조건 - 때리는 순간 대상에게 붙은 이 자원이 기준 이상이면 계수를 갈아끼움, 이름이 비면 조건 없음
	// 3216 연쇄 폭발(약점 5중첩이면 320% 대신 440%), 대상 체력 조건과 같은 교체 계수를 씀
	UPROPERTY(EditDefaultsOnly)
	FName ConditionTargetStack;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	int32 ConditionTargetStackCount = 0;

	// 위 조건(체력 · 중첩)을 채운 대상에게 기본 계수 대신 쓸 계수
	UPROPERTY(EditDefaultsOnly)
	float ConditionalCoefficient = 0.f;

	// 치명타를 굴리지 않고 반드시 치명타로 적중 (3121 나이아의 유도), 피해 항목에만 의미가 있음
	UPROPERTY(EditDefaultsOnly)
	bool bForceCritical = false;
};

// 전투 루트 액션, 처음 선택지
UENUM(BlueprintType)
enum class ENUBattleRootAction : uint8
{
	Attack,
	Skill,
	Item,
	Flee,
	NaiaSwap	// 나이아 교체, 턴을 소비하지 않아 명령을 제출하지 않음
};

// 공격시 속성 상성이 어떤 관계인지 확인하는 용도
// 데미지 팝업 표시할때 해당 공격이 속성 상성 관계가 어떤 관계였는지 확인할때 사용함
UENUM(BlueprintType)
enum class ENUElementAffinity : uint8
{
	None,		// 상성 없음
	Weak,		// 약점
	Resist		// 내성
};

// 전투원 머리 위에 띄울 팝업 정보 구조체
USTRUCT(BlueprintType)
struct FNUCombatPopupData
{
	GENERATED_BODY()
	
	// 표시할 문구의 타입, 타입으로 색 정해짐
	UPROPERTY(BlueprintReadOnly)
	ENUCombatPopupType Type = ENUCombatPopupType::Damage;
	
	// 수치, 수치가 없는 종류는 0
	UPROPERTY(BlueprintReadOnly)
	int32 Amount = 0;
	
	UPROPERTY(BlueprintReadOnly)
	ENUElementAffinity Affinity = ENUElementAffinity::None;
	
	
	// 생성자 정의
	
	FNUCombatPopupData() = default;
	
	explicit FNUCombatPopupData(ENUCombatPopupType InType, int32 InAmount = 0,
		ENUElementAffinity InAffinity = ENUElementAffinity::None)
			: Type(InType), Amount(InAmount), Affinity(InAffinity)
	{
	}
};

// 턴 기반 효과 정보
USTRUCT()
struct FNUTurnBasedEffect
{
	GENERATED_BODY()
	
	// 남은 턴, 0이면 전투 끝날때까지 유지
	UPROPERTY()
	int32 RemainingTurns = 0;
	
	// 턴 종료 피해를 계산할때 정의를 찾기위한 키
	// 상태이상이 아니면 0
	UPROPERTY()
	int32 StatusEffectId = 0;
	
	// 중첩 횟수
	UPROPERTY()
	int32 StackCount = 1;
	
	// 이 효과를 부여한 스킬, 같은 스킬을 다시 걸었을 때 덮어쓸 대상을 찾는 키
	// 상태이상은 StatusEffectId 로 찾으므로 0
	UPROPERTY()
	int32 SkillId = 0;

	// 스탯 변경 GE 가 건드리는 축, 같은 시전자 · 같은 스킬이 다시 걸 때 덮어쓸 대상을 가르는 키
	// 회복 스탯(HealStat)과 달리 턴마다 하는 일은 없음, 스탯 변경이 아니면 None
	UPROPERTY()
	ENUStatusAffectedStat ModifiedStat = ENUStatusAffectedStat::None;
	
	// 턴마다 회복시킬 스탯, None이면 X
	// 시트의 Effect_Stat, Effect_Value, IsPercent 를 그대로 옮겨 담음
	UPROPERTY()
	ENUStatusAffectedStat HealStat = ENUStatusAffectedStat::None;
	
	UPROPERTY()
	float HealValue = 0.f;
	
	// true면 비율, false면 고정값
	UPROPERTY()
	bool bHealIsPercent = false;
	
	// 부여한 사람의 ASC
	// 부여한 사람이 처치한 판정이 되도록 기억해둬야 함
	UPROPERTY()
	TWeakObjectPtr<UNUAbilitySystemComponent> SourceASC;
};

// 체력보다 먼저 닳는 보호막
// 부여한 스킬마다 같은 스킬은 갱신, 다른 스킬은 따로 처리함
USTRUCT()
struct FNUShieldInstance
{
	GENERATED_BODY()
	
	// 이 보호막을 부여한 스킬, 같은 스킬을 다시 걸었을 때 덮어쓸 대상을 찾는 키
	UPROPERTY()
	int32 SourceSkillId = 0;
	
	// 남은 흡수량, 비율이 아니라 부여 시점에 풀어둔 절대값
	// 비율인 채로 들고 있으면 전투 중에 최대 체력이 오를 때 이미 걸린 보호막이 같이 커짐
	UPROPERTY()
	float Amount = 0.f;
	
	// 남은 턴, 0이면 전투가 끝날 때까지
	UPROPERTY()
	int32 RemainingTurns = 0;
	
	// 부여한 사람의 ASC, 소모된 양을 자원으로 돌려주는 행이 있어 출처를 들고 있어야 함
	UPROPERTY()
	TWeakObjectPtr<UNUAbilitySystemComponent> SourceASC;
};

// 반사 피해
// 적에게 거는 디버프이고, 걸린 쪽이 공격하면 그 공격으로 가한 만큼 자신이 돌려받음
// 맞은 쪽이 아니라 때린 쪽이 들고 있는 효과 (2205 니들 해지 · 2509 바람의 족쇄)
USTRUCT()
struct FNUReflectInstance
{
	GENERATED_BODY()
	
	UPROPERTY()
	int32 SourceSkillId = 0;
	
	// 되받는 비율, 1이면 가한 피해 그대로
	UPROPERTY()
	float Ratio = 1.f;
	
	// 남은 턴, 0이면 전투가 끝날 때까지
	UPROPERTY()
	int32 RemainingTurns = 0;
	
	// 반사 피해의 가해자로 남길 ASC, 이 효과를 건 쪽
	// 반사로 쓰러뜨렸을 때 처치 판정이 그쪽으로 가야 함
	UPROPERTY()
	TWeakObjectPtr<UNUAbilitySystemComponent> SourceASC;
};

// 몇 턴 동안만 켜지는 받는 피해 배수
// 상시(Permanent) 줄은 패시브·직업 상시 층이 CalcDamageTakenMultiplier 에서 직접 세므로
// 이 축에는 액티브 스킬로 건 기간제 효과만 들어온다
USTRUCT()
struct FNUDamageTakenModInstance
{
	GENERATED_BODY()

	// 이 배수를 건 스킬, 같은 스킬을 다시 걸면 덮어쓸 대상을 찾는 키
	UPROPERTY()
	int32 SourceSkillId = 0;

	// 곱할 배수, 0.65 면 받는 피해 65%
	UPROPERTY()
	float Multiplier = 1.f;

	// 남은 턴, 0이면 전투가 끝날 때까지
	UPROPERTY()
	int32 RemainingTurns = 0;
};

// 직업 자원
// 어느 자원인지는 자원표(DT_JobResource)의 KoreanName 으로 구분함
// 누구에게 쌓이는지는 자원표의 OwnerScope 가 정함
USTRUCT()
struct FNUJobResourceInstance
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName ResourceName = NAME_None;
	
	// 현재 수치, 0 이상이고 상한을 넘지 않음
	UPROPERTY()
	int32 Amount = 0;
	
	// TimeoutAll 자원만 씀 — 마지막으로 쌓인 뒤 지난 턴 수 (마크 : 3턴이면 전량 소멸)
	UPROPERTY()
	int32 IdleTurns = 0;
	
	// TimeoutEach 자원만 씀 — 중첩마다 남은 턴을 따로 셈 (약점)
	// Amount 와 길이가 같게 유지되고, 먼저 넣은 것이 먼저 빠짐
	UPROPERTY()
	TArray<int32> StackTurns;
	
	// 남은 감쇠 면역 턴 (3237 지맥 연결 — 이번 턴 문양이 안 줄어듦)
	// 0 이면 평소대로 줄어듦
	UPROPERTY()
	int32 DecayImmuneTurns = 0;
	
	// 이번 턴에 쌓인 양, 턴당 획득 상한이 있는 자원만 씀 (3230 초과 부하 — 마력 응축 턴당 1)
	// 턴이 닫힐 때 0 으로 돌아감
	UPROPERTY()
	int32 GainedThisTurn = 0;
	
	// 기간제 획득 배수와 남은 턴 (3437 대지의 뿌리 — 2턴간 응전 획득 2배)
	UPROPERTY()
	float GainMultiplier = 1.f;
	
	UPROPERTY()
	int32 GainBonusTurns = 0;
};

// 자원이 켠 상태 (3104 템포 -> 번아웃), 자원표의 StateName 이 이름이 됨
// 켜짐 = 시작 턴 이상 끝 턴 이하, 시작 전이면 대기 중
USTRUCT()
struct FNUJobStateInstance
{
	GENERATED_BODY()

	UPROPERTY()
	FName StateName = NAME_None;

	// 상태를 켠 자원의 이름, 끝날 때 자원표의 규칙(전량 소멸 / 유지)을 찾는 데 씀
	UPROPERTY()
	FName ResourceName = NAME_None;

	UPROPERTY()
	int32 StartTurn = 0;

	UPROPERTY()
	int32 EndTurn = 0;
	// EndTurn 이 이 값이면 턴 제한이 없는 상태, 자원이 소모될 때 끝남 (17018 임계)
	static constexpr int32 OpenEnded = TNumericLimits<int32>::Max();
};

// 속성 위력 배수
// 그 속성으로 공격할 때만 켜지는 곱연산 항
USTRUCT()
struct FNUElementPowerInstance
{
	GENERATED_BODY()
	
	UPROPERTY()
	int32 SourceSkillId = 0;
	
	// 이 배수가 켜지는 속성, 비어 있으면 어디에도 붙지 않음
	UPROPERTY()
	FGameplayTag Element;
	
	// 곱할 배수, 1.5 면 그 속성 위력 1.5배
	UPROPERTY()
	float Multiplier = 1.f;
	
	// 남은 턴, 0이면 전투가 끝날 때까지
	UPROPERTY()
	int32 RemainingTurns = 0;
	
	// 이 배수가 켜지는 누적 사용 횟수 (2407 과전류 - 번개를 한 번 더 써야 켜짐)
	// 걸 때의 누적 횟수에 요구 횟수를 더해 적어 둠, 남은 횟수로 들고 있으면
	// 줄이는 자리를 또 만들어야 하고 그 자리를 빠뜨리면 영영 안 켜진다
	UPROPERTY()
	int32 ActivateAtUseCount = 0;
};

// 스킬이 스스로를 묶어두는 상태 (2210 개화 - 두 턴 못 움직이고 풀릴 때 터짐), 상태이상이 아님
USTRUCT()
struct FNUSelfBoundState
{
	GENERATED_BODY()
	
	UPROPERTY()
	int32 SourceSkillId = 0;
	
	// 남은 턴, 0 이하가 되면 풀린다
	UPROPERTY()
	int32 RemainingTurns = 0;
	
	// 이 상태인 동안 행동을 못 함
	UPROPERTY()
	bool bBlocksAction = false;
	
	// 이 상태인 동안 얹히는 흡혈 비율
	UPROPERTY()
	float Lifesteal = 0.f;
	
	// 풀릴 때 이 스킬의 피해를 터뜨릴지
	UPROPERTY()
	bool bReleaseOnEnd = false;
};

// 스킬이 걸어두는 "때릴 때마다" (2007 불의 의지 · 2008 불바다)
USTRUCT()
struct FNUOnAttackGrant
{
	GENERATED_BODY()
	
	UPROPERTY()
	int32 SourceSkillId = 0;
	
	// 걸 상태이상과 확률
	UPROPERTY()
	int32 StatusEffectId = 0;
	
	UPROPERTY()
	float Chance = 0.f;
	
	// 이 속성으로 공격할 때만 켜짐, 비어 있으면 속성을 가리지 않음
	UPROPERTY()
	FGameplayTagContainer Elements;
	
	// 기본 공격도 포함할지 (표의 "Normal")
	UPROPERTY()
	bool bIncludeBasicAttack = false;
	
	// 남은 턴, 0이면 전투가 끝날 때까지
	UPROPERTY()
	int32 RemainingTurns = 0;
};

// 속성을 가려 거는 디버프 (2109 수중 감옥 - 물 피해를 더 받고 불 스킬을 못 씀)
//
// 속성 위력과 방향이 반대
// 속성 위력은 공격하는 쪽이 내가 해당 속성으로 때릴 때 켜지고, 이건 맞는쪽이 해당 속성으로 맞을 때 켜짐
USTRUCT()
struct FNUElementAfflictionInstance
{
	GENERATED_BODY()
	
	UPROPERTY()
	int32 SourceSkillId = 0;
	
	// 어느 속성에 영향을 받는지
	UPROPERTY()
	FGameplayTag Element;
	
	// 그 속성 공격으로 받는 피해 배수, 1이면 그대로
	UPROPERTY()
	float DamageTakenMultiplier = 1.f;
	
	// 그 속성 액티브 스킬을 쓸 수 없음
	UPROPERTY()
	bool bBlocksSkill = false;
	
	// 남은 턴, 0이면 전투가 끝날 때까지
	UPROPERTY()
	int32 RemainingTurns = 0;
};
	