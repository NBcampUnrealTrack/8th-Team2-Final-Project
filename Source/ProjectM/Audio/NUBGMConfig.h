// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NUBGMTypes.h"
#include "NUBGMConfig.generated.h"

// BGM Id(FName) -> BGM 재생 정보 표
// DataAsset으로 처리, 나중에 DataTable이 필요하면 전환하기
UCLASS()
class PROJECTM_API UNUBGMConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	const FNUBGMEntry* FindEntry(FName BGMId) const
	{
		return Entries.Find(BGMId);
	}

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TMap<FName, FNUBGMEntry> Entries;
};
