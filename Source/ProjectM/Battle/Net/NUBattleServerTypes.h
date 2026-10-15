// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "NUBattleServerTypes.generated.h"


struct FGrpcRevGameBattleEvent;
struct FGrpcRevGameSVR_Battle_Reaction;
struct FGrpcRevGameBattleCombatant;
struct FNUStatusEffectView;
struct FNUJobResourceView;
class UNUBattleDataSubsystem;
// 서버의 BattleEventType 번호
UENUM(BlueprintType)
enum class ENUBattleEventType : uint8
{
	None		  = 0,
	
	Damage		  = 1,
	Heal		  = 2,
	StatusApplied = 3,
	StatusRemoved = 4,
	StatusStack   = 5,
	Death		  = 6,
	SPChanged	  = 7,
	ActionFailed  = 8,
	Hallucinated  = 9,
	Reaction	  = 10,
	Capture		  = 11,
	NaiaSwapped   = 12,
	ItemUsed	  = 13,
	Flee		  = 14,
	AttackStart   = 15,	// 피해 없이 모션만 시작하라는 신호, 반응 대기로 나뉜 행동의 첫 조각 맨 앞에만 옴
	SelfCost        = 16,	// 스킬이 시전자 자신의 HP 를 낸 것, 맞은 게 아니라 피격 연출 없이 체력만 바뀜
	ResourceChanged = 17,	// 직업 자원 수치 변화, 자원 번호와 새 수치 · 상한 · 남은 턴이 실림
	JobStateChanged = 18,	// 직업 상태 시작 · 변경, 상태 이름과 시작 · 끝 턴이 실림
	
	Count
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerBattleStatus
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 StatusId = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Stacks = 0;
	
	// 0이면 전투가 끝날때까지 유지 (폭발, 감전 같은 무한 지속 스택형)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 RemainingTurns = 0;
};

// 서버가 준 직업 자원 칸 하나, 자기 칸 자원(템포 · 영력)이든 대상에게 붙는 자원(마크 · 약점 · 문양)이든 같은 모양
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerJobResource
{
	GENERATED_BODY()

	// 자원표(res_jobresourcetable)의 번호
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 ResourceId = 0;

	// 수치, 대상에게 붙는 자원은 중첩 수
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Value = 0;

	// 0이면 상한 없음, 문양은 3 + INT÷10 · 템포는 속도÷2 를 서버가 셈
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Max = 0;

	// 사라질 때까지 남은 턴, 셀 것이 없는 자원은 0
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 RemainingTurns = 0;
};

// 서버가 준 직업 상태 하나 (번아웃 · 지맥 붕괴)
// 시작 턴 <= 지금 턴 <= 끝 턴 이면 켜져 있음
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerJobState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	FName StateName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 StartTurn = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 EndTurn = 0;

	// 이 상태를 건 자원
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 ResourceId = 0;
};
// 서버에서 보내준 전투원 스냅샷
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerCombatant
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 CombatantId = INDEX_NONE;
	
	// 0 아군, 1 적
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Side = 0;
	
	// 0 캐릭터, 1 나이아, 2 적
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Kind = 0;
	
	// 조종하는 캐릭터, 적이면 0
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int64 OwnerCharacterIdx = 0;
	
	// 플레이어가 소유한 나이아가 아니면 0
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int64 NaiaInstanceId = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 NaiaId = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	FString Name;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Level = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Health = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 MaxHealth = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 SP = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 MaxSP = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	double Speed = 0.0;
	
	// 이중속성인 개체가 있어서 TArray
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	TArray<FString> Elements;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	TArray<FNUServerBattleStatus> Statuses;

	// 쓸 수 있는 액티브 스킬, 나이아는 로스터 · 캐릭터는 성장 노드 보상이 서버 기준
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	TArray<int32> ActiveSkillIds;

	// 패시브, 받아만 두고 화면에는 쓰지 않음
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	TArray<int32> PassiveSkillIds;

	// 직업 자원 칸, 적에게 붙은 마크 · 약점 · 문양도 그 적의 목록에 옴
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	TArray<FNUServerJobResource> Resources;

	// 직업 상태 (번아웃 · 지맥 붕괴)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	TArray<FNUServerJobState> JobStates;
	
	// 캐릭터(Kind 0) 외형, SVR_Field_Appeared 와 같은 값 - 파티원을 그 사람 모습으로 그릴 때 씀, 아니면 0
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 RaceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Gender = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 HairId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 FaceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 JobId = 0;

	bool IsValid() const
	{
		return CombatantId >= 0;
	}
	
	bool IsAlive() const
	{
		return Health > 0;
	}
};

// 서버가 전송한 행동 이벤트
// type으로 갈라 읽고 안쓰는 값은 0
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerBattleEvent
{
	GENERATED_BODY()
	
	// 원본 이벤트 타입 보관, 로그용
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 RawType = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	ENUBattleEventType Type = ENUBattleEventType::None;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 ActorId = INDEX_NONE;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 TargetId = INDEX_NONE;
	
	// 수치 (데미지면 데미지 값, 힐이면 힐량)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Amount = 0;
	
	// 크리티컬 이었는지
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	bool bCritical = false;
	
	// 속성 상성 반영
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	double ElementMultiplier = 0.0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 StatusId = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 Stacks = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 SkillId = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 ItemId = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 HealthAfter = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 SPAfter = 0;
	
	// 패링/회피/포획/도주
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	bool bSuccess = false;
	
	// 포획 시 흔들림
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	TArray<bool> CaptureShakes;
	
	// 상태이상 남은 턴, 0이면 전투가 끝날때까지 유지
	// StatusApplied/StatusStack 이 채움, 아이콘의 남은 턴 표시가 이 값으로만 갱신됨
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 RemainingTurns = 0;

	// 직업 자원 번호, ResourceChanged 와 문양 지속 피해(Damage)에 실림
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 ResourceId = 0;

	// 그 자원의 상한, 0이면 상한 없음 (ResourceChanged)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 ResourceMax = 0;

	// 직업 상태 이름과 켜지는 턴 · 끝나는 턴 (JobStateChanged)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	FName StateName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 StartTurn = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 EndTurn = 0;
};

// BattleCommand 요청의 response_data를 파싱한 값
// 서버가 확정한 SP, 타수, 대상을 적용함
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerCommandAck
{
	GENERATED_BODY()
	
	// 참여한 전투 번호
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int64 BattleId = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 CombatantId = INDEX_NONE;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 SpentSP = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 HitCount = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 TargetId = INDEX_NONE;
	
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 RemainingSP = 0;
	
	// 아직 명령을 제출하지 않은 전투원
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	TArray<int32> PendingCombatantIds;
	
	bool IsValid() const
	{
		return BattleId > 0 && CombatantId >= 0;
	}
};

// 서버 BattleCommand 패킷과 대응하는 전송용 구조체
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerBattleCommandRequest
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int32 CombatantId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int32 Action = 0;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int32 SkillId = 0;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int32 TargetId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int32 InvestSP = 0;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int32 ItemId = 0;

	bool IsValid() const
	{
		// item, skill 은 사용하지 않을 수 있으므로 검사 대상에서 제외
		return BattleId > 0
			&& CombatantId >= 0
			&& Action >= 0
			&& TargetId >= INDEX_NONE
			&& InvestSP >= 0;
	}
};

// 패링·회피 반응 전송용 구조체
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerBattleReactionRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int32 CombatantId = INDEX_NONE;

	// 창이 열린 시점부터 키를 누를 때까지의 경과, -1 은 입력 없음
	// 서버는 판정에 쓰지 않고 기록만 하므로 이 값으로 성공이 갈리지 않음
	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int32 PressTimeMs = 0;

	// 서버가 준 창 번호를 그대로 돌려보냄, 지난 창의 입력은 이 값으로 버려짐
	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	int32 ReactionId = 0;

	// 클라가 낸 판정 결과, 현재는 서버가 이 값을 그대로 믿음
	UPROPERTY(BlueprintReadWrite, Category = "NU|Battle|Server")
	bool bSuccess = false;

	bool IsValid() const
	{
		// PressTimeMs 의 하한이 -1, 안 눌러도 보고해야 하는 경우(bMustReport)이 있어서
		// 0 으로 막으면 그 보고가 전송 직전에 버려지고 서버가 마감까지 멈춤
		return BattleId > 0
			&& CombatantId >= 0
			&& ReactionId > 0
			&& PressTimeMs >= -1;
	}
};

// 서버가 연 패링·회피 창
// 그 행동의 SVR_Battle_Action 보다 먼저 오므로 받은 시점엔 재생할 대상이 아직 없음 -
// ActionSeq 로 보관해 두었다가 그 행동을 재생할 때 꺼내 쓴다
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerBattleReactionWindow
{
	GENERATED_BODY()

	// 보고할 때 그대로 실어 보내는 창 번호
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 ReactionId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 AttackerId = INDEX_NONE;

	// 반응하는 전투원, CombatantId 가 해당함
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 DefenderId = INDEX_NONE;

	// 어느 행동의 몇 번째 타격인지, 그 행동의 Damage 와 순서대로 1:1
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 ActionSeq = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 HitIndex = 0;

	// 0 이면 기본 공격, 패링 타이밍 표를 찾는 키
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 SkillId = 0;

	// false면 회피 창, 성공하면 피해뿐 아니라 상태이상·흡혈까지 전부 무효
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	bool bParryable = false;

	// true면 안 눌러도 보고해야 함, 서버가 이 타격에서 멈추고 기다림
	// 이 값이 곧 회피/패링이 결과에 영향을 미치는 상황을 나타냄, 서버가 기다림
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	bool bMustReport = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 ChallengeDurationMs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 SuccessStartMs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 SuccessEndMs = 0;

	// 시도 비용, 젖음 보정이 반영된 값이라 클라가 다시 곱하면 안 된다
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Server")
	int32 SpCost = 0;

	bool IsValid() const
	{
		return ReactionId > 0 && DefenderId >= 0;
	}

	// 로컬 전투의 판정기가 쓰는 형태로 바꿈
	// 서버는 성공 구간을 시작·끝 ms 로 주고, 판정기는 기준점 비율과 앞뒤 여유(초)로 받는다
	FNUParryTimingSpec ToTimingSpec() const;
};

namespace NUBattleServer
{
	// TurboLink에서 받은 내용을 프로젝트 전용 값 구조체로 복사함
	PROJECTM_API bool ConvertCombatant(
		const FGrpcRevGameBattleCombatant& Source,
		FNUServerCombatant& OutCombatant,
		FString& OutError);
	
	// SVR_Battle_Reaction 을 프로젝트 값 구조체로 복사함
	PROJECTM_API bool ConvertReactionWindow(
		const FGrpcRevGameSVR_Battle_Reaction& Source,
		FNUServerBattleReactionWindow& OutWindow,
		FString& OutError);
	
	PROJECTM_API bool ConvertEvent(
		const FGrpcRevGameBattleEvent& Source,
		FNUServerBattleEvent& OutEvent,
		FString& OutError);
	
	// 상태이상 이벤트 하나를 목록에 반영함
	// 서버 미러(UNUBattleNetSubsystem)와 재생 큐의 표시용 사본이 같은 규칙으로 갱신되도록 한 곳에 둠
	// StatusRemoved 면 지우고, 나머지는 없으면 넣고 이미 있으면 스택·남은 턴을 덮어씀
	PROJECTM_API void ApplyStatusEvent(
		const FNUServerBattleEvent& Event,
		TArray<FNUServerBattleStatus>& InOutStatuses);

	// 직업 자원 · 상태 이벤트의 칸 주인, 둘 다 actor_id 가 주인임
	// 상태 이벤트는 target_id 를 채우지 않아 0 이 오므로 target_id 를 쓰면 0번 전투원에 붙어 버림
	PROJECTM_API int32 ResolveResourceEventOwner(const FNUServerBattleEvent& Event);

	// 직업 자원 이벤트 하나를 목록에 반영함, 없으면 넣고 있으면 수치 · 상한 · 남은 턴을 덮어씀
	// 0 이 된 칸도 지우지 않음 - 자기 칸 자원은 0 이어도 게이지가 남아야 하고, 대상 자원 아이콘은 보기를 만들 때 수치 0 을 뺌
	PROJECTM_API void ApplyResourceEvent(
		const FNUServerBattleEvent& Event,
		TArray<FNUServerJobResource>& InOutResources);

	// 직업 상태 이벤트 하나를 목록에 반영함, 한 이름에 하나 - 열림 · 늘어남(success)이면 시작 · 끝 턴을 덮어쓰고 닫힘이면 지움
	PROJECTM_API void ApplyJobStateEvent(
		const FNUServerBattleEvent& Event,
		TArray<FNUServerJobState>& InOutStates);

	// 서버 직업 자원 · 상태를 체력바 표시용 보기로 바꿈, 로컬 ASC 경로와 같은 NUJobResourceView 규칙을 씀
	// 자기 칸 자원은 서버가 칸을 보냈으면 게이지(0이어도), 대상에게 붙는 자원은 수치가 있을 때만 아이콘으로 띄움
	// 상한 · 남은 턴은 서버 값 그대로 - 문양 상한(3 + INT÷10)은 클라가 못 셈
	PROJECTM_API void BuildJobResourceViews(
		const UNUBattleDataSubsystem* BattleData,
		const TArray<FNUServerJobResource>& Resources,
		const TArray<FNUServerJobState>& JobStates,
		int32 Turn,
		TArray<FNUJobResourceView>& OutGauges,
		TArray<FNUJobResourceView>& OutBadges);

	// 서버 상태이상 목록을 아이콘 표시용 뷰로 바꿈
	// UNUAbilitySystemComponent::GetActiveStatusViews 와 같은 규약을 지킴 - 배지 숫자는 스택형이면
	// 스택 수, 아니면 남은 턴이고 StatusEffectId 오름차순으로 정렬함
	// 규칙을 두 곳에서 각자 해석하면 로컬 전투와 넷 전투의 아이콘이 달라짐
	PROJECTM_API void BuildStatusViews(
		const UNUBattleDataSubsystem* BattleData,
		const TArray<FNUServerBattleStatus>& Statuses,
		TArray<FNUStatusEffectView>& OutViews);

	PROJECTM_API bool ParseCommandAck(
		const FString& ResponseData,
		FNUServerCommandAck& OutAck,
		FString& OutError);
	
	// gRPC result code 를 기존 커맨드 컴포넌트 결과로 변환
	PROJECTM_API ENUBattleCommandSubmitResult MapCommandSubmitResult(int32 ResultCode);

	// 거절 코드의 뜻을 로그용 한 줄로, 서버 가이드의 result_code 표를 옮긴 것
	PROJECTM_API const TCHAR* DescribeCommandResult(int32 ResultCode);

	// 루트 액션을 서버 BattleCommand.action 으로 변환, 보낼 수 없는 행동이면 false
	// 0 공격 · 2 도망 · 3 포획 · 4 아이템 사용, 1은 폐기된 방어라 비어 있음
	// bCommuneItem은 Item 행동일 때 교감석인지 여부 - 포획은 3, 그 밖의 아이템 사용은 4
	PROJECTM_API bool MapRootActionToServerAction(ENUBattleRootAction RootAction, bool bCommuneItem, int32& OutAction);
}
