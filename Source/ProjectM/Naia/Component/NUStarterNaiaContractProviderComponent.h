#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Interface/NUInteractionOptionProviderInterface.h"
#include "NUStarterNaiaContractProviderComponent.generated.h"

class ANUPlayerController;
class ANUPlayerState;
class UNUNaiaNetSubsystem;

// 최초 직업 선택 NPC에서 미완료 스타팅 나이아 계약을 다시 시작할 수 있게 한다.
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUStarterNaiaContractProviderComponent : public UActorComponent, public INUInteractionOptionProviderInterface
{
	GENERATED_BODY()

public:
	UNUStarterNaiaContractProviderComponent();

	virtual void CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const override;
	virtual bool CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const override;
	virtual bool ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Interaction", meta = (DisplayName = "그룹 내 정렬 순서"))
	int32 SortOrder = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Interaction", meta = (DisplayName = "이용 불가 시 표시"))
	bool bShowWhenUnavailable = true;

protected:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	bool IsContextValid(const FNUInteractionContext& Context) const;
	bool IsContractHandle(const FNUInteractionOptionHandle& Handle) const;
	bool CanProvideContract(const FNUInteractionContext& Context) const;
	bool IsContractAvailable(AActor* Interactor) const;
	FText GetUnavailableReason(AActor* Interactor) const;
	FName GetQuestActorId() const;
	ANUPlayerController* FindPlayerController(AActor* Interactor) const;
	ANUPlayerState* FindPlayerState(AActor* Interactor) const;
	UNUNaiaNetSubsystem* FindNaiaNetSubsystem() const;
};
