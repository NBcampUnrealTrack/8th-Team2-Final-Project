// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/NUBGMSubsystem.h"

#include "Audio/NUAudioSettings.h"
#include "Audio/NUBGMConfig.h"
#include "Audio/NUBGMZone.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"

#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

void UNUBGMSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UNUAudioSettings* Settings = GetDefault<UNUAudioSettings>();
	if (!IsValid(Settings))
	{
		return;
	}

	if (!Settings->BGMConfig.IsNull())
	{
		UNUBGMConfig* LoadedConfig = Settings->BGMConfig.LoadSynchronous();
		if (IsValid(LoadedConfig))
		{
			BGMConfig = LoadedConfig;

			UE_LOG(LogTemp, Log, TEXT("BGMSubsystem: 설정 로드=%s"), *BGMConfig->GetName());
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNUBattleNetSubsystem* BattleNet = GI->GetSubsystem<UNUBattleNetSubsystem>())
		{
			BattleNet->OnBattleStateChanged.AddUObject(this, &ThisClass::HandleNetBattleStateChanged);
		}
	}

	// FCoreUObjectDelegates 바인딩 - 레벨 전환 창을 가리는 용도
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);

	// 서브시스템 생성 시점에 이미 월드가 있을 수 있어 수동으로 한 번 맞춰준다
	BindNetStageSubsystem(GetWorld());
}

void UNUBGMSubsystem::Deinitialize()
{
	StopBossPolling();
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNUBattleNetSubsystem* BattleNet = GI->GetSubsystem<UNUBattleNetSubsystem>())
		{
			BattleNet->OnBattleStateChanged.RemoveAll(this);
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UNUBattleNetStageSubsystem* StageNet = World->GetSubsystem<UNUBattleNetStageSubsystem>())
		{
			StageNet->OnNetStageReadyChanged.RemoveAll(this);
		}
	}

	StopBGM(0.f);
	BGMConfig = nullptr;
	RegisteredZones.Reset();

	Super::Deinitialize();
}

void UNUBGMSubsystem::RegisterZone(ANUBGMZone* Zone)
{
	if (!IsValid(Zone))
	{
		return;
	}

	switch (Zone->GetScope())
	{
	case ENUBGMZoneScope::WholeLevel:
		SetLayer(ENUBGMLayer::Level, Zone->GetBGMId());
		return;

	case ENUBGMZoneScope::BattleStage:
		// 세트가 유지되는 동안은 다시 등록되지 않으므로 여기서 RecomputeBattleLayer를 부를 필요가 없음
		// Battle 레이어가 다음에 켜질 때 이 값을 읽어감
		CurrentBattleStageBGMId = Zone->GetBGMId();
		return;

	case ENUBGMZoneScope::Area:
		RegisteredZones.AddUnique(Zone);
		RefreshAreaLayer();
		return;
	}
}

void UNUBGMSubsystem::UnregisterZone(ANUBGMZone* Zone)
{
	if (!IsValid(Zone))
	{
		return;
	}

	switch (Zone->GetScope())
	{
	case ENUBGMZoneScope::WholeLevel:
		SetLayer(ENUBGMLayer::Level, NAME_None);
		return;

	case ENUBGMZoneScope::BattleStage:
		// 전투 세트가 교체될 때만 호출됨, 해당 시점은 항상 전투가 끝난 뒤라 화면에 영향 없음
		CurrentBattleStageBGMId = NAME_None;
		return;

	case ENUBGMZoneScope::Area:
		RegisteredZones.RemoveSingleSwap(Zone);
		RefreshAreaLayer();
		return;
	}
}

void UNUBGMSubsystem::RefreshAreaLayer()
{
	FName SelectedBGMId = NAME_None;
	int32 SelectedPriority = 0;

	for (auto It = RegisteredZones.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
			continue;
		}

		ANUBGMZone* Zone = It->Get();
		if (!Zone->IsLocalPawnInside())
		{
			continue;
		}

		// 우선순위가 같다면 나중에 등록된 쪽을 선택함
		if (SelectedBGMId.IsNone() || Zone->GetPriority() >= SelectedPriority)
		{
			SelectedBGMId = Zone->GetBGMId();
			SelectedPriority = Zone->GetPriority();
		}
	}

	SetLayer(ENUBGMLayer::Area, SelectedBGMId);
}

void UNUBGMSubsystem::SetLocalBattleActive(bool bActive)
{
	if (bLocalBattleActive == bActive)
	{
		return;
	}

	bLocalBattleActive = bActive;
	RecomputeBattleLayer();
}

void UNUBGMSubsystem::SetLayer(ENUBGMLayer Layer, FName BGMId)
{
	LayerRequests[static_cast<uint8>(Layer)] = BGMId;
	Resolve();
}

void UNUBGMSubsystem::Resolve()
{
	if (bSuspendResolve)
	{
		return;
	}

	for (int32 LayerIndex = static_cast<int32>(ENUBGMLayer::Count) - 1; LayerIndex >= 0; --LayerIndex)
	{
		if (!LayerRequests[LayerIndex].IsNone())
		{
			ApplyResolved(LayerRequests[LayerIndex]);
			return;
		}
	}

	const UNUAudioSettings* Settings = GetDefault<UNUAudioSettings>();
	ApplyResolved(Settings ? Settings->FallbackBGMId : NAME_None);
}

const FNUBGMEntry* UNUBGMSubsystem::FindEntry(FName BGMId) const
{
	return IsValid(BGMConfig) ? BGMConfig->FindEntry(BGMId) : nullptr;
}

void UNUBGMSubsystem::ApplyResolved(FName NewBGMId)
{
	// 곡이 실제로 바뀐 경우에만 크로스페이드
	if (NewBGMId == CurrentBGMId)
	{
		return;
	}

	const FNUBGMEntry* NewEntry = FindEntry(NewBGMId);
	if (NewEntry == nullptr || !IsValid(NewEntry->Sound))
	{
		if (!NewBGMId.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("BGMSubsystem: BGMId=%s 항목을 찾을 수 없음, 무음으로 전환"), *NewBGMId.ToString());
		}

		StopBGM(1.f);
		CurrentBGMId = NewBGMId;
		return;
	}

	// 서로 다른 두 BGMId가 같은 곡을 가리켜도 재생 위치를 유지함
	if (IsValid(AudioComponent) && AudioComponent->IsPlaying() && AudioComponent->GetSound() == NewEntry->Sound)
	{
		CurrentBGMId = NewBGMId;
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (IsValid(AudioComponent))
	{
		float PrevFadeOutSeconds = 1.f;
		if (const FNUBGMEntry* PrevEntry = FindEntry(CurrentBGMId))
		{
			PrevFadeOutSeconds = PrevEntry->FadeOutSeconds;
		}

		AudioComponent->FadeOut(PrevFadeOutSeconds, 0.f);
	}

	AudioComponent = UGameplayStatics::CreateSound2D(
		World,
		NewEntry->Sound,
		NewEntry->Volume,
		1.f,
		0.f,
		nullptr,
		true,
		true
		);

	if (IsValid(AudioComponent))
	{
		AudioComponent->FadeIn(NewEntry->FadeInSeconds);
	}

	CurrentBGMId = NewBGMId;
}

void UNUBGMSubsystem::StopBGM(float FadeOutSeconds)
{
	if (IsValid(AudioComponent))
	{
		AudioComponent->FadeOut(FadeOutSeconds, 0.f);
	}

	AudioComponent = nullptr;
}

void UNUBGMSubsystem::RecomputeBattleLayer()
{
	UGameInstance* GI = GetGameInstance();
	const UNUBattleNetSubsystem* BattleNet = IsValid(GI) ? GI->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;

	UWorld* World = GetWorld();
	const UNUBattleNetStageSubsystem* StageNet = IsValid(World) ? World->GetSubsystem<UNUBattleNetStageSubsystem>() : nullptr;

	// 처치 연출이 끝나기 전에 필드 HUD가 먼저 튀어나오던 버그를 고친 것과 같은 식
	// (UNU_BaseInGameWidget의 RecomputeNetBattleActive) - BGM도 같은 이유로 똑같이 처리함
	const bool bNewBattleActive = bLocalBattleActive
		|| (IsValid(BattleNet) && BattleNet->IsInBattle())
		|| (IsValid(StageNet) && StageNet->IsNetStageReady());

	if (bNewBattleActive == bBattleActive)
	{
		return;
	}

	bBattleActive = bNewBattleActive;

	if (bBattleActive)
	{
		// 적 아바타가 생긴 뒤에 한 번만 곡을 고른다 - 먼저 일반곡을 틀었다가 보스곡으로 바꾸면 앞머리가 잠깐 섞여 들린다
		StopBossPolling();
		BossPollElapsed = 0.f;
		bBattleLayerChosen = false;
		bool bBoss = false;
		if (FindEnemyAvatars(bBoss))
		{
			ChooseBattleLayer(bBoss);
		}
		else
		{
			BossPollStartFrame = GFrameCounter;
			BossPollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::PollBossBattle), 0.f);
		}
		return;
	}

	StopBossPolling();
	bBossBattle = false;
	bBattleLayerChosen = false;
	SetLayer(ENUBGMLayer::Battle, NAME_None);

	// 넷/로컬 둘 다 전투 중 필드 폰의 콜리전을 껐다 켜서 구역 오버랩 이벤트가 흐트러질 수 있다
	RefreshAreaLayer();
}

FName UNUBGMSubsystem::ResolveBattleBGMId(bool bBoss) const
{
	const UNUAudioSettings* Settings = GetDefault<UNUAudioSettings>();

	// 보스전은 어느 전투 세트에서 싸우든 보스곡이 우선
	if (bBoss && Settings != nullptr && !Settings->BossBattleBGMId.IsNone() && FindEntry(Settings->BossBattleBGMId) != nullptr)
	{
		return Settings->BossBattleBGMId;
	}

	// 지금 로드된 전투 세트가 자기 BGM을 지정했으면 그걸 우선시 함 -
	// 지역마다 전투 세트가 다르면 이걸로 지역별 전투곡이 코드 변경 없이 달라짐
	if (!CurrentBattleStageBGMId.IsNone())
	{
		return CurrentBattleStageBGMId;
	}

	return Settings != nullptr ? Settings->BattleBGMId : NAME_None;
}

bool UNUBGMSubsystem::FindEnemyAvatars(bool& bOutBoss) const
{
	bOutBoss = false;
	UGameInstance* GI = GetGameInstance();
	UWorld* World = IsValid(GI) ? GI->GetWorld() : nullptr;
	const UNUNaiaDataSubsystem* NaiaData = IsValid(GI) ? GI->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	if (World == nullptr)
	{
		return false;
	}

	bool bFound = false;
	for (TActorIterator<ANUBattleAvatar> It(World); It; ++It)
	{
		if (It->IsAllyCombatant() || It->GetNaiaId() <= 0)
		{
			continue;
		}
		bFound = true;
		const FNUNaiaDefinition* Definition = NaiaData != nullptr ? NaiaData->FindDefinitionById(It->GetNaiaId()) : nullptr;
		if (Definition != nullptr && Definition->Category == ENUNaiaCategory::Boss)
		{
			bOutBoss = true;
			return true;
		}
	}
	return bFound;
}

void UNUBGMSubsystem::ChooseBattleLayer(bool bBoss)
{
	bBossBattle = bBoss;
	bBattleLayerChosen = true;
	const FName BGMId = ResolveBattleBGMId(bBoss);
	UE_LOG(LogTemp, Log, TEXT("BGMSubsystem: 전투 BGM=%s (보스전=%d, 대기 %.2f초)"), *BGMId.ToString(), bBoss ? 1 : 0, BossPollElapsed);
	SetLayer(ENUBGMLayer::Battle, BGMId);
}

bool UNUBGMSubsystem::PollBossBattle(float DeltaSeconds)
{
	BossPollElapsed += DeltaSeconds;
	if (!bBattleActive || bBossBattle)
	{
		BossPollHandle.Reset();
		return false;
	}

	bool bBoss = false;
	if (FindEnemyAvatars(bBoss))
	{
		if (!bBattleLayerChosen || bBoss)
		{
			ChooseBattleLayer(bBoss);
		}
		BossPollHandle.Reset();
		return false;
	}

	// 적이 늦게 뜨면(넷 무대 로딩 등) 일반곡을 먼저 틀고, 보스가 나오면 그때 바꾼다
	// 시간과 프레임을 같이 본다 - 프레임이 느린 환경(에디터 백그라운드 등)에서 적이 생기기 전에 시간만 지나 버리는 것을 막음
	const uint64 Frames = GFrameCounter - BossPollStartFrame;
	if (!bBattleLayerChosen && BossPollElapsed > 1.5f && Frames > 20)
	{
		ChooseBattleLayer(false);
	}
	if (BossPollElapsed > 10.f && Frames > 120)
	{
		BossPollHandle.Reset();
		return false;
	}
	return true;
}

void UNUBGMSubsystem::StopBossPolling()
{
	if (BossPollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(BossPollHandle);
		BossPollHandle.Reset();
	}
}

void UNUBGMSubsystem::HandleNetBattleStateChanged()
{
	RecomputeBattleLayer();
}

void UNUBGMSubsystem::HandleNetStageReadyChanged(bool bReady)
{
	RecomputeBattleLayer();
}

void UNUBGMSubsystem::BindNetStageSubsystem(UWorld* World)
{
	UNUBattleNetStageSubsystem* StageNet = IsValid(World) ? World->GetSubsystem<UNUBattleNetStageSubsystem>() : nullptr;
	if (!IsValid(StageNet))
	{
		return;
	}

	StageNet->OnNetStageReadyChanged.RemoveAll(this);
	StageNet->OnNetStageReadyChanged.AddUObject(this, &ThisClass::HandleNetStageReadyChanged);
}

void UNUBGMSubsystem::HandlePreLoadMap(const FString& MapName)
{
	bSuspendResolve = true;
}

void UNUBGMSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 로드 실패 시 RAII 경로가 nullptr로도 브로드캐스트한다 - 다음 정상 로드까지 보류 상태로 둠
	if (!IsValid(LoadedWorld))
	{
		return;
	}

	RegisteredZones.RemoveAll([](const TWeakObjectPtr<ANUBGMZone>& Zone) { return !Zone.IsValid(); });

	BindNetStageSubsystem(LoadedWorld);

	bSuspendResolve = false;

	// 존이 이미 BeginPlay로 등록을 마친 시점 - 스폰 지점이 박스 안이었던 경우까지 커버하려고
	// 오버랩 이벤트를 기다리지 않고 여기서 한 번 직접 다시 뽑음
	RefreshAreaLayer();
	Resolve();
}
