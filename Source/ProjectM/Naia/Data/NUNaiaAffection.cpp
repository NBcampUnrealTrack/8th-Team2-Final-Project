// Fill out your copyright notice in the Description page of Project Settings.

#include "Naia/Data/NUNaiaAffection.h"


namespace
{
    constexpr int32 AffectionTierThresholds[NUNaiaAffection::MaxTier + 1] =
    {
        0,       // 0단계
        1000,    // 1
        2500,    // 2
        4500,    // 3
        7000,    // 4
        10000,   // 5
        13500,   // 6
        17500,   // 7
        22000,   // 8
        27000,   // 9
        32500,   // 10
        38500,   // 11
        45000,   // 12
        52000,   // 13
        59500,   // 14
        67500    // 15
    };
}

int32 NUNaiaAffection::ResolveTier(int32 StoredPoints)
{
    if (StoredPoints <= 0)
    {
        return 0;
    }

    // 뒤에서부터 탐색해서 처음 넘긴 단계가 현재 애정도
    for (int32 Tier = MaxTier; Tier > 0; --Tier)
    {
        if (StoredPoints >= AffectionTierThresholds[Tier])
        {
            return Tier;
        }
    }

    return 0;
}

int32 NUNaiaAffection::GetTierThreshold(int32 Tier)
{
    return AffectionTierThresholds[FMath::Clamp(Tier, 0, MaxTier)];
}
