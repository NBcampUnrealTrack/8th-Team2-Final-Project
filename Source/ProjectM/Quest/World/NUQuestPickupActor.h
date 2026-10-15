#pragma once

#include "CoreMinimal.h"
#include "Interaction/Interface/NUInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "NUQuestPickupActor.generated.h"

class UPaperFlipbookComponent;
class USphereComponent;
class UNUInteractionRouterComponent;
class UNUQuestMarkerComponent;
class UNUQuestProviderComponent;
class UNUSpriteVisualComponent;

// 퀘스트를 수락한 동안만 레벨에 활성화되고, 
// 조사가 끝나면 설정된 아이템을 지급하는 오브젝트
UCLASS(Blueprintable)
class PROJECTM_API ANUQuestPickupActor : public AActor, public INUInteractableInterface
{
	GENERATED_BODY()

public:
	ANUQuestPickupActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest Pickup|Collision")
	TObjectPtr<USphereComponent> InteractionVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest Pickup|Visual")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest Pickup|Visual")
	TObjectPtr<UNUSpriteVisualComponent> SpriteVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest Pickup|Interaction")
	TObjectPtr<UNUInteractionRouterComponent> InteractionRouter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest Pickup|Interaction")
	TObjectPtr<UNUQuestProviderComponent> QuestProvider;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest Pickup|Marker")
	TObjectPtr<UNUQuestMarkerComponent> QuestMarker;

private:
	void RefreshLocalVisibility();
};
