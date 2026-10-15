#include "Field/NPC/Data/NUNPCDataSubsystem.h"

#include "Field/NPC/Data/NUNPCDataSettings.h"
#include "Field/NPC/Data/NUNPCDefinition.h"

#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUNPCData, Log, All);

void UNUNPCDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadDefinitionTable();
	BuildNpcIdIndex();

	UE_LOG(LogNUNPCData, Log, TEXT("NPC 데이터 준비 완료. Definitions=%d"), NpcIdToRowName.Num());
}

void UNUNPCDataSubsystem::Deinitialize()
{
	NpcIdToRowName.Reset();
	NPCDefinitionTable = nullptr;

	Super::Deinitialize();
}

const FNUNPCDefinition* UNUNPCDataSubsystem::FindDefinition(int32 NpcId) const
{
	if (!IsValid(NPCDefinitionTable) || NpcId <= 0)
	{
		return nullptr;
	}

	const FName* RowName = NpcIdToRowName.Find(NpcId);
	return RowName != nullptr
		? NPCDefinitionTable->FindRow<FNUNPCDefinition>(*RowName, TEXT("UNUNPCDataSubsystem::FindDefinition"), false)
		: nullptr;
}

bool UNUNPCDataSubsystem::TryGetDefinition(int32 NpcId, FNUNPCDefinition& OutDefinition) const
{
	const FNUNPCDefinition* Definition = FindDefinition(NpcId);
	if (Definition == nullptr)
	{
		OutDefinition = FNUNPCDefinition();
		return false;
	}

	OutDefinition = *Definition;
	return true;
}

bool UNUNPCDataSubsystem::HasDefinition(int32 NpcId) const
{
	return FindDefinition(NpcId) != nullptr;
}

int32 UNUNPCDataSubsystem::GetDefinitionCount() const
{
	return NpcIdToRowName.Num();
}

void UNUNPCDataSubsystem::LoadDefinitionTable()
{
	const UNUNPCDataSettings* Settings = GetDefault<UNUNPCDataSettings>();
	if (!IsValid(Settings) || Settings->NPCDefinitionTable.IsNull())
	{
		UE_LOG(LogNUNPCData, Warning, TEXT("NPCDefinitionTable이 설정되지 않았습니다."));
		return;
	}

	UDataTable* LoadedTable = Settings->NPCDefinitionTable.LoadSynchronous();
	if (!IsValid(LoadedTable))
	{
		UE_LOG(LogNUNPCData, Warning, TEXT("NPCDefinitionTable 로드 실패. Path=%s"),
			*Settings->NPCDefinitionTable.ToString());
		return;
	}

	if (LoadedTable->GetRowStruct() != FNUNPCDefinition::StaticStruct())
	{
		UE_LOG(LogNUNPCData, Warning, TEXT("NPCDefinitionTable RowStruct 불일치. Table=%s Expected=%s Actual=%s"),
			*LoadedTable->GetName(),
			*FNUNPCDefinition::StaticStruct()->GetName(),
			*GetNameSafe(LoadedTable->GetRowStruct()));
		return;
	}

	NPCDefinitionTable = LoadedTable;
}

void UNUNPCDataSubsystem::BuildNpcIdIndex()
{
	NpcIdToRowName.Reset();

	if (!IsValid(NPCDefinitionTable))
	{
		return;
	}

	int32 RejectedCount = 0;
	for (const TPair<FName, uint8*>& Pair : NPCDefinitionTable->GetRowMap())
	{
		const FNUNPCDefinition* Row = reinterpret_cast<const FNUNPCDefinition*>(Pair.Value);
		if (Row == nullptr)
		{
			++RejectedCount;
			continue;
		}

		FString ValidationError;
		int32 NpcId = 0;
		if (!Row->ValidateRow(Pair.Key, ValidationError)
			|| !FNUNPCDefinition::TryParseNpcId(Pair.Key, NpcId))
		{
			UE_LOG(LogNUNPCData, Warning, TEXT("유효하지 않은 NPC 행입니다. Row=%s Error=%s"),
				*Pair.Key.ToString(),
				*ValidationError);
			++RejectedCount;
			continue;
		}

		if (NpcIdToRowName.Contains(NpcId))
		{
			UE_LOG(LogNUNPCData, Warning, TEXT("중복 NpcId입니다. NpcId=%d Row=%s"), NpcId, *Pair.Key.ToString());
			++RejectedCount;
			continue;
		}

		NpcIdToRowName.Add(NpcId, Pair.Key);
	}

	UE_LOG(LogNUNPCData, Log, TEXT("NPC DataTable 인덱스 생성 완료. Rows=%d Loaded=%d Rejected=%d"),
		NPCDefinitionTable->GetRowMap().Num(),
		NpcIdToRowName.Num(),
		RejectedCount);
}
