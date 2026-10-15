#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Interface/NUInteractionOptionProviderInterface.h"
#include "NUJobProviderComponent.generated.h"

class UNUJobComponent;

// 설정된 전직 NPC에서 전직 목표가 활성화된 플레이어에게 최초 직업 선택 기능을 제공한다.
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUJobProviderComponent : public UActorComponent, public INUInteractionOptionProviderInterface
{
	GENERATED_BODY()

public:
	UNUJobProviderComponent();

	virtual void CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const override;
	virtual bool CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const override;
	virtual bool ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) override;

private:
	bool IsJobSelectionHandle(const FNUInteractionOptionHandle& Handle) const;
	FName GetQuestActorId() const;
	UNUJobComponent* FindJobComponent(AActor* Interactor) const;
	bool CanProvideJobSelection(const FNUInteractionContext& Context) const;
};
