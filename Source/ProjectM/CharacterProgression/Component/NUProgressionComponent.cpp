#include "CharacterProgression/Component/NUProgressionComponent.h"

#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Data/NULevelProgression.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterProgression/Data/NUStatTuning.h"
#include "CharacterProgression/Effect/NUGE_PlayerBaseSecondaryStats.h"
#include "CharacterProgression/Effect/NUGE_PlayerSecondaryStatScaling.h"
#include "CharacterProgression/Net/NUProgressionNetSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "NUGAS/Attribute/NUStatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/Interface/NUCombatantInterface.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "Net/EmberServerManager.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

/*
* 서버 성장 상태 반영 순서
* ApplyState(NewState)
* -> ValidateState()에서 입력 상태 검사
* -> ProjectStateToASC()에서 GAS 반영
*		- Owner의 ASC 조회
*		- 기본 2차 스탯 GE 적용 보장
*		- 2차 스탯 환산 GE 적용 보장
*		- Primary Stat Base 값 설정
* -> 환산 GE가 2차 스탯 자동 갱신
* -> State 저장
* -> OnProgressionChanged로 UI에 알림
*/

UNUProgressionComponent::UNUProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	BaseSecondaryStatsEffect = UNUGE_PlayerBaseSecondaryStats::StaticClass();		// 레벨1 Secondary 기본값
	SecondaryStatScalingEffect = UNUGE_PlayerSecondaryStatScaling::StaticClass();	// Primary -> Secondary 환산
}

void UNUProgressionComponent::BeginPlay()
{
	Super::BeginPlay();
	UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem();
	if (ProgressionNet != nullptr)
	{
		ProgressionNet->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleProgressionSnapshotChanged);
		ProgressionNet->OnRequestStateChanged.AddUObject(this, &ThisClass::HandleProgressionRequestStateChanged);
		ProgressionNet->OnRequestFailed.AddUObject(this, &ThisClass::HandleProgressionRequestFailed);
		ApplyCachedServerSnapshot();
	}

#if !UE_BUILD_SHIPPING
	AActor* Owner = GetOwner();
	if (Owner != nullptr && Owner->HasAuthority() && (ProgressionNet == nullptr || !ProgressionNet->IsExternalSessionActive()))
	{
		bLocalProgressionBackendReady = true;
		bHasAppliedServerProgressionSnapshot = true;
		Owner->ForceNetUpdate();
		UE_LOG(LogNUProgression, Display, TEXT("외부 성장 세션이 없어 Local UE 성장 백엔드를 사용합니다."));
	}
#endif
}

void UNUProgressionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem())
	{
		ProgressionNet->OnSnapshotChanged.RemoveAll(this);
		ProgressionNet->OnRequestStateChanged.RemoveAll(this);
		ProgressionNet->OnRequestFailed.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UNUProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, State, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, bLocalProgressionBackendReady, COND_OwnerOnly);
}

// -------------------------------------------------------------------------
// 성장 상태 적용
// -------------------------------------------------------------------------

bool UNUProgressionComponent::ApplyState(const FNUProgressionState& NewState)
{
	// 성장 상태는 서버에서만 변경
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	// 잘못된 레벨, 경험치, 스탯, 노드 상태를 거부
	FString ValidationError;
	if (!ValidateState(NewState, BuildNodeRuleContext(), ValidationError))
	{
		UE_LOG(LogNUProgression, Error, TEXT("성장 상태 적용 거부: %s"), *ValidationError);
		return false;
	}

	// 같은 상태를 다시 받으면 GAS와 UI를 다시 갱신하지 않음
	if (NewState == State)
	{
		return true;
	}

	// 레벨과 경험치만 바뀌었을 때, GAS를 건들면 안됨
	if (NewState.PrimaryStats != State.PrimaryStats && !ProjectStateToASC(NewState))
	{
		return false;
	}

	// GAS 반영까지 성공한 상태만 저장하고 UI에 알림
	State = NewState;
	OnProgressionChanged.Broadcast();

	return true;
}

bool UNUProgressionComponent::ApplyCachedServerSnapshot()
{
	const UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem();
	return ProgressionNet != nullptr && ProgressionNet->HasSnapshot() && ApplyServerSnapshot(ProgressionNet->GetSnapshot());
}

bool UNUProgressionComponent::ApplyServerSnapshot(const FNUProgressionServerSnapshot& Snapshot)
{
	UNUJobComponent* JobComponent = GetJobComponent();
	if (JobComponent == nullptr)
	{
		return false;
	}

	FString Error;
	if (!JobComponent->CanApplyState(Snapshot.JobState, Error))
	{
		UE_LOG(LogNUProgression, Error, TEXT("서버 직업 상태 적용 거부: %s"), *Error);
		return false;
	}

	FNUProgressionNodeRuleContext RuleContext;
	RuleContext.JobId = Snapshot.JobState.JobId;
	RuleContext.AdvancementId = Snapshot.JobState.AdvancementId;
	if (!ValidateState(Snapshot.ProgressionState, RuleContext, Error))
	{
		UE_LOG(LogNUProgression, Error, TEXT("서버 성장 상태 적용 거부: %s"), *Error);
		return false;
	}

	const bool bJobChanged = Snapshot.JobState != JobComponent->GetState();
	const bool bProgressionChanged = Snapshot.ProgressionState != State;
	if (!bJobChanged && !bProgressionChanged)
	{
		bHasAppliedServerProgressionSnapshot = true;
		return true;
	}
	if (Snapshot.ProgressionState.PrimaryStats != State.PrimaryStats && !ProjectStateToASC(Snapshot.ProgressionState))
	{
		return false;
	}
	if (!JobComponent->ApplyStateInternal(Snapshot.JobState, false))
	{
		return false;
	}

	State = Snapshot.ProgressionState;
	bHasAppliedServerProgressionSnapshot = true;
	if (AActor* Owner = GetOwner(); Owner != nullptr && Owner->HasAuthority())
	{
		Owner->ForceNetUpdate();
	}
	if (bProgressionChanged)
	{
		OnProgressionChanged.Broadcast();
	}
	if (bJobChanged)
	{
		JobComponent->NotifyStateChanged();
	}
	RefreshCharacterStatsAfterProgressionChange();
	return true;
}

bool UNUProgressionComponent::RequestProgressionState()
{
	if (bLocalProgressionBackendReady)
	{
		bHasAppliedServerProgressionSnapshot = true;
		OnProgressionRequestStateChanged.Broadcast();
		return true;
	}

	UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem();
	return ProgressionNet != nullptr && ProgressionNet->RequestProgression();
}

bool UNUProgressionComponent::ApplyLocalExperienceReward(int64 Amount)
{
#if UE_BUILD_SHIPPING
	// 로컬 성장 백엔드는 개발 빌드에서만 켜지고(BeginPlay), 경험치 계산(SimulateIncreaseExperience)도 개발 빌드 전용
	return false;
#else
	if (!bLocalProgressionBackendReady)
	{
		return false;
	}

	FNUProgressionState NewState;
	FString Error;
	if (!FNULevelProgression::SimulateIncreaseExperience(State, Amount, NewState, Error))
	{
		UE_LOG(LogNUProgression, Error, TEXT("로컬 경험치 보상 적용 실패: %s"), *Error);
		return false;
	}

	const int32 PreviousEarnedPointCount = FNUProgressionNodeRules::GetEarnedPointCountForLevel(State.Level);
	const int32 NewEarnedPointCount = FNUProgressionNodeRules::GetEarnedPointCountForLevel(NewState.Level);
	const int32 EarnedPointDelta = FMath::Max(0, NewEarnedPointCount - PreviousEarnedPointCount);
	NewState.AvailablePoints += EarnedPointDelta;
	NewState.TotalPoints += EarnedPointDelta;

	return ApplyState(NewState);
#endif
}

bool UNUProgressionComponent::RequestNodeAcquire(int32 NodeId, int32 ChoiceAdvancementId)
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	const FNUProgressionNodeDefinitionRow* Node = Data != nullptr ? Data->FindNode(NodeId) : nullptr;
	if (Node == nullptr || Node->NodeType == ENUProgressionNodeType::Root)
	{
		return false;
	}
	if ((Node->NodeType == ENUProgressionNodeType::Advancement) != (ChoiceAdvancementId > 0))
	{
		return false;
	}

	const FNUProgressionNodeRuleContext RuleContext = BuildNodeRuleContext();
	FString Error;
	if (!CanAcquireNode(*Node, RuleContext, GetProgressionGraphId(), Error))
	{
		UE_LOG(LogNUProgression, Warning, TEXT("성장 노드 요청 거부: NodeId=%d 이유=%s"), NodeId, *Error);
		return false;
	}
	if (ChoiceAdvancementId > 0)
	{
		const FNUJobAdvancementDefinitionRow* Advancement = Data->FindAdvancement(ChoiceAdvancementId);
		if (Advancement == nullptr || Advancement->ParentJobId != RuleContext.JobId || Advancement->ParentAdvancementId != RuleContext.AdvancementId)
		{
			return false;
		}
	}

	if (bLocalProgressionBackendReady)
	{
		if (ChoiceAdvancementId > 0)
		{
			ServerAcquireAdvancementNode(NodeId, ChoiceAdvancementId);
		}
		else
		{
			ServerAcquireProgressionNode(NodeId);
		}
		return true;
	}

	UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem();
	return ProgressionNet != nullptr && ProgressionNet->RequestNodeAcquire(NodeId, ChoiceAdvancementId);
}

bool UNUProgressionComponent::HasServerProgressionSnapshot() const
{
	if (bLocalProgressionBackendReady)
	{
		return true;
	}

	const UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem();
	return bHasAppliedServerProgressionSnapshot && ProgressionNet != nullptr && ProgressionNet->HasSnapshot();
}

bool UNUProgressionComponent::IsServerProgressionRequestPending() const
{
	if (bLocalProgressionBackendReady)
	{
		return false;
	}

	const UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem();
	return ProgressionNet != nullptr && (ProgressionNet->IsStateRequestPending() || ProgressionNet->IsMutationPending());
}

bool UNUProgressionComponent::RebuildAttributes()
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	FString ValidationError;
	if (!ValidateState(State, BuildNodeRuleContext(), ValidationError))
	{
		UE_LOG(LogNUProgression, Error, TEXT("어트리뷰트 재적용 거부: %s"), *ValidationError);
		return false;
	}

	return ProjectStateToASC(State);
}

bool UNUProgressionComponent::InitializeCurrentVitalsOnce()
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || bCurrentVitalsInitialized)
	{
		return false;
	}

	UNUAbilitySystemComponent* ASC = GetOwnerASC();
	if (ASC == nullptr)
	{
		UE_LOG(LogNUProgression, Error, TEXT("초기 HP/SP 설정 실패: ASC 없음"));
		return false;
	}

	InitializeCurrentVitals(ASC);
	bCurrentVitalsInitialized = true;
	return true;
}

// 외부 서버가 없는 비 Shipping 로컬 PIE에서만 UE Authority가 최종 판정한다.
void UNUProgressionComponent::ServerAcquireProgressionNode_Implementation(int32 NodeId)
{
	if (!bLocalProgressionBackendReady)
	{
		UE_LOG(LogNUProgression, Warning, TEXT("Local UE 성장 백엔드가 비활성 상태입니다. NodeId=%d"), NodeId);
		return;
	}

	UNUJobComponent* JobComponent = GetJobComponent();
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	const FNUProgressionNodeDefinitionRow* Node = Data != nullptr ? Data->FindNode(NodeId) : nullptr;
	FString Error;
	if (JobComponent == nullptr || Node == nullptr || Node->NodeType == ENUProgressionNodeType::Root
		|| !TryAcquireLocalNode(*Node, JobComponent->GetState(), Error))
	{
		UE_LOG(LogNUProgression, Warning, TEXT("Local UE 성장 노드 획득 거부: NodeId=%d 이유=%s"), NodeId, *Error);
	}
}

void UNUProgressionComponent::ServerAcquireAdvancementNode_Implementation(int32 NodeId, int32 AdvancementId)
{
	if (!bLocalProgressionBackendReady)
	{
		UE_LOG(LogNUProgression, Warning, TEXT("Local UE 성장 백엔드가 비활성 상태입니다. NodeId=%d AdvancementId=%d"), NodeId, AdvancementId);
		return;
	}

	UNUJobComponent* JobComponent = GetJobComponent();
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	const FNUProgressionNodeDefinitionRow* Node = Data != nullptr ? Data->FindNode(NodeId) : nullptr;
	FNUJobState RequestedJobState = JobComponent != nullptr ? JobComponent->GetState() : FNUJobState();
	RequestedJobState.AdvancementId = AdvancementId;
	FString Error;
	if (Node == nullptr || Node->NodeType != ENUProgressionNodeType::Advancement
		|| !TryAcquireLocalNode(*Node, RequestedJobState, Error))
	{
		UE_LOG(LogNUProgression, Warning, TEXT("Local UE 전직 노드 획득 거부: NodeId=%d AdvancementId=%d 이유=%s"), NodeId, AdvancementId, *Error);
	}
}

ENUProgressionNodeState UNUProgressionComponent::GetNodeState(int32 NodeId) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	return Data != nullptr
		? Data->EvaluateNodeState(NodeId, BuildEffectiveProgressionState(), BuildNodeRuleContext())
		: ENUProgressionNodeState::Locked;
}

int32 UNUProgressionComponent::GetAvailablePoints() const
{
	return BuildEffectiveProgressionState().AvailablePoints;
}

int32 UNUProgressionComponent::ResolveAvailableParentNodeId(int32 NodeId) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	const FNUProgressionNodeDefinitionRow* Node = Data != nullptr ? Data->FindNode(NodeId) : nullptr;
	return Node != nullptr
		? FNUProgressionNodeRules::ResolveAvailableParentId(*Node, BuildEffectiveProgressionState(), BuildNodeRuleContext())
		: 0;
}

int32 UNUProgressionComponent::GetProgressionGraphId() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	const FNUProgressionNodeRuleContext RuleContext = BuildNodeRuleContext();
	const FNUJobDefinitionRow* Job = Data != nullptr
		? Data->FindJob(RuleContext.JobId)
		: nullptr;
	return Job != nullptr ? Job->GraphId : 0;
}

void UNUProgressionComponent::RequestCharacterStats()
{
	// 같은 캐릭터에 대한 조회와 배분 요청을 동시에 보내지 않는다.
	if (bCharacterStatRequestPending)
	{
		return;
	}

	UEmberServerManager* Server = GetServerManager();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		return;
	}

	bCharacterStatRequestPending = true;
	OnCharacterStatSnapshotChanged.Broadcast();
	Server->SendCharacterStat();
}

void UNUProgressionComponent::HandleServerCharacterStatResponse(int32 ResultCode, const FString& ResponseData)
{
	bCharacterStatRequestPending = false;
	if (bRefreshCharacterStatsAfterPendingRequest)
	{
		bRefreshCharacterStatsAfterPendingRequest = false;
		bHasCharacterStatSnapshot = false;
		OnCharacterStatSnapshotChanged.Broadcast();
		RequestCharacterStats();
		return;
	}
	if (ResultCode != 200)
	{
		UE_LOG(LogNUProgression, Error, TEXT("캐릭터 스탯 응답 실패 (%d)"), ResultCode);
		OnCharacterStatSnapshotChanged.Broadcast();
		return;
	}

	// 조회와 배분 응답은 같은 JSON 계약이므로 하나의 파서와 적용 경로를 사용
	FNUCharacterStatSnapshot Parsed;
	FString Error;
	if (!NUCharacterStatServer::ParseSnapshot(ResponseData, Parsed, Error))
	{
		UE_LOG(LogNUProgression, Error, TEXT("캐릭터 스탯 JSON 파싱 실패: %s / %s"), *Error, *ResponseData);
		OnCharacterStatSnapshotChanged.Broadcast();
		return;
	}

	const bool bGrowthStateMismatch = HasServerProgressionSnapshot()
		&& (Parsed.Level != State.Level
			|| Parsed.TotalExperience != State.TotalExperience
			|| Parsed.ExperienceIntoLevel != State.ExperienceIntoLevel
			|| Parsed.Strength != State.PrimaryStats.Strength
			|| Parsed.Dexterity != State.PrimaryStats.Dexterity
			|| Parsed.Intelligence != State.PrimaryStats.Intelligence
			|| Parsed.Luck != State.PrimaryStats.Luck
			|| Parsed.Vitality != State.PrimaryStats.Vitality);

	// 18번은 장비가 반영된 최종 전투 능력치만 소유한다. 성장 값이 다르면 170번 전체 상태로 다시 맞춘다.
	CharacterStatSnapshot = Parsed;
	bHasCharacterStatSnapshot = true;
	UE_LOG(LogNUProgression, Log, TEXT("캐릭터 스탯 적용: Lv.%d, FreePoint=%d"),
		CharacterStatSnapshot.Level,
		CharacterStatSnapshot.FreePoint);
	OnCharacterStatSnapshotChanged.Broadcast();
	if (bGrowthStateMismatch)
	{
		UE_LOG(LogNUProgression, Warning, TEXT("CharacterStat과 CharacterProgression의 성장 값이 달라 170번 상태를 다시 요청합니다."));
		RequestProgressionState();
	}
}

// -------------------------------------------------------------------------
// PIE 디버그
// -------------------------------------------------------------------------

void UNUProgressionComponent::ServerDebugSetPrimaryStats_Implementation(int32 InStrength, int32 InDexterity, int32 InIntelligence, int32 InLuck, int32 InVitality)
{
	FNUProgressionState NewState = State;
	NewState.PrimaryStats.Strength = FMath::Max(0, InStrength);
	NewState.PrimaryStats.Dexterity = FMath::Max(0, InDexterity);
	NewState.PrimaryStats.Intelligence = FMath::Max(0, InIntelligence);
	NewState.PrimaryStats.Luck = FMath::Max(0, InLuck);
	NewState.PrimaryStats.Vitality = FMath::Max(0, InVitality);

	if (ApplyState(NewState))
	{
		UE_LOG(LogNUProgression, Log,
			TEXT("디버그 1차 스탯 설정: 힘=%d 민첩=%d 지능=%d 행운=%d 활력=%d"),
			NewState.PrimaryStats.Strength,
			NewState.PrimaryStats.Dexterity,
			NewState.PrimaryStats.Intelligence,
			NewState.PrimaryStats.Luck,
			NewState.PrimaryStats.Vitality);
	}
}

void UNUProgressionComponent::ServerDebugIncreaseExperience_Implementation(int64 Amount)
{
	if (!ApplyLocalExperienceReward(Amount))
	{
		UE_LOG(LogNUProgression, Error, TEXT("디버그 경험치 지급 실패: 로컬 성장 백엔드가 아니거나 입력값이 유효하지 않습니다."));
		return;
	}

	UE_LOG(LogNUProgression, Log, TEXT("디버그 경험치 지급: 지급량=%lld, 총경험치=%lld, 레벨=%d, 현재 경험치=%lld"),
		Amount,
		State.TotalExperience,
		State.Level,
		State.ExperienceIntoLevel);
}

void UNUProgressionComponent::ServerDebugPrepareSecondAdvancement_Implementation()
{
#if !UE_BUILD_SHIPPING
	UNUJobComponent* JobComponent = GetJobComponent();
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	const int32 GraphId = GetProgressionGraphId();
	const FNUProgressionNodeDefinitionRow* CurrentNode = Data != nullptr ? Data->FindNode(State.CurrentNodeId) : nullptr;
	if (JobComponent == nullptr || JobComponent->GetJobId() <= 0 || JobComponent->GetAdvancementId() > 0
		|| Data == nullptr || GraphId <= 0 || CurrentNode == nullptr
		|| CurrentNode->NodeType != ENUProgressionNodeType::Root || State.AcquiredNodeIds.Num() != 1)
	{
		UE_LOG(LogNUProgression, Warning, TEXT("2차 전직 디버그 준비 거부: 최초 직업 선택 직후 상태가 아님"));
		return;
	}

	int64 Level20Experience = 0;
	if (!FNULevelProgression::Get().TryGetRequiredCumulativeExperience(20, Level20Experience))
	{
		UE_LOG(LogNUProgression, Error, TEXT("2차 전직 디버그 준비 실패: LV20 누적 경험치가 없음"));
		return;
	}

	FNUProgressionState PreparedState = State;
	PreparedState.Level = 20;
	PreparedState.TotalExperience = Level20Experience;
	PreparedState.ExperienceIntoLevel = 0;
	PreparedState.AvailablePoints = FMath::Max(
		PreparedState.AvailablePoints,
		FNUProgressionNodeRules::GetEarnedPointCountForLevel(20));
	PreparedState.TotalPoints = FMath::Max(PreparedState.TotalPoints, FNUProgressionNodeRules::GetEarnedPointCountForLevel(20));
	FNUProgressionNodeRuleContext RuleContext;
	RuleContext.JobId = JobComponent->GetJobId();

	const TArray<const FNUProgressionNodeDefinitionRow*> GraphNodes = Data->GetNodesByGraph(GraphId);
	for (int32 Stage = 1; Stage <= 9; ++Stage)
	{
		TArray<const FNUProgressionNodeDefinitionRow*> Candidates = GraphNodes.FilterByPredicate([Stage](const FNUProgressionNodeDefinitionRow* Node)
		{
			return Node != nullptr
				&& Node->StemType == ENUProgressionStemType::Entry
				&& Node->StemStage == Stage;
		});
		Candidates.Sort([](const FNUProgressionNodeDefinitionRow& Left, const FNUProgressionNodeDefinitionRow& Right)
		{
			const float LeftLaneDistance = FMath::Abs(Left.StemLane);
			const float RightLaneDistance = FMath::Abs(Right.StemLane);
			return FMath::IsNearlyEqual(LeftLaneDistance, RightLaneDistance)
				? Left.NodeId < Right.NodeId
				: LeftLaneDistance < RightLaneDistance;
		});

		const FNUProgressionNodeDefinitionRow* SelectedNode = nullptr;
		for (const FNUProgressionNodeDefinitionRow* Candidate : Candidates)
		{
			if (Candidate != nullptr
				&& FNUProgressionNodeRules::ResolveAvailableParentId(*Candidate, PreparedState, RuleContext) > 0)
			{
				SelectedNode = Candidate;
				break;
			}
		}
		if (SelectedNode == nullptr || PreparedState.AvailablePoints < SelectedNode->PointCost)
		{
			UE_LOG(LogNUProgression, Error, TEXT("2차 전직 디버그 준비 실패: 진입 줄기 %d단계 경로가 없음"), Stage);
			return;
		}

		const int32 ParentNodeId = FNUProgressionNodeRules::ResolveAvailableParentId(*SelectedNode, PreparedState, RuleContext);
		for (const FNUProgressionNodeRewardRow* Reward : Data->GetRewardsByNode(SelectedNode->NodeId))
		{
			FString Error;
			if (Reward != nullptr
				&& Reward->RewardType == ENUProgressionNodeRewardType::PrimaryStat
				&& !ApplyPrimaryStatReward(PreparedState, Reward->RewardId, Reward->Value, Error))
			{
				UE_LOG(LogNUProgression, Error, TEXT("2차 전직 디버그 준비 실패: %s"), *Error);
				return;
			}
		}

		PreparedState.AvailablePoints -= SelectedNode->PointCost;
		PreparedState.AcquiredNodeIds.Add(SelectedNode->NodeId);
		FNUProgressionNodeLinkState& Link = PreparedState.AcquiredNodeLinks.AddDefaulted_GetRef();
		Link.ParentNodeId = ParentNodeId;
		Link.ChildNodeId = SelectedNode->NodeId;
		PreparedState.CurrentNodeId = SelectedNode->NodeId;
		PreparedState.ActiveStemId = SelectedNode->StemId;
	}

	if (PreparedState.Revision == MAX_int64)
	{
		UE_LOG(LogNUProgression, Error, TEXT("2차 전직 디버그 준비 실패: 리비전 오버플로"));
		return;
	}
	++PreparedState.Revision;
	FString Error;
	if (!ValidateState(PreparedState, RuleContext, Error) || !ApplyState(PreparedState))
	{
		UE_LOG(LogNUProgression, Error, TEXT("2차 전직 디버그 준비 실패: %s"), *Error);
		return;
	}

	ClientDebugUseProgressionState(PreparedState);
	UE_LOG(LogNUProgression, Display, TEXT("2차 전직 디버그 준비 완료: 중앙의 2차 전직 선택 노드를 누르세요."));
#endif
}

void UNUProgressionComponent::ClientDebugUseProgressionState_Implementation(const FNUProgressionState& DebugState)
{
#if !UE_BUILD_SHIPPING
	CharacterStatSnapshot.Level = DebugState.Level;
	CharacterStatSnapshot.Strength = DebugState.PrimaryStats.Strength;
	CharacterStatSnapshot.Dexterity = DebugState.PrimaryStats.Dexterity;
	CharacterStatSnapshot.Intelligence = DebugState.PrimaryStats.Intelligence;
	CharacterStatSnapshot.Luck = DebugState.PrimaryStats.Luck;
	CharacterStatSnapshot.Vitality = DebugState.PrimaryStats.Vitality;
	bHasCharacterStatSnapshot = true;
	bCharacterStatRequestPending = false;
	OnCharacterStatSnapshotChanged.Broadcast();
	OnProgressionChanged.Broadcast();
#endif
}

// -------------------------------------------------------------------------
// 레벨 및 경험치 조회
// -------------------------------------------------------------------------

int32 UNUProgressionComponent::GetLevel() const
{
	return BuildEffectiveProgressionState().Level;
}

int64 UNUProgressionComponent::GetRequiredExperienceToNext() const
{
	int64 RequiredExperience = 0;
	FNULevelProgression::Get().TryGetRequiredExperienceToNext(GetLevel(), RequiredExperience);
	
	return RequiredExperience;
}

float UNUProgressionComponent::GetExperienceProgressRatio() const
{
	const int64 RequiredExperience = GetRequiredExperienceToNext();
	if (RequiredExperience <= 0)
	{
		return 1.f;
	}

	return FMath::Clamp(static_cast<float>(State.ExperienceIntoLevel) / static_cast<float>(RequiredExperience), 0.f, 1.f);
}

// -------------------------------------------------------------------------
// 복제 및 내부 처리
// -------------------------------------------------------------------------

void UNUProgressionComponent::OnRep_State()
{
	// 어트리뷰트는 ASC 복제로 오므로 클라에서 다시 투영하지 않는다
	// 여기서는 포인트와 노드 상태를 포함한 UI 갱신만 알림
	OnProgressionChanged.Broadcast();
}

void UNUProgressionComponent::OnRep_LocalProgressionBackendReady()
{
	bHasAppliedServerProgressionSnapshot = bLocalProgressionBackendReady;
	OnProgressionRequestStateChanged.Broadcast();
	OnProgressionChanged.Broadcast();
}

UNUAbilitySystemComponent* UNUProgressionComponent::GetOwnerASC() const
{
	if (const INUCombatantInterface* Combatant = Cast<INUCombatantInterface>(GetOwner()))
	{
		return Combatant->GetNUASC();
	}

	return nullptr;
}

UNUJobComponent* UNUProgressionComponent::GetJobComponent() const
{
	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(GetOwner());
	return PlayerState != nullptr ? PlayerState->GetJobComponent() : nullptr;
}

UNUProgressionNetSubsystem* UNUProgressionComponent::GetProgressionNetSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionNetSubsystem>() : nullptr;
}

void UNUProgressionComponent::HandleProgressionSnapshotChanged()
{
	UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem();
	if (ProgressionNet == nullptr || !ProgressionNet->HasSnapshot())
	{
		bHasAppliedServerProgressionSnapshot = false;
	}
	else
	{
		if (AActor* Owner = GetOwner(); Owner != nullptr && Owner->HasAuthority() && bLocalProgressionBackendReady)
		{
			bLocalProgressionBackendReady = false;
			Owner->ForceNetUpdate();
		}
		if (!ApplyCachedServerSnapshot())
		{
			bHasAppliedServerProgressionSnapshot = false;
			OnProgressionRequestFailed.Broadcast(0, TEXT("서버 성장 상태를 현재 클라이언트 데이터에 적용하지 못했습니다."));
		}
	}
	OnProgressionRequestStateChanged.Broadcast();
}

void UNUProgressionComponent::HandleProgressionRequestStateChanged()
{
	OnProgressionRequestStateChanged.Broadcast();
}

void UNUProgressionComponent::HandleProgressionRequestFailed(int32 ResultCode, const FText& Message)
{
	OnProgressionRequestFailed.Broadcast(ResultCode, Message.ToString());
}

void UNUProgressionComponent::RefreshCharacterStatsAfterProgressionChange()
{
	if (bCharacterStatRequestPending)
	{
		bRefreshCharacterStatsAfterPendingRequest = true;
		return;
	}
	if (!bHasCharacterStatSnapshot)
	{
		return;
	}

	bHasCharacterStatSnapshot = false;
	OnCharacterStatSnapshotChanged.Broadcast();
	RequestCharacterStats();
}

FNUProgressionState UNUProgressionComponent::BuildEffectiveProgressionState() const
{
	return State;
}

FNUProgressionNodeRuleContext UNUProgressionComponent::BuildNodeRuleContext() const
{
	FNUProgressionNodeRuleContext Context;
	if (const UNUJobComponent* JobComponent = GetJobComponent())
	{
		Context.JobId = JobComponent->GetJobId();
		Context.AdvancementId = JobComponent->GetAdvancementId();
	}
	return Context;
}

// UI에서 계산한 상태를 믿지 않고 서버에서 같은 조건을 다시 검사한다.
bool UNUProgressionComponent::CanAcquireNode(const FNUProgressionNodeDefinitionRow& Node, const FNUProgressionNodeRuleContext& RuleContext, int32 ProgressionGraphId, FString& OutError) const
{
	const FNUProgressionState CurrentState = BuildEffectiveProgressionState();
	if (Node.RequiredJobId > 0 && Node.RequiredJobId != RuleContext.JobId)
	{
		OutError = FString::Printf(TEXT("요구 직업 불일치: 현재=%d 필요=%d"), RuleContext.JobId, Node.RequiredJobId);
		return false;
	}
	if (Node.RequiredAdvancementId > 0 && Node.RequiredAdvancementId != RuleContext.AdvancementId)
	{
		OutError = FString::Printf(
			TEXT("요구 전직 불일치: 현재=%d 필요=%d"),
			RuleContext.AdvancementId,
			Node.RequiredAdvancementId);
		return false;
	}
	if (Node.GraphId != ProgressionGraphId)
	{
		OutError = FString::Printf(TEXT("현재 직업의 성장 그래프가 아님: GraphId=%d"), Node.GraphId);
		return false;
	}

	// 이미 획득한 노드는 효과와 포인트를 다시 적용하지 않음
	if (CurrentState.AcquiredNodeIds.Contains(Node.NodeId))
	{
		OutError = TEXT("이미 획득한 노드");
		return false;
	}
	if (Node.NodeType == ENUProgressionNodeType::Advancement && RuleContext.AdvancementId > 0)
	{
		OutError = TEXT("2차 전직 갈래는 하나만 선택할 수 있음");
		return false;
	}

	const int32 RequiredLevel = FNUProgressionNodeRules::ResolveRequiredLevel(Node, CurrentState);
	if (RequiredLevel > 0 && CurrentState.Level < RequiredLevel)
	{
		OutError = FString::Printf(TEXT("요구 레벨 부족: 현재=%d 필요=%d"), CurrentState.Level, RequiredLevel);
		return false;
	}

	// 현재 포인트로 노드 비용을 지불할 수 있어야 함
	if (CurrentState.AvailablePoints < Node.PointCost)
	{
		OutError = FString::Printf(TEXT("포인트 부족: 현재=%d 필요=%d"), CurrentState.AvailablePoints, Node.PointCost);
		return false;
	}

	if (Node.NodeType == ENUProgressionNodeType::Root)
	{
		if (CurrentState.CurrentNodeId > 0 || CurrentState.ActiveStemId > 0 || !CurrentState.AcquiredNodeIds.IsEmpty())
		{
			OutError = TEXT("성장 경로를 시작한 뒤에는 루트를 다시 선택할 수 없음");
			return false;
		}
	}
	else if (FNUProgressionNodeRules::ResolveAvailableParentId(Node, CurrentState, RuleContext) <= 0)
	{
		OutError = FString::Printf(TEXT("현재 경로에서 선택할 수 없음: 현재 노드=%d 진행 줄기=%d"),
			CurrentState.CurrentNodeId, CurrentState.ActiveStemId);
		return false;
	}

	return true;
}

bool UNUProgressionComponent::TryInitializeLocalJobProgression(int32 JobId, FString& OutError)
{
	AActor* Owner = GetOwner();
	UNUJobComponent* JobComponent = GetJobComponent();
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	const FNUJobDefinitionRow* Job = Data != nullptr ? Data->FindJob(JobId) : nullptr;
	if (Owner == nullptr || !Owner->HasAuthority() || !bLocalProgressionBackendReady || JobComponent == nullptr)
	{
		OutError = TEXT("Local UE 직업 선택 권한이 없음");
		return false;
	}
	if (Job == nullptr)
	{
		OutError = FString::Printf(TEXT("존재하지 않는 직업: JobId=%d"), JobId);
		return false;
	}
	if (JobComponent->HasJob())
	{
		OutError = FString::Printf(TEXT("이미 직업을 선택함: JobId=%d"), JobComponent->GetJobId());
		return false;
	}
	if (!State.AcquiredNodeIds.IsEmpty())
	{
		OutError = TEXT("이미 성장 노드를 획득함");
		return false;
	}

	const FNUProgressionNodeDefinitionRow* RootNode = nullptr;
	for (const FNUProgressionNodeDefinitionRow* Node : Data->GetNodesByGraph(Job->GraphId))
	{
		if (Node == nullptr || Node->NodeType != ENUProgressionNodeType::Root)
		{
			continue;
		}
		if (RootNode != nullptr)
		{
			OutError = FString::Printf(TEXT("직업 성장 그래프에 루트 노드가 중복됨: GraphId=%d"), Job->GraphId);
			return false;
		}
		RootNode = Node;
	}
	if (RootNode == nullptr)
	{
		OutError = FString::Printf(TEXT("직업 성장 그래프에 루트 노드가 없음: GraphId=%d"), Job->GraphId);
		return false;
	}
	if (RootNode->RequiredLevel > 0 || RootNode->PointCost > 0)
	{
		OutError = FString::Printf(TEXT("직업 시작 노드에는 레벨 조건이나 포인트 비용을 둘 수 없음: NodeId=%d Level=%d Cost=%d"), RootNode->NodeId, RootNode->RequiredLevel, RootNode->PointCost);
		return false;
	}

	int32 StartingSkillCount = 0;
	for (const FNUProgressionNodeRewardRow* Reward : Data->GetRewardsByNode(RootNode->NodeId))
	{
		if (Reward != nullptr && (Reward->RewardType == ENUProgressionNodeRewardType::ActiveSkill || Reward->RewardType == ENUProgressionNodeRewardType::PassiveSkill))
		{
			++StartingSkillCount;
		}
	}
	if (StartingSkillCount != 3)
	{
		OutError = FString::Printf(TEXT("직업 시작 스킬은 정확히 3개여야 함: NodeId=%d Count=%d"), RootNode->NodeId, StartingSkillCount);
		return false;
	}

	FNUJobState NewJobState;
	NewJobState.JobId = JobId;
	return TryAcquireLocalNode(*RootNode, NewJobState, OutError);
}

bool UNUProgressionComponent::TryAcquireLocalNode(const FNUProgressionNodeDefinitionRow& Node, const FNUJobState& RequestedJobState, FString& OutError)
{
	UNUJobComponent* JobComponent = GetJobComponent();
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	const FNUJobDefinitionRow* Job = Data != nullptr ? Data->FindJob(RequestedJobState.JobId) : nullptr;
	if (!bLocalProgressionBackendReady || JobComponent == nullptr || Job == nullptr)
	{
		OutError = TEXT("Local UE 노드 획득에 필요한 직업 상태 또는 정적 데이터가 없음");
		return false;
	}

	const FNUJobState CurrentJobState = JobComponent->GetState();
	const FNUJobState& AcquisitionJobState = Node.NodeType == ENUProgressionNodeType::Advancement ? CurrentJobState : RequestedJobState;
	FNUProgressionNodeRuleContext RuleContext;
	RuleContext.JobId = AcquisitionJobState.JobId;
	RuleContext.AdvancementId = AcquisitionJobState.AdvancementId;
	if (!CanAcquireNode(Node, RuleContext, Job->GraphId, OutError))
	{
		return false;
	}

	const int32 AcquiredParentNodeId = FNUProgressionNodeRules::ResolveAvailableParentId(Node, State, RuleContext);
	FNUProgressionState NewState = State;
	if (NewState.Revision == MAX_int64)
	{
		OutError = TEXT("성장 상태 리비전 오버플로");
		return false;
	}
	++NewState.Revision;
	FNUJobState NewJobState = RequestedJobState;
	TArray<const FNUProgressionNodeRewardRow*> ApplicableRewards;
	for (const FNUProgressionNodeRewardRow* Reward : Data->GetRewardsByNode(Node.NodeId))
	{
		if (Reward != nullptr && (Reward->ChoiceAdvancementId <= 0 || Reward->ChoiceAdvancementId == RequestedJobState.AdvancementId))
		{
			ApplicableRewards.Add(Reward);
		}
	}
	if (!ApplyLocalNodeRewards(Node, ApplicableRewards, NewState, NewJobState, OutError))
	{
		return false;
	}

	NewState.AvailablePoints -= Node.PointCost;
	NewState.AcquiredNodeIds.Add(Node.NodeId);
	if (AcquiredParentNodeId > 0)
	{
		FNUProgressionNodeLinkState& Link = NewState.AcquiredNodeLinks.AddDefaulted_GetRef();
		Link.ParentNodeId = AcquiredParentNodeId;
		Link.ChildNodeId = Node.NodeId;
	}
	NewState.CurrentNodeId = Node.NodeId;

	if (Node.NodeType == ENUProgressionNodeType::Advancement || Node.NodeType == ENUProgressionNodeType::Main)
	{
		if (Node.StemId > 0)
		{
			NewState.CompletedStemIds.AddUnique(Node.StemId);
		}
		NewState.ActiveStemId = 0;
	}
	else if (Node.NodeType == ENUProgressionNodeType::Final)
	{
		NewState.SelectedFinalNodeId = Node.NodeId;
		NewState.ActiveStemId = 0;
	}
	else if (Node.StemId > 0 && NewState.ActiveStemId <= 0)
	{
		NewState.ActiveStemId = Node.StemId;
	}

	FNUProgressionServerSnapshot LocalSnapshot;
	LocalSnapshot.JobState = NewJobState;
	LocalSnapshot.ProgressionState = MoveTemp(NewState);
	if (!ApplyServerSnapshot(LocalSnapshot))
	{
		OutError = FString::Printf(TEXT("Local UE 성장 상태 적용 실패: NodeId=%d"), Node.NodeId);
		return false;
	}

	UE_LOG(LogNUProgression, Display, TEXT("Local UE 성장 노드 획득: NodeId=%d Revision=%lld"), Node.NodeId, State.Revision);
	return true;
}

bool UNUProgressionComponent::ApplyLocalNodeRewards(const FNUProgressionNodeDefinitionRow& Node, const TArray<const FNUProgressionNodeRewardRow*>& Rewards, FNUProgressionState& InOutProgressionState, FNUJobState& InOutJobState, FString& OutError) const
{
	int32 AdvancementRewardCount = 0;
	for (const FNUProgressionNodeRewardRow* Reward : Rewards)
	{
		if (Reward == nullptr)
		{
			continue;
		}

		switch (Reward->RewardType)
		{
		case ENUProgressionNodeRewardType::PrimaryStat:
			if (!ApplyPrimaryStatReward(InOutProgressionState, Reward->RewardId, Reward->Value, OutError))
			{
				return false;
			}
			break;

		case ENUProgressionNodeRewardType::Advancement:
			++AdvancementRewardCount;
			if (Node.NodeType != ENUProgressionNodeType::Advancement || AdvancementRewardCount > 1 || Reward->RewardId != InOutJobState.AdvancementId)
			{
				OutError = TEXT("선택한 전직과 전직 노드 보상이 일치하지 않음");
				return false;
			}
			break;

		case ENUProgressionNodeRewardType::ActiveSkill:
		case ENUProgressionNodeRewardType::PassiveSkill:
			break;

		default:
			OutError = TEXT("지원하지 않는 노드 보상 타입");
			return false;
		}
	}

	if (Node.NodeType == ENUProgressionNodeType::Advancement && AdvancementRewardCount != 1)
	{
		OutError = TEXT("전직 노드에 선택한 전직 보상이 정확히 한 개 필요함");
		return false;
	}
	return true;
}

bool UNUProgressionComponent::ApplyPrimaryStatReward(FNUProgressionState& InOutState, int32 StatId, float Value, FString& OutError) const
{
	// Primary Stat은 정수이므로 테이블의 float 값을 반올림
	const int32 Amount = FMath::RoundToInt(Value);
	if (Amount <= 0)
	{
		OutError = TEXT("스탯 증가량은 0보다 커야 함");
		return false;
	}

	int32* StatValue = nullptr;
	switch (static_cast<ENUPrimaryStatId>(StatId))
	{
	case ENUPrimaryStatId::Strength:
		StatValue = &InOutState.PrimaryStats.Strength;
		break;
	case ENUPrimaryStatId::Dexterity:
		StatValue = &InOutState.PrimaryStats.Dexterity;
		break;
	case ENUPrimaryStatId::Intelligence:
		StatValue = &InOutState.PrimaryStats.Intelligence;
		break;
	case ENUPrimaryStatId::Luck:
		StatValue = &InOutState.PrimaryStats.Luck;
		break;
	case ENUPrimaryStatId::Vitality:
		StatValue = &InOutState.PrimaryStats.Vitality;
		break;
	default:
		break;
	}

	if (StatValue == nullptr)
	{
		OutError = FString::Printf(TEXT("지원하지 않는 1차 스탯 ID: %d"), StatId);
		return false;
	}

	if (*StatValue > MAX_int32 - Amount)
	{
		OutError = FString::Printf(TEXT("1차 스탯 오버플로: StatId=%d"), StatId);
		return false;
	}

	// 획득 전 State 복사본에만 적용하고 마지막에 한 번에 저장
	*StatValue += Amount;
	return true;
}

bool UNUProgressionComponent::ValidateState(const FNUProgressionState& CandidateState, const FNUProgressionNodeRuleContext& RuleContext, FString& OutError) const
{
	if (CandidateState.GraphVersion != NUProgressionGraphVersion || CandidateState.Revision < 0)
	{
		OutError = FString::Printf(TEXT("성장 그래프 버전 또는 상태 리비전이 잘못됨: GraphVersion=%d Revision=%lld"),
			CandidateState.GraphVersion,
			CandidateState.Revision);
		return false;
	}
	if (!FNULevelProgression::Get().ValidateExperienceState(CandidateState, OutError))
	{
		return false;
	}

	if (CandidateState.AvailablePoints < 0 || CandidateState.TotalPoints < 0 || CandidateState.AvailablePoints > CandidateState.TotalPoints)
	{
		OutError = TEXT("성장 포인트 합계와 사용 가능 포인트가 서로 맞지 않음");
		return false;
	}
	if (CandidateState.ActiveStemId < 0)
	{
		OutError = TEXT("줄기 ID는 음수일 수 없음");
		return false;
	}
	if (CandidateState.SelectedFinalNodeId < 0)
	{
		OutError = TEXT("최종 노드 ID는 음수일 수 없음");
		return false;
	}

	TSet<int32> UniqueNodeIds;
	for (const int32 NodeId : CandidateState.AcquiredNodeIds)
	{
		if (NodeId <= 0 || UniqueNodeIds.Contains(NodeId))
		{
			OutError = TEXT("획득 노드 ID는 비어 있거나 중복될 수 없음");
			return false;
		}
		UniqueNodeIds.Add(NodeId);
	}

	TSet<int32> UniqueCompletedStemIds;
	for (const int32 StemId : CandidateState.CompletedStemIds)
	{
		if (StemId <= 0 || UniqueCompletedStemIds.Contains(StemId))
		{
			OutError = TEXT("완료 줄기 ID는 비어 있거나 중복될 수 없음");
			return false;
		}
		UniqueCompletedStemIds.Add(StemId);
	}
	if (CandidateState.ActiveStemId > 0 && UniqueCompletedStemIds.Contains(CandidateState.ActiveStemId))
	{
		OutError = TEXT("진행 중인 줄기는 완료 줄기에 포함될 수 없음");
		return false;
	}

	TMap<int32, const FNUProgressionNodeLinkState*> LinkByChildNodeId;
	for (const FNUProgressionNodeLinkState& Link : CandidateState.AcquiredNodeLinks)
	{
		if (Link.ParentNodeId <= 0 || Link.ChildNodeId <= 0 || Link.ParentNodeId == Link.ChildNodeId
			|| LinkByChildNodeId.Contains(Link.ChildNodeId))
		{
			OutError = TEXT("획득 연결선의 노드 ID가 잘못되었거나 자식 노드가 중복됨");
			return false;
		}
		LinkByChildNodeId.Add(Link.ChildNodeId, &Link);
	}

	if (CandidateState.AcquiredNodeIds.IsEmpty())
	{
		if (CandidateState.CurrentNodeId > 0
			|| CandidateState.ActiveStemId > 0
			|| CandidateState.SelectedFinalNodeId > 0
			|| !CandidateState.CompletedStemIds.IsEmpty()
			|| !CandidateState.AcquiredNodeLinks.IsEmpty())
		{
			OutError = TEXT("획득 노드가 없으면 노드 진행 상태도 비어 있어야 함");
			return false;
		}
	}
	else
	{
		if (!UniqueNodeIds.Contains(CandidateState.CurrentNodeId))
		{
			OutError = TEXT("현재 노드는 획득 노드 목록에 포함되어야 함");
			return false;
		}
		if (CandidateState.AcquiredNodeLinks.Num() != CandidateState.AcquiredNodeIds.Num() - 1)
		{
			OutError = TEXT("루트를 제외한 모든 획득 노드에는 확정 연결선이 필요함");
			return false;
		}

		const UWorld* World = GetWorld();
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
			? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
			: nullptr;
		if (Data == nullptr || !Data->IsReady())
		{
			OutError = TEXT("성장 경로 검증에 필요한 노드 데이터가 없음");
			return false;
		}
		const FNUJobDefinitionRow* Job = Data->FindJob(RuleContext.JobId);
		if (Job == nullptr)
		{
			OutError = FString::Printf(TEXT("성장 경로의 직업을 찾을 수 없음: JobId=%d"), RuleContext.JobId);
			return false;
		}

		int32 RootNodeCount = 0;
		int64 SpentPoints = 0;
		FNUProgressionState ExpectedRewardState;
		for (const int32 NodeId : CandidateState.AcquiredNodeIds)
		{
			const FNUProgressionNodeDefinitionRow* Node = Data->FindNode(NodeId);
			if (Node == nullptr)
			{
				OutError = FString::Printf(TEXT("존재하지 않는 획득 노드: %d"), NodeId);
				return false;
			}
			if (Node->GraphId != Job->GraphId)
			{
				OutError = FString::Printf(TEXT("다른 성장 그래프의 노드: %d"), NodeId);
				return false;
			}
			if ((Node->RequiredJobId > 0 && Node->RequiredJobId != RuleContext.JobId)
				|| (Node->RequiredAdvancementId > 0
					&& Node->RequiredAdvancementId != RuleContext.AdvancementId))
			{
				OutError = FString::Printf(TEXT("현재 직업 또는 전직으로 획득할 수 없는 노드: %d"), NodeId);
				return false;
			}
			if (Node->RequiredLevel > 0 && Node->RequiredLevel > CandidateState.Level)
			{
				OutError = FString::Printf(TEXT("현재 레벨보다 높은 노드를 획득함: %d"), NodeId);
				return false;
			}
			SpentPoints += Node->PointCost;
			for (const FNUProgressionNodeRewardRow* Reward : Data->GetRewardsByNode(NodeId))
			{
				if (Reward != nullptr
					&& Reward->RewardType == ENUProgressionNodeRewardType::PrimaryStat
					&& !ApplyPrimaryStatReward(ExpectedRewardState, Reward->RewardId, Reward->Value, OutError))
				{
					return false;
				}
			}

			if (Node->NodeType == ENUProgressionNodeType::Root)
			{
				++RootNodeCount;
				if (LinkByChildNodeId.Contains(NodeId))
				{
					OutError = TEXT("루트 노드에는 부모 연결이 있을 수 없음");
					return false;
				}
			}
			else
			{
				const FNUProgressionNodeLinkState* const* LinkPtr = LinkByChildNodeId.Find(NodeId);
				const FNUProgressionNodeLinkState* Link = LinkPtr != nullptr ? *LinkPtr : nullptr;
				const FNUProgressionNodeDefinitionRow* LinkedParent = Link != nullptr
					? Data->FindNode(Link->ParentNodeId)
					: nullptr;
				const bool bHasStaticParent = Link != nullptr
					&& (Node->ParentNodeId1 == Link->ParentNodeId
						|| Node->ParentNodeId2 == Link->ParentNodeId
						|| Node->ParentNodeId3 == Link->ParentNodeId);
				const bool bHasFinalStemParent = Node->NodeType == ENUProgressionNodeType::Final
					&& LinkedParent != nullptr
					&& LinkedParent->NodeType == ENUProgressionNodeType::Main
					&& CandidateState.CompletedStemIds.Contains(LinkedParent->StemId);
				if (Link == nullptr || !UniqueNodeIds.Contains(Link->ParentNodeId)
					|| (!bHasStaticParent && !bHasFinalStemParent))
				{
					OutError = FString::Printf(TEXT("잘못된 성장 경로 연결: ChildNodeId=%d"), NodeId);
					return false;
				}
			}
		}
		if (RootNodeCount != 1)
		{
			OutError = FString::Printf(TEXT("성장 경로에는 루트 노드가 정확히 하나 필요함: Count=%d"), RootNodeCount);
			return false;
		}
		if (SpentPoints > CandidateState.TotalPoints || CandidateState.TotalPoints - SpentPoints != CandidateState.AvailablePoints)
		{
			OutError = FString::Printf(TEXT("성장 포인트 정산 불일치: Total=%d Spent=%lld Available=%d"), CandidateState.TotalPoints, SpentPoints, CandidateState.AvailablePoints);
			return false;
		}
		if (ExpectedRewardState.PrimaryStats != CandidateState.PrimaryStats)
		{
			OutError = TEXT("획득 노드 보상 합계와 서버 1차 스탯이 일치하지 않음");
			return false;
		}

		const FNUProgressionNodeDefinitionRow* CurrentNode = Data->FindNode(CandidateState.CurrentNodeId);
		const bool bCurrentNodeEndsStem = CurrentNode != nullptr
			&& (CurrentNode->NodeType == ENUProgressionNodeType::Advancement
				|| CurrentNode->NodeType == ENUProgressionNodeType::Main
				|| CurrentNode->NodeType == ENUProgressionNodeType::Final);
		const bool bCurrentNodeRequiresActiveStem = CurrentNode != nullptr
			&& CurrentNode->StemId > 0
			&& !bCurrentNodeEndsStem;
		if (CurrentNode == nullptr
			|| (bCurrentNodeRequiresActiveStem && CandidateState.ActiveStemId != CurrentNode->StemId)
			|| (!bCurrentNodeRequiresActiveStem && CandidateState.ActiveStemId > 0))
		{
			OutError = TEXT("진행 중 줄기와 현재 노드가 일치하지 않음");
			return false;
		}

		int32 CompletedEntryStemCount = 0;
		int32 CompletedRadialStemCount = 0;
		for (const int32 CompletedStemId : CandidateState.CompletedStemIds)
		{
			const FNUProgressionNodeDefinitionRow* TerminalNode = nullptr;
			for (const int32 NodeId : CandidateState.AcquiredNodeIds)
			{
				const FNUProgressionNodeDefinitionRow* Node = Data->FindNode(NodeId);
				if (Node != nullptr
					&& Node->StemId == CompletedStemId
					&& (Node->NodeType == ENUProgressionNodeType::Main
						|| Node->NodeType == ENUProgressionNodeType::Advancement))
				{
					TerminalNode = Node;
					break;
				}
			}
			if (TerminalNode == nullptr)
			{
				OutError = FString::Printf(TEXT("완료 줄기의 종착 노드를 획득하지 않음: StemId=%d"), CompletedStemId);
				return false;
			}
			CompletedEntryStemCount += TerminalNode->NodeType == ENUProgressionNodeType::Advancement ? 1 : 0;
			CompletedRadialStemCount += TerminalNode->NodeType == ENUProgressionNodeType::Main ? 1 : 0;
		}
		if (CompletedEntryStemCount > 1
			|| CompletedRadialStemCount > 4
			|| CompletedRadialStemCount != FNUProgressionNodeRules::GetCompletedRadialStemCount(CandidateState))
		{
			OutError = FString::Printf(TEXT("완료 줄기 구성이 잘못됨: Entry=%d Radial=%d"), CompletedEntryStemCount, CompletedRadialStemCount);
			return false;
		}

		int32 AcquiredFinalNodeCount = 0;
		int32 AcquiredFinalNodeId = 0;
		for (const int32 NodeId : CandidateState.AcquiredNodeIds)
		{
			const FNUProgressionNodeDefinitionRow* Node = Data->FindNode(NodeId);
			if (Node != nullptr && Node->NodeType == ENUProgressionNodeType::Final)
			{
				++AcquiredFinalNodeCount;
				AcquiredFinalNodeId = NodeId;
			}
		}
		if (AcquiredFinalNodeCount > 1
			|| (AcquiredFinalNodeCount == 0 && CandidateState.SelectedFinalNodeId != 0)
			|| (AcquiredFinalNodeCount == 1
				&& (CandidateState.SelectedFinalNodeId != AcquiredFinalNodeId || CompletedRadialStemCount != 4)))
		{
			OutError = TEXT("최종 노드 선택 상태가 획득 경로와 다름");
			return false;
		}

		int32 CurrentRequiredLevel = FNUProgressionNodeRules::ResolveRequiredLevel(*CurrentNode, CandidateState);
		if (CurrentNode->NodeType == ENUProgressionNodeType::Main)
		{
			CurrentRequiredLevel = 20 + CompletedRadialStemCount * 10;
		}
		if (CurrentRequiredLevel > 0 && CandidateState.Level < CurrentRequiredLevel)
		{
			OutError = FString::Printf(TEXT("현재 성장 단계보다 레벨이 낮음: 현재=%d 필요=%d"), CandidateState.Level, CurrentRequiredLevel);
			return false;
		}

		int32 LastAcquiredAdvancementId = 0;
		int32 MatchingAdvancementRewardCount = 0;
		bool bHasAcquiredAdvancementNode = false;
		for (const int32 NodeId : CandidateState.AcquiredNodeIds)
		{
			const FNUProgressionNodeDefinitionRow* AcquiredNode = Data->FindNode(NodeId);
			bHasAcquiredAdvancementNode |= AcquiredNode != nullptr
				&& AcquiredNode->NodeType == ENUProgressionNodeType::Advancement;
			for (const FNUProgressionNodeRewardRow* Reward : Data->GetRewardsByNode(NodeId))
			{
				if (Reward != nullptr
					&& Reward->RewardType == ENUProgressionNodeRewardType::Advancement
					&& Reward->ChoiceAdvancementId == RuleContext.AdvancementId
					&& Reward->RewardId == RuleContext.AdvancementId)
				{
					++MatchingAdvancementRewardCount;
					LastAcquiredAdvancementId = Reward->RewardId;
				}
			}
		}
		if ((RuleContext.AdvancementId <= 0 && bHasAcquiredAdvancementNode)
			|| (RuleContext.AdvancementId > 0 && MatchingAdvancementRewardCount != 1)
			|| RuleContext.AdvancementId != LastAcquiredAdvancementId)
		{
			OutError = FString::Printf(TEXT("직업 상태와 획득한 전직 노드가 다름: Job=%d Acquired=%d"),
				RuleContext.AdvancementId, LastAcquiredAdvancementId);
			return false;
		}
		if ((RuleContext.AdvancementId <= 0 && CompletedEntryStemCount != 0)
			|| (RuleContext.AdvancementId > 0 && CompletedEntryStemCount != 1))
		{
			OutError = TEXT("전직 상태와 진입 줄기 완료 상태가 다름");
			return false;
		}
	}

	const FNUPrimaryStats Primary = ResolvePrimaryStats(CandidateState);
	if (!NUProgression::ArePrimaryStatsValid(Primary))
	{
		OutError = TEXT("1차 스탯은 0~29 범위여야 하며 STR과 INT를 동시에 가질 수 없음");
		return false;
	}

	return FNUStatTuning::Get().Validate(OutError);
}

FNUPrimaryStats UNUProgressionComponent::ResolvePrimaryStats(const FNUProgressionState& CandidateState) const
{
	return CandidateState.PrimaryStats;
}

bool UNUProgressionComponent::ProjectStateToASC(const FNUProgressionState& CandidateState)
{
	UNUAbilitySystemComponent* ASC = GetOwnerASC();
	if (ASC == nullptr)
	{
		UE_LOG(LogNUProgression, Error, TEXT("성장 상태 GAS 반영 실패: ASC 없음"));
		return false;
	}

	if (!EnsureStatEffectsApplied(ASC))
	{
		return false;
	}

	ApplyPrimaryStats(ASC, ResolvePrimaryStats(CandidateState));
	return true;
}

void UNUProgressionComponent::ApplyPrimaryStats(UNUAbilitySystemComponent* ASC, const FNUPrimaryStats& Primary) const
{

	ASC->SetNumericAttributeBase(UNUStatAttributeSet::GetStrengthAttribute(),     Primary.Strength);
	ASC->SetNumericAttributeBase(UNUStatAttributeSet::GetDexterityAttribute(),    Primary.Dexterity);
	ASC->SetNumericAttributeBase(UNUStatAttributeSet::GetIntelligenceAttribute(), Primary.Intelligence);
	ASC->SetNumericAttributeBase(UNUStatAttributeSet::GetLuckAttribute(),         Primary.Luck);
	ASC->SetNumericAttributeBase(UNUStatAttributeSet::GetVitalityAttribute(),     Primary.Vitality);
}

bool UNUProgressionComponent::EnsureStatEffectsApplied(UNUAbilitySystemComponent* ASC)
{
	return EnsureEffectApplied(ASC, BaseSecondaryStatsEffect, BaseSecondaryStatsHandle, TEXT("기본 2차 스탯 효과"))
		&& EnsureEffectApplied(ASC, SecondaryStatScalingEffect, SecondaryStatScalingHandle, TEXT("2차 스탯 환산 효과"));
}

bool UNUProgressionComponent::EnsureEffectApplied(UNUAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass, FActiveGameplayEffectHandle& InOutHandle, const TCHAR* EffectLabel)
{
	if (!EffectClass)
	{
		UE_LOG(LogNUProgression, Error, TEXT("%s 미설정"), EffectLabel);
		return false;
	}

	if (InOutHandle.IsValid() && ASC->GetActiveGameplayEffect(InOutHandle) != nullptr)
	{
		return true;
	}
	InOutHandle.Invalidate();

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogNUProgression, Error, TEXT("%s Spec 생성 실패"), EffectLabel);
		return false;
	}

	InOutHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	if (!InOutHandle.IsValid() || ASC->GetActiveGameplayEffect(InOutHandle) == nullptr)
	{
		UE_LOG(LogNUProgression, Error, TEXT("%s 적용 실패"), EffectLabel);
		InOutHandle.Invalidate();
		return false;
	}

	return true;
}

void UNUProgressionComponent::InitializeCurrentVitals(UNUAbilitySystemComponent* ASC) const
{
	const UNUVitalAttributeSet* VitalSet = ASC->GetVitalAttributeSet();
	if (VitalSet == nullptr)
	{
		return;
	}

	ASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetHealthAttribute(), VitalSet->GetMaxHealth());
	ASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetSPAttribute(), VitalSet->GetMaxSP());
}

UEmberServerManager* UNUProgressionComponent::GetServerManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
}
