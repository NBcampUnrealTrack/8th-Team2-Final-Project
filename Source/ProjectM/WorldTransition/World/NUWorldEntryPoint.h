#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NUWorldEntryPoint.generated.h"

class UArrowComponent;
class USceneComponent;
class ANUWorldPortal;

// 캐릭터의 도착 위치를 제공하는 액터로, 출입구에서 EntryPointId로 지정한다.
UCLASS(Blueprintable)
class PROJECTM_API ANUWorldEntryPoint : public AActor
{
	GENERATED_BODY()

public:
	ANUWorldEntryPoint();

	FName GetEntryPointId() const;
	bool UsesFeetLocation() const { return bFeetLocation; }

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	// 실내외 크기가 달라도 같은 바닥 위치를 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	bool bFeetLocation = false;

	// 배치를 연동할 출입구.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "NU|WorldTransition")
	TObjectPtr<ANUWorldPortal> PlacementPortal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	TObjectPtr<USceneComponent> SceneRoot;

	// 캡슐 중심 기준 도착 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	TObjectPtr<UArrowComponent> DirectionArrow;

	// 목적 레벨 안에서 고유한 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition")
	FName EntryPointId = NAME_None;

private:
	TWeakObjectPtr<ANUWorldPortal> PreviousPlacementPortal;
};
