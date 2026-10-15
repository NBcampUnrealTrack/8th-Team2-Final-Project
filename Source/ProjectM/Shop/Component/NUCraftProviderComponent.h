// 공방 NPC에 붙여 "제작" 선택지를 노출하고 제작 창을 연다.
// 상점 선택지(UNUShopProviderComponent)와 같은 자리에 나란히 뜬다 --- 루디처럼 상점 겸 공방인 NPC는 둘 다 붙인다.
//
// 상점 컴포넌트는 ShopType이 Shop인 상점만 열고 공방은 "아직 지원하지 않는 상점 유형"으로 막는다(진영 님 파일).
// 이 컴포넌트가 Workshop 쪽을 맡는다 --- 남의 파일을 고치지 않고 유형을 나눠 갖는 방식.
//
// 서버 제작 패킷이 아직 없어 제작은 로컬에서만 돈다. 레시피 표(NU Shop 세팅)도 기획 표가 오기 전까지는 임시.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Interface/NUInteractionOptionProviderInterface.h"
#include "NUCraftProviderComponent.generated.h"

UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUCraftProviderComponent : public UActorComponent, public INUInteractionOptionProviderInterface
{
	GENERATED_BODY()

public:
	UNUCraftProviderComponent();

	virtual void CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const override;
	virtual bool CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const override;
	virtual bool ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) override;

	// 이 공방의 상점 번호 (NPC 정의의 ShopId, 재정의가 있으면 그 값)
	UFUNCTION(BlueprintPure, Category = "NU|Craft|Interaction")
	int32 GetShopId() const;

	// 공방이고 레시피가 하나라도 있으면 참
	UFUNCTION(BlueprintPure, Category = "NU|Craft|Interaction")
	bool IsCraftAvailable() const;

	// 0이면 소유자(ANUNPCBase)의 NpcId로 NPC 정의에서 찾는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft|Interaction", meta = (ClampMin = "0", DisplayName = "상점 ID 재정의"))
	int32 ShopIdOverride = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft|Interaction", meta = (DisplayName = "그룹 내 정렬 순서"))
	int32 SortOrder = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft|Interaction", meta = (DisplayName = "이용 불가 시 표시"))
	bool bShowWhenUnavailable = true;

	// 선택지 문구. 상호작용 세팅은 남의 파일이라 여기서 들고 있는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft|Interaction", meta = (DisplayName = "선택지 문구"))
	FText CraftOptionText = NSLOCTEXT("NUCraft", "CraftOption", "제작");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Craft|Interaction", meta = (DisplayName = "선택지 분류"))
	FText CraftCategoryText = NSLOCTEXT("NUCraft", "CraftCategory", "공방");

private:
	bool IsContextValid(const FNUInteractionContext& Context) const;
	bool IsCraftHandle(const FNUInteractionOptionHandle& Handle, int32 ShopId) const;
	FText GetUnavailableReason(int32 ShopId) const;
};
