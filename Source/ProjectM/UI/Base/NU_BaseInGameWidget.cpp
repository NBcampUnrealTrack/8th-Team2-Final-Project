// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Base/NU_BaseInGameWidget.h"

// ANUBattleEncounter* 를 TWeakObjectPtr 에 담으려면 UObject 로의 변환이 필요하므로 완전한 타입이 필요하다
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "NUGAS/NUAbilitySystemComponent.h"

void UNU_BaseInGameWidget::InitializeWidget()
{
	// 자식 베이스가 Super::InitializeWidget() 을 먼저 부르므로,
	// 자식이 구독을 시작하는 시점에는 이미 해석이 끝나 있다.
	// 실패해도(PlayerState 미복제) 진행한다 — 구독이 필요한 자식이 다시 시도한다.
	ResolvePlayerContext();

	Super::InitializeWidget();

	BindLocalASC();
	BindNetBattle();

	// 구독 직후 1회 수동 동기화 (팀 관례).
	// 위젯이 ASC 보다 늦게 생성되면 이미 지나간 통지를 받을 수 없으므로 현재 상태를 직접 재생한다.
	// 전투 밖이면 nullptr 을 그대로 흘려 자식이 "필드 상태" 표시를 맞출 수 있게 한다.
	const UNUAbilitySystemComponent* ASC = GetNUASC();
	HandleEncounterChanged_Internal(ASC ? ASC->GetBattleEncounter() : nullptr);
}

void UNU_BaseInGameWidget::ReleaseWidget()
{
	UnbindLocalASC();
	UnbindNetBattle();
	CachedEncounter.Reset();
	bNetBattleActive = false;

	Super::ReleaseWidget();
}

bool UNU_BaseInGameWidget::ResolvePlayerContext()
{
	if (CachedNUPlayerState.IsValid() && CachedNUASC.IsValid())
	{
		return true;
	}

	if (!CachedNUPlayerController.IsValid())
	{
		CacheNUPlayerController();
	}

	const APlayerController* PC = CachedNUPlayerController.Get();
	ANUPlayerState* PS = PC ? Cast<ANUPlayerState>(PC->PlayerState) : nullptr;
	if (PS == nullptr)
	{
		return false;
	}

	CachedNUPlayerState = PS;
	CachedNUASC = PS->GetNUASC();

	return CachedNUASC.IsValid();
}

ANUPlayerState* UNU_BaseInGameWidget::GetNUPlayerState() const
{
	return CachedNUPlayerState.Get();
}

UNUAbilitySystemComponent* UNU_BaseInGameWidget::GetNUASC() const
{
	return CachedNUASC.Get();
}

ANUBattleEncounter* UNU_BaseInGameWidget::GetBattleEncounter() const
{
	return CachedEncounter.Get();
}

bool UNU_BaseInGameWidget::IsInBattle() const
{
	return CachedEncounter.IsValid() || bNetBattleActive;
}

//-----------------------구독-------------------------

void UNU_BaseInGameWidget::BindLocalASC()
{
	// PlayerState 복제가 늦어 InitializeWidget 진입 시점에 해석하지 못했을 수 있으므로 한 번 더 시도한다
	if (!ResolvePlayerContext())
	{
		return;
	}

	UNUAbilitySystemComponent* ASC = GetNUASC();
	if (ASC == nullptr)
	{
		return;
	}

	// 위젯 캐시 재사용(재오픈) 시 중복 구독 방지 — 해제 선행 후 구독
	ASC->OnBattleEncounterChanged.RemoveAll(this);
	ASC->OnBattleEncounterChanged.AddUObject(this, &UNU_BaseInGameWidget::HandleEncounterChanged_Internal);
}

void UNU_BaseInGameWidget::UnbindLocalASC()
{
	if (UNUAbilitySystemComponent* ASC = GetNUASC())
	{
		ASC->OnBattleEncounterChanged.RemoveAll(this);
	}
}

void UNU_BaseInGameWidget::BindNetBattle()
{
	UGameInstance* GameInstance = GetGameInstance();
	UNUBattleNetSubsystem* BattleNet = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleNetSubsystem>()
		: nullptr;
	
	if (IsValid(BattleNet))
	{
		BattleNet->OnBattleStateChanged.RemoveAll(this);
		BattleNet->OnBattleStateChanged.AddUObject(this, &ThisClass::HandleNetBattleStateChanged);
	}

	if (UNUBattleNetStageSubsystem* StageNet = GetNetStageSubsystem())
	{
		StageNet->OnNetStageReadyChanged.RemoveAll(this);
		StageNet->OnNetStageReadyChanged.AddUObject(this, &ThisClass::HandleNetStageReadyChanged);
	}

	RecomputeNetBattleActive();
}

void UNU_BaseInGameWidget::UnbindNetBattle()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (UNUBattleNetSubsystem* BattleNet = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleNetSubsystem>()
		: nullptr)
	{
		BattleNet->OnBattleStateChanged.RemoveAll(this);
	}

	if (UNUBattleNetStageSubsystem* StageNet = GetNetStageSubsystem())
	{
		StageNet->OnNetStageReadyChanged.RemoveAll(this);
	}
}

void UNU_BaseInGameWidget::HandleNetBattleStateChanged()
{
	RecomputeNetBattleActive();

	// Encounter 포인터가 그대로여도 IsInBattle 결과가 바뀌었으므로 자식의 표시 판정을 다시 실행함
	HandleEncounterChanged_Internal(CachedEncounter.Get());
}

void UNU_BaseInGameWidget::HandleNetStageReadyChanged(bool bReady)
{
	RecomputeNetBattleActive();
	HandleEncounterChanged_Internal(CachedEncounter.Get());
}

void UNU_BaseInGameWidget::RecomputeNetBattleActive()
{
	UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleNetSubsystem* BattleNet = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUBattleNetSubsystem>()
		: nullptr;
	const UNUBattleNetStageSubsystem* StageNet = GetNetStageSubsystem();

	// IsInBattle()만 보면, 처치 연출이 끝나기 전에 SVR_Battle_End에 의해 필드 HUD가 먼저 튀어나옴 
	// 전투 세트가 실제로 내려가는 시점(IsNetStageReady가 false로 바뀌는 시점)까지 전투 UI를 유지해야 함
	bNetBattleActive = (IsValid(BattleNet) && BattleNet->IsInBattle())
		|| (IsValid(StageNet) && StageNet->IsNetStageReady());
}

UNUBattleNetStageSubsystem* UNU_BaseInGameWidget::GetNetStageSubsystem() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetSubsystem<UNUBattleNetStageSubsystem>() : nullptr;
}

void UNU_BaseInGameWidget::HandleEncounterChanged_Internal(ANUBattleEncounter* NewEncounter)
{
	// 같은 값이어도 걸러내지 않는다.
	// 초기 동기화에서 nullptr -> nullptr 이 되는 경우가 정상 경로이고,
	// 그때도 자식은 "필드 상태" 통지를 받아야 초기 표시를 맞출 수 있다.
	CachedEncounter = NewEncounter;

	HandleEncounterChanged(NewEncounter);
	BP_OnEncounterChanged(NewEncounter);
}

//-------------------------C++ 확장 함수---------------------------

void UNU_BaseInGameWidget::HandleEncounterChanged(ANUBattleEncounter* NewEncounter)
{
}
