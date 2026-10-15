#include "Quest/Component/NUQuestMarkerComponent.h"

#include "CharacterState/NUPlayerState.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUJobProviderComponent.h"
#include "Field/Presence/NUActorPresenceComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Component/NUQuestProviderComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	// 마커를 띄우지 않는 단계는 무효 태그를 돌려준다
	FGameplayTag ToMarkerTag(ENUQuestInteractionPhase Phase, bool bShowLockedQuestMarker)
	{
		switch (Phase)
		{
		case ENUQuestInteractionPhase::Offer:		return NU::Marker::QuestOffer;
		case ENUQuestInteractionPhase::Locked:		return bShowLockedQuestMarker ? NU::Marker::QuestLocked : FGameplayTag();
		case ENUQuestInteractionPhase::TurnIn:		return NU::Marker::QuestTurnIn;
		case ENUQuestInteractionPhase::Target:		return NU::Marker::QuestTarget;
		// 진행 중 대사는 분위기 대사라 말을 걸면 들을 수 있을 뿐 찾아갈 곳을 알리지 않는다.
		// 찾아가야 하는 NPC는 대화 목표의 대상 마커가 안내한다.
		case ENUQuestInteractionPhase::InProgress:	return FGameplayTag();
		default:									return FGameplayTag();
		}
	}
}

void UNUQuestMarkerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsMarkerEnabled())
	{
		return;
	}

	QuestProvider = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UNUQuestProviderComponent>()
		: nullptr;

	if (!IsValid(QuestProvider))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestMarker: 같은 액터에 UNUQuestProviderComponent가 없습니다 [%s]"),
			GetOwner() != nullptr ? *GetOwner()->GetName() : TEXT("Unknown")
		);

		return;
	}

	TryBindQuestComponent();
}

void UNUQuestMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}

	if (IsValid(BoundQuestComponent))
	{
		BoundQuestComponent->OnQuestProgressChanged.RemoveDynamic(this, &ThisClass::HandleQuestProgressChanged);
		const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(BoundQuestComponent->GetOwner());
		if (UNUJobComponent* JobComponent = PlayerState != nullptr ? PlayerState->GetJobComponent() : nullptr)
		{
			JobComponent->OnJobChanged.RemoveDynamic(this, &ThisClass::HandleQuestProgressChanged);
		}
	}

	BoundQuestComponent = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UNUQuestMarkerComponent::TryBindQuestComponent()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const APawn* LocalPawn = GetLocalPlayerPawn();
	const ANUPlayerState* PlayerState = LocalPawn != nullptr
		? LocalPawn->GetPlayerState<ANUPlayerState>()
		: nullptr;
	UNUQuestComponent* QuestComponent = PlayerState != nullptr
		? PlayerState->GetQuestComponent()
		: nullptr;

	if (!IsValid(QuestComponent))
	{
		// 아직 준비되지 않았다. 찾을 때까지 다시 시도한다
		World->GetTimerManager().SetTimer(
			BindRetryTimer,
			this,
			&UNUQuestMarkerComponent::TryBindQuestComponent,
			BindRetryInterval,
			false
		);

		return;
	}

	World->GetTimerManager().ClearTimer(BindRetryTimer);

	BoundQuestComponent = QuestComponent;
	BoundQuestComponent->OnQuestProgressChanged.AddUniqueDynamic(this, &ThisClass::HandleQuestProgressChanged);
	// 전직 NPC 마커는 직업 선택 가능 여부도 보므로 직업이 바뀌어도 다시 계산한다(전직 뒤 남는 목표 마커).
	if (UNUJobComponent* JobComponent = PlayerState->GetJobComponent())
	{
		JobComponent->OnJobChanged.AddUniqueDynamic(this, &ThisClass::HandleQuestProgressChanged);
	}

	UE_LOG(LogNUQuest, Log, TEXT("QuestMarker: 바인딩 완료 [%s] ActorId=%s"),
		GetOwner() != nullptr ? *GetOwner()->GetName() : TEXT("Unknown"),
		*QuestProvider->QuestActorId.ToString()
	);

	RefreshMarker();
}

void UNUQuestMarkerComponent::HandleQuestProgressChanged()
{
	RefreshMarker();
}

void UNUQuestMarkerComponent::RefreshMarker()
{
	if (!IsValid(QuestProvider))
	{
		return;
	}

	APawn* LocalPawn = GetLocalPlayerPawn();
	const ENUQuestInteractionPhase Phase = QuestProvider->GetInteractionPhase(LocalPawn);
	FGameplayTag DesiredMarkerTag = ToMarkerTag(Phase, bShowLockedQuestMarker);
	const UNUActorPresenceComponent* Presence = GetOwner()->FindComponentByClass<UNUActorPresenceComponent>();
	// 직업 선택은 퀘스트 대화와 별도 서비스지만 선택 가능한 NPC를 같은 목표 마커로 안내한다.
	if (Phase != ENUQuestInteractionPhase::TurnIn && Phase != ENUQuestInteractionPhase::Target
		&& (Presence == nullptr || !Presence->IsPlacementTransitionPending()))
	{
		if (const UNUJobProviderComponent* JobProvider = GetOwner()->FindComponentByClass<UNUJobProviderComponent>())
		{
			FNUInteractionContext Context;
			Context.Interactor = LocalPawn;
			Context.Target = GetOwner();
			TArray<FNUInteractionOption> Options;
			JobProvider->CollectInteractionOptions_Implementation(Context, Options);
			if (Options.ContainsByPredicate([](const FNUInteractionOption& Option) { return Option.bEnabled; }))
			{
				DesiredMarkerTag = NU::Marker::QuestTarget;
			}
		}
	}

	SetMarkerTag(DesiredMarkerTag);
}

APawn* UNUQuestMarkerComponent::GetLocalPlayerPawn() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World != nullptr
		? World->GetFirstPlayerController()
		: nullptr;

	return PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
}
