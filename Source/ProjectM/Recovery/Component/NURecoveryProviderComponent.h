#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Interface/NUInteractionOptionProviderInterface.h"
#include "NURecoveryProviderComponent.generated.h"

class ANUPlayerController;
class ANUPlayerState;
class UGameplayEffect;
class UNUDialogueDefinition;

// NPC에 즉시 회복 선택지를 추가하고 플레이어와 계약 나이아의 HP를 함께 회복한다.
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNURecoveryProviderComponent : public UActorComponent, public INUInteractionOptionProviderInterface
{
	GENERATED_BODY()

public:
	UNURecoveryProviderComponent();

	virtual void CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const override;
	virtual bool CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const override;
	virtual bool ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) override;

	UFUNCTION(BlueprintPure, Category = "NU|Recovery|Interaction")
	bool IsInstantRecoveryAvailable(AActor* Interactor) const;

	UFUNCTION(BlueprintPure, Category = "NU|Recovery|Interaction")
	int32 GetRecoveryCost(AActor* Interactor) const;
	bool IsInstantRecoveryUnlocked(AActor* Interactor) const;
	bool IsLocationServiceUnlocked(AActor* Interactor) const;
	UNUDialogueDefinition* GetRecoveryPromptDialogue(AActor* Interactor) const;

	// 플레이어나 계약 나이아가 다쳤을 때만 NPC 인사 대신 회복 안내를 보여준다.
	bool ShouldShowRecoveryPrompt(AActor* Interactor) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Dialogue")
	TObjectPtr<UNUDialogueDefinition> RestOnlyDialogue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Dialogue")
	TObjectPtr<UNUDialogueDefinition> FreeRecoveryDialogue;
	// 대사 내 {Cost}를 실제 비용으로 치환한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Dialogue")
	TObjectPtr<UNUDialogueDefinition> PaidRecoveryDialogue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Interaction")
	bool bUseRecoveryPrompt = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Dialogue")
	TObjectPtr<UNUDialogueDefinition> RestExecutionDialogue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Dialogue")
	TObjectPtr<UNUDialogueDefinition> InstantExecutionDialogue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Dialogue")
	TObjectPtr<UNUDialogueDefinition> InsufficientGoldDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Interaction", meta = (DisplayName = "그룹 내 정렬 순서"))
	int32 SortOrder = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Interaction")
	bool bOfferInstantRecovery = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Interaction")
	bool bOfferRest = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Interaction", meta = (ClampMin = "1"))
	float ServiceRadius = 500.0f;

	// 모두 한 번 이상 완료해야 이용 가능. 빈 목록은 퀘스트 제한 없음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Recovery|Unlock")
	TArray<FName> RequiredQuestIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Recovery|GameplayEffect")
	TSubclassOf<UGameplayEffect> RecoveryEffectClass;

protected:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	bool IsContextValid(const FNUInteractionContext& Context) const;
	bool IsInstantRecoveryHandle(const FNUInteractionOptionHandle& Handle) const;
	bool TryGetRecoveryCost(AActor* Interactor, int32& OutCost) const;
	FText BuildOptionText(AActor* Interactor) const;
	FText GetUnavailableReason(AActor* Interactor) const;
	ANUPlayerController* FindPlayerController(AActor* Interactor) const;
	ANUPlayerState* FindPlayerState(AActor* Interactor) const;
};
