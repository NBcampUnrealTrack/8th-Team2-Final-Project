#include "Cinematics/NUStoryPreviewGameMode.h"

#include "Cinematics/NUStorySequenceCue.h"
#include "Cinematics/NUStorySlideshowWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ANUStoryPreviewGameMode::ANUStoryPreviewGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}

void ANUStoryPreviewGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	ActiveCue = Cast<ANUStorySequenceCue>(UGameplayStatics::GetActorOfClass(this, ANUStorySequenceCue::StaticClass()));
	if (ActiveCue == nullptr) { return; }
	ActiveCue->OnFinished.AddDynamic(this, &ThisClass::HandleStoryFinished);
	if (!ActiveCue->PlayStory(Controller, StoryData, StorySequence))
	{
		return;
	}

	// 테스트 맵의 입력 정책이다. 실제 퀘스트에서는 기존 UI 관리 흐름과 연결한다.
	Controller->bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(Controller, ActiveCue->Slideshow);
}

void ANUStoryPreviewGameMode::HandleStoryFinished(bool bCompleted)
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	if (!Controller)
	{
		return;
	}

	UWidgetBlueprintLibrary::SetInputMode_GameOnly(Controller);
	Controller->bShowMouseCursor = false;
}

void ANUStoryPreviewGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ActiveCue))
	{
		ActiveCue->OnFinished.RemoveDynamic(this, &ThisClass::HandleStoryFinished);
		ActiveCue->StopStory();
	}
	ActiveCue = nullptr;
	Super::EndPlay(EndPlayReason);
}
