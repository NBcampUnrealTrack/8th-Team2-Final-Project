#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/SaveGame.h"
#include "NUPrologueSettings.generated.h"

class UNUStorySlideshowData;

// 게임 시작 프롤로그. 새 캐릭터가 처음 필드에 들어왔을 때 한 번 재생한다.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Prologue"))
class PROJECTM_API UNUPrologueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	// 재생할 그림책 데이터. 시퀀스는 데이터의 Sequence를 쓴다. 비우면 프롤로그를 끈다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prologue")
	TSoftObjectPtr<UNUStorySlideshowData> PrologueData;

	// 새 캐릭터 첫 진입에 자동으로 재생한다. 콘솔 nu.Prologue.Play 는 이 값과 상관없이 재생한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prologue")
	bool bAutoPlayForNewCharacters = true;

	// 이 레벨 이하인 캐릭터만 "새 캐릭터"로 본다. 서버에 새 캐릭터 표시가 없어 레벨로 가른다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prologue", meta = (ClampMin = "1"))
	int32 MaxCharacterLevel = 1;

	// 로딩 화면이 걷힌 뒤 재생까지 기다리는 시간. 그동안은 검은 덮개 아래라 화면에 보이지 않는다(도착 위치에 자리 잡을 틈).
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prologue", meta = (ClampMin = "0"))
	float StartDelaySeconds = 0.15f;

	// 프롤로그가 끝나고(또는 스킵하고) 게임 화면이 검은 화면에서 밝아지는 시간
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prologue", meta = (ClampMin = "0"))
	float ReturnFadeSeconds = 1.2f;
};

// 이 PC에서 프롤로그를 본 캐릭터 목록. 서버에 저장할 칸이 생기면 그쪽으로 옮긴다.
UCLASS()
class PROJECTM_API UNUPrologueSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FString> SeenCharacterKeys;
};
