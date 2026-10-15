#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Interface/NUInteractionOptionProviderInterface.h"
#include "NUShopProviderComponent.generated.h"

class ANUPlayerController;

// NPC 정의의 ShopId를 상호작용 선택지로 노출하고, 검증된 상점 창을 연다.
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUShopProviderComponent : public UActorComponent, public INUInteractionOptionProviderInterface
{
	GENERATED_BODY()

public:
	UNUShopProviderComponent();

	virtual void CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const override;
	virtual bool CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const override;
	virtual bool ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) override;

	UFUNCTION(BlueprintPure, Category = "NU|Shop|Interaction")
	int32 GetShopId() const;

	UFUNCTION(BlueprintPure, Category = "NU|Shop|Interaction")
	bool IsShopAvailable() const;

	// 0이면 ANUNPCBase의 NpcId를 통해 NPC 정의에 연결된 ShopId를 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop|Interaction", meta = (ClampMin = "0", DisplayName = "상점 ID 재정의"))
	int32 ShopIdOverride = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop|Interaction", meta = (DisplayName = "그룹 내 정렬 순서"))
	int32 SortOrder = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Shop|Interaction", meta = (DisplayName = "이용 불가 시 표시"))
	bool bShowWhenUnavailable = true;

protected:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	bool IsContextValid(const FNUInteractionContext& Context) const;
	bool IsShopHandle(const FNUInteractionOptionHandle& Handle, int32 ShopId) const;
	bool IsSupportedShopType(int32 ShopId) const;
	FText GetUnavailableReason(int32 ShopId) const;
	ANUPlayerController* FindPlayerController(AActor* Interactor) const;
};
