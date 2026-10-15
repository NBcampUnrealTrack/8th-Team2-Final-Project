#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NUActorPresenceSpot.generated.h"

class UArrowComponent;
class USceneComponent;

// 퀘스트 진행에 따라 옮겨 다니는 액터가 설 자리. 액터 등장 표(ActorPresenceTable)의 SpotId가 이 값을 가리키고,
// 액터는 이 위치·방향으로 옮겨진다. 게임 화면에는 보이지 않는다.
UCLASS(Blueprintable)
class PROJECTM_API ANUActorPresenceSpot : public AActor
{
	GENERATED_BODY()

public:
	ANUActorPresenceSpot();

	FName GetSpotId() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	TObjectPtr<USceneComponent> SceneRoot;

	// 액터가 바라볼 방향
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	TObjectPtr<UArrowComponent> DirectionArrow;

	// 레벨 안에서 고유한 자리 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	FName SpotId = NAME_None;
};
