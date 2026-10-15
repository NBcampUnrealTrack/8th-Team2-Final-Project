#include "Recovery/Component/NURecoveryComponent.h"

#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Field/FieldPlayerCharacter.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Field/NUSpiritFollowComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "PlayerController/NUPlayerController.h"
#include "Recovery/Data/NURecoverySettings.h"
#include "Recovery/Net/NURecoveryNetSubsystem.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "WorldLocation/Data/NUWorldDataSettings.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"
#include "WorldTransition/Data/NUWorldPortalDataSubsystem.h"
#include "WorldTransition/Net/NULocationRestoreSubsystem.h"
#include "WorldTransition/World/NUWorldTransitionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNURecoveryTravel, Log, All);

bool UNURecoveryComponent::IsResting() const
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNURecoveryNetSubsystem* Net = GI ? GI->GetSubsystem<UNURecoveryNetSubsystem>() : nullptr;
	return IsUsingExternalServer() ? Net && Net->IsResting() : bResting;
}

// NPC 실행 요청과 실패 안내
bool UNURecoveryComponent::RequestServerRecovery(int32 NpcId, bool bRest, UNUDialogueDefinition* FailureDialogue)
{
	UNURecoveryNetSubsystem* Net = GetWorld()->GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>();
	ServerFailureDialogue = FailureDialogue;
	if (Net->RequestService(NpcId, bRest)) { return true; }
	Notify(NSLOCTEXT("NURecovery", "RequestPending", "서버 연결 상태 또는 처리 중인 회복 요청을 확인해 주세요."));
	return false;
}

void UNURecoveryComponent::HandleRecoveryRequestFailed(int32 Code, const FText& Message)
{
	const ANUPlayerState* PS = GetPlayerState();
	ANUPlayerController* PC = PS ? Cast<ANUPlayerController>(PS->GetOwner()) : nullptr;
	if (!PC || !PC->IsLocalController()) { return; }
	const UNURecoveryNetSubsystem* Net = GetWorld()->GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>();
	if (Net->IsAwaitingRespawn() && !Net->HasPendingRespawn()) { PC->ClientRecoveryFade(false); }
	if (Code == 2302 && ServerFailureDialogue.IsValid()) { PC->ClientOpenStandaloneDialogue(ServerFailureDialogue.Get()); }
	else { PC->ClientRecoveryNotice(Message); }
}

// 서버 확정 상태를 로컬 플레이어에 반영
void UNURecoveryComponent::ApplyServerRecoveryState()
{
	UGameInstance* GI = GetWorld()->GetGameInstance();
	UNURecoveryNetSubsystem* Net = GI ? GI->GetSubsystem<UNURecoveryNetSubsystem>() : nullptr;
	ANUPlayerState* PS = GetPlayerState();
	ANUPlayerController* PC = PS ? Cast<ANUPlayerController>(PS->GetOwner()) : nullptr;
	if (!Net || !Net->IsActive() || !Net->HasState()) { GetWorld()->GetTimerManager().ClearTimer(ServerRecoveryTimer); return; }
	if (PC && !PC->IsLocalController()) { return; }
	UNUAbilitySystemComponent* ASC = PS ? PS->GetNUASC() : nullptr;

	// 캐릭터 선택 응답이 Pawn/GAS 초기화보다 먼저 올 수 있기 때문에 준비가 끝난 뒤 적용한다.
	if (!PC || !PS->GetPawn() || !ASC || !ASC->GetVitalAttributeSet() || !ASC->GetAvatarActor()
		|| ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()) <= 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ServerRecoveryTimer, this, &ThisClass::ApplyServerRecoveryState, 0.1f, false);
		return;
	}
	const FNURecoveryServerState& State = Net->GetState();
	RegisteredLocationId = static_cast<int32>(State.RegisteredPointId);
	if (AppliedGoldState != Net->GetStateSerial() && State.bHasGold && PS->GetInventoryComponent())
	{
		PS->GetInventoryComponent()->ApplyServerGold(State.Gold);
		AppliedGoldState = Net->GetStateSerial();
	}

	// 전투 중 사용하는 GAS에 필드 HP를 덮어쓰지 않도록 전투와 연출 정리를 기다린다.
	const UNUBattleNetStageSubsystem* Stage = GetWorld()->GetSubsystem<UNUBattleNetStageSubsystem>();
	if (GI->GetSubsystem<UNUBattleNetSubsystem>()->IsInBattle() || (Stage && Stage->IsTransitionActive()) || ASC->GetBattleEncounter())
	{
		GetWorld()->GetTimerManager().SetTimer(ServerRecoveryTimer, this, &ThisClass::ApplyServerRecoveryState, 0.1f, false);
		return;
	}

	// 표시 보간값 대신 서버 확정 HP만 적용한다.
	if (AppliedServerState != Net->GetStateSerial())
	{
		// 최대 HP는 성장 GE가 계산한다. 서버의 최종 최대치를 Base에 넣으면 GE 값이 중복 합산된다.
		// 회복 응답은 현재 HP만 반영하고 최대 HP와 SP는 변경하지 않는다.
		if (State.PlayerHp > 0) { ASC->ReviveIfDead(State.PlayerHp); }
		ASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetHealthAttribute(), State.PlayerHp);
		AppliedServerState = Net->GetStateSerial();
	}
	if (Net->HasPendingRespawn()) { ProcessServerRespawn(); }
}

// 서버 부활 처리 이후의 거점 이동과 화면 복귀
void UNURecoveryComponent::ProcessServerRespawn()
{
	UNURecoveryNetSubsystem* Net = GetWorld()->GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>();
	ANUPlayerState* PS = GetPlayerState();
	ANUPlayerController* PC = PS ? Cast<ANUPlayerController>(PS->GetOwner()) : nullptr;
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	if (!Net->IsActive() || !Net->HasPendingRespawn() || !PC || !PC->IsLocalController() || !Pawn) { return; }
	const UNUBattleNetStageSubsystem* Stage = GetWorld()->GetSubsystem<UNUBattleNetStageSubsystem>();
	if (Stage && Stage->IsTransitionActive()) { return; }
	if (AppliedServerState != Net->GetStateSerial()) { ApplyServerRecoveryState(); return; }

	// 전투 정리 후 목적지 준비와 이동이 끝날 때까지 암전을 유지한다.
	PC->ClientHoldRecoveryFade();
	if (bServerRespawnTransitionActive) { return; }
	if (bServerRespawnFieldEntryPending)
	{
		const UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>();
		if (Sync != nullptr && Sync->HasConfirmedFieldEntry())
		{
			int32 ConfirmedAreaId = 0;
			int32 ConfirmedLocationId = 0;
			bool bConfirmedIndoor = false;
			if (Sync->GetConfirmedSpace(ConfirmedAreaId, ConfirmedLocationId, bConfirmedIndoor)
				&& ConfirmedAreaId == ServerRespawnAreaId && ConfirmedLocationId == ServerRespawnLocationId && !bConfirmedIndoor)
			{
				CompleteServerRespawn();
				return;
			}
			FailPendingDeath(NSLOCTEXT("NURecovery", "ServerFieldEntryMismatch", "서버가 부활 위치와 다른 장소를 확정했습니다."));
			return;
		}
		if (Sync == nullptr || Sync->HasFieldEntryFailed() || GetWorld()->GetTimeSeconds() - ServerRespawnFieldEntryStarted >= GetDefault<UNURecoverySettings>()->DestinationLoadTimeoutSeconds)
		{
			FailPendingDeath(NSLOCTEXT("NURecovery", "ServerFieldEntryFailed", "부활 후 필드 장소 동기화에 실패했습니다. 다시 시도해 주세요."));
			return;
		}
		GetWorld()->GetTimerManager().SetTimer(ServerRecoveryTimer, this, &ThisClass::ProcessServerRespawn, 0.1f, false);
		return;
	}
	FNURecoveryPointDefinition Point;
	if (!GetDefault<UNURecoverySettings>()->TryGetRecoveryPoint(static_cast<int32>(Net->GetState().EffectivePointId), Point))
	{
		FailPendingDeath(NSLOCTEXT("NURecovery", "ServerPointMissing", "서버 부활 거점의 클라이언트 설정이 없습니다."));
		return;
	}
	// 거점 맵에 맞는 포탈 진입 설정을 사용해 활성 공간도 함께 바꾼다.
	const FString PointPackage = Point.Map.ToSoftObjectPath().GetLongPackageName();
	const bool bPersistent = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) == PointPackage;
	const int32 AreaId = GetDefault<UNUWorldDataSettings>()->GetAreaIdForMap(PointPackage);
	const UNUWorldPortalDataSubsystem* PortalData = GetWorld()->GetGameInstance()->GetSubsystem<UNUWorldPortalDataSubsystem>();
	const FNUWorldPortalEntryBinding* Entry = nullptr;
	if (PortalData != nullptr)
	{
		for (const FNUWorldPortalEntryBinding& Binding : PortalData->GetEntryBindings())
		{
			const FNUWorldDestination& Candidate = Binding.Destination;
			if (Binding.AreaId == AreaId && Candidate.LevelTransform.Equals(Point.LevelTransform)
				&& (bPersistent ? Candidate.IsPersistentLevelDestination() : Candidate.Level.ToSoftObjectPath().GetLongPackageName() == PointPackage))
			{
				Entry = &Binding;
				break;
			}
		}
	}
	if (Entry == nullptr || !IsValid(Cast<AFieldPlayerCharacter>(Pawn)))
	{
		FailPendingDeath(NSLOCTEXT("NURecovery", "ServerPointEntryMissing", "부활 거점의 포탈 진입 설정을 찾을 수 없습니다."));
		return;
	}
	UNUWorldTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UNUWorldTransitionSubsystem>();
	UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>();
	if (Transition == nullptr || Sync == nullptr)
	{
		FailPendingDeath(NSLOCTEXT("NURecovery", "ServerTransitionMissing", "부활 공간 전환을 시작할 수 없습니다."));
		return;
	}
	FNUWorldRestoreTransitionRequest Request;
	Request.Destination = Entry->Destination;
	Request.Destination.ResolvedAreaId = AreaId;
	Request.Destination.bUseFade = false;
	Request.Facing = static_cast<int32>(CastChecked<AFieldPlayerCharacter>(Pawn)->Facing);
	Request.RecoveryPointId = Point.LocationId;
	Request.LocationId = Entry->LocationId;
	Request.FallbackLocationId = Entry->LocationId;
	Request.OnSucceeded = FSimpleDelegate::CreateUObject(this, &ThisClass::HandleServerRespawnTransitionSucceeded);
	Request.OnFailed = FSimpleDelegate::CreateUObject(this, &ThisClass::HandleServerRespawnTransitionFailed);
	UE_LOG(LogNURecoveryTravel, Log, TEXT("부활 공간 전환 시작 recovery_point=%d area=%d entry=%s"),
		Point.LocationId, AreaId, *Request.Destination.EntryPointId.ToString());
	Sync->SuspendPositionReporting(this);
	bServerRespawnTransitionActive = true;
	if (!Transition->RequestRestoreTransition(Pawn, Request) && bServerRespawnTransitionActive && !Transition->IsTransitionInProgress())
	{
		bServerRespawnTransitionActive = false;
		FailPendingDeath(NSLOCTEXT("NURecovery", "ServerPointUnavailable", "부활 거점의 레벨 또는 도착 위치를 준비하지 못했습니다."));
	}
}

void UNURecoveryComponent::HandleServerRespawnTransitionSucceeded()
{
	bServerRespawnTransitionActive = false;
	ANUPlayerState* PS = GetPlayerState();
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	UNUWorldLocationComponent* Location = PS ? PS->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;
	UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>();
	// 전투 사망 중 꺼진 충돌을 되살린 뒤 침대 위치의 장소 트리거를 다시 판정한다.
	if (Pawn != nullptr) { Pawn->SetActorEnableCollision(true); }
	if (Location != nullptr) { Location->RefreshLocation(); }
	const int32 LocationId = Location ? Location->GetCurrentLocationId() : 0;
	const int32 AreaId = Location ? Location->GetLastKnownAreaId() : 0;
	const UNUWorldDataSubsystem* WorldData = GetWorld()->GetGameInstance()->GetSubsystem<UNUWorldDataSubsystem>();
	const FNULocationDefinition* Definition = WorldData != nullptr ? WorldData->FindLocationDefinition(LocationId) : nullptr;
	const UNUWorldTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UNUWorldTransitionSubsystem>();
	if (Sync == nullptr || Definition == nullptr || Definition->AreaId != AreaId || Definition->bIndoor
		|| Transition == nullptr || Transition->GetActiveAreaId(Pawn) != AreaId)
	{
		FailPendingDeath(NSLOCTEXT("NURecovery", "ServerPointLocationMissing", "부활 위치의 필드 장소를 찾을 수 없습니다."));
		return;
	}
	// 로그인 복원 장소는 사망 전 위치이므로 새 필드 입장 전에 폐기한다.
	if (UNULocationRestoreSubsystem* Restore = GetWorld()->GetGameInstance()->GetSubsystem<UNULocationRestoreSubsystem>()) { Restore->ForgetAppliedLocationAfterRespawn(); }
	ServerRespawnAreaId = AreaId;
	ServerRespawnLocationId = LocationId;
	bServerRespawnFieldEntryPending = true;
	ServerRespawnFieldEntryStarted = GetWorld()->GetTimeSeconds();
	UE_LOG(LogNURecoveryTravel, Log, TEXT("부활 필드 재입장 요청 area=%d location=%d pos=%s"), AreaId, LocationId, *Pawn->GetActorLocation().ToCompactString());
	if (!Sync->EnterFieldAtLocation(LocationId))
	{
		FailPendingDeath(NSLOCTEXT("NURecovery", "ServerFieldEntryUnavailable", "부활 후 필드 재입장을 시작할 수 없습니다."));
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(ServerRecoveryTimer, this, &ThisClass::ProcessServerRespawn, 0.1f, false);
}

void UNURecoveryComponent::HandleServerRespawnTransitionFailed()
{
	bServerRespawnTransitionActive = false;
	FailPendingDeath(NSLOCTEXT("NURecovery", "ServerPointUnavailable", "부활 거점의 레벨 또는 도착 위치를 준비하지 못했습니다."));
}

void UNURecoveryComponent::CompleteServerRespawn()
{
	UNURecoveryNetSubsystem* Net = GetWorld()->GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>();
	ANUPlayerState* PS = GetPlayerState();
	ANUPlayerController* PC = PS ? Cast<ANUPlayerController>(PS->GetOwner()) : nullptr;
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	if (Net == nullptr || !Net->HasPendingRespawn() || PC == nullptr || Pawn == nullptr) { return; }
	bServerRespawnFieldEntryPending = false;
	ServerRespawnFieldEntryStarted = -1.0;
	ServerRespawnAreaId = 0;
	ServerRespawnLocationId = 0;
	if (UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>()) { Sync->ResumePositionReporting(this, false); }
	UE_LOG(LogNURecoveryTravel, Log, TEXT("부활 공간 및 필드 재입장 확정 pos=%s"), *Pawn->GetActorLocation().ToCompactString());
	Pawn->SetActorHiddenInGame(false);
	Pawn->SetActorEnableCollision(true);
	if (ACharacter* Character = Cast<ACharacter>(Pawn)) { Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
	if (UNUNaiaRosterComponent* Roster = PS->GetNaiaRosterComponent())
	{
		ANUSpiritState* Spirit = Roster->GetActiveNaia();
		AActor* Avatar = Spirit ? Spirit->GetCurrentAvatar() : nullptr;
		UNUSpiritFollowComponent* Follow = Avatar ? Avatar->FindComponentByClass<UNUSpiritFollowComponent>() : nullptr;
		if (Follow) { Avatar->SetActorLocation(Follow->GetDesiredLocation(Pawn), false, nullptr, ETeleportType::TeleportPhysics); Follow->ResetBob(); }
	}
	if (UNUWorldLocationComponent* Location = PS->FindComponentByClass<UNUWorldLocationComponent>()) { Location->RefreshLocation(); }

	// 이동이 완료된 사망 ID를 기록한 다음 화면을 복귀한다.
	const int64 Loss = Net->GetState().ChargedGold;
	Net->CompleteRespawnPresentation();
	PC->ClientCompleteRecovery(Pawn->GetActorLocation());
	PC->ClientRecoveryNotice(FText::Format(NSLOCTEXT("NURecovery", "Respawned", "{0}골드를 잃었습니다"), FText::AsNumber(Loss)));
}
