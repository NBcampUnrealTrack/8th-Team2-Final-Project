#include "Cinematics/NUPrologueSubsystem.h"

#include "Cinematics/NUPrologueSettings.h"
#include "Cinematics/NUStorySequenceCue.h"
#include "Cinematics/NUStorySlideshowData.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "Field/FieldPlayerCharacter.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Net/EmberServerManager.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/NUUIManager.h"
#include "WorldTransition/World/NUFieldLoadingSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Sound/SoundMix.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"

namespace NUPrologue
{
	static const FName Reason(TEXT("Prologue"));
	static const TCHAR* SaveSlot = TEXT("NUPrologue");
	// 검은 덮개: 필드 로딩 화면(10000) 바로 아래, 프롤로그 화면(전환 층)과 HUD 위
	constexpr int32 CoverZOrder = 9000;
	// 프롤로그 화면이 그려지고 덮개를 걷기까지(시퀀스 첫 1초 남짓은 검은 화면이라 이음매가 없다)
	constexpr float CoverHideDelay = 0.15f;
	// 로딩이 끝난 뒤 이만큼 기다려도 시작하지 못하면 덮개를 걷고 포기한다.
	constexpr float GiveUpSeconds = 20.0f;
	// 에셋을 미리 다 못 읽었어도 이만큼 지나면 그냥 시작한다(재생이 동기 로드로 마저 읽는다).
	constexpr float PreloadWaitSeconds = 1.0f;

	static TAutoConsoleVariable<int32> CVarAutoPlay(
		TEXT("nu.Prologue.Auto"),
		1,
		TEXT("1이면 새 캐릭터의 첫 필드 진입에 프롤로그를 자동 재생한다. 0이면 끈다(nu.Prologue.Play 는 그대로 동작)."));

	static UNUPrologueSaveGame* LoadRecords()
	{
		if (UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
		{
			if (UNUPrologueSaveGame* Loaded = Cast<UNUPrologueSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)))
			{
				return Loaded;
			}
		}
		return Cast<UNUPrologueSaveGame>(UGameplayStatics::CreateSaveGameObject(UNUPrologueSaveGame::StaticClass()));
	}
}

bool UNUPrologueSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UNUPrologueSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNUPrologueSubsystem, STATGROUP_Tickables);
}

void UNUPrologueSubsystem::Deinitialize()
{
	if (IsValid(Cue))
	{
		Cue->OnFinished.RemoveDynamic(this, &ThisClass::HandlePrologueFinished);
		Cue->StopStory();
		Cue->Destroy();
	}
	Cue = nullptr;
	CancelAutoPlay();
	HideCover();
	Super::Deinitialize();
}

void UNUPrologueSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 프롤로그 화면(검은 화면으로 시작)이 한 번 그려졌으면 덮개를 걷는다.
	if (CoverHideCountdown >= 0.0f)
	{
		CoverHideCountdown -= DeltaTime;
		if (CoverHideCountdown < 0.0f)
		{
			HideCover();
		}
	}

	if (bAutoPlayResolved || Cue != nullptr)
	{
		return;
	}

	const UNUPrologueSettings* Settings = GetDefault<UNUPrologueSettings>();
	if (NUPrologue::CVarAutoPlay.GetValueOnGameThread() == 0 || !Settings->bAutoPlayForNewCharacters || Settings->PrologueData.IsNull())
	{
		return;
	}

	ANUPlayerController* Controller = nullptr;
	FString CharacterKey;
	if (!bPrepared)
	{
		const ECandidate Candidate = CheckCandidate(Controller, CharacterKey);
		if (Candidate == ECandidate::No)
		{
			bAutoPlayResolved = true;
			return;
		}
		if (Candidate == ECandidate::NotYet)
		{
			return;
		}
		// 로딩 화면이 아직 떠 있을 때 준비해 두면, 로딩 화면이 걷혀도 필드가 비치지 않는다.
		PendingCharacterKey = CharacterKey;
		PrepareAutoPlay(Controller);
	}

	Controller = PreparedController.Get();
	UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUFieldLoadingSubsystem* Loading = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUFieldLoadingSubsystem>() : nullptr;
	if (Controller == nullptr || (Loading != nullptr && Loading->HasFailedField()))
	{
		// 입장 실패 화면에서는 재생하지 않는다(실패 화면은 덮개 위에 뜬다).
		UE_LOG(LogTemp, Log, TEXT("[Prologue] 자동 재생 취소 (%s)"), Controller == nullptr ? TEXT("컨트롤러 없음") : TEXT("필드 입장 실패"));
		bAutoPlayResolved = true;
		CancelAutoPlay();
		return;
	}

	const bool bLoadingField = Loading != nullptr && Loading->IsLoadingField();
	if (!bLoadingField)
	{
		WaitAfterLoadingSeconds += DeltaTime;
	}
	if (!IsReadyToStart(Controller))
	{
		ReadySeconds = 0.0f;
		if (WaitAfterLoadingSeconds > NUPrologue::GiveUpSeconds)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Prologue] 로딩이 끝난 뒤 %.0f초 동안 시작 조건이 맞지 않아 자동 재생을 포기함"), NUPrologue::GiveUpSeconds);
			bAutoPlayResolved = true;
			CancelAutoPlay();
		}
		return;
	}

	// 덮개 아래라 화면에는 안 보이지만, 로딩 직후 도착 위치에 자리를 잡을 틈을 잠깐 준다.
	ReadySeconds += DeltaTime;
	if (ReadySeconds < Settings->StartDelaySeconds)
	{
		return;
	}

	bAutoPlayResolved = true;
	if (!PlayPrologue())
	{
		PendingCharacterKey.Reset();
		CancelAutoPlay();
	}
}

UNUPrologueSubsystem::ECandidate UNUPrologueSubsystem::CheckCandidate(ANUPlayerController*& OutController, FString& OutCharacterKey)
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	if (GameInstance == nullptr)
	{
		return ECandidate::NotYet;
	}

	ANUPlayerController* Controller = Cast<ANUPlayerController>(World->GetFirstPlayerController());
	if (Controller == nullptr || !Controller->IsLocalController())
	{
		return ECandidate::NotYet;
	}

	// 캐릭터 선택을 거쳐 들어온 경우만 자동 재생한다. 맵을 직접 연 PIE는 nu.Prologue.Play 로 본다.
	const UNUCharacterManager* Characters = GameInstance->GetSubsystem<UNUCharacterManager>();
	FNUCharacterSummary Selected;
	if (Characters == nullptr || !Characters->GetSelectedCharacter(Selected))
	{
		return ECandidate::NotYet;
	}
	if (Selected.Level > GetDefault<UNUPrologueSettings>()->MaxCharacterLevel)
	{
		return ECandidate::No;
	}

	const UEmberServerManager* Server = GameInstance->GetSubsystem<UEmberServerManager>();
	const bool bOnline = Server != nullptr && Server->IsLoggedIn();
	const FString CharacterKey = FString::Printf(TEXT("%s:%lld"), bOnline ? TEXT("online") : TEXT("offline"), Selected.CharacterIdx);
	if (HasSeen(CharacterKey))
	{
		return ECandidate::No;
	}

	OutController = Controller;
	OutCharacterKey = CharacterKey;
	return ECandidate::Yes;
}

bool UNUPrologueSubsystem::IsReadyToStart(const ANUPlayerController* Controller) const
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	if (GameInstance == nullptr || Controller == nullptr || !Cast<AFieldPlayerCharacter>(Controller->GetPawn()))
	{
		return false;
	}

	const UNUFieldLoadingSubsystem* Loading = GameInstance->GetSubsystem<UNUFieldLoadingSubsystem>();
	if (Loading != nullptr && Loading->IsLoadingField())
	{
		return false;
	}

	// 서버에 접속해 있으면 필드 입장이 확정된 뒤(도착 위치가 정해진 뒤)에 시작한다.
	const UEmberServerManager* Server = GameInstance->GetSubsystem<UEmberServerManager>();
	if (Server != nullptr && Server->IsLoggedIn())
	{
		const UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>();
		if (Sync == nullptr || !Sync->HasConfirmedFieldEntry())
		{
			return false;
		}
	}

	// 그림·시퀀스·음악을 다 읽어야 첫 장면이 멈칫하지 않는다. 너무 오래 걸리면 그냥 시작한다.
	return IsPreloaded() || WaitAfterLoadingSeconds > NUPrologue::PreloadWaitSeconds;
}

void UNUPrologueSubsystem::PrepareAutoPlay(ANUPlayerController* Controller)
{
	bPrepared = true;
	PreparedController = Controller;
	WaitAfterLoadingSeconds = 0.0f;
	ShowCover();
	// 로딩 화면이 걷히고 프롤로그가 시작되기 전까지 움직이거나 HUD가 비치지 않게 미리 막는다.
	Controller->RequestGameInputBlock(NUPrologue::Reason, true, this);
	if (UNUUIManager* UIManager = UNUUIManager::Get(Controller))
	{
		UIManager->RequestFieldHUDHidden(NUPrologue::Reason, true, this, false);
	}
	StartPreload();
	UE_LOG(LogTemp, Log, TEXT("[Prologue] 자동 재생 대상 (%s) - 로딩 화면 아래를 검게 덮고 에셋을 미리 읽음"), *PendingCharacterKey);
}

void UNUPrologueSubsystem::CancelAutoPlay()
{
	if (!bPrepared)
	{
		return;
	}
	bPrepared = false;
	DataHandle.Reset();
	SequenceHandle.Reset();
	PopEarlyMute();
	HideCover();
	CoverHideCountdown = -1.0f;
	if (ANUPlayerController* Controller = PreparedController.Get())
	{
		Controller->RequestGameInputBlock(NUPrologue::Reason, false, this);
		if (UNUUIManager* UIManager = UNUUIManager::Get(Controller))
		{
			UIManager->ReleaseFieldHUDHideRequests(this, false);
		}
	}
	PreparedController.Reset();
}

void UNUPrologueSubsystem::StartPreload()
{
	const TSoftObjectPtr<UNUStorySlideshowData>& DataPath = GetDefault<UNUPrologueSettings>()->PrologueData;
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	DataHandle = Streamable.RequestAsyncLoad(DataPath.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this]()
	{
		UNUStorySlideshowData* Data = GetDefault<UNUPrologueSettings>()->PrologueData.Get();
		if (Data == nullptr || !bPrepared)
		{
			return;
		}
		// 레벨 BGM 은 로딩 화면 뒤에서부터 꺼 둔다(시작할 때 끄면 마을 곡이 잠깐 들렸다 끊긴다).
		PushEarlyMute(Data->SoundMix);
		if (!Data->Sequence.IsNull())
		{
			SequenceHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Data->Sequence.ToSoftObjectPath());
		}
	}));
}

bool UNUPrologueSubsystem::IsPreloaded() const
{
	const UNUStorySlideshowData* Data = GetDefault<UNUPrologueSettings>()->PrologueData.Get();
	return Data != nullptr && (Data->Sequence.IsNull() || Data->Sequence.Get() != nullptr);
}

void UNUPrologueSubsystem::PushEarlyMute(USoundMix* Mix)
{
	if (Mix == nullptr || EarlyMuteMix != nullptr)
	{
		return;
	}
	EarlyMuteMix = Mix;
	UGameplayStatics::PushSoundMixModifier(this, EarlyMuteMix);
}

void UNUPrologueSubsystem::PopEarlyMute()
{
	if (EarlyMuteMix != nullptr)
	{
		UGameplayStatics::PopSoundMixModifier(this, EarlyMuteMix);
		EarlyMuteMix = nullptr;
	}
}

void UNUPrologueSubsystem::ShowCover()
{
	UGameViewportClient* Viewport = GetWorld() != nullptr ? GetWorld()->GetGameViewport() : nullptr;
	if (Viewport == nullptr || Cover.IsValid())
	{
		return;
	}
	Cover = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("BlackBrush"))
		.Visibility(EVisibility::HitTestInvisible);
	Viewport->AddViewportWidgetContent(Cover.ToSharedRef(), NUPrologue::CoverZOrder);
	CoverViewport = Viewport;
}

void UNUPrologueSubsystem::HideCover()
{
	CoverHideCountdown = -1.0f;
	if (Cover.IsValid() && CoverViewport.IsValid())
	{
		CoverViewport->RemoveViewportWidgetContent(Cover.ToSharedRef());
	}
	Cover.Reset();
	CoverViewport.Reset();
}

bool UNUPrologueSubsystem::PlayPrologue()
{
	UWorld* World = GetWorld();
	if (World == nullptr || Cue != nullptr)
	{
		return false;
	}

	UNUStorySlideshowData* Data = GetDefault<UNUPrologueSettings>()->PrologueData.LoadSynchronous();
	ULevelSequence* Sequence = Data != nullptr ? Data->Sequence.LoadSynchronous() : nullptr;
	ANUPlayerController* Controller = Cast<ANUPlayerController>(World->GetFirstPlayerController());
	if (Data == nullptr || Sequence == nullptr || Controller == nullptr || !Controller->IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Prologue] 재생할 수 없음: 데이터=%d 시퀀스=%d 컨트롤러=%d"), Data != nullptr, Sequence != nullptr, Controller != nullptr);
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	ANUStorySequenceCue* NewCue = World->SpawnActor<ANUStorySequenceCue>(SpawnParameters);
	if (NewCue == nullptr)
	{
		return false;
	}
	NewCue->ViewportZOrder = NUUILayer::Transition;
	if (!NewCue->PlayStory(Controller, Data, Sequence))
	{
		NewCue->Destroy();
		UE_LOG(LogTemp, Warning, TEXT("[Prologue] PlayStory 실패: %s"), *Sequence->GetPathName());
		return false;
	}

	Cue = NewCue;
	PlayingController = Controller;
	PlayingCharacterKey = PendingCharacterKey;
	PendingCharacterKey.Reset();
	Cue->OnFinished.AddDynamic(this, &ThisClass::HandlePrologueFinished);

	// 자동 재생 준비를 재생으로 넘긴다: 시퀀스가 같은 BGM 믹스를 걸었으니 미리 건 것은 내려놓고,
	// 프롤로그 화면(검은 화면으로 시작)이 한 번 그려진 뒤 덮개를 걷는다.
	bPrepared = false;
	PreparedController.Reset();
	DataHandle.Reset();
	SequenceHandle.Reset();
	PopEarlyMute();
	if (Cover.IsValid())
	{
		CoverHideCountdown = NUPrologue::CoverHideDelay;
	}

	// 재생하는 동안 필드 입력과 HUD를 막는다. 스킵 버튼을 누를 수 있게 커서는 연다.
	Controller->RequestGameInputBlock(NUPrologue::Reason, true, this);
	Controller->RequestCursor(NUPrologue::Reason, true, this);
	if (UNUUIManager* UIManager = UNUUIManager::Get(Controller))
	{
		UIManager->RequestFieldHUDHidden(NUPrologue::Reason, true, this, false);
	}
	UE_LOG(LogTemp, Log, TEXT("[Prologue] 재생 시작 (%s)"), PlayingCharacterKey.IsEmpty() ? TEXT("다시 보기") : *PlayingCharacterKey);
	return true;
}

void UNUPrologueSubsystem::HandlePrologueFinished(bool bCompleted)
{
	const bool bSkipped = IsValid(Cue) && Cue->bWasSkipped;
	if (IsValid(Cue))
	{
		Cue->OnFinished.RemoveDynamic(this, &ThisClass::HandlePrologueFinished);
		Cue->Destroy();
	}
	Cue = nullptr;

	// 끝까지 보거나 스킵했을 때만 본 것으로 친다. 맵 전환 같은 외부 중단은 다음 접속에 다시 나온다.
	const bool bFinishedByPlayer = bCompleted || bSkipped;
	if (bFinishedByPlayer && !PlayingCharacterKey.IsEmpty())
	{
		MarkSeen(PlayingCharacterKey);
	}
	UE_LOG(LogTemp, Log, TEXT("[Prologue] 종료 (끝까지=%d 스킵=%d)"), bCompleted, bSkipped);
	PlayingCharacterKey.Reset();
	ReleasePlayer(bFinishedByPlayer);
}

void UNUPrologueSubsystem::ReleasePlayer(bool bFadeIn)
{
	ANUPlayerController* Controller = PlayingController.Get();
	PlayingController.Reset();
	if (Controller == nullptr)
	{
		return;
	}

	Controller->RequestGameInputBlock(NUPrologue::Reason, false, this);
	Controller->RequestCursor(NUPrologue::Reason, false, this);
	if (UNUUIManager* UIManager = UNUUIManager::Get(Controller))
	{
		UIManager->ReleaseFieldHUDHideRequests(this, true);
	}
	// 그림책 위젯이 가져갔던 키보드·패드 포커스를 게임 화면으로 돌려준다.
	UWidgetBlueprintLibrary::SetFocusToGameViewport();

	// 프롤로그는 검은 화면으로 끝나므로 게임 화면은 검은 화면에서 밝아진다.
	const float FadeSeconds = GetDefault<UNUPrologueSettings>()->ReturnFadeSeconds;
	if (bFadeIn && FadeSeconds > 0.0f && Controller->PlayerCameraManager != nullptr)
	{
		Controller->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeSeconds, FLinearColor::Black, false, false);
	}
}

bool UNUPrologueSubsystem::HasSeen(const FString& CharacterKey)
{
	const UNUPrologueSaveGame* Records = NUPrologue::LoadRecords();
	return Records != nullptr && Records->SeenCharacterKeys.Contains(CharacterKey);
}

void UNUPrologueSubsystem::MarkSeen(const FString& CharacterKey)
{
	if (UNUPrologueSaveGame* Records = NUPrologue::LoadRecords())
	{
		Records->SeenCharacterKeys.AddUnique(CharacterKey);
		UGameplayStatics::SaveGameToSlot(Records, NUPrologue::SaveSlot, 0);
	}
}

void UNUPrologueSubsystem::ResetSeenRecords()
{
	UGameplayStatics::DeleteGameInSlot(NUPrologue::SaveSlot, 0);
}

static FAutoConsoleCommandWithWorld GNUProloguePlayCmd(
	TEXT("nu.Prologue.Play"),
	TEXT("게임 시작 프롤로그를 지금 재생한다(본 기록과 상관없음)."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (UNUPrologueSubsystem* Prologue = World != nullptr ? World->GetSubsystem<UNUPrologueSubsystem>() : nullptr)
		{
			Prologue->PlayPrologue();
		}
	}));

static FAutoConsoleCommand GNUPrologueResetCmd(
	TEXT("nu.Prologue.ResetSeen"),
	TEXT("이 PC의 프롤로그 본 기록을 지운다. 다음 새 캐릭터 진입 때 다시 자동 재생된다."),
	FConsoleCommandDelegate::CreateStatic(&UNUPrologueSubsystem::ResetSeenRecords));
