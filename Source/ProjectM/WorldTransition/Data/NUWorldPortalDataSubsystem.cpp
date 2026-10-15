#include "WorldTransition/Data/NUWorldPortalDataSubsystem.h"
#include "WorldTransition/Data/NUWorldTransitionSettings.h"
#include "WorldLocation/Data/NUWorldDataSettings.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUWorldPortalData, Log, All);

void UNUWorldPortalDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadData();
}

void UNUWorldPortalDataSubsystem::Deinitialize()
{
	ResetData();
	Super::Deinitialize();
}

void UNUWorldPortalDataSubsystem::ResetData()
{
	for (UDataTable* Table : { PortalTable.Get(), AreaTable.Get(), LocationTable.Get() })
	{
		if (IsValid(Table))
		{
			Table->OnDataTableChanged().RemoveAll(this);
		}
	}
	EntryBindings.Reset();
	VariantMap.Reset();
	VariantBindings.Reset();
	PortalTable = nullptr;
	AreaTable = nullptr;
	LocationTable = nullptr;
	bDataReady = false;
}

bool UNUWorldPortalDataSubsystem::ReloadData()
{
	ResetData();
	const UNUWorldTransitionSettings* Settings = GetDefault<UNUWorldTransitionSettings>();
	if (Settings->PortalDefinitionTable.IsNull())
	{
		return false;
	}
	const UNUWorldDataSettings* WorldSettings = GetDefault<UNUWorldDataSettings>();
	PortalTable = Settings->PortalDefinitionTable.LoadSynchronous();
	AreaTable = WorldSettings->AreaDefinitionTable.LoadSynchronous();
	LocationTable = WorldSettings->LocationDefinitionTable.LoadSynchronous();
	TSet<UDataTable*> Tables = { PortalTable.Get(), AreaTable.Get(), LocationTable.Get() };
	for (UDataTable* Table : Tables)
	{
		if (IsValid(Table))
		{
			Table->OnDataTableChanged().AddUObject(this, &ThisClass::HandleTableChanged);
		}
	}
	HandleTableChanged();
	return bDataReady;
}

void UNUWorldPortalDataSubsystem::HandleTableChanged()
{
	TArray<FString> Errors;
	const bool bPortalsValid = ValidateTable(PortalTable, AreaTable, LocationTable, Errors);
	const bool bEntriesValid = ReadEntryAssets(GetDefault<UNUWorldTransitionSettings>()->PortalEntryAssets, EntryBindings, Errors);
	VariantMap.Reset();
	bDataReady = bPortalsValid && bEntriesValid;
	for (const FString& Error : Errors)
	{
		UE_LOG(LogNUWorldPortalData, Warning, TEXT("%s"), *Error);
	}
}

bool UNUWorldPortalDataSubsystem::IsDataReady() const
{
	return bDataReady;
}

const TArray<FNUWorldPortalEntryBinding>& UNUWorldPortalDataSubsystem::GetEntryBindings() const
{
	const UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	const FString Map = World != nullptr ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : FString();
	if (!VariantMap.IsSet() || VariantMap.GetValue() != Map)
	{
		VariantMap = Map;
		VariantBindings = EntryBindings;
		ApplyMapVariant(Map, VariantBindings);
	}
	return VariantBindings;
}

void UNUWorldPortalDataSubsystem::ApplyMapVariant(const FString& MapPackage, TArray<FNUWorldPortalEntryBinding>& InOutBindings)
{
	const auto IsMap = [&MapPackage](const TSoftObjectPtr<UWorld>& Level)
	{
		return !Level.IsNull() && Level.ToSoftObjectPath().GetLongPackageName() == MapPackage;
	};
	for (const FNUWorldMapVariant& Variant : GetDefault<UNUWorldTransitionSettings>()->MapVariants)
	{
		bool bApplies = IsMap(Variant.PersistentLevel);
		for (const TPair<TSoftObjectPtr<UWorld>, TSoftObjectPtr<UWorld>>& Pair : Variant.Levels)
		{
			bApplies |= IsMap(Pair.Value);
		}
		if (!bApplies)
		{
			continue;
		}
		for (FNUWorldPortalEntryBinding& Binding : InOutBindings)
		{
			for (FNUWorldDestination* Destination : { &Binding.Destination, &Binding.ReturnDestination })
			{
				if (const TSoftObjectPtr<UWorld>* Replacement = Variant.Levels.Find(Destination->Level))
				{
					Destination->Level = *Replacement;
				}
			}
		}
		return;
	}
}

bool UNUWorldPortalDataSubsystem::ReadEntryAssets(const TArray<TSoftObjectPtr<UNUWorldPortalEntryAsset>>& Assets, TArray<FNUWorldPortalEntryBinding>& OutBindings, TArray<FString>& OutErrors)
{
	OutBindings.Reset();
	const int32 InitialErrorCount = OutErrors.Num();
	if (Assets.IsEmpty()) { OutErrors.Add(TEXT("PortalEntryAssets에 장소 에셋을 등록하십시오.")); }
	TSet<FString> Keys;
	for (const TSoftObjectPtr<UNUWorldPortalEntryAsset>& Reference : Assets)
	{
		const UNUWorldPortalEntryAsset* Asset = Reference.LoadSynchronous();
		if (!IsValid(Asset))
		{
			OutErrors.Add(FString::Printf(TEXT("포탈 장소 에셋을 읽을 수 없습니다: %s"), *Reference.ToString()));
			continue;
		}
		const FNUWorldPortalEntryBinding& Binding = Asset->Binding;
		const FString Key = FString::Printf(TEXT("%d/%d/%s"), Binding.AreaId, Binding.LocationId, *Binding.Destination.EntryPointId.ToString());
		const auto ValidTransform = [](const FTransform& Transform)
		{
			return !Transform.ContainsNaN() && Transform.IsValid() && Transform.GetScale3D().GetMin() > 0.0;
		};
		if (Binding.AreaId <= 0 || Binding.LocationId <= 0 || !Binding.Destination.IsConfigured() || Keys.Contains(Key)
			|| !ValidTransform(Binding.Destination.LevelTransform) || !ValidTransform(Binding.ReturnDestination.LevelTransform)
			|| Binding.ReturnLocationId < 0 || ((Binding.ReturnLocationId > 0) != Binding.ReturnDestination.IsConfigured()))
		{
			OutErrors.Add(FString::Printf(TEXT("PortalEntry Asset=%s: ID, 중복 배치, Transform 또는 복귀 설정을 확인하십시오."), *Asset->GetPathName()));
		}
		Keys.Add(Key);
		OutBindings.Add(Binding);
	}
	if (OutErrors.Num() != InitialErrorCount)
	{
		OutBindings.Reset();
		return false;
	}
	return true;
}

const FNUWorldPortalDefinition* UNUWorldPortalDataSubsystem::FindPortalDefinition(FName PortalId) const
{
	return bDataReady && IsValid(PortalTable) ? PortalTable->FindRow<FNUWorldPortalDefinition>(PortalId, TEXT("FindPortalDefinition"), false) : nullptr;
}

bool UNUWorldPortalDataSubsystem::ResolvePortal(FName PortalId, ENUWorldPortalSide PlacementSide, FNUWorldPortalRoute& OutRoute, FString& OutError) const
{
	OutRoute = {};
	const FNUWorldPortalDefinition* Definition = FindPortalDefinition(PortalId);
	if (Definition == nullptr)
	{
		OutError = FString::Printf(TEXT("포탈 테이블이 준비되지 않았거나 PortalId=%s가 없습니다."), *PortalId.ToString());
		return false;
	}
	if (PlacementSide != ENUWorldPortalSide::Source && PlacementSide != ENUWorldPortalSide::Destination)
	{
		OutError = TEXT("포탈의 배치된 쪽 설정이 유효하지 않습니다.");
		return false;
	}
	return ResolveRoute(*Definition, PlacementSide == ENUWorldPortalSide::Destination, GetEntryBindings(), OutRoute, OutError);
}

bool UNUWorldPortalDataSubsystem::ValidateTable(const UDataTable* Portals, const UDataTable* Areas, const UDataTable* Locations, TArray<FString>& OutErrors)
{
	UNUWorldDataSubsystem::ValidateTables(Areas, Locations, OutErrors);
	if (!IsValid(Portals) || Portals->GetRowStruct() != FNUWorldPortalDefinition::StaticStruct())
	{
		OutErrors.Add(TEXT("포탈 DataTable이 없거나 RowStruct가 FNUWorldPortalDefinition이 아닙니다."));
	}
	if (!OutErrors.IsEmpty())
	{
		return false;
	}
	if (Portals->GetRowNames().IsEmpty())
	{
		OutErrors.Add(TEXT("포탈 테이블에 행이 없습니다."));
	}
	TSet<FName> Keys;
	for (FName RowName : Portals->GetRowNames())
	{
		const FNUWorldPortalDefinition* Row = Portals->FindRow<FNUWorldPortalDefinition>(RowName, TEXT("ValidatePortalTable"), false);
		FString Error;
		if (Row == nullptr || !Row->ValidateRow(RowName, Error))
		{
			OutErrors.Add(FString::Printf(TEXT("Portal Row=%s: %s"), *RowName.ToString(), *Error));
			continue;
		}
		if (Keys.Contains(Row->PortalKey))
		{
			OutErrors.Add(FString::Printf(TEXT("PortalId=%d: 중복 PortalKey입니다."), Row->PortalId));
		}
		Keys.Add(Row->PortalKey);
		const FNULocationDefinition* Source = Locations->FindRow<FNULocationDefinition>(FName(*LexToString(Row->SourceLocationId)), TEXT("ValidatePortalTable"), false);
		const FNULocationDefinition* Target = Locations->FindRow<FNULocationDefinition>(FName(*LexToString(Row->DestinationLocationId)), TEXT("ValidatePortalTable"), false);
		if (Source == nullptr || Source->AreaId != Row->SourceAreaId || Target == nullptr || Target->AreaId != Row->DestinationAreaId)
		{
			OutErrors.Add(FString::Printf(TEXT("PortalId=%d: 양 끝의 장소가 없거나 소속 지역이 다릅니다."), Row->PortalId));
		}
	}
	return OutErrors.IsEmpty();
}

bool UNUWorldPortalDataSubsystem::ResolveRoute(const FNUWorldPortalDefinition& Definition, bool bReverseDirection, const TArray<FNUWorldPortalEntryBinding>& Bindings, FNUWorldPortalRoute& OutRoute, FString& OutError)
{
	OutRoute = {};
	OutError.Reset();
	if (!Definition.ValidateRow(FName(*LexToString(Definition.PortalId)), OutError))
	{
		return false;
	}
	if (bReverseDirection && !Definition.bTwoWay)
	{
		OutError = TEXT("단방향 포탈을 반대 방향으로 사용할 수 없습니다.");
		return false;
	}
	const FNUWorldPortalEntryBinding* Source = nullptr;
	const FNUWorldPortalEntryBinding* Target = nullptr;
	for (const FNUWorldPortalEntryBinding& Binding : Bindings)
	{
		const bool bSource = Binding.AreaId == Definition.SourceAreaId && Binding.LocationId == Definition.SourceLocationId && Binding.Destination.EntryPointId == Definition.SourceEntryPointId;
		const bool bTarget = Binding.AreaId == Definition.DestinationAreaId && Binding.LocationId == Definition.DestinationLocationId && Binding.Destination.EntryPointId == Definition.DestinationEntryPointId;
		if ((bSource && Source != nullptr) || (bTarget && Target != nullptr))
		{
			OutError = TEXT("지역/장소/EntryPoint 조합에 중복 배치 매핑이 있습니다.");
			return false;
		}
		if (bSource || bTarget)
		{
			const FTransform& Transform = Binding.Destination.LevelTransform;
			if (Transform.ContainsNaN() || !Transform.IsValid() || Transform.GetScale3D().GetMin() <= 0.0)
			{
				OutError = TEXT("포탈 배치 Transform이 유효하지 않습니다.");
				return false;
			}
		}
		if (bSource) { Source = &Binding; }
		if (bTarget) { Target = &Binding; }
	}
	if (Source == nullptr || Target == nullptr)
	{
		OutError = TEXT("양 끝의 지역/장소/EntryPoint 배치 매핑이 필요합니다.");
		return false;
	}
	OutRoute.SourceSide = bReverseDirection ? 1 : 0;
	OutRoute.Definition = Definition;
	OutRoute.Source = bReverseDirection ? *Target : *Source;
	OutRoute.Target = bReverseDirection ? *Source : *Target;
	return true;
}
