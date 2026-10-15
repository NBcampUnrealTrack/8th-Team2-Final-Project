// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleNetStageSubsystem.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Avatar/NUBattlePlayerAvatar.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "Battle/Encounter/NUBattleEncounterSubsystem.h"
#include "Battle/Formation/NUBattleFormationTypes.h"
#include "Battle/Net/NUBattleNetActionQueueSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Battle/Presentation/NUBattleActionPresentation.h"
#include "Camera/CameraComponent.h"
#include "Field/NUFieldTypes.h"
#include "NUGAS/Ability/NUBattleGameplayAbility.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "PlayerController/NUPlayerController.h"
#include "Recovery/Net/NURecoveryNetSubsystem.h"
#include "Recovery/Component/NURecoveryComponent.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUBattleNetStage, Log, All);


void UNUBattleNetStageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		BattleNet->OnCombatantsReplaced.AddUObject(this, &ThisClass::ReconcileAvatars);
		BattleNet->OnBattleStateChanged.AddUObject(this, &ThisClass::HandleBattleStateChanged);
	}
}

void UNUBattleNetStageSubsystem::Deinitialize()
{
	if (UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		BattleNet->OnCombatantsReplaced.RemoveAll(this);
		BattleNet->OnBattleStateChanged.RemoveAll(this);
	}

	TeardownStage();

	Super::Deinitialize();
}

void UNUBattleNetStageSubsystem::SetPendingStageLevel(const TSoftObjectPtr<UWorld>& StageLevel)
{
	PendingStageLevel = StageLevel;
}

ANUBattleAvatar* UNUBattleNetStageSubsystem::FindAvatar(int32 CombatantId) const
{
	const FNUNetStageAvatar* Found = Avatars.Find(CombatantId);
	return Found ? Found->Avatar.Get() : nullptr;
}

bool UNUBattleNetStageSubsystem::IsNetStageReady() const
{
	return bNetStageReady;
}

void UNUBattleNetStageSubsystem::SetVitalsAutoRefreshSuppressed(bool bSuppressed)
{
	bVitalsAutoRefreshSuppressed = bSuppressed;
}

void UNUBattleNetStageSubsystem::NotifyActionPlaybackDrained()
{
	if (!bTeardownDeferred)
	{
		return;
	}

	bTeardownDeferred = false;

	BeginTeardownStage();
}

bool UNUBattleNetStageSubsystem::IsActionPlaybackBusy() const
{
	UWorld* World = GetWorld();
	const UNUBattleNetActionQueueSubsystem* ActionQueue = IsValid(World)
		? World->GetSubsystem<UNUBattleNetActionQueueSubsystem>()
		: nullptr;

	return IsValid(ActionQueue) && ActionQueue->IsPlaybackBusy();
}

UNUBattleNetSubsystem* UNUBattleNetStageSubsystem::GetBattleNet() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
}

void UNUBattleNetStageSubsystem::HandleBattleStateChanged()
{
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	const bool bIsInBattle = IsValid(BattleNet) && BattleNet->IsInBattle();
	
	if (bIsInBattle && !bWasInBattle)
	{
		// 전투에 방금 진입한 경우
		BeginSpawningStage();
	}
	else if (!bIsInBattle && bWasInBattle)
	{
		// 전투가 종료된 경우
		// 서버가 처치 행동과 종료를 거의 같이 보내서, 여기서 바로 정리하면 마지막 타격 연출이 잘림
		// 큐가 재생 중이면 다 끝난 뒤 NotifyActionPlaybackDrained로 되돌아옴
		if (IsActionPlaybackBusy())
		{
			bTeardownDeferred = true;
		}
		else
		{
			BeginTeardownStage();
		}
	}
	else if (bIsInBattle)
	{
		if (!bVitalsAutoRefreshSuppressed)
		{
			// 새 스냅샷만 반영, 턴마다 여러 번 오는 상태 변화를 처리함
			RefreshVitals();
		}
	}

	bWasInBattle = bIsInBattle;
}

void UNUBattleNetStageSubsystem::BeginSpawningStage()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	PlayFadeOut();

	if (UNUBattleEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UNUBattleEncounterSubsystem>())
	{
		if (!PendingStageLevel.IsNull())
		{
			// 로컬 전투와 같은 아레나·같은 서브레벨 로딩 처리를 그대로 씀, 비동기 요청
			EncounterSubsystem->EnsureStageLoaded(PendingStageLevel);
		}
	}

	// 페이드아웃 재생 시간만큼은 무조건 화면이 검게 유지됨, 그 사이 배경도 같이 스트리밍됨
	World->GetTimerManager().SetTimer(
		StageWaitTimerHandle, 
		this, 
		&ThisClass::HandleFadeOutFinished,
		GetFadeOutSeconds(),
		false
		);
}

void UNUBattleNetStageSubsystem::HandleFadeOutFinished()
{
	// 화면이 완전히 검게 된 시점에 필드 캐릭터 숨기기
	SetLocalFieldPawnHidden(true);

	UWorld* World = GetWorld();
	UNUBattleEncounterSubsystem* EncounterSubsystem = IsValid(World)
		? World->GetSubsystem<UNUBattleEncounterSubsystem>()
		: nullptr;

	// 암전이 끝나도 로딩이 안끝난 경우
	if (IsValid(EncounterSubsystem) && !EncounterSubsystem->IsStageReady())
	{
		// 폴링 대신 레벨 스트리밍의 OnLevelShown(=OnStageShown)을 기다림
		EncounterSubsystem->OnStageShown.AddUObject(this, &ThisClass::HandleStageShown);

		// 에셋 경로가 잘못되면 이벤트가 영영 안 올 수 있어 최대 대기 시간을 안전망으로 둠
		// 로컬 ANUBattleEncounter::MaxStageWaitSeconds와 같은 값
		constexpr float MaxStageWaitSeconds = 5.f;
		World->GetTimerManager().SetTimer(
			StageWaitTimerHandle, 
			this, 
			&ThisClass::HandleStageWaitTimeout,
			MaxStageWaitSeconds, 
			false
			);
		return;
	}

	FinishWaitingForStage();
}

void UNUBattleNetStageSubsystem::HandleStageShown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StageWaitTimerHandle);

		if (UNUBattleEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UNUBattleEncounterSubsystem>())
		{
			EncounterSubsystem->OnStageShown.RemoveAll(this);
		}
	}

	FinishWaitingForStage();
}

// 최대 대기 시간을 넘어 그냥 진행하는 경우
// 클라 멈춤 or 잘못된 상황
void UNUBattleNetStageSubsystem::HandleStageWaitTimeout()
{
	if (UWorld* World = GetWorld())
	{
		if (UNUBattleEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UNUBattleEncounterSubsystem>())
		{
			EncounterSubsystem->OnStageShown.RemoveAll(this);
		}
	}

	UE_LOG(LogNUBattleNetStage, Warning, TEXT("전투 배경이 올라오지 않아 배경 없이 시작함"));

	FinishWaitingForStage();
}

void UNUBattleNetStageSubsystem::FinishWaitingForStage()
{
	// 로딩 콜백/안전망 타이머가 전투 종료 직후 늦게 도착할 수 있음
	// 이때 새 전투 세트를 다시 만들면 종료 중인 전투가 부활할 수 있음
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (!IsValid(BattleNet) || !BattleNet->IsInBattle())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UNUBattleEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UNUBattleEncounterSubsystem>())
		{
			EncounterSubsystem->ApplyStageLighting();
		}
	}

	SpawnStage();
	PlayFadeIn();
	UNUSFXSubsystem::Play(GetWorld(), NU::SFX::Battle::Ready);
	SetNetStageReady(true);
}

void UNUBattleNetStageSubsystem::SetNetStageReady(bool bReady)
{
	if (bNetStageReady == bReady)
	{
		return;
	}

	bNetStageReady = bReady;
	OnNetStageReadyChanged.Broadcast(bNetStageReady);
}

void UNUBattleNetStageSubsystem::SpawnStage()
{
	UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	UWorld* World = GetWorld();
	if (!IsValid(BattleNet) || !IsValid(World))
	{
		return;
	}

	TArray<int32> CombatantIds;
	BattleNet->GetCombatants().GenerateKeyArray(CombatantIds);
	CombatantIds.Sort();

	for (int32 CombatantId : CombatantIds)
	{
		const FNUServerCombatant* Combatant = BattleNet->FindCombatant(CombatantId);
		if (!Combatant)
		{
			continue;
		}

		int32 SlotIndex = 0;
		int32 SlotCount = 1;
		ResolveFormationSlot(CombatantId, SlotIndex, SlotCount);

		if (ANUBattleAvatar* Avatar = SpawnAvatarFor(*Combatant, SlotIndex, SlotCount))
		{
			RegisterAvatar(Avatar, *Combatant);
		}
	}

	EnterBattleCamera();

	UE_LOG(LogNUBattleNetStage, Log, TEXT("넷 전투 세트 생성 Avatars=%d"), Avatars.Num());

	RefreshVitals();
}

void UNUBattleNetStageSubsystem::ReconcileAvatars()
{
	// 전투 세트가 준비가 안됐다면 전투 시작 스냅샷임, SpawnStage가 처음부터 제대로 만듦
	if (!bNetStageReady)
	{
		return;
	}

	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (!IsValid(BattleNet))
	{
		return;
	}

	const TMap<int32, FNUServerCombatant>& Combatants = BattleNet->GetCombatants();

	// 명단에서 빠진 아바타 (교체로 물러난 나이아)
	TArray<int32> RetiredIds;
	for (const TPair<int32, FNUNetStageAvatar>& Pair : Avatars)
	{
		if (!Combatants.Contains(Pair.Key))
		{
			RetiredIds.Add(Pair.Key);
		}
	}

	// 아바타가 없는 전투원 (교체로 들어온 나이아)
	TArray<int32> ArrivedIds;
	for (const TPair<int32, FNUServerCombatant>& Pair : Combatants)
	{
		if (!Avatars.Contains(Pair.Key))
		{
			ArrivedIds.Add(Pair.Key);
		}
	}

	// 번호가 그대로면 손댈 것이 없음, 값 반영은 재생 큐가 이벤트 순서대로 함
	if (RetiredIds.IsEmpty() && ArrivedIds.IsEmpty())
	{
		return;
	}

	ArrivedIds.Sort();

	TArray<int32> UnmatchedArrivedIds;
	for (int32 ArrivedId : ArrivedIds)
	{
		const FNUServerCombatant* Incoming = Combatants.Find(ArrivedId);
		if (!Incoming)
		{
			continue;
		}

		// 같은 자리(종류·소유 캐릭터)에서 물러난 아바타가 있으면 그것을 그대로 씀
		// 아바타 클래스는 Kind로 갈리는데 같은 Kind끼리만 짝지으므로 클래스도 그대로 맞음
		const int32 MatchIndex = RetiredIds.IndexOfByPredicate(
			[this, Incoming](int32 RetiredId)
			{
				const FNUNetStageAvatar* Retired = Avatars.Find(RetiredId);
				return Retired
					&& Retired->Kind == Incoming->Kind
					&& Retired->OwnerCharacterIdx == Incoming->OwnerCharacterIdx;
			});

		if (MatchIndex == INDEX_NONE)
		{
			UnmatchedArrivedIds.Add(ArrivedId);
			continue;
		}

		const int32 RetiredId = RetiredIds[MatchIndex];
		RetiredIds.RemoveAt(MatchIndex);

		FNUNetStageAvatar Entry;
		Avatars.RemoveAndCopyValue(RetiredId, Entry);

		ANUBattleAvatar* Avatar = Entry.Avatar;
		if (!IsValid(Avatar))
		{
			UnmatchedArrivedIds.Add(ArrivedId);
			continue;
		}

		// 로컬 교체(ANUBattleEncounter::ApplyNaiaSwap)와 같은 방식으로 액터를 두고 그림만 바꿈
		// InitializeNetAvatar 안의 ApplyCombatantVisual이 애님 BP를 갈아끼우고 방향을 다시 넣음
		const bool bIsPlayerVisual = Incoming->Kind == 0;
		const EFieldFacing Facing = ANUBattleAvatar::ResolveBattleFacing(Incoming->Side != 1, bIsPlayerVisual);

		Avatar->InitializeNetAvatar(ArrivedId, Incoming->NaiaId, bIsPlayerVisual, Facing);
		Avatar->SetIsAlly(Incoming->Side != 1);

		// 물러난 쪽이 남긴 연출 상태(전진 좌표·피격 플래시)를 지우고 제자리로 되돌림
		// HomeTransform은 스폰할 때 잡은 값 그대로라 진형 좌표를 다시 풀 필요가 없음
		Avatar->ResetPresentationState();
		Avatar->SetVitals(Incoming->Health, Incoming->MaxHealth, Incoming->SP, Incoming->MaxSP);
		Avatar->SetCombatantIdentity(FText::FromString(Incoming->Name), Incoming->Level);
		PushStatusViews(Avatar, Incoming->Statuses);
		PushJobResourceViews(Avatar, Incoming->Resources, Incoming->JobStates);
		Avatar->SetDeadVisual(!Incoming->IsAlive());

		Entry.Kind = Incoming->Kind;
		Entry.OwnerCharacterIdx = Incoming->OwnerCharacterIdx;
		Avatars.Add(ArrivedId, MoveTemp(Entry));

		UE_LOG(LogNUBattleNetStage, Log,
			TEXT("전투원 교체 반영 %d -> %d (NaiaId=%d), 아바타 재사용"),
			RetiredId, ArrivedId, Incoming->NaiaId);

		UNUSFXSubsystem::Play(GetWorld(), NU::SFX::Battle::NaiaSwap);
	}

	// 짝이 없는 것만 실제로 스폰·파괴함, 전투원 수 자체가 달라진 경우의 안전망
	for (int32 ArrivedId : UnmatchedArrivedIds)
	{
		const FNUServerCombatant* Incoming = Combatants.Find(ArrivedId);
		if (!Incoming)
		{
			continue;
		}

		int32 SlotIndex = 0;
		int32 SlotCount = 1;
		ResolveFormationSlot(ArrivedId, SlotIndex, SlotCount);

		if (ANUBattleAvatar* Avatar = SpawnAvatarFor(*Incoming, SlotIndex, SlotCount))
		{
			RegisterAvatar(Avatar, *Incoming);
			UE_LOG(LogNUBattleNetStage, Log, TEXT("전투원 %d 아바타 새로 스폰"), ArrivedId);
		}
	}

	for (int32 RetiredId : RetiredIds)
	{
		FNUNetStageAvatar Entry;
		if (!Avatars.RemoveAndCopyValue(RetiredId, Entry))
		{
			continue;
		}

		if (IsValid(Entry.Avatar))
		{
			Entry.Avatar->Destroy();
		}

		UE_LOG(LogNUBattleNetStage, Log, TEXT("전투원 %d 아바타 제거"), RetiredId);
	}
}

void UNUBattleNetStageSubsystem::ResolveFormationSlot(int32 CombatantId, int32& OutSlotIndex, int32& OutSlotCount) const
{
	OutSlotIndex = 0;
	OutSlotCount = 1;

	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (!IsValid(BattleNet))
	{
		return;
	}

	// 같은 진형 행끼리 번호 순으로 늘어섬, 적은 한 행이고 아군은 캐릭터 행·나이아 행으로 갈림
	// 파티 전투에서는 캐릭터 행에 여러 명이 서므로 줄 안의 순서가 필요함
	const FNUServerCombatant* Target = BattleNet->FindCombatant(CombatantId);
	if (!Target)
	{
		return;
	}

	TArray<int32> RowIds;
	for (const TPair<int32, FNUServerCombatant>& Pair : BattleNet->GetCombatants())
	{
		const FNUServerCombatant& Other = Pair.Value;
		if (Other.Side != Target->Side)
		{
			continue;
		}

		// 아군은 나이아(Kind 1)와 나머지(캐릭터)로 행이 갈림
		if (Target->Side != 1 && (Other.Kind == 1) != (Target->Kind == 1))
		{
			continue;
		}

		RowIds.Add(Pair.Key);
	}
	RowIds.Sort();

	OutSlotIndex = FMath::Max(0, RowIds.IndexOfByKey(CombatantId));
	OutSlotCount = FMath::Max(1, RowIds.Num());
}

void UNUBattleNetStageSubsystem::RegisterAvatar(ANUBattleAvatar* Avatar, const FNUServerCombatant& Combatant)
{
	if (!IsValid(Avatar))
	{
		return;
	}

	FNUNetStageAvatar Entry;
	Entry.Avatar = Avatar;
	Entry.Kind = Combatant.Kind;
	Entry.OwnerCharacterIdx = Combatant.OwnerCharacterIdx;

	Avatars.Add(Combatant.CombatantId, MoveTemp(Entry));
}

float UNUBattleNetStageSubsystem::GetCaptureShakeRemainingSeconds() const
{
	float Remaining = 0.f;

	for (const TPair<int32, FNUNetStageAvatar>& Pair : Avatars)
	{
		if (IsValid(Pair.Value.Avatar))
		{
			Remaining = FMath::Max(Remaining, Pair.Value.Avatar->GetCaptureShakeRemainingSeconds());
		}
	}

	return Remaining;
}

void UNUBattleNetStageSubsystem::BeginTeardownStage()
{
	SetNetStageReady(false);
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StageWaitTimerHandle);
		if (UNUBattleEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UNUBattleEncounterSubsystem>())
		{
			EncounterSubsystem->OnStageShown.RemoveAll(this);
		}
	}
	
	PlayFadeOut();
	UNUSFXSubsystem::Play(GetWorld(), NU::SFX::Battle::Exit);

	// 아바타가 사라지는 걸 가림, 아바타 자체를 숨기면 페이드가 시작되기 전에 먼저 사라져 보임 (로컬과 같은 이유)
	for (const TPair<int32, FNUNetStageAvatar>& Pair : Avatars)
	{
		if (IsValid(Pair.Value.Avatar))
		{
			Pair.Value.Avatar->SetVitalBarHidden(true);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StageWaitTimerHandle,
			this, 
			&ThisClass::TeardownStage,
			GetFadeOutSeconds(),
			false
			);
		return;
	}

	TeardownStage();
}

bool UNUBattleNetStageSubsystem::IsTransitionActive() const
{
	return bWasInBattle || bTeardownDeferred || !Avatars.IsEmpty() || (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(StageWaitTimerHandle));
}

void UNUBattleNetStageSubsystem::TeardownStage()
{
	SetNetStageReady(false);
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StageWaitTimerHandle);

		if (UNUBattleEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UNUBattleEncounterSubsystem>())
		{
			EncounterSubsystem->OnStageShown.RemoveAll(this);
		}
	}

	// 기존 전투 아바타 삭제
	for (const TPair<int32, FNUNetStageAvatar>& Pair : Avatars)
	{
		if (IsValid(Pair.Value.Avatar))
		{
			Pair.Value.Avatar->Destroy();
		}
	}
	Avatars.Reset();

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UNURecoveryNetSubsystem* RecoveryNet = GI ? GI->GetSubsystem<UNURecoveryNetSubsystem>() : nullptr;
	if (RecoveryNet && RecoveryNet->IsAwaitingRespawn())
	{
		if (ANUPlayerController* PC = GetLocalPlayerController()) { PC->ClientHoldRecoveryFade(); }
	}
	RestoreFieldCamera();
	SetLocalFieldPawnHidden(false);

	if (UWorld* World = GetWorld())
	{
		if (UNUBattleEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UNUBattleEncounterSubsystem>())
		{
			// 세트 자체는 로컬 전투처럼 내리지 않음, 조명만 필드로 되돌림
			EncounterSubsystem->RestoreFieldLighting();
		}
	}

	PendingStageLevel.Reset();
	bTeardownDeferred = false;

	// 로컬의 StopTransitionSequence처럼 화면을 즉시 정상으로 되돌림 (부드러운 페이드인이 아님, 여기까진 이미 검은 화면)
	StopFadeTransition();
	if (RecoveryNet && RecoveryNet->IsAwaitingRespawn())
	{
		if (ANUPlayerController* PC = GetLocalPlayerController())
		{
			PC->ClientHoldRecoveryFade();
			if (ANUPlayerState* PS = PC->GetPlayerState<ANUPlayerState>())
			{
				if (UNURecoveryComponent* Recovery = PS->FindComponentByClass<UNURecoveryComponent>()) { Recovery->ApplyServerRecoveryState(); }
			}
		}
	}

	UE_LOG(LogNUBattleNetStage, Log, TEXT("넷 전투 세트 정리함"));
}

ANUBattleAvatar* UNUBattleNetStageSubsystem::SpawnAvatarFor(const FNUServerCombatant& Combatant, int32 SlotIndex,
	int32 SlotCount)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();

	TSoftClassPtr<ANUBattleAvatar> SoftClass;
	switch (Combatant.Kind)
	{
	case 0:		// 캐릭터
		SoftClass = Settings->NetPlayerAvatarClass;
		break;
		
	case 1:		// 나이아
		SoftClass = Settings->NetSpiritAvatarClass;
		break;
		
	default:	// 적
		SoftClass = Settings->NetEnemyAvatarClass;
		break;
	}

	UClass* AvatarClass = SoftClass.LoadSynchronous();
	if (!AvatarClass)
	{
		UE_LOG(LogNUBattleNetStage, Error,
			TEXT("전투원 %d 아바타 클래스가 설정되지 않음 (Kind=%d), NUBattleDataSettings 확인 필요"),
			Combatant.CombatantId, Combatant.Kind);
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANUBattleAvatar* Avatar = World->SpawnActor<ANUBattleAvatar>(
		AvatarClass, 
		GetSpawnTransform(Combatant, SlotIndex, SlotCount),
		SpawnParams
		);

	if (!IsValid(Avatar))
	{
		UE_LOG(LogNUBattleNetStage, Error, TEXT("전투원 %d 아바타 스폰 실패"), Combatant.CombatantId);
		return nullptr;
	}

	const bool bIsPlayerVisual = Combatant.Kind == 0;

	// 파티원 캐릭터는 서버가 준 그 사람 외형으로, 내 캐릭터는 장비까지 있는 로컬 정보로 그림
	// 외형을 입히는 InitializeNetAvatar 보다 먼저 넣어야 함. 종족 0 은 서버가 외형을 안 보낸 것
	const UGameInstance* GameInstance = World->GetGameInstance();
	const UNUCharacterManager* Characters = GameInstance ? GameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;
	const int64 MyCharacterIdx = IsValid(Characters) ? Characters->GetSelectedCharacterIdx() : 0;

	if (bIsPlayerVisual && Combatant.RaceId > 0 && Combatant.OwnerCharacterIdx != MyCharacterIdx)
	{
		if (ANUBattlePlayerAvatar* PlayerAvatar = Cast<ANUBattlePlayerAvatar>(Avatar))
		{
			PlayerAvatar->SetServerAppearance(
				Combatant.RaceId, Combatant.Gender, Combatant.HairId, Combatant.FaceId, Combatant.JobId);
		}
	}

	const EFieldFacing Facing = ANUBattleAvatar::ResolveBattleFacing(Combatant.Side != 1, bIsPlayerVisual);

	Avatar->InitializeNetAvatar(Combatant.CombatantId, Combatant.NaiaId, bIsPlayerVisual, Facing);
	Avatar->SetIsAlly(Combatant.Side != 1);
	Avatar->SetCombatantIdentity(FText::FromString(Combatant.Name), Combatant.Level);
	Avatar->SetVitalBarHidden(false);

	// 세트를 처음 세울 때는 암전 중이라 여기서 불러도 끊김이 안 보임
	// 연출은 로컬과 같이 기본 행동 어빌리티의 기본값 위에 스냅샷 스킬의 표 행을 덮어 정해짐 (UNUBattleNetActionQueueSubsystem)
	const UClass* DefaultAbilityClass = Settings->NetDefaultActionAbility.LoadSynchronous();
	const UNUBattleGameplayAbility* DefaultAbility =
		DefaultAbilityClass ? DefaultAbilityClass->GetDefaultObject<UNUBattleGameplayAbility>() : nullptr;
	Avatar->PreloadActionVFX(Combatant.ActiveSkillIds, { DefaultAbility });

	return Avatar;
}

FTransform UNUBattleNetStageSubsystem::GetSpawnTransform(const FNUServerCombatant& Combatant, int32 SlotIndex,
	int32 SlotCount) const
{
	UWorld* World = GetWorld();
	UNUBattleEncounterSubsystem* EncounterSubsystem = IsValid(World)
		? World->GetSubsystem<UNUBattleEncounterSubsystem>()
		: nullptr;

	if (!IsValid(EncounterSubsystem))
	{
		return FTransform::Identity;
	}

	const bool bIsEnemy = Combatant.Side == 1;
	const ENUBattleFormationRow Row = bIsEnemy
		? ENUBattleFormationRow::Enemy
		: (Combatant.Kind == 1 ? ENUBattleFormationRow::Naia : ENUBattleFormationRow::Player);

	FTransform StageTransform;
	if (EncounterSubsystem->ResolveFormationTransform(Row, SlotIndex, SlotCount, StageTransform))
	{
		return StageTransform;
	}

	// 세트에 진형 슬롯이 없을 때의 대체 좌표, VFX 뷰어와 같은 함수
	const FVector LocalOffset = NUBattlePresentation::GetFallbackFormationOffset(Row, SlotIndex);

	return FTransform(FRotator::ZeroRotator, EncounterSubsystem->GetArenaLocation() + LocalOffset);
}

void UNUBattleNetStageSubsystem::RefreshVitals()
{
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (!IsValid(BattleNet))
	{
		return;
	}

	for (const TPair<int32, FNUNetStageAvatar>& Pair : Avatars)
	{
		const FNUServerCombatant* Combatant = BattleNet->FindCombatant(Pair.Key);
		ANUBattleAvatar* Avatar = Pair.Value.Avatar;
		if (!Combatant || !IsValid(Avatar))
		{
			continue;
		}

		Avatar->SetVitals(Combatant->Health, Combatant->MaxHealth, Combatant->SP, Combatant->MaxSP);
		Avatar->SetCombatantIdentity(FText::FromString(Combatant->Name), Combatant->Level);
		PushStatusViews(Avatar, Combatant->Statuses);
		PushJobResourceViews(Avatar, Combatant->Resources, Combatant->JobStates);
		Avatar->SetDeadVisual(!Combatant->IsAlive());
	}
}

void UNUBattleNetStageSubsystem::PushStatusViews(ANUBattleAvatar* Avatar,
	const TArray<FNUServerBattleStatus>& Statuses) const
{
	if (!IsValid(Avatar))
	{
		return;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* BattleData = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	TArray<FNUStatusEffectView> Views;
	NUBattleServer::BuildStatusViews(BattleData, Statuses, Views);

	Avatar->SetStatusViews(Views);
}

void UNUBattleNetStageSubsystem::PushJobResourceViews(ANUBattleAvatar* Avatar,
	const TArray<FNUServerJobResource>& Resources, const TArray<FNUServerJobState>& JobStates) const
{
	if (!IsValid(Avatar))
	{
		return;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* BattleData = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();

	TArray<FNUJobResourceView> Gauges;
	TArray<FNUJobResourceView> Badges;
	NUBattleServer::BuildJobResourceViews(BattleData, Resources, JobStates,
		IsValid(BattleNet) ? BattleNet->GetTurn() : 0, Gauges, Badges);

	Avatar->SetJobResourceViews(Gauges, Badges);
}

void UNUBattleNetStageSubsystem::SetLocalFieldPawnHidden(bool bHidden)
{
	UWorld* World = GetWorld();
	APlayerController* PC = IsValid(World) ? World->GetFirstPlayerController() : nullptr;

	if (bHidden)
	{
		APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;
		if (!IsValid(Pawn))
		{
			return;
		}

		HiddenFieldPawn = Pawn;
		Pawn->SetActorHiddenInGame(true);
		Pawn->SetActorEnableCollision(false);

		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
				Movement->DisableMovement();
			}
		}

		return;
	}

	APawn* Cached = HiddenFieldPawn.Get();
	if (!IsValid(Cached))
	{
		return;
	}

	Cached->SetActorHiddenInGame(false);
	Cached->SetActorEnableCollision(true);

	if (ACharacter* Character = Cast<ACharacter>(Cached))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	HiddenFieldPawn.Reset();
}

void UNUBattleNetStageSubsystem::EnterBattleCamera()
{
	UWorld* World = GetWorld();
	APlayerController* PC = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC))
	{
		return;
	}

	// 퀘스트 카메라 컷처럼 잠깐 잡은 시점은 돌아갈 곳이 아님, 로컬 전투(ANUPlayerController::EnterBattleCamera)처럼 캐릭터일 때만 기억
	PreviousViewTarget = Cast<APawn>(PC->GetViewTarget());

	// 배경 세트가 자기 카메라를 들고 있으면 그 구도로 봄, 로컬 전투(ANUPlayerController::EnterBattleCamera)와 같은 방식
	UNUBattleEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UNUBattleEncounterSubsystem>();
	AActor* StageCamera = IsValid(EncounterSubsystem) ? EncounterSubsystem->GetStageViewTarget() : nullptr;

	if (IsValid(StageCamera))
	{
		// ACameraActor는 기본이 16:9 고정이라 그대로 쓰면 넓은 화면에서 좌우가 잘림, 로컬 전투 카메라와 같은 보정
		if (UCameraComponent* CameraComponent = StageCamera->FindComponentByClass<UCameraComponent>())
		{
			CameraComponent->SetConstraintAspectRatio(false);
		}

		PC->SetViewTarget(StageCamera);
		return;
	}

	// 세트에 카메라가 없으면 최소한 내 캐릭터 아바타라도 비춤
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (!IsValid(BattleNet))
	{
		return;
	}

	// 파티 전투에는 캐릭터가 여럿이라 내 것을 골라야 함
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	const UNUCharacterManager* Characters = GameInstance ? GameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;
	const int64 MyCharacterIdx = IsValid(Characters) ? Characters->GetSelectedCharacterIdx() : 0;

	for (const TPair<int32, FNUServerCombatant>& Pair : BattleNet->GetCombatants())
	{
		if (Pair.Value.Kind != 0 || (MyCharacterIdx != 0 && Pair.Value.OwnerCharacterIdx != MyCharacterIdx))
		{
			continue;
		}

		if (ANUBattleAvatar* CharacterAvatar = FindAvatar(Pair.Key))
		{
			PC->SetViewTarget(CharacterAvatar);
		}
		break;
	}
}

void UNUBattleNetStageSubsystem::RestoreFieldCamera()
{
	UWorld* World = GetWorld();
	APlayerController* PC = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC))
	{
		return;
	}

	AActor* RestoreTarget = PreviousViewTarget.Get();
	if (!IsValid(RestoreTarget))
	{
		RestoreTarget = PC->GetPawn();
	}

	if (IsValid(RestoreTarget))
	{
		PC->SetViewTarget(RestoreTarget);
	}

	PreviousViewTarget.Reset();
}

ANUPlayerController* UNUBattleNetStageSubsystem::GetLocalPlayerController() const
{
	UWorld* World = GetWorld();

	return IsValid(World) ? Cast<ANUPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

void UNUBattleNetStageSubsystem::PlayFadeOut()
{
	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		PC->PlayNetBattleTransition(GetDefault<UNUBattleDataSettings>()->NetFadeOutSequence.LoadSynchronous());
	}
}

void UNUBattleNetStageSubsystem::PlayFadeIn()
{
	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		PC->PlayNetBattleTransition(GetDefault<UNUBattleDataSettings>()->NetFadeInSequence.LoadSynchronous());
	}
}

void UNUBattleNetStageSubsystem::StopFadeTransition()
{
	if (ANUPlayerController* PC = GetLocalPlayerController())
	{
		PC->StopNetBattleTransition();
	}
}

float UNUBattleNetStageSubsystem::GetFadeOutSeconds() const
{
	constexpr float DefaultSeconds = 0.3f;

	const ULevelSequence* Sequence = GetDefault<UNUBattleDataSettings>()->NetFadeOutSequence.LoadSynchronous();
	const UMovieScene* MovieScene = IsValid(Sequence) ? Sequence->GetMovieScene() : nullptr;
	if (!IsValid(MovieScene))
	{
		return DefaultSeconds;
	}

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();

	return TickResolution.AsSeconds(FFrameTime(Range.Size<FFrameNumber>()));
}
