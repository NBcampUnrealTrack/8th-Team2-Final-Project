#include "WorldLocation/Data/NUWorldDataSubsystem.h"

#include "WorldLocation/Data/NUWorldDataSettings.h"

#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUWorldData, Log, All);

void UNUWorldDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadData();
}

void UNUWorldDataSubsystem::Deinitialize()
{
	ResetData();
	Super::Deinitialize();
}

const FNUAreaDefinition* UNUWorldDataSubsystem::FindAreaDefinition(int32 AreaId) const
{
	if (!bDataReady || !IsValid(AreaDefinitionTable))
	{
		return nullptr;
	}

	const FName* RowName = AreaIdToRowName.Find(AreaId);
	return RowName != nullptr
		? AreaDefinitionTable->FindRow<FNUAreaDefinition>(*RowName, TEXT("UNUWorldDataSubsystem::FindAreaDefinition"), false)
		: nullptr;
}

const FNULocationDefinition* UNUWorldDataSubsystem::FindLocationDefinition(int32 LocationId) const
{
	if (!bDataReady || !IsValid(LocationDefinitionTable))
	{
		return nullptr;
	}

	const FName* RowName = LocationIdToRowName.Find(LocationId);
	return RowName != nullptr
		? LocationDefinitionTable->FindRow<FNULocationDefinition>(*RowName, TEXT("UNUWorldDataSubsystem::FindLocationDefinition"), false)
		: nullptr;
}

bool UNUWorldDataSubsystem::TryGetAreaDefinition(int32 AreaId, FNUAreaDefinition& OutDefinition) const
{
	const FNUAreaDefinition* Definition = FindAreaDefinition(AreaId);

	if (Definition == nullptr)
	{
		OutDefinition = FNUAreaDefinition();
		return false;
	}

	OutDefinition = *Definition;
	return true;
}

bool UNUWorldDataSubsystem::TryGetLocationDefinition(int32 LocationId, FNULocationDefinition& OutDefinition) const
{
	const FNULocationDefinition* Definition = FindLocationDefinition(LocationId);

	if (Definition == nullptr)
	{
		OutDefinition = FNULocationDefinition();
		return false;
	}

	OutDefinition = *Definition;
	return true;
}

void UNUWorldDataSubsystem::GetLocationIdsInArea(int32 AreaId, TArray<int32>& OutLocationIds) const
{
	OutLocationIds.Reset();
	const TArray<int32>* LocationIds = bDataReady ? AreaIdToLocationIds.Find(AreaId) : nullptr;

	if (LocationIds != nullptr)
	{
		OutLocationIds = *LocationIds;
	}
}

bool UNUWorldDataSubsystem::IsDataReady() const
{
	return bDataReady;
}

bool UNUWorldDataSubsystem::ReloadData()
{
	ResetData();

	const UNUWorldDataSettings* Settings = GetDefault<UNUWorldDataSettings>();

	if (!IsValid(Settings) || Settings->AreaDefinitionTable.IsNull() || Settings->LocationDefinitionTable.IsNull())
	{
		UE_LOG(LogNUWorldData, Warning, TEXT("지역과 장소 DataTable을 모두 지정해야 합니다."));
		return false;
	}

	AreaDefinitionTable = Settings->AreaDefinitionTable.LoadSynchronous();
	LocationDefinitionTable = Settings->LocationDefinitionTable.LoadSynchronous();

	// 재임포트나 행 수정 시 인덱스와 검증 결과도 갱신한다.

	if (IsValid(AreaDefinitionTable))
	{
		AreaDefinitionTable->OnDataTableChanged().AddUObject(this, &ThisClass::HandleDefinitionTableChanged);
	}

	if (IsValid(LocationDefinitionTable) && LocationDefinitionTable != AreaDefinitionTable)
	{
		LocationDefinitionTable->OnDataTableChanged().AddUObject(this, &ThisClass::HandleDefinitionTableChanged);
	}

	HandleDefinitionTableChanged();
	return bDataReady;
}

void UNUWorldDataSubsystem::ResetData()
{
	if (IsValid(AreaDefinitionTable))
	{
		AreaDefinitionTable->OnDataTableChanged().RemoveAll(this);
	}

	if (IsValid(LocationDefinitionTable))
	{
		LocationDefinitionTable->OnDataTableChanged().RemoveAll(this);
	}

	bDataReady = false;
	AreaIdToRowName.Reset();
	LocationIdToRowName.Reset();
	AreaIdToLocationIds.Reset();
	AreaDefinitionTable = nullptr;
	LocationDefinitionTable = nullptr;
}

void UNUWorldDataSubsystem::HandleDefinitionTableChanged()
{
	// 잘못된 정의가 일부만 노출되지 않도록 두 테이블을 함께 검증한다.
	bDataReady = false;
	AreaIdToRowName.Reset();
	LocationIdToRowName.Reset();
	AreaIdToLocationIds.Reset();

	TArray<FString> Errors;

	if (!ValidateTables(AreaDefinitionTable, LocationDefinitionTable, Errors))
	{
		for (const FString& Error : Errors)
		{
			UE_LOG(LogNUWorldData, Warning, TEXT("지역/장소 데이터 검증 실패: %s"), *Error);
		}

		return;
	}

	BuildDefinitionIndices();
	bDataReady = true;
	UE_LOG(LogNUWorldData, Log, TEXT("지역/장소 데이터 준비 완료. Areas=%d Locations=%d"), AreaIdToRowName.Num(), LocationIdToRowName.Num());
}

void UNUWorldDataSubsystem::BuildDefinitionIndices()
{
	for (const FName RowName : AreaDefinitionTable->GetRowNames())
	{
		const FNUAreaDefinition* Row = AreaDefinitionTable->FindRow<FNUAreaDefinition>(RowName, TEXT("BuildDefinitionIndices"), false);
		AreaIdToRowName.Add(Row->AreaId, RowName);
	}

	for (const FName RowName : LocationDefinitionTable->GetRowNames())
	{
		const FNULocationDefinition* Row = LocationDefinitionTable->FindRow<FNULocationDefinition>(RowName, TEXT("BuildDefinitionIndices"), false);
		LocationIdToRowName.Add(Row->LocationId, RowName);
		AreaIdToLocationIds.FindOrAdd(Row->AreaId).Add(Row->LocationId);
	}

	for (TPair<int32, TArray<int32>>& Pair : AreaIdToLocationIds)
	{
		Pair.Value.Sort();
	}
}

bool UNUWorldDataSubsystem::ValidateTables(const UDataTable* AreaTable, const UDataTable* LocationTable, TArray<FString>& OutErrors)
{
	OutErrors.Reset();

	if (!IsValid(AreaTable) || AreaTable->GetRowStruct() != FNUAreaDefinition::StaticStruct())
	{
		OutErrors.Add(TEXT("지역 DataTable이 없거나 RowStruct가 FNUAreaDefinition이 아닙니다."));
	}

	if (!IsValid(LocationTable) || LocationTable->GetRowStruct() != FNULocationDefinition::StaticStruct())
	{
		OutErrors.Add(TEXT("장소 DataTable이 없거나 RowStruct가 FNULocationDefinition이 아닙니다."));
	}

	if (!OutErrors.IsEmpty())
	{
		return false;
	}

	TMap<int32, const FNUAreaDefinition*> Areas;
	TMap<int32, const FNULocationDefinition*> Locations;
	TSet<FName> AreaKeys;
	TSet<FName> LocationKeys;

	for (const FName RowName : AreaTable->GetRowNames())
	{
		const FNUAreaDefinition* Row = AreaTable->FindRow<FNUAreaDefinition>(RowName, TEXT("ValidateTables"), false);
		FString Error;

		if (Row == nullptr || !Row->ValidateRow(RowName, Error))
		{
			OutErrors.Add(FString::Printf(TEXT("Area Row=%s: %s"), *RowName.ToString(), *Error));
			continue;
		}

		if (Areas.Contains(Row->AreaId) || AreaKeys.Contains(Row->AreaKey))
		{
			OutErrors.Add(FString::Printf(TEXT("Area Row=%s: 중복 AreaId 또는 AreaKey입니다."), *RowName.ToString()));
		}

		Areas.Add(Row->AreaId, Row);
		AreaKeys.Add(Row->AreaKey);
	}

	for (const FName RowName : LocationTable->GetRowNames())
	{
		const FNULocationDefinition* Row = LocationTable->FindRow<FNULocationDefinition>(RowName, TEXT("ValidateTables"), false);
		FString Error;

		if (Row == nullptr || !Row->ValidateRow(RowName, Error))
		{
			OutErrors.Add(FString::Printf(TEXT("Location Row=%s: %s"), *RowName.ToString(), *Error));
			continue;
		}

		if (Locations.Contains(Row->LocationId) || LocationKeys.Contains(Row->LocationKey))
		{
			OutErrors.Add(FString::Printf(TEXT("Location Row=%s: 중복 LocationId 또는 LocationKey입니다."), *RowName.ToString()));
		}

		Locations.Add(Row->LocationId, Row);
		LocationKeys.Add(Row->LocationKey);
	}

	if (Areas.IsEmpty() || Locations.IsEmpty())
	{
		OutErrors.Add(TEXT("지역과 장소 테이블에는 각각 유효한 행이 하나 이상 필요합니다."));
	}

	if (!OutErrors.IsEmpty())
	{
		return false;
	}

	for (const TPair<int32, const FNUAreaDefinition*>& Pair : Areas)
	{
		const FNUAreaDefinition& Area = *Pair.Value;

		if (Area.ParentAreaId != 0 && !Areas.Contains(Area.ParentAreaId))
		{
			OutErrors.Add(FString::Printf(TEXT("AreaId=%d: 상위 지역 %d가 없습니다."), Area.AreaId, Area.ParentAreaId));
		}

		if (Area.RespawnLocationId != 0 && !Locations.Contains(Area.RespawnLocationId))
		{
			OutErrors.Add(FString::Printf(TEXT("AreaId=%d: 부활 장소 %d가 없습니다."), Area.AreaId, Area.RespawnLocationId));
		}

		for (int32 ConnectedAreaId : Area.ConnectedAreas)
		{
			if (!Areas.Contains(ConnectedAreaId))
			{
				OutErrors.Add(FString::Printf(TEXT("AreaId=%d: 연결 지역 %d가 없습니다."), Area.AreaId, ConnectedAreaId));
			}
		}

		TSet<int32> VisitedIds;
		const FNUAreaDefinition* Current = &Area;

		while (Current != nullptr)
		{
			if (VisitedIds.Contains(Current->AreaId))
			{
				OutErrors.Add(FString::Printf(TEXT("AreaId=%d: 상위 지역 관계에 순환 참조가 있습니다."), Area.AreaId));
				break;
			}

			VisitedIds.Add(Current->AreaId);
			Current = Areas.FindRef(Current->ParentAreaId);
		}
	}

	// 장소는 소속 지역의 존재 여부를 검사한다.
	for (const TPair<int32, const FNULocationDefinition*>& Pair : Locations)
	{
		const FNULocationDefinition& Location = *Pair.Value;

		if (!Areas.Contains(Location.AreaId))
		{
			OutErrors.Add(FString::Printf(TEXT("LocationId=%d: 소속 지역 %d가 없습니다."), Location.LocationId, Location.AreaId));
		}
	}

	return OutErrors.IsEmpty();
}
