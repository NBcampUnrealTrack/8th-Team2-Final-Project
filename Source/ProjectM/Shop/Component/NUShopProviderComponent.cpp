#include "Shop/Component/NUShopProviderComponent.h"

#include "Field/NPC/Data/NUNPCDataSubsystem.h"
#include "Field/NPC/Data/NUNPCDefinition.h"
#include "Field/NPC/NUNPCBase.h"
#include "Interaction/Data/NUInteractionSettings.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Shop/NUShopTypes.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Field/NPC/Data/NUNPCDataSettings.h"
#include "Misc/DataValidation.h"
#endif

UNUShopProviderComponent::UNUShopProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UNUShopProviderComponent::CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const
{
	if (!IsContextValid(Context))
	{
		return;
	}

	const int32 ShopId = GetShopId();
	const bool bAvailable = IsShopAvailable();
	if (!bAvailable && !bShowWhenUnavailable)
	{
		return;
	}

	const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();
	FNUInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Handle.OptionType = NU::Interaction::Option::Shop;
	Option.Handle.OptionId = FName(*FString::FromInt(ShopId));
	Option.DisplayText = Settings->ShopOptionText;
	Option.CategoryText = Settings->ShopCategoryText;
	Option.Group = ENUInteractionOptionGroup::Service;
	Option.SortOrder = SortOrder;
	Option.bEnabled = bAvailable;
	if (!bAvailable)
	{
		Option.DisabledReason = GetUnavailableReason(ShopId);
	}
}

bool UNUShopProviderComponent::CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const
{
	const int32 ShopId = GetShopId();
	return IsContextValid(Context) && IsShopHandle(Handle, ShopId) && IsShopAvailable();
}

bool UNUShopProviderComponent::ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle)
{
	if (!CanExecuteInteraction_Implementation(Context, Handle) || !Context.Interactor->HasAuthority())
	{
		return false;
	}

	ANUPlayerController* PlayerController = FindPlayerController(Context.Interactor);
	if (!IsValid(PlayerController))
	{
		return false;
	}

	PlayerController->ClientOpenShop(GetShopId(), Context.Target.Get());
	return true;
}

int32 UNUShopProviderComponent::GetShopId() const
{
	if (ShopIdOverride > 0)
	{
		return ShopIdOverride;
	}

	const ANUNPCBase* NPC = Cast<ANUNPCBase>(GetOwner());
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUNPCDataSubsystem* NPCDataSubsystem = IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUNPCDataSubsystem>() : nullptr;
	const FNUNPCDefinition* Definition = IsValid(NPCDataSubsystem) && IsValid(NPC) ? NPCDataSubsystem->FindDefinition(NPC->GetNpcId()) : nullptr;
	return Definition != nullptr ? Definition->ShopId : 0;
}

bool UNUShopProviderComponent::IsShopAvailable() const
{
	const int32 ShopId = GetShopId();
	return ShopId > 0 && NUShop::FindShopRow(ShopId) != nullptr && IsSupportedShopType(ShopId);
}

bool UNUShopProviderComponent::IsContextValid(const FNUInteractionContext& Context) const
{
	return Context.IsValid() && Context.Target.Get() == GetOwner();
}

bool UNUShopProviderComponent::IsShopHandle(const FNUInteractionOptionHandle& Handle, int32 ShopId) const
{
	return Handle.OptionType == NU::Interaction::Option::Shop && Handle.OptionId == FName(*FString::FromInt(ShopId));
}

bool UNUShopProviderComponent::IsSupportedShopType(int32 ShopId) const
{
	const FNUShopRow* Shop = NUShop::FindShopRow(ShopId);
	return Shop != nullptr && Shop->ShopType == ENUShopType::Shop;
}

FText UNUShopProviderComponent::GetUnavailableReason(int32 ShopId) const
{
	if (ShopId <= 0)
	{
		return NSLOCTEXT("NUShop", "ShopNotAssigned", "상점이 연결되지 않았습니다.");
	}
	if (NUShop::FindShopRow(ShopId) == nullptr)
	{
		return NSLOCTEXT("NUShop", "ShopDefinitionMissing", "상점 정보를 찾을 수 없습니다.");
	}
	return NSLOCTEXT("NUShop", "ShopTypeUnsupported", "아직 지원하지 않는 상점 유형입니다.");
}

ANUPlayerController* UNUShopProviderComponent::FindPlayerController(AActor* Interactor) const
{
	if (ANUPlayerController* PlayerController = Cast<ANUPlayerController>(Interactor))
	{
		return PlayerController;
	}

	const APawn* Pawn = Cast<APawn>(Interactor);
	return IsValid(Pawn) ? Cast<ANUPlayerController>(Pawn->GetController()) : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UNUShopProviderComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UNUInteractionSettings* InteractionSettings = GetDefault<UNUInteractionSettings>();
	if (!IsValid(InteractionSettings) || InteractionSettings->ShopOptionText.IsEmpty() || InteractionSettings->ShopCategoryText.IsEmpty())
	{
		Context.AddError(NSLOCTEXT("NUShop", "MissingShopInteractionText", "NU Interaction 설정의 상점 선택지 또는 카테고리 문구가 비어 있습니다."));
		Result = EDataValidationResult::Invalid;
	}

	int32 ShopId = ShopIdOverride;
	int32 NpcId = 0;
	if (ShopId <= 0)
	{
		const ANUNPCBase* NPC = Cast<ANUNPCBase>(GetOwner());
		if (!IsValid(NPC))
		{
			const UBlueprintGeneratedClass* BlueprintClass = GetTypedOuter<UBlueprintGeneratedClass>();
			NPC = IsValid(BlueprintClass) ? Cast<ANUNPCBase>(BlueprintClass->GetDefaultObject(false)) : nullptr;
		}
		NpcId = IsValid(NPC) ? NPC->GetNpcId() : 0;
		if (NpcId <= 0)
		{
			Context.AddError(NSLOCTEXT("NUShop", "MissingShopSource", "ShopIdOverride 또는 유효한 ANUNPCBase의 NpcId가 필요합니다."));
			return EDataValidationResult::Invalid;
		}

		const UNUNPCDataSettings* NPCSettings = GetDefault<UNUNPCDataSettings>();
		UDataTable* NPCDefinitionTable = IsValid(NPCSettings) ? NPCSettings->NPCDefinitionTable.LoadSynchronous() : nullptr;
		if (!IsValid(NPCDefinitionTable) || NPCDefinitionTable->GetRowStruct() != FNUNPCDefinition::StaticStruct())
		{
			Context.AddError(NSLOCTEXT("NUShop", "InvalidNPCDefinitionTable", "NU NPC Data 설정의 NPCDefinitionTable이 없거나 RowStruct가 FNUNPCDefinition이 아닙니다."));
			return EDataValidationResult::Invalid;
		}

		const FName NPCRowName(*FString::FromInt(NpcId));
		const FNUNPCDefinition* NPCDefinition = NPCDefinitionTable->FindRow<FNUNPCDefinition>(NPCRowName, TEXT("UNUShopProviderComponent::IsDataValid"), false);
		if (NPCDefinition == nullptr || NPCDefinition->ShopId <= 0)
		{
			Context.AddError(FText::Format(NSLOCTEXT("NUShop", "MissingNPCShopDefinition", "NpcId {0}에 연결된 유효한 ShopId가 없습니다."), FText::AsNumber(NpcId)));
			return EDataValidationResult::Invalid;
		}
		ShopId = NPCDefinition->ShopId;
	}

	const FNUShopRow* Shop = NUShop::FindShopRow(ShopId);
	if (Shop == nullptr)
	{
		Context.AddError(FText::Format(NSLOCTEXT("NUShop", "MissingShopDefinition", "ShopId {0}에 해당하는 상점 행이 없습니다."), FText::AsNumber(ShopId)));
		return EDataValidationResult::Invalid;
	}
	if (Shop->ShopId != ShopId)
	{
		Context.AddError(FText::Format(NSLOCTEXT("NUShop", "MismatchedShopId", "상점 행의 ShopId가 조회 ID {0}과 일치하지 않습니다."), FText::AsNumber(ShopId)));
		Result = EDataValidationResult::Invalid;
	}
	if (Shop->ShopType != ENUShopType::Shop)
	{
		Context.AddError(FText::Format(NSLOCTEXT("NUShop", "UnsupportedValidatedShopType", "ShopId {0}은 일반 상점 타입이 아닙니다."), FText::AsNumber(ShopId)));
		Result = EDataValidationResult::Invalid;
	}
	if (ShopIdOverride <= 0 && Shop->NpcId > 0 && Shop->NpcId != NpcId)
	{
		Context.AddError(FText::Format(NSLOCTEXT("NUShop", "MismatchedShopNPC", "ShopId {0}의 NpcId가 Provider 소유 NPC와 일치하지 않습니다."), FText::AsNumber(ShopId)));
		Result = EDataValidationResult::Invalid;
	}
	if (NUShop::GetShopItems(ShopId).IsEmpty())
	{
		Context.AddWarning(FText::Format(NSLOCTEXT("NUShop", "EmptyShopItems", "ShopId {0}에 등록된 로컬 상품이 없습니다."), FText::AsNumber(ShopId)));
	}

	return Result;
}
#endif
