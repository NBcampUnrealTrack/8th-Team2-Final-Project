#include "WorldTransition/World/NUWorldStreamingSubsystem.h"

#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

bool UNUWorldStreamingSubsystem::AcquireSpace(const FNUWorldDestination& Destination, FNUWorldSpaceHandle& OutHandle, ENUWorldTransitionFailure& OutFailure)
{
	OutHandle = {};
	OutFailure = ENUWorldTransitionFailure::None;

	UWorld* World = GetWorld();
	if (!IsValid(World) || Destination.LevelTransform.ContainsNaN() || !Destination.LevelTransform.IsValid())
	{
		OutFailure = ENUWorldTransitionFailure::InvalidRequest;
		return false;
	}

	// 월드의 기본 레벨은 새로 로드하지 않는다.
	const FString PackageName = Destination.Level.ToSoftObjectPath().GetLongPackageName();
	if (Destination.IsPersistentLevelDestination() || PackageName == UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()))
	{
		return true;
	}

	// 같은 레벨과 배치는 재사용하고 참조 횟수만 늘린다.
	for (TPair<FGuid, FNUWorldStreamingRecord>& Pair : Spaces)
	{
		FNUWorldStreamingRecord& Record = Pair.Value;
		if (Record.Destination.Level == Destination.Level && Record.Destination.LevelTransform.Equals(Destination.LevelTransform) && IsValid(Record.StreamingLevel) && !Record.StreamingLevel->GetIsRequestingUnloadAndRemoval())
		{
			++Record.Users;
			OutHandle.Id = Pair.Key;
			return true;
		}
	}

	FNUWorldStreamingRecord Record;
	Record.Destination = Destination;
	Record.Users = 1;
	Record.RequestedAt = FPlatformTime::Seconds();

	// 다른 기능이 로드한 레벨은 빌려 쓰고 해제하지 않는다.
	for (ULevelStreaming* Level : World->GetStreamingLevels())
	{
		if (IsValid(Level) && !Level->GetIsRequestingUnloadAndRemoval() && Level->ShouldBeLoaded() && Level->ShouldBeVisible() && Level->LevelTransform.Equals(Destination.LevelTransform)
			&& (UWorld::RemovePIEPrefix(Level->GetWorldAssetPackageName()) == PackageName || Level->PackageNameToLoad.ToString() == PackageName))
		{
			Record.StreamingLevel = Level;
			break;
		}
	}

	if (!IsValid(Record.StreamingLevel))
	{
		bool bAccepted = false;
		Record.StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(World, Destination.Level, Destination.LevelTransform, bAccepted);
		if (!bAccepted || !IsValid(Record.StreamingLevel))
		{
			OutFailure = ENUWorldTransitionFailure::LoadRequestRejected;
			return false;
		}
		Record.bOwned = true;
	}
	OutHandle.Id = FGuid::NewGuid();
	Spaces.Add(OutHandle.Id, MoveTemp(Record));
	return true;
}

ENUWorldSpacePreparation UNUWorldStreamingSubsystem::GetPreparation(const FNUWorldSpaceHandle& Handle, ENUWorldTransitionFailure& OutFailure) const
{
	OutFailure = ENUWorldTransitionFailure::None;
	if (Handle.IsPersistent())
	{
		return ENUWorldSpacePreparation::Ready;
	}
	const FNUWorldStreamingRecord* Record = Spaces.Find(Handle.Id);
	if (Record == nullptr || !IsValid(Record->StreamingLevel) || Record->StreamingLevel->GetIsRequestingUnloadAndRemoval())
	{
		OutFailure = ENUWorldTransitionFailure::UnexpectedUnload;
		return ENUWorldSpacePreparation::Failed;
	}

	// 로드와 표시가 모두 완료되어야 이동할 수 있다.
	if (Record->StreamingLevel->IsLevelLoaded() && Record->StreamingLevel->IsLevelVisible())
	{
		return ENUWorldSpacePreparation::Ready;
	}
	if (FPlatformTime::Seconds() - Record->RequestedAt >= LoadTimeoutSeconds)
	{
		OutFailure = ENUWorldTransitionFailure::LoadTimeout;
		return ENUWorldSpacePreparation::Failed;
	}
	return ENUWorldSpacePreparation::Pending;
}

ULevel* UNUWorldStreamingSubsystem::GetPreparedLevel(const FNUWorldSpaceHandle& Handle) const
{
	ENUWorldTransitionFailure Failure;
	if (GetPreparation(Handle, Failure) != ENUWorldSpacePreparation::Ready)
	{
		return nullptr;
	}
	return Handle.IsPersistent() ? GetWorld()->PersistentLevel.Get() : Spaces.FindChecked(Handle.Id).StreamingLevel->GetLoadedLevel();
}

ULevelStreaming* UNUWorldStreamingSubsystem::GetStreamingLevel(const FNUWorldSpaceHandle& Handle) const
{
	const FNUWorldStreamingRecord* Record = Spaces.Find(Handle.Id);
	return Record != nullptr ? Record->StreamingLevel.Get() : nullptr;
}

void UNUWorldStreamingSubsystem::ReleaseSpace(FNUWorldSpaceHandle& Handle)
{
	FNUWorldStreamingRecord* Record = Spaces.Find(Handle.Id);

	// 마지막 참조가 해제되면 공간도 정리한다.
	if (Record != nullptr && --Record->Users == 0)
	{
		RemoveSpace(Handle.Id);
	}
	Handle = {};
}

void UNUWorldStreamingSubsystem::RemoveSpace(const FGuid& Id)
{
	FNUWorldStreamingRecord* Record = Spaces.Find(Id);
	if (Record == nullptr || Record->Users > 0)
	{
		return;
	}

	if (Record->bOwned && IsValid(Record->StreamingLevel))
	{
		Record->StreamingLevel->SetIsRequestingUnloadAndRemoval(true);
	}
	Spaces.Remove(Id);
}

// 직접 로드한 레벨 정리
void UNUWorldStreamingSubsystem::Deinitialize()
{
	for (TPair<FGuid, FNUWorldStreamingRecord>& Pair : Spaces)
	{
		if (Pair.Value.bOwned && IsValid(Pair.Value.StreamingLevel))
		{
			Pair.Value.StreamingLevel->SetIsRequestingUnloadAndRemoval(true);
		}
	}
	Spaces.Reset();
	Super::Deinitialize();
}
