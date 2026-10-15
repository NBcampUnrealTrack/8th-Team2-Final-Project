#include "Naia/Data/NUNaiaDefinition.h"

#include "CharacterProgression/Data/NULevelProgression.h"
#include "Naia/Data/NUNaiaAffection.h"


namespace
{
    // 애정도 단계당 증가분 = max(하한, round₀.₁(스탯별 계수 - 0.04 × 기본스탯))
    float ResolveAffectionStep(float BaseStat, float Coefficient, float MinimumStep)
    {
        // round₀.₁ - 소수 첫째 자리 반올림
        const float Rounded = FMath::RoundToFloat((Coefficient - 0.04f * BaseStat) * 10.f) / 10.f;

        return FMath::Max(MinimumStep, Rounded);
    }
}

FNUNaiaStats FNUNaiaDefinition::ResolveStats(int32 Level, int32 AffectionTier) const
{
    const int32 SafeLevel = FMath::Max(1, Level);
    
    // 기준치로 사용할 레벨이 10레벨이라 차이만큼 성장치를 더하거나 빼야함
    // SpawnLevel이 10 미만이면 기준값에서 스탯이 내려감
    const float LevelOffset = static_cast<float>(SafeLevel - BaseStatLevel);

    FNUNaiaStats Result;

    Result.MaxHealth = FMath::Max(1.f, BaseHealth + HealthGrowth * LevelOffset);
    Result.MaxSP = FMath::Max(0.f, BaseSP + SPGrowth * LevelOffset);
    Result.AttackPower = FMath::Max(0.f, BaseAttackPower + AttackGrowth * LevelOffset);
    Result.MagicAttackPower = FMath::Max(0.f, BaseMagicAttackPower + MagicAttackGrowth * LevelOffset);
    Result.Defense = FMath::Max(0.f, BaseDefense + DefenseGrowth * LevelOffset);
    Result.Speed =  FMath::Max(0.f, BaseSpeed + SpeedGrowth * LevelOffset);
    
    Result.CriticalRate = FMath::Max(0.f, CriticalRate);
    Result.CriticalDamage = FMath::Max(1.f, CriticalDamage);

    // 애정도는 HP·ATK·MATK·DEF만 올림, SP·속도·치명타는 레벨로만 올라감
    // 최종값은 반올림하지 않음 - 기획서가 반올림을 증가분에만 걸었고, 레벨 성장분도
    // 소수를 그대로 쓰고 있어서 여기만 정수로 만들면 같은 스탯에 규칙이 둘이 됨
    const int32 Tier = FMath::Clamp(AffectionTier, 0, NUNaiaAffection::MaxTier);
    if (Tier > 0)
    {
        Result.MaxHealth        += Tier * ResolveAffectionStep(BaseHealth, 16.f, 1.0f);
        Result.AttackPower      += Tier * ResolveAffectionStep(BaseAttackPower, 1.7f, 0.2f);
        Result.MagicAttackPower += Tier * ResolveAffectionStep(BaseMagicAttackPower, 1.7f, 0.2f);
        Result.Defense          += Tier * ResolveAffectionStep(BaseDefense, 1.2f, 0.2f);
    }

    return Result;
}

int32 FNUNaiaDefinition::GetRequiredExperienceForNextLevel(int32 Level) const
{
	if (Level < 1 || Level >= FNULevelProgression::Get().GetMaxLevel())
    {
        return 0;
    }

    return FMath::Max(0, BaseRequiredExperience + RequiredExperienceGrowth * (Level - BaseStatLevel));
}

// MaxSpawnLevel 이 "-" 인 행은 읽을때 0으로 읽으므로 SpawnLevel 그대로 사용
int32 FNUNaiaDefinition::GetMaxSpawnLevel() const
{
    return MaxSpawnLevel > 0 
        ? MaxSpawnLevel
        : FMath::Max(1, SpawnLevel);
}
