// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleEnemyAvatar.h"

#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/Ability/NUEnemyTelegraphAbility.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "NUGAS/NUGameplayTags.h"

ANUBattleEnemyAvatar::ANUBattleEnemyAvatar()
{
	AbilitySystemComponent = CreateDefaultSubobject<UNUAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	
	VitalSet = CreateDefaultSubobject<UNUVitalAttributeSet>(TEXT("VitalSet"));
	CombatSet = CreateDefaultSubobject<UNUCombatAttributeSet>(TEXT("CombatSet"));
	
	bShowSPBar = false;
}

void ANUBattleEnemyAvatar::BeginPlay()
{
	Super::BeginPlay();
	
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

UAbilitySystemComponent* ANUBattleEnemyAvatar::GetAbilitySystemComponent() const
{
	return GetNUASC();
}

UNUAbilitySystemComponent* ANUBattleEnemyAvatar::GetNUASC() const
{
	return AbilitySystemComponent;
}

void ANUBattleEnemyAvatar::InitializeEnemy(const FNUNaiaDefinition& NaiaDefinition)
{
	if (!HasAuthority()) return;

	Level = FMath::RandRange(NaiaDefinition.SpawnLevel, NaiaDefinition.GetMaxSpawnLevel());
	// 야생 적은 애정도가 없으므로 단계 0
	const FNUNaiaStats NaiaStats = NaiaDefinition.ResolveStats(Level, 0);
	
	ApplyInitialVitals(NaiaStats);
	ApplyInitialCombatStats(NaiaStats);
	
	if (UNUAbilitySystemComponent* NUASC = GetNUASC())
	{
		NUASC->GiveStartupAbilities();	
		
		// 패턴 표의 페이즈 진입 행동(경고)은 BP 없이 쓰는 C++ 행동이라 여기서 줌
		// 패턴이 안 부르면 후보가 아니라 무해함, 아바타를 다시 쓰면 이미 있으므로 확인함
		if (NUASC->FindAbilitySpecFromClass(UNUEnemyTelegraphAbility::StaticClass()) == nullptr)
		{
			NUASC->GiveAbility(FGameplayAbilitySpec(UNUEnemyTelegraphAbility::StaticClass(), 1));
		}
	}
	
	
	// 기존 속성 태그 삭제
	FGameplayTagContainer TagContainer;
	AbilitySystemComponent->GetOwnedGameplayTags(TagContainer);
	AbilitySystemComponent->RemoveLooseGameplayTags(
		TagContainer.Filter(FGameplayTagContainer(NU::Element::Root)));	// 기존 속성태그 삭제
	
	if (NaiaDefinition.ElementTag.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(NaiaDefinition.ElementTag);
	}
	
	// 이중속성은 규칙 확인되면 나중에
	// if (NaiaDefinition.SecondaryElementTag.IsValid())
	// {
	// 	AbilitySystemComponent->AddLooseGameplayTag(NaiaDefinition.SecondaryElementTag);
	// }
}

void ANUBattleEnemyAvatar::ApplyInitialVitals(const FNUNaiaStats& NaiaStats)
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent)) return;
	
	FNUAbilitySystemInitVitalData InitData;
	InitData.MaxHealth = NaiaStats.MaxHealth;
	InitData.Health = NaiaStats.MaxHealth;
	InitData.MaxSP = NaiaStats.MaxSP;
	InitData.SP = NaiaStats.MaxSP;
	
	AbilitySystemComponent->ApplyInitialVitals(InitData, InitVitalsEffect, Level);
}

void ANUBattleEnemyAvatar::ApplyInitialCombatStats(const FNUNaiaStats& NaiaStats)
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent)) return;
	
	FNUAbilitySystemInitCombatData InitData;
	InitData.AttackPower = NaiaStats.AttackPower;
	InitData.MagicAttackPower = NaiaStats.MagicAttackPower;
	InitData.Defense = NaiaStats.Defense;
	InitData.Speed = NaiaStats.Speed;
	InitData.CriticalRate = NaiaStats.CriticalRate;
	InitData.CriticalDamage = NaiaStats.CriticalDamage;
	
	AbilitySystemComponent->ApplyInitialCombatStats(InitData, InitCombatStatsEffect, Level);
}

int32 ANUBattleEnemyAvatar::GetLevel() const
{
	return Level;
}
