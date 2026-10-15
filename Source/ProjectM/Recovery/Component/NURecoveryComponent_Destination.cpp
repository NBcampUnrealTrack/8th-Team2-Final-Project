#include "Recovery/Component/NURecoveryComponent.h"

#include "CharacterState/NUPlayerState.h"
#include "Recovery/World/NURecoveryPoint.h"
#include "PlayerController/NUPlayerController.h"
#include "Recovery/Data/NURecoverySettings.h"
#include "Field/Net/FieldSyncSubsystem.h"

#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

bool UNURecoveryComponent::FindLoadedRecoveryDestination(const FNURecoveryPointDefinition& Point, FTransform& OutTransform) const
{
	const FString Package = Point.Map.ToSoftObjectPath().GetLongPackageName();
	const ULevel* DestinationLevel = nullptr;
	if (UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) == Package) { DestinationLevel = GetWorld()->PersistentLevel; }
	else
	{
		for (const ULevelStreaming* Level : GetWorld()->GetStreamingLevels())
		{
			if (IsValid(Level) && Level->IsLevelLoaded() && Level->IsLevelVisible() && Level->LevelTransform.Equals(Point.LevelTransform)
				&& (UWorld::RemovePIEPrefix(Level->GetWorldAssetPackageName()) == Package || Level->PackageNameToLoad.ToString() == Package))
			{
				DestinationLevel = Level->GetLoadedLevel();
				break;
			}
		}
	}

	// 같은 맵을 여러 번 배치할 수 있으므로 목적지 레벨과 거점 ID가 모두 일치해야 한다.
	const ANURecoveryPoint* Found = nullptr;
	for (TActorIterator<ANURecoveryPoint> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() != DestinationLevel || It->LocationId != Point.LocationId) { continue; }
		if (Found != nullptr || It->GetRecoveryTransform().ContainsNaN()) { return false; }
		Found = *It;
	}
	if (Found != nullptr) { OutTransform = Found->GetRecoveryTransform(); return true; }
	return false;
}

bool UNURecoveryComponent::PrepareRecoveryDestination(const FNURecoveryPointDefinition& Point, FTransform& OutTransform)
{
	if (FindLoadedRecoveryDestination(Point, OutTransform)) { DestinationWaitStarted = -1.0; return true; }
	const FString DestinationPackage = Point.Map.ToSoftObjectPath().GetLongPackageName();
	bool bDestinationLoaded = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) == DestinationPackage;
	for (const ULevelStreaming* Level : GetWorld()->GetStreamingLevels())
	{
		bDestinationLoaded |= IsValid(Level) && Level->IsLevelLoaded() && Level->IsLevelVisible() && Level->LevelTransform.Equals(Point.LevelTransform)
			&& (UWorld::RemovePIEPrefix(Level->GetWorldAssetPackageName()) == DestinationPackage || Level->PackageNameToLoad.ToString() == DestinationPackage);
	}
	if (bDestinationLoaded)
	{
		FailPendingDeath(NSLOCTEXT("NURecovery", "InvalidAnchor", "부활 거점 액터가 없거나 중복되었습니다. 골드는 차감하지 않았습니다."));
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();

	// 최초 시도에만 로딩을 요청한다. 이후 호출은 준비 상태와 제한 시간을 확인한다.
	if (DestinationWaitStarted < 0.0)
	{
		DestinationWaitStarted = Now;
		bool bAlreadyRequested = false;
		const FString Package = Point.Map.ToSoftObjectPath().GetLongPackageName();
		for (ULevelStreaming* Level : GetWorld()->GetStreamingLevels())
		{
			if (IsValid(Level) && Level->LevelTransform.Equals(Point.LevelTransform)
				&& (UWorld::RemovePIEPrefix(Level->GetWorldAssetPackageName()) == Package || Level->PackageNameToLoad.ToString() == Package))
			{
				Level->SetShouldBeLoaded(true);
				Level->SetShouldBeVisible(true);
				bAlreadyRequested = true;
				break;
			}
		}
		if (!bAlreadyRequested)
		{
			bool bAccepted = false;
			ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(GetWorld(), Point.Map, Point.LevelTransform, bAccepted);
			if (!bAccepted)
			{
				FailPendingDeath(NSLOCTEXT("NURecovery", "LoadRejected", "부활 거점 로딩에 실패했습니다. 골드는 차감하지 않았습니다."));
				return false;
			}
		}
	}
	const float Timeout = GetDefault<UNURecoverySettings>()->DestinationLoadTimeoutSeconds;
	if (!FMath::IsFinite(Timeout) || Timeout < 1.0f || Now - DestinationWaitStarted >= Timeout)
	{
		FailPendingDeath(NSLOCTEXT("NURecovery", "LoadTimeout", "부활 거점 로딩 시간이 초과됐습니다. 다시 시도할 수 있습니다."));
		return false;
	}

	// 로딩 완료 후 이어갈 처리는 서버 부활과 로컬 부활 경로에 맞춰 예약한다.
	if (IsUsingExternalServer()) { GetWorld()->GetTimerManager().SetTimer(ServerRecoveryTimer, this, &ThisClass::ProcessServerRespawn, 0.1f, false); }
	else { GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &ThisClass::ProcessPendingDeath, 0.1f, false); }
	return false;
}

void UNURecoveryComponent::FailPendingDeath(const FText& Message)
{
	DestinationWaitStarted = -1.0;
	ServerRespawnFieldEntryStarted = -1.0;
	ServerRespawnAreaId = 0;
	ServerRespawnLocationId = 0;
	bServerRespawnTransitionActive = false;
	bServerRespawnFieldEntryPending = false;
	GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
	GetWorld()->GetTimerManager().ClearTimer(ServerRecoveryTimer);
	if (UFieldSyncSubsystem* Sync = GetWorld()->GetSubsystem<UFieldSyncSubsystem>()) { Sync->ResumePositionReporting(this, false); }

	Notify(Message);
	if (const ANUPlayerState* PS = GetPlayerState())
	{
		if (ANUPlayerController* PC = Cast<ANUPlayerController>(PS->GetOwner())) { PC->ClientRecoveryFade(false); }
	}
}
