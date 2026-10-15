#pragma once

#include "CoreMinimal.h"
#include "Interaction/Interface/NUInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "NUNPCBase.generated.h"

class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class UPaperZDAnimInstance;
class USphereComponent;
class UNUDialogueProviderComponent;
class UNUInteractionRouterComponent;
class UNUQuestProviderComponent;
class UNUSpriteVisualComponent;

// 필드 NPC의 공통 부모
// 상호작용 라우팅과 일반 대화만 공통으로 제공하고, 
// 퀘스트, 상점 등의 역할은 파생 클래스나 Provider 컴포넌트가 담당
// Blueprint에서 상속해서 사용

UCLASS(Abstract, Blueprintable)
class PROJECTM_API ANUNPCBase : public AActor, public INUInteractableInterface
{
	GENERATED_BODY()

public:
	ANUNPCBase();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintPure, Category = "NU|NPC|Interaction")
	UNUQuestProviderComponent* GetQuestProviderComponent() const;

	UFUNCTION(BlueprintPure, Category = "NU|NPC|Interaction")
	UNUInteractionRouterComponent* GetInteractionRouterComponent() const;

	UFUNCTION(BlueprintPure, Category = "NU|NPC|Interaction")
	UNUDialogueProviderComponent* GetDialogueProviderComponent() const;

	UFUNCTION(BlueprintPure, Category = "NU|NPC|Identity")
	int32 GetNpcId() const;

	UFUNCTION(BlueprintPure, Category = "NU|NPC|Identity")
	FText GetNPCDisplayName() const;

	// NpcId에 해당하는 정적 정의를 다시 적용한다. 
	UFUNCTION(BlueprintCallable, Category = "NU|NPC|Data")
	bool ApplyNPCDefinition();

protected:
	virtual void BeginPlay() override;

	// 크기·밝기·도트 보정 (프로젝트 세팅 NU NPC Visual, NUNPCVisualSettings.h)
	void ApplyVisualTuning();

	// DataTable RowName과 같은 숫자 ID
	// 0이면 기존 Blueprint 설정만 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Identity", meta = (ClampMin = "0"))
	int32 NpcId = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "NU|NPC|Identity")
	FText ResolvedDisplayName;

	UPROPERTY(Transient)
	TSoftClassPtr<UPaperZDAnimInstance> ResolvedAnimationClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|NPC|Collision")
	TObjectPtr<USphereComponent> InteractionVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|NPC|Visual")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|NPC|Animation")
	TObjectPtr<UPaperZDAnimationComponent> Animation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|NPC|Visual")
	TObjectPtr<UNUSpriteVisualComponent> SpriteVisual;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|NPC|Visual", meta = (ClampMin = "0.016", UIMin = "0.016"))
	float BillboardUpdateInterval = 0.1f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|NPC|Interaction", meta = (DisplayName = "Interaction Router"))
	TObjectPtr<UNUInteractionRouterComponent> InteractionRouter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|NPC|Interaction", meta = (DisplayName = "Dialogue Provider"))
	TObjectPtr<UNUDialogueProviderComponent> DialogueProvider;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Interaction")
	bool bInteractionEnabled = true;
};
