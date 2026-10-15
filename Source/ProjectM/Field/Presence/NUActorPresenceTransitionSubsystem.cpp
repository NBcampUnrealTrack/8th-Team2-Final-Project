#include "Field/Presence/NUActorPresenceTransitionSubsystem.h"

#include "Field/Presence/NUActorPresenceComponent.h"
#include "Dialogue/UI/NUDialogueUIController.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestSceneComponent.h"
#include "Quest/Component/NUQuestMarkerComponent.h"
#include "Quest/Data/NUQuestTypes.h"
#include "UI/NUUIManager.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const FName PlacementInputReason(TEXT("ActorPresenceTransition"));
	constexpr float PlacementFadeSeconds = 0.2f;
	constexpr float PlacementBlackHoldSeconds = 0.4f;
	// 조건 연출은 연출 컴포넌트가 0.25초마다 대기열에 올리므로 그만큼은 기다린 뒤 암전한다.
	constexpr double SceneQueueGraceSeconds = 0.3;
}

void UNUActorPresenceTransitionSubsystem::Enqueue(UNUActorPresenceComponent* Component)
{
	Pending.AddUnique(Component);
	// 진행 알림에서 마커가 먼저 갱신됐더라도 이동 대기 중에는 즉시 숨긴다.
	if (UNUQuestMarkerComponent* Marker = Component->GetOwner()->FindComponentByClass<UNUQuestMarkerComponent>()) { Marker->RefreshMarker(); }
	if (!GetWorld()->GetTimerManager().IsTimerActive(UpdateTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateTransition, 0.03f, true);
	}
}

void UNUActorPresenceTransitionSubsystem::Cancel(UNUActorPresenceComponent* Component)
{
	Pending.Remove(Component);
}

void UNUActorPresenceTransitionSubsystem::ApplyPending()
{
	const TArray<TWeakObjectPtr<UNUActorPresenceComponent>> Batch = MoveTemp(Pending);
	Pending.Reset();
	for (const auto& Component : Batch)
	{
		if (Component.IsValid()) { Component->ApplyPendingPlacement(); }
	}
}

void UNUActorPresenceTransitionSubsystem::UpdateTransition()
{
	Pending.RemoveAll([](const auto& Component) { return !Component.IsValid(); });
	const double Now = GetWorld()->GetTimeSeconds();
	if (Phase != 0)
	{
		if (Now < PhaseEnd) { return; }
		ANUPlayerController* Controller = FadeController.Get();
		if (Phase == 1 && IsValid(Controller) && IsValid(Controller->PlayerCameraManager))
		{
			Controller->PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, false);
			ApplyPending();
			Phase = 2;
			PhaseEnd = Now + PlacementBlackHoldSeconds;
			return;
		}
		if (Phase == 2 && IsValid(Controller) && IsValid(Controller->PlayerCameraManager))
		{
			// 검은 화면을 유지하는 동안 추가된 재배치도 밝아지기 전에 마친다.
			ApplyPending();
			Controller->PlayerCameraManager->StartCameraFade(1.f, 0.f, PlacementFadeSeconds, FLinearColor::Black, false, false);
			Phase = 3;
			PhaseEnd = Now + PlacementFadeSeconds;
			return;
		}
		ReleaseFade();
	}
	if (Pending.IsEmpty())
	{
		UncoveredSince = -1.0;
		GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
		return;
	}
	ANUPlayerController* Controller = Cast<ANUPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!IsValid(Controller) || !Controller->IsLocalController()) { return; }

	APlayerCameraManager* Camera = Controller->PlayerCameraManager;
	// 실내 이동 등 다른 기능의 암전은 소유권을 빼앗지 않고 이미 가려진 순간에만 이용한다.
	if (Camera != nullptr && Camera->bEnableFading && Camera->FadeAmount > KINDA_SMALL_NUMBER)
	{
		if (Camera->FadeAmount >= .99f) { ApplyPending(); }
		return;
	}
	// 월드 시퀀스가 끝나며 이전 값을 복원하므로 재생 중에는 옮기지 않는다.
	const UNUQuestSceneComponent* Scene = Controller->FindComponentByClass<UNUQuestSceneComponent>();
	if (Scene != nullptr && Scene->IsWorldSequencePlaying()) { return; }

	// 지금 화면(연출 카메라 포함)에 원래 자리도 새 자리도 보이지 않으면 바로 옮긴다.
	const TArray<TWeakObjectPtr<UNUActorPresenceComponent>> Snapshot = Pending;
	for (const auto& Component : Snapshot)
	{
		if (Component.IsValid() && !Component->IsPlacementVisibleTo(*Controller))
		{
			Component->ApplyPendingPlacement();
		}
	}
	if (Pending.IsEmpty())
	{
		return;
	}

	// 대화·창·연출이 끝나기를 기다린다. 그동안 연출 카메라가 다른 곳을 비추면 위에서 옮겨진다.
	UNUUIManager* UI = UNUUIManager::Get(Controller);
	const UNUDialogueUIController* Dialogue = UI != nullptr ? UI->GetController<UNUDialogueUIController>() : nullptr;
	if ((Dialogue != nullptr && Dialogue->IsDialogueOpen())
		|| (UI != nullptr && UI->IsAnyWindowOpen(NUUILayer::Window))
		|| (Scene != nullptr && (Scene->IsScenePlaying() || Scene->HasPendingCameraScene())))
	{
		UncoveredSince = -1.0;
		return;
	}
	if (UncoveredSince < 0.0) { UncoveredSince = Now; }
	if (Now - UncoveredSince < SceneQueueGraceSeconds) { return; }
	if (Camera == nullptr)
	{
		ApplyPending();
		return;
	}

	// 가려 줄 연출이 없으면 짧은 암전 한 번에 남은 재배치를 모두 옮긴다.
	for (const auto& Component : Pending)
	{
		UE_LOG(LogNUQuest, Log, TEXT("ActorPresence: 가려 줄 연출이 없어 암전으로 옮깁니다 [%s]"), *GetNameSafe(Component.IsValid() ? Component->GetOwner() : nullptr));
	}
	FadeController = Controller;
	Controller->RequestGameInputBlock(PlacementInputReason, true, this);
	Camera->StartCameraFade(0.f, 1.f, PlacementFadeSeconds, FLinearColor::Black, false, true);
	Phase = 1;
	PhaseEnd = Now + PlacementFadeSeconds;
}

void UNUActorPresenceTransitionSubsystem::ReleaseFade()
{
	if (ANUPlayerController* Controller = FadeController.Get())
	{
		Controller->RequestGameInputBlock(PlacementInputReason, false, this);
		if (Controller->PlayerCameraManager != nullptr) { Controller->PlayerCameraManager->StopCameraFade(); }
	}
	FadeController.Reset();
	UncoveredSince = -1.0;
	Phase = 0;
}

void UNUActorPresenceTransitionSubsystem::Deinitialize()
{
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	ReleaseFade();
	Pending.Reset();
	Super::Deinitialize();
}
