#include "Shop/Component/NUCraftProviderComponent.h"

#include "Engine/GameInstance.h"
#include "Field/NPC/Data/NUNPCDataSubsystem.h"
#include "Field/NPC/Data/NUNPCDefinition.h"
#include "Field/NPC/NUNPCBase.h"
#include "GameFramework/PlayerController.h"
#include "NUGAS/NUGameplayTags.h"
#include "Shop/NUCraftTypes.h"
#include "Shop/NUShopTypes.h"
#include "Shop/UI/NUCraftWindowWidget.h"

UNUCraftProviderComponent::UNUCraftProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UNUCraftProviderComponent::CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const
{
	if (!IsContextValid(Context))
	{
		return;
	}

	const int32 ShopId = GetShopId();
	const bool bAvailable = IsCraftAvailable();
	if (!bAvailable && !bShowWhenUnavailable)
	{
		return;
	}

	FNUInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Handle.OptionType = NU::Interaction::Option::Crafting;
	Option.Handle.OptionId = FName(*FString::FromInt(ShopId));
	Option.DisplayText = CraftOptionText;
	Option.CategoryText = CraftCategoryText;
	Option.Group = ENUInteractionOptionGroup::Service;
	Option.SortOrder = SortOrder;
	Option.bEnabled = bAvailable;
	if (!bAvailable)
	{
		Option.DisabledReason = GetUnavailableReason(ShopId);
	}
}

bool UNUCraftProviderComponent::CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const
{
	return IsContextValid(Context) && IsCraftHandle(Handle, GetShopId()) && IsCraftAvailable();
}

bool UNUCraftProviderComponent::ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle)
{
	if (!CanExecuteInteraction_Implementation(Context, Handle))
	{
		return false;
	}

	// 제작은 아직 서버 패킷이 없어 로컬에서만 돈다 --- 상점처럼 컨트롤러를 거치지 않고 이 자리에서 연다.
	// (서버 제작이 들어오면 상점과 같은 ClientOpenX 경로로 옮긴다)
	const APawn* Pawn = Cast<APawn>(Context.Interactor);
	APlayerController* PlayerController = Pawn != nullptr ? Cast<APlayerController>(Pawn->GetController())
		: Cast<APlayerController>(Context.Interactor);
	if (PlayerController == nullptr || !PlayerController->IsLocalController())
	{
		return false;
	}

	UNUCraftWindowWidget::Show(PlayerController, GetShopId());
	return true;
}

int32 UNUCraftProviderComponent::GetShopId() const
{
	if (ShopIdOverride > 0)
	{
		return ShopIdOverride;
	}

	const ANUNPCBase* NPC = Cast<ANUNPCBase>(GetOwner());
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUNPCDataSubsystem* NPCData = IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUNPCDataSubsystem>() : nullptr;
	const FNUNPCDefinition* Definition = IsValid(NPCData) && IsValid(NPC) ? NPCData->FindDefinition(NPC->GetNpcId()) : nullptr;
	return Definition != nullptr ? Definition->ShopId : 0;
}

bool UNUCraftProviderComponent::IsCraftAvailable() const
{
	const int32 ShopId = GetShopId();
	if (ShopId <= 0)
	{
		return false;
	}

	// 공방(또는 상점 겸 공방)이면서 레시피가 하나라도 있어야 연다.
	// 표에 레시피가 없으면 빈 창이 뜨므로 선택지 자체를 비활성으로 둔다
	const FNUShopRow* Shop = NUShop::FindShopRow(ShopId);
	return Shop != nullptr && !NUCraft::GetRecipes(ShopId).IsEmpty();
}

bool UNUCraftProviderComponent::IsContextValid(const FNUInteractionContext& Context) const
{
	return Context.IsValid() && Context.Target.Get() == GetOwner();
}

bool UNUCraftProviderComponent::IsCraftHandle(const FNUInteractionOptionHandle& Handle, int32 ShopId) const
{
	return Handle.OptionType == NU::Interaction::Option::Crafting && Handle.OptionId == FName(*FString::FromInt(ShopId));
}

FText UNUCraftProviderComponent::GetUnavailableReason(int32 ShopId) const
{
	if (ShopId <= 0)
	{
		return NSLOCTEXT("NUCraft", "ShopNotAssigned", "공방이 연결되지 않았습니다.");
	}
	if (NUShop::FindShopRow(ShopId) == nullptr)
	{
		return NSLOCTEXT("NUCraft", "ShopDefinitionMissing", "상점 정보를 찾을 수 없습니다.");
	}
	return NSLOCTEXT("NUCraft", "NoRecipe", "아직 만들 수 있는 것이 없습니다.");
}
