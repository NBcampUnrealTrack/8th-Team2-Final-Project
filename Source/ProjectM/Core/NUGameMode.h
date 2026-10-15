#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NUGameMode.generated.h"

UCLASS()
class PROJECTM_API ANUGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANUGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 전투 공간 관련
	
	FVector GetArenaCenter() const;
	
private:
	/* 전투시 사용할 빈 공간의 위치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Arena", meta = (AllowPrivateAccess = "true"))
	FVector ArenaCenter = FVector(0.0f, 0.0f, -10000.0f);
};
