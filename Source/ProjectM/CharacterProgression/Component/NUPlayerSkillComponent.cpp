#include "CharacterProgression/Component/NUPlayerSkillComponent.h"

#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterProgression/Data/NUProgressionNodeRewardTypes.h"
#include "CharacterState/NUPlayerState.h"
#include "NUGAS/NUAbilitySystemComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUPlayerSkill, Log, All);

UNUPlayerSkillComponent::UNUPlayerSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNUPlayerSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Owner);
	UNUProgressionComponent* Progression = PlayerState != nullptr ? PlayerState->GetProgressionComponent() : nullptr;
	if (Progression != nullptr)
	{
		Progression->OnProgressionChanged.AddDynamic(this, &ThisClass::HandleProgressionChanged);
	}

	RefreshProgressionGrants();
}

void UNUPlayerSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ANUPlayerState* PlayerState = Cast<ANUPlayerState>(GetOwner());
	UNUProgressionComponent* Progression = PlayerState != nullptr ? PlayerState->GetProgressionComponent() : nullptr;
	if (Progression != nullptr)
	{
		Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UNUPlayerSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, State, COND_OwnerOnly);
}

bool UNUPlayerSkillComponent::ApplyState(const FNUPlayerSkillState& NewState)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Owner);
	if (!PlayerState)
	{
		return false;
	}

	FString Error;
	if (!ValidateState(NewState, Error))
	{
		UE_LOG(LogNUPlayerSkill, Warning, TEXT("플레이어 스킬 상태 적용 거부: %s"), *Error);
		return false;
	}

	if (State == NewState)
	{
		return true;
	}

	State = NewState;
	OnSkillsChanged.Broadcast();
	Owner->ForceNetUpdate();

	TArray<int32> ActiveSkillIds;
	TArray<int32> PassiveSkillIds;
	BuildEffectiveActiveSkillIds(State, ActiveSkillIds);
	BuildEffectivePassiveSkillIds(State, PassiveSkillIds);
	UE_LOG(LogNUPlayerSkill, Log,
		TEXT("플레이어 직업 스킬 보유 동기화: 액티브=%d 패시브=%d"),
		ActiveSkillIds.Num(),
		PassiveSkillIds.Num());
	UE_LOG(LogNUPlayerSkill, Verbose,
		TEXT("보유 액티브 스킬: [%s]"),
		*FString::JoinBy(ActiveSkillIds, TEXT(", "), [](const int32 SkillId)
		{
			return FString::FromInt(SkillId);
		}));

	// 전투 쪽으로 보냄
	TArray<int32> JobSkillIds = PassiveSkillIds;
	JobSkillIds.Append(ActiveSkillIds);

	if (UNUAbilitySystemComponent* ASC = PlayerState->GetNUASC())
	{
		ASC->SetJobSkillIds(JobSkillIds);
	}

	return true;
}

bool UNUPlayerSkillComponent::RefreshProgressionGrants()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Owner);
	const UNUProgressionComponent* Progression = PlayerState != nullptr ? PlayerState->GetProgressionComponent() : nullptr;
	const UNUJobComponent* Job = PlayerState != nullptr ? PlayerState->GetJobComponent() : nullptr;
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* ProgressionData = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	if (Progression == nullptr || Job == nullptr || ProgressionData == nullptr)
	{
		UE_LOG(LogNUPlayerSkill, Warning, TEXT("성장 노드 스킬 동기화 실패: 성장 상태 또는 정적 데이터 없음"));
		return false;
	}

	FNUPlayerSkillState NewState = State;
	NewState.OwnedActiveSkills.RemoveAll([](const FNUOwnedActiveSkill& Entry)
	{
		return Entry.GrantSource == ENUPlayerSkillGrantSource::ProgressionNode;
	});
	NewState.OwnedPassiveSkills.RemoveAll([](const FNUOwnedPassiveSkill& Entry)
	{
		return Entry.GrantSource == ENUPlayerSkillGrantSource::ProgressionNode;
	});

	for (const int32 NodeId : Progression->GetState().AcquiredNodeIds)
	{
		for (const FNUProgressionNodeRewardRow* Reward : ProgressionData->GetRewardsByNode(NodeId))
		{
			if (Reward == nullptr
				|| (Reward->ChoiceAdvancementId > 0 && Reward->ChoiceAdvancementId != Job->GetAdvancementId())
				|| (Reward->RewardType != ENUProgressionNodeRewardType::ActiveSkill
					&& Reward->RewardType != ENUProgressionNodeRewardType::PassiveSkill))
			{
				continue;
			}
			if (Reward->RewardId <= 0)
			{
				UE_LOG(LogNUPlayerSkill, Warning,
					TEXT("성장 노드 스킬 보상 무시: NodeId=%d SkillId=%d"),
					NodeId,
					Reward->RewardId);
				continue;
			}

			if (Reward->RewardType == ENUProgressionNodeRewardType::ActiveSkill)
			{
				FNUOwnedActiveSkill Entry;
				Entry.SkillId = Reward->RewardId;
				Entry.GrantSource = ENUPlayerSkillGrantSource::ProgressionNode;
				Entry.GrantSourceId = NodeId;
				NewState.OwnedActiveSkills.AddUnique(Entry);
			}
			else
			{
				FNUOwnedPassiveSkill Entry;
				Entry.SkillId = Reward->RewardId;
				Entry.GrantSource = ENUPlayerSkillGrantSource::ProgressionNode;
				Entry.GrantSourceId = NodeId;
				NewState.OwnedPassiveSkills.AddUnique(Entry);
			}
		}
	}

	NewState.OwnedActiveSkills.Sort([](const FNUOwnedActiveSkill& A, const FNUOwnedActiveSkill& B)
	{
		if (A.GrantSource != B.GrantSource)
		{
			return static_cast<uint8>(A.GrantSource) < static_cast<uint8>(B.GrantSource);
		}
		if (A.GrantSourceId != B.GrantSourceId)
		{
			return A.GrantSourceId < B.GrantSourceId;
		}
		return A.SkillId < B.SkillId;
	});
	NewState.OwnedPassiveSkills.Sort([](const FNUOwnedPassiveSkill& A, const FNUOwnedPassiveSkill& B)
	{
		if (A.GrantSource != B.GrantSource)
		{
			return static_cast<uint8>(A.GrantSource) < static_cast<uint8>(B.GrantSource);
		}
		if (A.GrantSourceId != B.GrantSourceId)
		{
			return A.GrantSourceId < B.GrantSourceId;
		}
		return A.SkillId < B.SkillId;
	});

	return ApplyState(NewState);
}

TArray<int32> UNUPlayerSkillComponent::GetOwnedActiveSkillIds() const
{
	TArray<int32> SkillIds;
	BuildEffectiveActiveSkillIds(State, SkillIds);
	return SkillIds;
}

TArray<int32> UNUPlayerSkillComponent::GetOwnedPassiveSkillIds() const
{
	TArray<int32> SkillIds;
	BuildEffectivePassiveSkillIds(State, SkillIds);
	return SkillIds;
}

bool UNUPlayerSkillComponent::HasActiveSkill(int32 SkillId) const
{
	return SkillId > 0 && State.OwnedActiveSkills.ContainsByPredicate([SkillId](const FNUOwnedActiveSkill& Entry)
	{
		return Entry.SkillId == SkillId;
	});
}

bool UNUPlayerSkillComponent::HasPassiveSkill(int32 SkillId) const
{
	return SkillId > 0 && State.OwnedPassiveSkills.ContainsByPredicate([SkillId](const FNUOwnedPassiveSkill& Entry)
	{
		return Entry.SkillId == SkillId;
	});
}

void UNUPlayerSkillComponent::OnRep_State()
{
	OnSkillsChanged.Broadcast();
}

void UNUPlayerSkillComponent::HandleProgressionChanged()
{
	RefreshProgressionGrants();
}

bool UNUPlayerSkillComponent::ValidateState(const FNUPlayerSkillState& CandidateState, FString& OutError) const
{
	const UEnum* GrantSourceEnum = StaticEnum<ENUPlayerSkillGrantSource>();
	TSet<FString> UniqueEntries;
	for (const FNUOwnedActiveSkill& Entry : CandidateState.OwnedActiveSkills)
	{
		if (Entry.SkillId <= 0 || Entry.GrantSourceId <= 0)
		{
			OutError = TEXT("스킬 ID와 획득 출처 ID는 0보다 커야 함");
			return false;
		}
		if (GrantSourceEnum == nullptr
			|| !GrantSourceEnum->IsValidEnumValue(static_cast<int64>(Entry.GrantSource)))
		{
			OutError = TEXT("스킬 획득 출처가 잘못됨");
			return false;
		}

		const FString EntryKey = FString::Printf(
			TEXT("%d:%d:%d"),
			static_cast<int32>(Entry.GrantSource),
			Entry.GrantSourceId,
			Entry.SkillId);
		if (UniqueEntries.Contains(EntryKey))
		{
			OutError = FString::Printf(TEXT("중복된 스킬 획득 출처: %s"), *EntryKey);
			return false;
		}
		UniqueEntries.Add(EntryKey);
	}
	for (const FNUOwnedPassiveSkill& Entry : CandidateState.OwnedPassiveSkills)
	{
		if (Entry.SkillId <= 0 || Entry.GrantSourceId <= 0)
		{
			OutError = TEXT("스킬 ID와 획득 출처 ID는 0보다 커야 함");
			return false;
		}
		if (GrantSourceEnum == nullptr
			|| !GrantSourceEnum->IsValidEnumValue(static_cast<int64>(Entry.GrantSource)))
		{
			OutError = TEXT("스킬 획득 출처가 잘못됨");
			return false;
		}

		const FString EntryKey = FString::Printf(
			TEXT("%d:%d:%d"),
			static_cast<int32>(Entry.GrantSource),
			Entry.GrantSourceId,
			Entry.SkillId);
		if (UniqueEntries.Contains(EntryKey))
		{
			OutError = FString::Printf(TEXT("중복된 스킬 획득 출처: %s"), *EntryKey);
			return false;
		}
		UniqueEntries.Add(EntryKey);
	}

	return true;
}

void UNUPlayerSkillComponent::BuildEffectiveActiveSkillIds(const FNUPlayerSkillState& SourceState, TArray<int32>& OutSkillIds)
{
	OutSkillIds.Reset();
	for (const FNUOwnedActiveSkill& Entry : SourceState.OwnedActiveSkills)
	{
		OutSkillIds.AddUnique(Entry.SkillId);
	}
	OutSkillIds.Sort();
}
void UNUPlayerSkillComponent::BuildEffectivePassiveSkillIds(
	const FNUPlayerSkillState& SourceState,
	TArray<int32>& OutSkillIds)
{
	OutSkillIds.Reset();
	for (const FNUOwnedPassiveSkill& Entry : SourceState.OwnedPassiveSkills)
	{
		OutSkillIds.AddUnique(Entry.SkillId);
	}
	OutSkillIds.Sort();
}
