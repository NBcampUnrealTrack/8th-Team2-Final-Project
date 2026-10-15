// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUAudioSettings.generated.h"

class UNUBGMConfig;
class UNUSFXConfig;
class USoundClass;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Audio"))
class PROJECTM_API UNUAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	// BGM Id -> 재생 정보 표
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<UNUBGMConfig> BGMConfig;

	// 레벨/구역/전투 어느 층에도 요청이 없을 때 재생할 BGM
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio")
	FName FallbackBGMId = NAME_None;

	// 전투 진입 시 재생할 기본 BGM
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio|Battle")
	FName BattleBGMId = NAME_None;

	// 보스전 BGM - 적 중에 보스 나이아(Category=Boss)가 있으면 전투 세트 BGM 보다 우선해서 튼다
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio|Battle")
	FName BossBattleBGMId = NAME_None;

	// 옵션창 볼륨 슬라이더가 조절하는 사운드 클래스. UNUGameUserSettings가 기본 믹스에 클래스별 볼륨을 건다.
	// 마스터는 자식 클래스까지 곱해지므로 소리 애셋은 전부 이 아래 클래스여야 슬라이더가 먹는다
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio|Volume")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio|Volume")
	TSoftObjectPtr<USoundClass> BgmSoundClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio|Volume")
	TSoftObjectPtr<USoundClass> SfxSoundClass;

	// 효과음 태그(NU.SFX.*) -> 소리 표. 영역별로 여러 장을 걸 수 있고 앞에 건 표가 먼저다 (UNUSFXSubsystem 이 읽는다)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TArray<TSoftObjectPtr<UNUSFXConfig>> SFXConfigs;
};
