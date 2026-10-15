#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interface/NUInteractableInterface.h"
#include "Quest/Data/NUQuestStateCondition.h"
#include "NUQuestStoryProp.generated.h"

class UPointLightComponent;
class UPaperSpriteComponent;
class USphereComponent;
class UNUQuestComponent;

UENUM(BlueprintType)
enum class ENUQuestStoryLight : uint8
{
	None,
	Breathing,
	ApproachPulse
};

// 진행 목표가 아닌 이야기 소품. 배치와 조건만 옮기면 다른 맵에서도 같은 반응을 쓴다.
UCLASS(Blueprintable)
class PROJECTM_API ANUQuestStoryProp : public AActor, public INUInteractableInterface
{
	GENERATED_BODY()

public:
	ANUQuestStoryProp();
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story")
	FName StoryId;

	// 비어 있으면 장식이며 상호작용 대상이 아니다. 조사해도 퀘스트 진행을 보내지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story", meta = (MultiLine = "true"))
	FText InspectionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story")
	FNUQuestStateCondition ReactionCondition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story")
	FNUQuestStateCondition OpenCondition;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story")
	ENUQuestStoryLight LightReaction = ENUQuestStoryLight::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story", meta = (ClampMin = "1"))
	float ReactionRadius = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story", meta = (ClampMin = "0"))
	float IdleIntensity = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story", meta = (ClampMin = "0"))
	float ReactionIntensity = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story", meta = (ClampMin = "0"))
	float OpenIntensity = 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story", meta = (ClampMin = "0.1"))
	float PulseSeconds = 2.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story")
	TObjectPtr<USphereComponent> InteractionVolume;

	// 작은 빛덩어리 또는 문틈의 빛. SpriteColor의 알파로 반응 강도를 표현한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story")
	TObjectPtr<UPaperSpriteComponent> Glow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|Story")
	TObjectPtr<UPointLightComponent> Light;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	const UNUQuestComponent* FindQuest(const APawn* Pawn) const;
	bool IsConditionMet(const FNUQuestStateCondition& Condition, const UNUQuestComponent& Quest) const;
	float Elapsed = 0.f;
	float PulseElapsed = -1.f;
	float CurrentIntensity = 0.f;
	float NextInspectTime = 0.f;
	bool bApproachPlayed = false;
};
