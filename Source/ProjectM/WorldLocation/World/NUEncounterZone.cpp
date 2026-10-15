// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldLocation/World/NUEncounterZone.h"

#include "Components/BoxComponent.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"

#include "Engine/GameInstance.h"

ANUEncounterZone::ANUEncounterZone()
{
 	PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    ZoneBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBounds"));
    SetRootComponent(ZoneBounds);

    // 충돌 관리
    ZoneBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ZoneBounds->SetCollisionObjectType(ECC_WorldDynamic);
    ZoneBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    ZoneBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ZoneBounds->SetGenerateOverlapEvents(true);
}

void ANUEncounterZone::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 Enemy 선택 및 배틀 시작 판정
    if (!HasAuthority())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();

    UNUNaiaDataSubsystem* NaiaDataSubsystem =
        GameInstance
        ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>()
        : nullptr;

    if (!IsValid(NaiaDataSubsystem))
    {
        UE_LOG(LogTemp, Warning, TEXT("EncounterZone: Enemy 프로젝트 세팅 적용 안됨. Zone=%s"),
            *GetName()
        );
        return;
    }

    if (MinEnemyCount < 1 || MaxEnemyCount < MinEnemyCount || MaxEnemyCount > 4)
    {
        UE_LOG(LogTemp, Warning, TEXT("EncounterZone: 스폰 카운트 잘못 입력. Zone=%s, Min=%d, Max=%d"),
            *GetName(),
            MinEnemyCount,
            MaxEnemyCount
        );
        return;
    }

    if (EnemyEntries.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("EncounterZone: 에디터에서 EnemyEntries 적용 안됨. Zone=%s"),
            *GetName()
        );
        return;
    }

    TSet<FName> RegisteredEnemyIds;

    // EnemyEntries 제대로 적용됐는지 검사
    for (int32 Index = 0; Index < EnemyEntries.Num(); ++Index)
    {
        const FNUEncounterEnemyEntry& Entry = EnemyEntries[Index];

        // Enemy 정보 정확히 입력됐는지
        if (!Entry.IsValid())
        {
            UE_LOG( LogTemp, Warning, TEXT("EncounterZone: Invalid entry. Zone=%s, Index=%d"),
                *GetName(),
                Index
            );
            continue;
        }

        // Enemy 정보가 테이블에 있는지
        if (!NaiaDataSubsystem->HasDefinition(Entry.EnemyId))
        {
            UE_LOG(LogTemp, Warning, TEXT("EncounterZone: Enemy definition 정의 안됨. Zone=%s, EnemyId=%s"),
                *GetName(),
                *Entry.EnemyId.ToString()
            );
        }

        // Enemy 정보가 중복됐는지
        if (RegisteredEnemyIds.Contains(Entry.EnemyId))
        {
            UE_LOG(LogTemp, Warning, TEXT("EncounterZone: EnemyEntries 중복됨. Zone=%s, EnemyId=%s"),
                *GetName(),
                *Entry.EnemyId.ToString()
            );
        }

        RegisteredEnemyIds.Add(Entry.EnemyId);
    }
}

bool ANUEncounterZone::SelectWeightedEnemy(FName& OutEnemyId) const
{
    OutEnemyId = NAME_None;

    float TotalWeight = 0.0f;

    for (const FNUEncounterEnemyEntry& Entry : EnemyEntries)
    {
        if (Entry.IsValid())
        {
            TotalWeight += Entry.Weight;
        }
    }

    if (TotalWeight <= 0.0f)
    {
        return false;
    }

    float RandomValue = FMath::FRandRange(0.0f, TotalWeight);

    for (const FNUEncounterEnemyEntry& Entry : EnemyEntries)
    {
        if (!Entry.IsValid())
        {
            continue;
        }

        RandomValue -= Entry.Weight;

        if (RandomValue <= 0.0f)
        {
            OutEnemyId = Entry.EnemyId;
            return true;
        }
    }

    return false;
}

bool ANUEncounterZone::SelectRandomEnemies(TArray<FName>& OutEnemyIds) const
{
    OutEnemyIds.Reset();

    const int32 SafeMinCount = FMath::Clamp(MinEnemyCount, 1, 4);
    const int32 SafeMaxCount = FMath::Clamp(MaxEnemyCount, SafeMinCount, 4);
    const int32 SelectedCount = FMath::RandRange(SafeMinCount, SafeMaxCount);

    OutEnemyIds.Reserve(SelectedCount);

    for (int32 Index = 0; Index < SelectedCount; ++Index)
    {
        FName SelectedEnemyId;

        if (!SelectWeightedEnemy(SelectedEnemyId))
        {
            OutEnemyIds.Reset();
            return false;
        }

        OutEnemyIds.Add(SelectedEnemyId);
    }

    return !OutEnemyIds.IsEmpty();
}

const TSoftObjectPtr<UWorld>& ANUEncounterZone::GetBattleStageLevel() const
{
    return BattleStageLevel;
}
