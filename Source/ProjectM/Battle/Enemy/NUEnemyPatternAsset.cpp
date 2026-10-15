// Fill out your copyright notice in the Description page of Project Settings.


#include "Battle/Enemy/NUEnemyPatternAsset.h"

const UNUEnemyPatternAsset* UNUEnemyPatternRegistry::FindPattern(FName NaiaId) const
{
	if (NaiaId.IsNone())
	{
		return nullptr;
	}

	const TSoftObjectPtr<UNUEnemyPatternAsset>* Found = Patterns.Find(NaiaId);
	if (!Found || Found->IsNull())
	{
		return nullptr;
	}

	// 전투 준비 중 한 번만 불러서 동기 로드
	return Found->LoadSynchronous();
}
