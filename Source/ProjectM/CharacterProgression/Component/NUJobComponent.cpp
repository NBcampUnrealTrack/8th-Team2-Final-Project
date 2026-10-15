#include "CharacterProgression/Component/NUJobComponent.h"

#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUJobTypes.h"
#include "CharacterProgression/Data/NUProgressionDataSettings.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterProgression/Net/NUProgressionNetSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Data/NUQuestTutorialTags.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UNUJobComponent::UNUJobComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNUJobComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, State, COND_OwnerOnly);
}

bool UNUJobComponent::ApplyState(const FNUJobState& NewState)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	return ApplyStateInternal(NewState, true);
}

void UNUJobComponent::ServerDebugSetJobState_Implementation(int32 InJobId, int32 InAdvancementId)
{
	FNUJobState NewState;
	NewState.JobId = InJobId;
	NewState.AdvancementId = InAdvancementId;

	if (ApplyState(NewState))
	{
		UE_LOG(LogNUProgression, Log, TEXT("디버그 직업 상태 설정: JobId=%d AdvancementId=%d"),
			InJobId,
			InAdvancementId);
	}
}

bool UNUJobComponent::IsInitialJobSelectionNpc(FName NpcId) const
{
	const UNUProgressionDataSettings* Settings = GetDefault<UNUProgressionDataSettings>();
	return Settings != nullptr
		&& !Settings->InitialJobSelectionNpcId.IsNone()
		&& Settings->InitialJobSelectionNpcId == NpcId;
}

bool UNUJobComponent::CanAuthorizeInitialJobSelection(FName NpcId) const
{
	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(GetOwner());
	const UNUQuestComponent* Quest = PlayerState != nullptr ? PlayerState->GetQuestComponent() : nullptr;

	const UNUProgressionDataSettings* Settings = GetDefault<UNUProgressionDataSettings>();
	const bool bUnlockedByQuests = Quest != nullptr
		&& Settings != nullptr
		&& !Settings->InitialJobSelectionUnlockQuestIds.IsEmpty()
		&& Quest->HasCompletedQuests(Settings->InitialJobSelectionUnlockQuestIds);

	// 선행 의뢰와 대화 순서는 퀘스트 데이터의 목표 활성 조건을 따른다. 설정의 해금 퀘스트를 끝냈으면 목표 없이도 허용한다.
	return !HasJob()
		&& IsInitialJobSelectionNpc(NpcId)
		&& Quest != nullptr
		&& (Quest->HasActiveObjectiveFor(NU::Quest::Objective::JobSelected, NAME_None) || bUnlockedByQuests);
}

bool UNUJobComponent::TryAuthorizeInitialJobSelection(FName NpcId)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr
		|| !Owner->HasAuthority()
		|| !CanAuthorizeInitialJobSelection(NpcId))
	{
		return false;
	}

	bInitialJobSelectionAuthorized = true;
	return true;
}

bool UNUJobComponent::RequestInitialJobSelection(int32 JobId)
{
	const UNUProgressionDataSettings* Settings = GetDefault<UNUProgressionDataSettings>();
	if (Settings == nullptr || !CanAuthorizeInitialJobSelection(Settings->InitialJobSelectionNpcId) || JobId <= 0)
	{
		return false;
	}

	FNUJobState RequestedState;
	RequestedState.JobId = JobId;
	FString Error;
	if (!CanApplyState(RequestedState, Error))
	{
		UE_LOG(LogNUProgression, Warning, TEXT("직업 선택 요청 거부: JobId=%d 이유=%s"), JobId, *Error);
		return false;
	}

	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(GetOwner());
	UNUProgressionComponent* Progression = PlayerState != nullptr ? PlayerState->GetProgressionComponent() : nullptr;
	if (Progression != nullptr && Progression->IsUsingLocalProgressionBackend())
	{
		ServerSelectInitialJob(JobId);
		return true;
	}

	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	UNUProgressionNetSubsystem* ProgressionNet = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUProgressionNetSubsystem>() : nullptr;
	return ProgressionNet != nullptr && ProgressionNet->RequestInitialJobSelection(JobId);
}

void UNUJobComponent::ServerSelectInitialJob_Implementation(int32 JobId)
{
	const UNUProgressionDataSettings* Settings = GetDefault<UNUProgressionDataSettings>();
	if (!bInitialJobSelectionAuthorized || Settings == nullptr || !CanAuthorizeInitialJobSelection(Settings->InitialJobSelectionNpcId))
	{
		UE_LOG(LogNUProgression, Warning, TEXT("Local UE 직업 선택 거부: 최초 직업 선택 NPC 대화를 완료하지 않음"));
		return;
	}

	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(GetOwner());
	UNUProgressionComponent* Progression = PlayerState != nullptr ? PlayerState->GetProgressionComponent() : nullptr;
	if (Progression == nullptr || !Progression->IsUsingLocalProgressionBackend())
	{
		UE_LOG(LogNUProgression, Warning, TEXT("Local UE 직업 선택 거부: 로컬 성장 백엔드가 비활성 상태임"));
		return;
	}

	FString Error;
	if (!Progression->TryInitializeLocalJobProgression(JobId, Error))
	{
		UE_LOG(LogNUProgression, Warning, TEXT("Local UE 직업 선택 거부: JobId=%d 이유=%s"), JobId, *Error);
	}
}

void UNUJobComponent::OnRep_State()
{
	NotifyStateChanged();
}

bool UNUJobComponent::ApplyStateInternal(const FNUJobState& NewState, bool bNotify)
{
	FString Error;
	if (!CanApplyState(NewState, Error))
	{
		UE_LOG(LogNUProgression, Warning, TEXT("직업 상태 적용 거부: JobId=%d AdvancementId=%d 이유=%s"), NewState.JobId, NewState.AdvancementId, *Error);
		return false;
	}
	if (State == NewState)
	{
		return true;
	}

	State = NewState;
	if (State.JobId > 0)
	{
		bInitialJobSelectionAuthorized = false;
	}
	if (AActor* Owner = GetOwner(); Owner != nullptr && Owner->HasAuthority())
	{
		Owner->ForceNetUpdate();
	}
	if (bNotify)
	{
		NotifyStateChanged();
	}
	return true;
}

void UNUJobComponent::NotifyStateChanged()
{
	OnJobChanged.Broadcast();
}

bool UNUJobComponent::CanApplyState(const FNUJobState& CandidateState, FString& OutError) const
{
	if (CandidateState.JobId < 0 || CandidateState.AdvancementId < 0)
	{
		OutError = TEXT("직업과 전직 ID는 음수일 수 없음");
		return false;
	}

	// 로그인 상태가 아직 도착하지 않은 초기 상태
	if (CandidateState.JobId == 0)
	{
		if (CandidateState.AdvancementId != 0)
		{
			OutError = TEXT("기본 직업 없이 전직만 지정할 수 없음");
			return false;
		}
		return true;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	if (Data == nullptr || !Data->IsReady())
	{
		OutError = TEXT("직업 정적 데이터가 준비되지 않음");
		return false;
	}

	if (Data->FindJob(CandidateState.JobId) == nullptr)
	{
		OutError = FString::Printf(TEXT("존재하지 않는 JobId: %d"), CandidateState.JobId);
		return false;
	}

	if (CandidateState.AdvancementId <= 0)
	{
		return true;
	}

	const FNUJobAdvancementDefinitionRow* Advancement = Data->FindAdvancement(CandidateState.AdvancementId);
	if (Advancement == nullptr)
	{
		OutError = FString::Printf(TEXT("존재하지 않는 AdvancementId: %d"), CandidateState.AdvancementId);
		return false;
	}
	if (Advancement->ParentJobId != CandidateState.JobId)
	{
		OutError = FString::Printf(TEXT("직업과 전직의 기본 직업이 다름: JobId=%d AdvancementId=%d ParentJobId=%d"),
			CandidateState.JobId,
			CandidateState.AdvancementId,
			Advancement->ParentJobId);
		return false;
	}

	return true;
}
