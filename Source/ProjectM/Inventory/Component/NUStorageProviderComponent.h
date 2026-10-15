#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Interface/NUInteractionOptionProviderInterface.h"
#include "NUStorageProviderComponent.generated.h"

class ANUPlayerController;
class UEmberServerManager;
class UNUInventoryComponent;

// NPC와 상호작용할 때 아이템 창고 선택지를 제공하고, 서버 세션 확인 뒤 창고 UI를 연다.
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUStorageProviderComponent : public UActorComponent, public INUInteractionOptionProviderInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Storage|Dialogue")
	TObjectPtr<class UNUDialogueDefinition> StorageExecutionDialogue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Storage|Dialogue")
	TObjectPtr<class UNUDialogueDefinition> StorageUnavailableDialogue;
	UNUStorageProviderComponent();

	virtual void CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const override;
	virtual bool CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const override;
	virtual bool ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) override;

	UFUNCTION(BlueprintPure, Category = "NU|Storage|Interaction")
	bool IsStorageAvailable(AActor* Interactor) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Storage|Interaction", meta = (DisplayName = "그룹 내 정렬 순서"))
	int32 SortOrder = 40;


protected:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	bool IsContextValid(const FNUInteractionContext& Context) const;
	bool IsStorageHandle(const FNUInteractionOptionHandle& Handle) const;
	ANUPlayerController* FindPlayerController(AActor* Interactor) const;
	UNUInventoryComponent* FindInventoryComponent(AActor* Interactor) const;
	UEmberServerManager* FindServerManager() const;
};
