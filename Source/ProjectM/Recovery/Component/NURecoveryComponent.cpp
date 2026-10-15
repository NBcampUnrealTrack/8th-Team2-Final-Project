#include "Recovery/Component/NURecoveryComponent.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Recovery/Data/NURecoverySettings.h"
#include "Recovery/Data/NURecoveryTypes.h"
#include "Recovery/Net/NURecoveryNetSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "WorldTransition/World/NUWorldRestoreSafety.h"
#include "Inventory/NUInventoryComponent.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Net/EmberServerManager.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/GameplayEffect/NUGE_Resurrection.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Field/NUSpiritFollowComponent.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Field/Interior/NUWalkableInteriorComponent.h"

#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogNURecovery, Log, All);

UNURecoveryComponent::UNURecoveryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UNURecoveryComponent::BeginPlay()
{
	Super::BeginPlay();
	bServerBackedSession = IsUsingExternalServer();
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		UNURecoveryNetSubsystem* RecoveryNet = GI->GetSubsystem<UNURecoveryNetSubsystem>();
		RecoveryNet->OnStateChanged.AddUObject(this, &ThisClass::ApplyServerRecoveryState);
		RecoveryNet->OnRequestFailed.AddUObject(this, &ThisClass::HandleRecoveryRequestFailed);
		RecoveryNet->OnPointRegistered.AddUObject(this, &ThisClass::HandleServerPointRegistered);
		ApplyServerRecoveryState();
		if (UEmberServerManager* Server = GI->GetSubsystem<UEmberServerManager>())
		{
			Server->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleServerStateChanged);
		}
	}
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &ThisClass::InitializeBindings, 0.1f, true);
	}
}

void UNURecoveryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bResting = false;
	SetComponentTickEnabled(false);
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
	}
	if (ANUPlayerState* PS = GetPlayerState())
	{
		if (PS->GetNaiaRosterComponent() != nullptr) { PS->GetNaiaRosterComponent()->OnRosterChanged.RemoveDynamic(this, &ThisClass::HandleRestRosterChanged); }
		if (PS->GetQuestComponent() != nullptr)
		{
			PS->GetQuestComponent()->OnQuestProgressChanged.RemoveDynamic(this, &ThisClass::EvaluateAutomaticRegistration);
		}
		if (PS->GetNUASC() != nullptr)
		{
			PS->GetNUASC()->GetGameplayAttributeValueChangeDelegate(UNUVitalAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		}
	}
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UEmberServerManager* Server = GI->GetSubsystem<UEmberServerManager>())
		{
			Server->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleServerStateChanged);
		}
	}
	GetWorld()->GetTimerManager().ClearTimer(ServerRecoveryTimer);
	if (UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>()) { Sync->ResumePositionReporting(this, false); }
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		UNURecoveryNetSubsystem* Net = GI->GetSubsystem<UNURecoveryNetSubsystem>();
		Net->OnStateChanged.RemoveAll(this);
		Net->OnRequestFailed.RemoveAll(this);
		Net->OnPointRegistered.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

ANUPlayerState* UNURecoveryComponent::GetPlayerState() const
{
	return Cast<ANUPlayerState>(GetOwner());
}

void UNURecoveryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ThisClass, RegisteredLocationId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, bResting, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, PendingDeathId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, bServerBackedSession, COND_OwnerOnly);
}

// 외부 서버 세션과 로컬 권한 실행 구분
bool UNURecoveryComponent::CanExecuteAuthorityRecovery() const
{
	return GetOwner() != nullptr && GetOwner()->HasAuthority() && !IsUsingExternalServer();
}

bool UNURecoveryComponent::IsUsingExternalServer() const
{
	const UGameInstance* GI = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UEmberServerManager* Server = GI != nullptr ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
	const UNUCharacterManager* Characters = GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
	return bServerBackedSession || (Server != nullptr && Server->IsLoggedIn())
		|| (Characters != nullptr && Characters->GetSelectedCharacterIdx() != 0);
}

void UNURecoveryComponent::HandleServerStateChanged(EEmberConnState NewState)
{
	if (!IsUsingExternalServer()) { return; }
	// 한 번 외부 서버 캐릭터를 사용하면 연결이 끊겨도 UE 판정으로 전환하지 않는다.
	bServerBackedSession = true;
	FinishRest(false);
	GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
	PendingDeathId.Invalidate();
	PendingGoldLoss = 0;
	if (ANUPlayerController* PC = Cast<ANUPlayerController>(GetPlayerState()->GetOwner())) { PC->ClientRecoveryFade(false); }
}

void UNURecoveryComponent::InitializeBindings()
{
	ANUPlayerState* PS = GetPlayerState();
	if (PS == nullptr || PS->GetPawn() == nullptr || PS->GetNUASC() == nullptr)
	{
		return;
	}
	HealthChangedHandle = PS->GetNUASC()->GetGameplayAttributeValueChangeDelegate(UNUVitalAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	PS->GetQuestComponent()->OnQuestProgressChanged.AddUniqueDynamic(this, &ThisClass::EvaluateAutomaticRegistration);
	GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
	if (!CanExecuteAuthorityRecovery())
	{
		return;
	}
	EvaluateAutomaticRegistration();
	if (HasPendingDeath())
	{
		GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &ThisClass::ProcessPendingDeath, 0.25f, false);
	}
}

int32 UNURecoveryComponent::GetRegisteredLocationId() const
{
	return RegisteredLocationId;
}

// 거점 등록과 로컬 자동 등록
bool UNURecoveryComponent::CanRegisterPoint(int32 LocationId) const
{
	const ANUPlayerState* PS = GetPlayerState();
	FNURecoveryPointDefinition Point;
	if (PS == nullptr || HasPendingDeath() || PS->GetPawn() == nullptr || PS->GetNUASC() == nullptr || !PS->GetNUASC()->IsAlive()
		|| PS->GetNUASC()->GetBattleEncounter() != nullptr || !GetDefault<UNURecoverySettings>()->TryGetRecoveryPoint(LocationId, Point)
		|| PS->GetQuestComponent() == nullptr || !PS->GetQuestComponent()->HasCompletedQuests(Point.RequiredQuestIds))
	{
		return false;
	}
	if (IsUsingExternalServer())
	{
		const ANUPlayerController* PC = Cast<ANUPlayerController>(PS->GetOwner());
		const UNURecoveryNetSubsystem* Net = GetWorld()->GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>();
		// 거리·장소·해금 여부의 최종 판정은 서버가 한다.
		return PC && PC->IsLocalController() && Net && Net->IsActive() && !Net->IsAwaitingRespawn();
	}
	FTransform Destination;
	return FindLoadedRecoveryDestination(Point, Destination) && FVector::DistSquared(PS->GetPawn()->GetActorLocation(), Destination.GetLocation()) <= FMath::Square(Point.RegistrationRadius);
}

bool UNURecoveryComponent::RegisterPoint(int32 LocationId)
{
	if (IsUsingExternalServer())
	{
		if (!CanRegisterPoint(LocationId)) { return false; }
		return GetWorld()->GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>()->RequestRegisterPoint(LocationId);
	}
	if (GetOwner() == nullptr) { return false; }
	if (!GetOwner()->HasAuthority())
	{
		if (!CanRegisterPoint(LocationId)) { return false; }
		ServerRegisterPoint(LocationId);
		return true;
	}
	if (!CanRegisterPoint(LocationId))
	{
		return false;
	}
	RegisteredLocationId = LocationId;
	GetOwner()->ForceNetUpdate();
	FNURecoveryPointDefinition Point;
	GetDefault<UNURecoverySettings>()->TryGetRecoveryPoint(LocationId, Point);
	UNUSFXSubsystem::Play(this, NU::SFX::Field::PointRegister);
	Notify(FText::Format(NSLOCTEXT("NURecovery", "PointRegistered", "부활 거점을 {0}(으)로 등록했습니다."), Point.DisplayName));
	return true;
}

void UNURecoveryComponent::HandleServerPointRegistered(int32 LocationId)
{
	const ANUPlayerState* PS = GetPlayerState();
	const ANUPlayerController* PC = PS ? Cast<ANUPlayerController>(PS->GetOwner()) : nullptr;
	if (PC == nullptr || !PC->IsLocalController()) { return; }
	FNURecoveryPointDefinition Point;
	if (GetDefault<UNURecoverySettings>()->TryGetRecoveryPoint(LocationId, Point))
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Field::PointRegister);
		Notify(FText::Format(NSLOCTEXT("NURecovery", "ServerPointRegistered", "부활 거점을 {0}(으)로 등록했습니다."), Point.DisplayName));
	}
}

void UNURecoveryComponent::EvaluateAutomaticRegistration()
{
	// 기본 거점은 해금 후 최초 한 번, 별도 자동 등록 거점은 해당 퀘스트 완료 시 한 번만 적용한다.
	if (!CanExecuteAuthorityRecovery() || HasPendingDeath()) { return; }
	for (const FNURecoveryPointDefinition& Candidate : GetDefault<UNURecoverySettings>()->RecoveryPoints)
	{
		FNURecoveryPointDefinition Point;
		const bool bLocalDefault = Candidate.bRegisterOnStart && Candidate.AutoRegisterQuestId.IsNone()
			&& UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) == Candidate.Map.ToSoftObjectPath().GetLongPackageName();
		if ((bLocalDefault && RegisteredLocationId != 0) || (!Candidate.AutoRegisterQuestId.IsNone() && AppliedAutoRegistrationQuests.Contains(Candidate.AutoRegisterQuestId))) { continue; }
		if ((!bLocalDefault && Candidate.AutoRegisterQuestId.IsNone())
			|| !GetDefault<UNURecoverySettings>()->TryGetRecoveryPoint(Candidate.LocationId, Point)
			|| (!bLocalDefault && !GetPlayerState()->GetQuestComponent()->HasCompletedQuests({Point.AutoRegisterQuestId}))
			|| !GetPlayerState()->GetQuestComponent()->HasCompletedQuests(Point.RequiredQuestIds)) { continue; }
		RegisteredLocationId = Point.LocationId;
		if (!Point.AutoRegisterQuestId.IsNone()) { AppliedAutoRegistrationQuests.Add(Point.AutoRegisterQuestId); }
		GetOwner()->ForceNetUpdate();
		return;
	}
}

// 로컬 비용 조건과 사망 손실 조회
bool UNURecoveryComponent::IsPaidRecovery() const
{
#if !UE_BUILD_SHIPPING
	const IConsoleVariable* ForcePaid = IConsoleManager::Get().FindConsoleVariable(TEXT("nu.Recovery.Debug.ForcePaid"));
	if (ForcePaid && ForcePaid->GetBool()) { return true; }
#endif
	const ANUPlayerState* PS = GetPlayerState();
	return IsValid(PS) && PS->GetProgressionComponent() && PS->GetJobComponent() && PS->GetNaiaRosterComponent()
		&& PS->GetProgressionComponent()->GetLevel() >= GetDefault<UNURecoverySettings>()->PaidRecoveryRequiredLevel
		&& PS->GetJobComponent()->HasJob() && PS->GetNaiaRosterComponent()->HasSelectedStarterNaia();
}

bool UNURecoveryComponent::TryGetDeathGoldLoss(int64& OutLoss) const
{
	OutLoss = 0;
	if (!CanExecuteAuthorityRecovery()) { return false; }
	const UNURecoverySettings* Settings = GetDefault<UNURecoverySettings>();
	if (!IsPaidRecovery() || !GetPlayerState()->GetQuestComponent()->HasCompletedQuests(Settings->DeathPenaltyRequiredQuestIds)) { return true; }
	FNURecoveryCostRow Cost;
	if (!Settings->TryGetRecoveryCostRow(GetPlayerState()->GetProgressionComponent()->GetLevel(), Cost)) { return false; }
	OutLoss = Cost.ResurrectionGoldLoss;
	return true;
}

bool UNURecoveryComponent::ResolveDeathCost()
{
	if (bPendingCostResolved) { return true; }
	if (!TryGetDeathGoldLoss(PendingGoldLoss)) { return false; }
	bPendingCostResolved = true;
	return true;
}

bool UNURecoveryComponent::HasPendingDeath() const
{
	return PendingDeathId.IsValid();
}

// 로컬 사망 접수와 중복 처리 방지
bool UNURecoveryComponent::QueueDeath(const FGuid& DeathId, bool bBattleHandoff)
{
	if (!CanExecuteAuthorityRecovery() || !DeathId.IsValid())
	{
		return false;
	}
	if (PendingDeathId.IsValid() || LastCompletedDeathId == DeathId)
	{
		return true;
	}
	if (GetPlayerState()->GetNUASC()->IsAlive() || GetPlayerState()->GetNUASC()->GetBattleEncounter() != nullptr)
	{
		return false;
	}
	FinishRest(false);
	PendingDeathId = DeathId;
	PendingGoldLoss = 0;
	bPendingCostResolved = false;
	DestinationWaitStarted = -1.0;
	ResolveDeathCost();
	GetOwner()->ForceNetUpdate();
	if (ACharacter* Character = Cast<ACharacter>(GetPlayerState()->GetPawn()))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->GetCharacterMovement()->DisableMovement();
	}
	if (ANUPlayerController* PC = Cast<ANUPlayerController>(GetPlayerState()->GetOwner()))
	{
		PC->ClientRecoveryFade(true);
	}
	// 전투 종료 경로는 이미 암전 상태다. 별도의 대기 없이 목적지 처리를 시작한다.
	if (bBattleHandoff) { ProcessPendingDeath(); }
	else { GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &ThisClass::ProcessPendingDeath, 0.25f, false); }
	return true;
}

void UNURecoveryComponent::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	if (Change.NewValue <= KINDA_SMALL_NUMBER && Change.OldValue > KINDA_SMALL_NUMBER && !bProcessingDeath)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::CheckFieldDeath);
	}
}

void UNURecoveryComponent::CheckFieldDeath()
{
	if (CanExecuteAuthorityRecovery() && !HasPendingDeath() && !GetPlayerState()->GetNUASC()->IsAlive()
		&& GetPlayerState()->GetNUASC()->GetBattleEncounter() == nullptr)
	{
		QueueDeath(FGuid::NewGuid());
	}
}

// 로컬 부활 HP 복원. 효과 정리 전 SP를 보관하여 회복 후에도 유지한다.
bool UNURecoveryComponent::RestoreASC(UNUAbilitySystemComponent* ASC)
{
	if (ASC == nullptr)
	{
		return false;
	}
	const float MaxHealth = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
	const float PreviousSP = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute());
	if (MaxHealth <= KINDA_SMALL_NUMBER || !FMath::IsFinite(MaxHealth) || !FMath::IsFinite(PreviousSP))
	{
		return false;
	}
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UNUGE_Resurrection::StaticClass(), 1.0f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return false;
	}
	ASC->ClearTurnBasedEffects();
	ASC->ReviveIfDead(1.0f);
	Spec.Data->SetSetByCallerMagnitude(NU::Data::Recovery::Health, ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()));
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	ASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetSPAttribute(), PreviousSP);
	return ASC->IsAlive() && FMath::IsNearlyEqual(ASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute()), ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()));
}

bool UNURecoveryComponent::ValidateResurrectionTargets() const
{
	ANUPlayerState* PS = GetPlayerState();
	if (!IsValid(PS) || !IsValid(PS->GetNaiaRosterComponent())) { return false; }
	TArray<UNUAbilitySystemComponent*> Targets = {PS->GetNUASC()};
	if (UNUNaiaRosterComponent* Roster = PS->GetNaiaRosterComponent())
	{
		for (ANUSpiritState* Spirit : Roster->GetParty())
		{
			if (!IsValid(Spirit)) { return false; }
			Targets.Add(Spirit->GetNUASC());
		}
	}
	// 일부 대상만 회복한 뒤 잘못된 최대 능력치 때문에 중단하지 않도록 먼저 검사한다.
	for (UNUAbilitySystemComponent* Target : Targets)
	{
		if (Target == nullptr) { return false; }
		const float MaxHP = Target->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
		const float MaxSP = Target->GetNumericAttribute(UNUVitalAttributeSet::GetMaxSPAttribute());
		const float SP = Target->GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute());
		if (!FMath::IsFinite(MaxHP) || MaxHP <= KINDA_SMALL_NUMBER || !FMath::IsFinite(MaxSP) || MaxSP < 0.0f || !FMath::IsFinite(SP)) { return false; }
	}
	for (const FNUNaiaInstance& Entry : PS->GetNaiaRosterComponent()->GetStorage())
	{
		if (!FMath::IsFinite(Entry.HealthRatio) || Entry.HealthRatio < 0.0f || Entry.HealthRatio > 1.0f) { return false; }
	}
	return true;
}

bool UNURecoveryComponent::RestoreVitals()
{
	ANUPlayerState* PS = GetPlayerState();
	if (!ValidateResurrectionTargets()) { return false; }
	if (!RestoreASC(PS->GetNUASC()))
	{
		return false;
	}
	if (UNUNaiaRosterComponent* Roster = PS->GetNaiaRosterComponent())
	{
		for (ANUSpiritState* Spirit : Roster->GetParty())
		{
			if (IsValid(Spirit) && !RestoreASC(Spirit->GetNUASC()))
			{
				return false;
			}
		}
		return Roster->RestoreStorageHealth();
	}
	return true;
}

// 로컬 부활 실행: 목적지 준비 -> 이동 -> HP 복원 및 손실 반영
void UNURecoveryComponent::ProcessPendingDeath()
{
	ANUPlayerState* PS = GetPlayerState();
	if (!CanExecuteAuthorityRecovery() || !PendingDeathId.IsValid() || bProcessingDeath)
	{
		return;
	}
	TGuardValue<bool> Guard(bProcessingDeath, true);
	FNURecoveryPointDefinition Point;
	ANUPlayerController* PC = Cast<ANUPlayerController>(PS->GetOwner());
	APawn* Pawn = PS->GetPawn();
	UNUInventoryComponent* Inventory = PS->GetInventoryComponent();
	const bool bValidPoint = GetDefault<UNURecoverySettings>()->TryGetRecoveryPoint(RegisteredLocationId, Point);
	const FString Destination = Point.Map.ToSoftObjectPath().GetLongPackageName();
	if (!bValidPoint || Pawn == nullptr || Inventory == nullptr || !FPackageName::DoesPackageExist(Destination))
	{
		Notify(NSLOCTEXT("NURecovery", "InvalidDestination", "부활 거점을 불러올 수 없습니다. 골드는 차감하지 않았습니다."));
		if (PC != nullptr) { PC->ClientRecoveryFade(false); }
		return;
	}
	if (!ResolveDeathCost() || !ValidateResurrectionTargets())
	{
		FailPendingDeath(NSLOCTEXT("NURecovery", "InvalidRespawnState", "부활 상태 또는 비용을 확인할 수 없습니다. 다시 시도할 수 있습니다."));
		return;
	}
	FTransform ResolvedTransform;
	if (!PrepareRecoveryDestination(Point, ResolvedTransform)) { return; }

	// 잔액 부족은 부활을 막지 않는다. 음수 부채를 허용하되 int64 범위 초과는 차단한다.
	if (PendingGoldLoss < 0 || Inventory->GetGold() < MIN_int64 + PendingGoldLoss)
	{
		Notify(NSLOCTEXT("NURecovery", "InvalidGold", "사망 비용을 반영할 수 없습니다."));
		if (PC != nullptr) { PC->ClientRecoveryFade(false); }
		return;
	}
	// 부활은 문 출입을 거치지 않으므로 도착지의 실내 상태와 크기도 함께 복원한다.
	FVector Arrival = ResolvedTransform.GetLocation();
	UNUWalkableInteriorComponent* DestinationInterior = nullptr;
	for (TObjectIterator<UNUWalkableInteriorComponent> It; It; ++It)
	{
		if (It->GetWorld() != GetWorld() || !It->HasBegunPlay()) { continue; }
		const UBoxComponent* Bounds = It->GetInteriorBounds();
		if (!IsValid(Bounds)) { continue; }
		const FVector Extent = Bounds->GetUnscaledBoxExtent();
		if (!FBox(-Extent, Extent).IsInsideOrOn(Bounds->GetComponentTransform().InverseTransformPosition(Arrival))) { continue; }
		if (DestinationInterior != nullptr)
		{
			FailPendingDeath(NSLOCTEXT("NURecovery", "AmbiguousRespawnInterior", "부활 위치의 실내 범위가 겹쳐 있습니다. 골드는 차감하지 않았습니다."));
			return;
		}
		DestinationInterior = *It;
	}
	UNUInteriorParticipantComponent* Participant = Pawn->FindComponentByClass<UNUInteriorParticipantComponent>();
	// 배치된 거점 높이가 바닥과 조금 겹쳐도 목적지 크기로 보행 가능한 높이를 찾는다.
	ACharacter* Character = Cast<ACharacter>(Pawn);
	const bool bSafeArrival = Character == nullptr
		|| NUWorldRestoreSafety::IsSafe(Character, DestinationInterior, Arrival)
		|| NUWorldRestoreSafety::FindFloorArrival(Character, DestinationInterior, Arrival, Arrival);
	const bool bMoved = bSafeArrival && (Participant != nullptr
		? Participant->ApplyRestoredState(DestinationInterior, &Arrival)
		: DestinationInterior == nullptr && Pawn->TeleportTo(Arrival, Pawn->GetActorRotation(), false, false));
	if (!bMoved)
	{
		Notify(NSLOCTEXT("NURecovery", "BlockedDestination", "부활 위치가 막혀 있습니다. 골드는 차감하지 않았습니다."));
		if (PC != nullptr) { PC->ClientRecoveryFade(false); }
		return;
	}
	if (!RestoreVitals() || !Inventory->ApplyDeathGoldLoss(PendingGoldLoss))
	{
		Notify(NSLOCTEXT("NURecovery", "RestoreFailed", "부활 상태 적용에 실패했습니다."));
		if (PC != nullptr) { PC->ClientRecoveryFade(false); }
		return;
	}
	if (UNUNaiaRosterComponent* Roster = PS->GetNaiaRosterComponent())
	{
		ANUSpiritState* Spirit = Roster->GetActiveNaia();
		AActor* Avatar = IsValid(Spirit) ? Spirit->GetCurrentAvatar() : nullptr;
		UNUSpiritFollowComponent* Follow = IsValid(Avatar) ? Avatar->FindComponentByClass<UNUSpiritFollowComponent>() : nullptr;
		if (Follow != nullptr)
		{
			Avatar->SetActorLocation(Follow->GetDesiredLocation(Pawn), false, nullptr, ETeleportType::TeleportPhysics);
			Follow->ResetBob();
		}
	}
	const int64 Loss = PendingGoldLoss;
	LastCompletedDeathId = PendingDeathId;
	GetOwner()->ForceNetUpdate();
	PendingDeathId.Invalidate();
	PendingGoldLoss = 0;
	bPendingCostResolved = false;
	DestinationWaitStarted = -1.0;
	if (UNUWorldLocationComponent* Location = PS->FindComponentByClass<UNUWorldLocationComponent>()) { Location->RefreshLocation(); }
	Pawn->SetActorHiddenInGame(false);
	Pawn->SetActorEnableCollision(true);
	if (Character != nullptr)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	if (PC != nullptr)
	{
		PC->ClientCompleteRecovery(Pawn->GetActorLocation());
	}
	Notify(FText::Format(NSLOCTEXT("NURecovery", "Respawned", "{0}골드를 잃었습니다"), FText::AsNumber(Loss)));
	UE_LOG(LogNURecovery, Log, TEXT("Authority respawn Location=%d GoldLoss=%lld Gold=%lld"), Point.LocationId, Loss, Inventory->GetGold());
}

void UNURecoveryComponent::Notify(const FText& Message) const
{
	if (const ANUPlayerState* PS = GetPlayerState())
	{
		if (ANUPlayerController* PC = Cast<ANUPlayerController>(PS->GetOwner()))
		{
			PC->ClientRecoveryNotice(Message);
		}
	}
}

bool UNURecoveryComponent::RetryPendingRespawn()
{
	if (IsUsingExternalServer())
	{
		DestinationWaitStarted = -1.0;
		GetWorld()->GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>()->RequestStatus();
		ProcessServerRespawn();
		return true;
	}
	if (!IsUsingExternalServer() && GetOwner() != nullptr && !GetOwner()->HasAuthority() && HasPendingDeath())
	{
		ServerRetryPendingRespawn();
		return true;
	}
	if (!CanExecuteAuthorityRecovery() || !HasPendingDeath() || bProcessingDeath)
	{
		return false;
	}
	GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &ThisClass::ProcessPendingDeath, 0.1f, false);
	DestinationWaitStarted = -1.0;
	if (ANUPlayerController* PC = Cast<ANUPlayerController>(GetPlayerState()->GetOwner())) { PC->ClientRecoveryFade(true); }
	return true;
}

void UNURecoveryComponent::ServerRegisterPoint_Implementation(int32 LocationId)
{
	RegisterPoint(LocationId);
}

void UNURecoveryComponent::ServerRetryPendingRespawn_Implementation()
{
	RetryPendingRespawn();
}
