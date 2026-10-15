#include "Cinematics/NUStorySequenceCue.h"

#include "Cinematics/NUStorySlideshowWidget.h"
#include "Cinematics/NUStorySlideshowData.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "TimerManager.h"
#include "Tracks/MovieScenePropertyTrack.h"

namespace
{
	// 이 바인딩에 ANUStorySequenceCue 가 선언한 속성(Interp)의 트랙이 있는지
	// 바인딩의 클래스(FMovieScenePossessable::GetPossessedObjectClass)는 에디터 전용이라 패키지 빌드에서 못 읽어 트랙으로 가린다
	bool HasCuePropertyTrack(const FMovieSceneBinding& Binding)
	{
		for (const UMovieSceneTrack* Track : Binding.GetTracks())
		{
			const UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track);
			const FProperty* Property = PropertyTrack
				? ANUStorySequenceCue::StaticClass()->FindPropertyByName(PropertyTrack->GetPropertyName())
				: nullptr;

			if (Property && Property->GetOwnerClass() == ANUStorySequenceCue::StaticClass())
			{
				return true;
			}
		}

		return false;
	}
}

ANUStorySequenceCue::ANUStorySequenceCue()
{
	// 시퀀스가 PawnMove 트랙으로 플레이어를 걷게 할 때만 일한다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ANUStorySequenceCue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaying || (FMath::IsNearlyZero(PawnMoveX) && FMath::IsNearlyZero(PawnMoveY)))
	{
		return;
	}
	if (APawn* Pawn = PlayingController.IsValid() ? PlayingController->GetPawn() : nullptr)
	{
		// 연출 중에는 필드 입력이 막혀 있으므로 강제 입력으로 넣는다.
		Pawn->AddMovementInput(FVector(PawnMoveX, PawnMoveY, 0.0f), 1.0f, true);
	}
}

bool ANUStorySequenceCue::PlayStory(APlayerController* Controller, UNUStorySlideshowData* Data, ULevelSequence* Sequence)
{
	if (bPlaying || !IsValid(Controller) || !Controller->IsLocalController() || !Data || !Sequence)
	{
		return false;
	}
	if (Controller->GetWorld() != GetWorld())
	{
		return false;
	}

	// 사진 연출의 상태 트랙 하나를 현재 Cue 액터에 연결한다.
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	FGuid CueBinding;
	if (!MovieScene)
	{
		return false;
	}
	// 큐의 속성 트랙이 달린 포세서블 바인딩이 큐 자리다 (LS_Prologue_Spirit 의 Intro Timing)
	for (const FMovieSceneBinding& Binding : AsConst(*MovieScene).GetBindings())
	{
		if (!MovieScene->FindPossessable(Binding.GetObjectGuid()) || !HasCuePropertyTrack(Binding))
		{
			continue;
		}
		if (CueBinding.IsValid())
		{
			return false;
		}
		CueBinding = Binding.GetObjectGuid();
	}
	if (!CueBinding.IsValid())
	{
		return false;
	}

	ALevelSequenceActor* NewActor = nullptr;
	bWasSkipped = false;
	bAllowSkip = Data->bAllowSkip;
	SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence, FMovieSceneSequencePlaybackSettings(), NewActor);
	SequenceActor = NewActor;
	if (!SequencePlayer)
	{
		FinishPlayback(false);
		return false;
	}

	// 에셋에 저장된 액터 대신 현재 Cue를 사용한다.
	SequenceActor->SetBinding(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(CueBinding)), { this }, false);

	Slideshow = CreateWidget<UNUStorySlideshowWidget>(Controller);
	if (!Slideshow)
	{
		FinishPlayback(false);
		return false;
	}

	Slideshow->AddToViewport(ViewportZOrder);
	if (!Slideshow->StartSequencePresentation(Data, this))
	{
		FinishPlayback(false);
		return false;
	}

	bPlaying = true;
	PlayingController = Controller;
	if (Data->SoundMix != nullptr)
	{
		ActiveSoundMix = Data->SoundMix;
		UGameplayStatics::PushSoundMixModifier(this, ActiveSoundMix);
	}
	Slideshow->OnSkipRequested.AddDynamic(this, &ThisClass::HandleSkipRequested);
	Slideshow->OnFinished.AddDynamic(this, &ThisClass::StopStory);
	SequencePlayer->OnFinished.AddDynamic(this, &ThisClass::HandleSequenceFinished);
	SequencePlayer->OnStop.AddDynamic(this, &ThisClass::HandleSequenceStopped);
	SequencePlayer->Play();
	return true;
}

void ANUStorySequenceCue::StopStory()
{
	FinishPlayback(false);
}

void ANUStorySequenceCue::HandleSequenceFinished()
{
	bPendingNaturalFinish = true;
	HandleSequenceStopped();
}

void ANUStorySequenceCue::HandleSequenceStopped()
{
	// 자연 종료도 OnStop 뒤에 OnFinished를 보낸다. 평가 도중 액터를 파괴하지 않고 다음 틱에 둘을 구분한다.
	if (!PendingFinishHandle.IsValid())
	{
		PendingFinishHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FinishPendingPlayback);
	}
}

void ANUStorySequenceCue::FinishPendingPlayback()
{
	FinishPlayback(bPendingNaturalFinish);
}

void ANUStorySequenceCue::HandleSkipRequested()
{
	if (!bPlaying || !bAllowSkip)
	{
		return;
	}

	bWasSkipped = true;
	FinishPlayback(false);
}

void ANUStorySequenceCue::FinishPlayback(bool bCompleted)
{
	GetWorld()->GetTimerManager().ClearTimer(PendingFinishHandle);
	bPendingNaturalFinish = false;
	// 시작 도중 실패한 경우에도 생성된 객체는 정리하되 종료 알림은 생략한다.
	const bool bNotify = bPlaying;
	bPlaying = false;
	PlayingController.Reset();

	// 종료 콜백을 먼저 해제하여 스킵과 자연 종료가 중복 통지되지 않게 한다.
	if (SequencePlayer)
	{
		SequencePlayer->OnFinished.RemoveDynamic(this, &ThisClass::HandleSequenceFinished);
		SequencePlayer->OnStop.RemoveDynamic(this, &ThisClass::HandleSequenceStopped);
		SequencePlayer->Stop();
	}
	if (Slideshow)
	{
		Slideshow->OnSkipRequested.RemoveDynamic(this, &ThisClass::HandleSkipRequested);
		Slideshow->OnFinished.RemoveDynamic(this, &ThisClass::StopStory);
		Slideshow->FinishSlideshow();
		Slideshow->RemoveFromParent();
	}
	if (IsValid(SequenceActor))
	{
		SequenceActor->Destroy();
	}
	if (ActiveSoundMix != nullptr)
	{
		UGameplayStatics::PopSoundMixModifier(this, ActiveSoundMix);
		ActiveSoundMix = nullptr;
	}

	// 종료 알림에서 다음 연출을 시작할 수 있도록 이전 참조부터 비운다.
	SequencePlayer = nullptr;
	SequenceActor = nullptr;
	Slideshow = nullptr;
	if (bNotify)
	{
		OnFinished.Broadcast(bCompleted);
	}
}

void ANUStorySequenceCue::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStory();
	Super::EndPlay(EndPlayReason);
}
