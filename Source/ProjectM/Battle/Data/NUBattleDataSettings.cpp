// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleDataSettings.h"

#include "NUGAS/NUGameplayTags.h"

FLinearColor UNUBattleDataSettings::ResolveElementColor(const FGameplayTag& ElementTag) const
{
	if (ElementTag == NU::Element::Fire)			return FireElementColor;
	if (ElementTag == NU::Element::Water)		return WaterElementColor;
	if (ElementTag == NU::Element::Grass)		return GrassElementColor;
	if (ElementTag == NU::Element::Lightning)	return LightningElementColor;
	if (ElementTag == NU::Element::Earth)		return EarthElementColor;
	if (ElementTag == NU::Element::Wind)			return WindElementColor;

	return FLinearColor::White;
}
