#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NUApproachTrigger.generated.h"

class UBoxComponent;

// 장소에 거는 연출의 범위와 ID만 제공한다. 무엇을 언제 재생할지는 연출 표·등장 표의 ApproachActorId가 정하고,
// 플레이어가 범위 안에 있는지는 연출을 재생하는 쪽이 판단한다. NPC가 말하는 장면은 트리거 없이 NPC 반경을 쓴다.
UCLASS(Blueprintable)
class PROJECTM_API ANUApproachTrigger : public AActor
{
	GENERATED_BODY()

public:
	ANUApproachTrigger();

	FName GetApproachActorId() const;

	// 로컬 플레이어가 범위에 들어서면 알린다. 등장 표가 '다가갔다'를 기억하는 데 쓴다.
	FSimpleMulticastDelegate OnLocalPlayerEntered;

	bool IsLocalPlayerInside() const;

	// 레벨에 놓인 같은 ID의 트리거. 없으면 nullptr.
	static ANUApproachTrigger* Find(UWorld* World, FName ApproachActorId);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	TObjectPtr<UBoxComponent> TriggerBounds;

	// 연출 표의 ApproachActorId가 이 값을 가리킨다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue")
	FName ApproachActorId = NAME_None;
};
