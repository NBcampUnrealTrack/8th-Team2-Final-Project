#include "Naia/Data/NUNaiaDataSubsystem.h"

#include "Naia/Data/NUNaiaDataSettings.h"
#include "Naia/Data/NUNaiaDefinition.h"

#include "Battle/Data/NUStatusEffectDefinition.h"
#include "Battle/Enemy/NUEnemyPatternAsset.h"
#include "Engine/DataTable.h"

void UNUNaiaDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 프로젝트 세팅 설정
    const UNUNaiaDataSettings* Settings = GetDefault<UNUNaiaDataSettings>();
    if (!IsValid(Settings))
    {
        return;
    }

    // Naia 테이블 할당
    if (!Settings->NaiaDefinitionTable.IsNull())
    {
        UDataTable* LoadedTable = Settings->NaiaDefinitionTable.LoadSynchronous();

        // Naia 테이블인지 확인
        if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUNaiaDefinition::StaticStruct())
        {
            NaiaDefinitionTable = LoadedTable;

            UE_LOG(LogTemp, Log, TEXT("NaiaDataSubsystem: Loaded Table=%s, Rows=%d"),
                *NaiaDefinitionTable->GetName(),
                NaiaDefinitionTable->GetRowMap().Num()
            );
        }
    }

    // 적 행동 패턴 등록 에셋 할당
    // 테이블과 독립적으로 처리함, 비어 있으면 전투는 기본 공격으로 돌아감
    if (!Settings->EnemyPatternRegistry.IsNull())
    {
        EnemyPatternRegistry = Settings->EnemyPatternRegistry.LoadSynchronous();

        if (IsValid(EnemyPatternRegistry))
        {
            UE_LOG(LogTemp, Log, TEXT("NaiaDataSubsystem: Loaded EnemyPatternRegistry=%s, Patterns=%d"),
                *EnemyPatternRegistry->GetName(),
                EnemyPatternRegistry->Patterns.Num()
            );
        }
    }
}

void UNUNaiaDataSubsystem::Deinitialize()
{
    NaiaDefinitionTable = nullptr;
    EnemyPatternRegistry = nullptr;

    Super::Deinitialize();
}

const FNUNaiaDefinition* UNUNaiaDataSubsystem::FindDefinition(FName NaiaId) const
{
    if (!IsValid(NaiaDefinitionTable) || NaiaId.IsNone())
    {
        return nullptr;
    }

    return NaiaDefinitionTable->FindRow<FNUNaiaDefinition>(
        NaiaId, 
        TEXT("NaiaDataSubsystem::FindDefinition"),
        false
        );
}

const FNUNaiaDefinition* UNUNaiaDataSubsystem::FindDefinitionById(int32 NaiaId) const
{
    if (NaiaId <= 0)
    {
        return nullptr;
    }
        
    return FindDefinition(FName(*FString::FromInt(NaiaId)));
}

TArray<const FNUNaiaDefinition*> UNUNaiaDataSubsystem::GetStarterDefinitions() const
{
    TArray<const FNUNaiaDefinition*> Result;
    if (!IsValid(NaiaDefinitionTable))
    {
        return Result;
    }

    TArray<FNUNaiaDefinition*> Rows;
    NaiaDefinitionTable->GetAllRows(TEXT("NaiaDataSubsystem::GetStarterDefinitions"), Rows);
    for (const FNUNaiaDefinition* Row : Rows)
    {
        if (Row != nullptr && Row->IsStarterSelectionCandidate())
        {
            Result.Add(Row);
        }
    }

    Result.Sort([](const FNUNaiaDefinition& A, const FNUNaiaDefinition& B)
    {
        return A.DataIndex < B.DataIndex;
    });
    return Result;
}

TArray<const FNUNaiaDefinition*> UNUNaiaDataSubsystem::GetAllDefinitions() const
{
    TArray<const FNUNaiaDefinition*> Result;
    if (!IsValid(NaiaDefinitionTable))
    {
        return Result;
    }

    TArray<FNUNaiaDefinition*> Rows;
    NaiaDefinitionTable->GetAllRows(TEXT("NaiaDataSubsystem::GetAllDefinitions"), Rows);
    for (const FNUNaiaDefinition* Row : Rows)
    {
        if (Row != nullptr)
        {
            Result.Add(Row);
        }
    }

    Result.Sort([](const FNUNaiaDefinition& A, const FNUNaiaDefinition& B)
    {
        return A.DataIndex < B.DataIndex;
    });
    return Result;
}

bool UNUNaiaDataSubsystem::HasDefinition(FName NaiaId) const
{
    return FindDefinition(NaiaId) != nullptr;
}

const UNUEnemyPatternAsset* UNUNaiaDataSubsystem::FindEnemyPattern(FName NaiaId) const
{
    if (!IsValid(EnemyPatternRegistry))
    {
        return nullptr;
    }

    return EnemyPatternRegistry->FindPattern(NaiaId);
}
