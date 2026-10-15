#include "Recovery/Data/NURecoverySettings.h"

#include "Recovery/Data/NURecoveryTypes.h"

#include "Engine/DataTable.h"

bool UNURecoverySettings::TryGetRecoveryCostRow(int32 Level, FNURecoveryCostRow& OutRow) const
{
	if (Level < 1 || RecoveryCostTable.IsNull())
	{
		return false;
	}

	const UDataTable* Table = RecoveryCostTable.LoadSynchronous();
	if (!IsValid(Table) || Table->GetRowStruct() != FNURecoveryCostRow::StaticStruct())
	{
		return false;
	}

	const FNURecoveryCostRow* Row = Table->FindRow<FNURecoveryCostRow>(FName(*FString::FromInt(Level)), TEXT("UNURecoverySettings::TryGetRecoveryCostRow"), false);
	if (Row == nullptr || Row->Level != Level || Row->RecoveryCost < 0 || static_cast<int64>(Row->ResurrectionGoldLoss) != static_cast<int64>(Row->RecoveryCost) * 3)
	{
		return false;
	}

	OutRow = *Row;
	return true;
}

bool UNURecoverySettings::ValidateRecoveryCostTable(FString& OutError) const
{
	if (RecoveryCostTable.IsNull())
	{
		OutError = TEXT("회복 비용 테이블 경로가 비어 있습니다.");
		return false;
	}

	const UDataTable* Table = RecoveryCostTable.LoadSynchronous();
	if (!IsValid(Table))
	{
		OutError = TEXT("회복 비용 테이블을 불러오지 못했습니다.");
		return false;
	}
	if (Table->GetRowStruct() != FNURecoveryCostRow::StaticStruct())
	{
		OutError = TEXT("회복 비용 테이블의 RowStruct가 FNURecoveryCostRow가 아닙니다.");
		return false;
	}

	if (Table->GetRowMap().IsEmpty())
	{
		OutError = TEXT("회복 비용 테이블에 행이 없습니다.");
		return false;
	}

	int32 MaxLevel = 0;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		int32 RowLevel = 0;
		if (!LexTryParseString(RowLevel, *Pair.Key.ToString()) || RowLevel < 1)
		{
			OutError = FString::Printf(TEXT("회복 비용 테이블의 행 이름 '%s'은 1 이상의 레벨 숫자여야 합니다."), *Pair.Key.ToString());
			return false;
		}
		MaxLevel = FMath::Max(MaxLevel, RowLevel);
	}

	for (int32 Level = 1; Level <= MaxLevel; ++Level)
	{
		FNURecoveryCostRow Row;
		if (!TryGetRecoveryCostRow(Level, Row))
		{
			OutError = FString::Printf(TEXT("회복 비용 테이블의 Lv%d 행이 없거나 값이 잘못되었습니다."), Level);
			return false;
		}
	}

	return true;
}


bool UNURecoverySettings::TryGetRecoveryPoint(int32 LocationId, FNURecoveryPointDefinition& OutPoint) const
{
	const FNURecoveryPointDefinition* Found = nullptr;
	for (const FNURecoveryPointDefinition& Point : RecoveryPoints)
	{
		if (Point.LocationId != LocationId)
		{
			continue;
		}
		if (Found != nullptr || LocationId <= 0 || Point.Map.IsNull() || Point.LevelTransform.ContainsNaN()
			|| !FMath::IsFinite(Point.RegistrationRadius) || Point.RegistrationRadius < 0.0f)
		{
			return false;
		}
		Found = &Point;
	}
	if (Found == nullptr)
	{
		return false;
	}
	OutPoint = *Found;
	return true;
}
