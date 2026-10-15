// Fill out your copyright notice in the Description page of Project Settings.


#include "NUCombatantInterface.h"

#include "NUGAS/NUAbilitySystemComponent.h"


AActor* INUCombatantInterface::GetCurrentAvatar() const
{
	const UNUAbilitySystemComponent* ASC = GetNUASC();
	return ASC ? ASC->GetCurrentAvatar() : nullptr;
}

void INUCombatantInterface::SetCurrentAvatar(AActor* NewAvatar)
{
	if (UNUAbilitySystemComponent* ASC = GetNUASC())
	{
		ASC->SetCurrentAvatar(NewAvatar);
	}
}

ANUBattleEncounter* INUCombatantInterface::GetBattleEncounter() const
{
	const UNUAbilitySystemComponent* ASC = GetNUASC();
	return ASC ? ASC->GetBattleEncounter() : nullptr;
}

void INUCombatantInterface::SetBattleEncounter(ANUBattleEncounter* NewEncounter)
{
	if (UNUAbilitySystemComponent* ASC = GetNUASC())
	{
		ASC->SetBattleEncounter(NewEncounter);
	}
}
