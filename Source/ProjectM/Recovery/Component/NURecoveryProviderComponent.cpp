#include "Recovery/Component/NURecoveryProviderComponent.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Interaction/Data/NUInteractionSettings.h"
#include "Inventory/NUInventoryComponent.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Net/EmberServerManager.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/GameplayEffect/NUGE_FullRecovery.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Recovery/Data/NURecoverySettings.h"
#include "Recovery/Data/NURecoveryTypes.h"
#include "Recovery/Component/NURecoveryComponent.h"
#include "Field/NPC/NUNPCBase.h"
#include "Field/NPC/Data/NUNPCDataSubsystem.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogNURecoveryProvider, Log, All);

namespace NURecoveryProvider
{
	const FName InstantRecoveryOptionId(TEXT("InstantRecovery"));
	const FName RestOptionId(TEXT("Rest"));

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<bool> CVarForcePaidRecovery(TEXT("nu.Recovery.Debug.ForcePaid"), false, TEXT("직업/나이아 계약 조건과 관계없이 유료 회복을 테스트한다."));
#endif
}

UNURecoveryProviderComponent::UNURecoveryProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	RecoveryEffectClass = UNUGE_FullRecovery::StaticClass();
}

void UNURecoveryProviderComponent::CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const
{
	if (!IsContextValid(Context))
	{
		return;
	}

	if (bOfferRest)
	{
		FNUInteractionOption& RestOption = OutOptions.AddDefaulted_GetRef();
		RestOption.Handle.OptionType = NU::Interaction::Option::Recovery;
		RestOption.Handle.OptionId = NURecoveryProvider::RestOptionId;
		RestOption.DisplayText = GetDefault<UNUInteractionSettings>()->RestOptionText;
		RestOption.CategoryText = GetDefault<UNUInteractionSettings>()->RecoveryCategoryText;
		RestOption.Group = ENUInteractionOptionGroup::Service;
		RestOption.SortOrder = SortOrder + 1;
		RestOption.bEnabled = true;
		RestOption.ExecutionDialogue = RestExecutionDialogue;
	}

	// 해금 전에는 즉시 회복 선택지를 노출하지 않는다. 무료 휴식은 계속 제공한다.
	if (!IsInstantRecoveryUnlocked(Context.Interactor)) { return; }

	const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();
	FNUInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Handle.OptionType = NU::Interaction::Option::Recovery;
	Option.Handle.OptionId = NURecoveryProvider::InstantRecoveryOptionId;
	Option.DisplayText = BuildOptionText(Context.Interactor);
	Option.CategoryText = Settings->RecoveryCategoryText;
	Option.Group = ENUInteractionOptionGroup::Service;
	Option.SortOrder = SortOrder;
	Option.bEnabled = true;
	Option.ExecutionDialogue = InstantExecutionDialogue;
}

bool UNURecoveryProviderComponent::CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const
{
	if (!IsContextValid(Context)) { return false; }
	if (Handle.OptionType == NU::Interaction::Option::Recovery && Handle.OptionId == NURecoveryProvider::RestOptionId)
	{
		return bOfferRest;
	}
	// 선택지가 남아 있더라도 실행 시점의 퀘스트 상태를 다시 확인한다.
	return IsInstantRecoveryHandle(Handle) && IsInstantRecoveryUnlocked(Context.Interactor);
}

bool UNURecoveryProviderComponent::ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle)
{
	if (!CanExecuteInteraction_Implementation(Context, Handle) || !Context.Interactor->HasAuthority())
	{
		return false;
	}

	ANUPlayerState* PlayerState = FindPlayerState(Context.Interactor);
	UNURecoveryComponent* Recovery = IsValid(PlayerState) ? PlayerState->FindComponentByClass<UNURecoveryComponent>() : nullptr;
	if (!Recovery) { return false; }
	if (Recovery->IsUsingExternalServer())
	{
		const ANUNPCBase* NPC = Cast<ANUNPCBase>(GetOwner());
		return NPC && Recovery->RequestServerRecovery(NPC->GetNpcId(), Handle.OptionId == NURecoveryProvider::RestOptionId, InsufficientGoldDialogue);
	}
	if (Handle.OptionId == NURecoveryProvider::RestOptionId)
	{
		const bool bStarted = Recovery->StartRest(GetOwner());
		if (!bStarted) { if (ANUPlayerController* PC = FindPlayerController(Context.Interactor)) { PC->ClientRecoveryNotice(NSLOCTEXT("NURecovery", "RestUnavailable", "현재 휴식을 시작할 수 없습니다.")); } }
		return bStarted;
	}
	if (!IsInstantRecoveryAvailable(Context.Interactor))
	{
		if (ANUPlayerController* PC = FindPlayerController(Context.Interactor)) { PC->ClientRecoveryNotice(GetUnavailableReason(Context.Interactor)); }
		return false;
	}
	UNUInventoryComponent* Inventory = IsValid(PlayerState) ? PlayerState->GetInventoryComponent() : nullptr;
	int32 RecoveryCost = 0;
	if (!TryGetRecoveryCost(Context.Interactor, RecoveryCost) || !IsValid(Inventory))
	{
		return false;
	}

	if (RecoveryCost > 0 && Inventory->GetGold() < RecoveryCost)
	{
		if (ANUPlayerController* PC = FindPlayerController(Context.Interactor))
		{
			if (InsufficientGoldDialogue) { PC->ClientOpenStandaloneDialogue(InsufficientGoldDialogue); }
			else { PC->ClientRecoveryNotice(NSLOCTEXT("NURecovery", "InsufficientGold", "골드가 부족합니다.")); }
		}
		return false;
	}
	if (!Recovery->RecoverHealth(1.0f, RecoveryEffectClass, GetOwner())) { return false; }
	UNUSFXSubsystem::Play(this, NU::SFX::Field::Heal);
	Recovery->StopRest();
	if (RecoveryCost > 0)
	{
		Inventory->AddGold(-RecoveryCost);
	}

	UE_LOG(LogNURecoveryProvider, Log, TEXT("즉시 회복 완료. Cost=%d Player=%s NPC=%s"), RecoveryCost, *GetNameSafe(PlayerState), *GetNameSafe(GetOwner()));
	return true;
}

bool UNURecoveryProviderComponent::IsInstantRecoveryAvailable(AActor* Interactor) const
{
	if (!IsInstantRecoveryUnlocked(Interactor) || !RecoveryEffectClass) { return false; }
	const ANUPlayerState* PS = FindPlayerState(Interactor);
	const UNURecoveryComponent* Recovery = PS->FindComponentByClass<UNURecoveryComponent>();
	int32 Cost = 0;
	return Recovery && Recovery->CanRecoverHealth() && IsValid(PS->GetInventoryComponent()) && TryGetRecoveryCost(Interactor, Cost);
}

int32 UNURecoveryProviderComponent::GetRecoveryCost(AActor* Interactor) const
{
	int32 RecoveryCost = 0;
	return TryGetRecoveryCost(Interactor, RecoveryCost) ? RecoveryCost : 0;
}

bool UNURecoveryProviderComponent::IsInstantRecoveryUnlocked(AActor* Interactor) const
{
	const ANUPlayerState* PS = FindPlayerState(Interactor);
	return bOfferInstantRecovery && IsLocationServiceUnlocked(Interactor) && IsValid(PS) && PS->GetQuestComponent() && PS->GetQuestComponent()->HasCompletedQuests(RequiredQuestIds);
}

bool UNURecoveryProviderComponent::ShouldShowRecoveryPrompt(AActor* Interactor) const
{
	const ANUPlayerState* PS = FindPlayerState(Interactor);
	if (!bUseRecoveryPrompt || !IsValid(PS))
	{
		return false;
	}

	// 회복 대상과 같은 범위(플레이어, 동행 나이아, 보관 나이아)를 본다.
	TArray<const UNUAbilitySystemComponent*> Targets;
	Targets.Add(PS->GetNUASC());
	const UNUNaiaRosterComponent* Roster = PS->GetNaiaRosterComponent();
	if (IsValid(Roster))
	{
		for (const ANUSpiritState* Spirit : Roster->GetParty())
		{
			if (IsValid(Spirit))
			{
				Targets.Add(Spirit->GetNUASC());
			}
		}

		for (const FNUNaiaInstance& Entry : Roster->GetStorage())
		{
			if (Entry.HealthRatio < 1.0f)
			{
				return true;
			}
		}
	}

	for (const UNUAbilitySystemComponent* ASC : Targets)
	{
		if (IsValid(ASC) && ASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute()) < ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()))
		{
			return true;
		}
	}

	return false;
}

bool UNURecoveryProviderComponent::IsLocationServiceUnlocked(AActor* Interactor) const
{
	const ANUNPCBase* NPC = Cast<ANUNPCBase>(GetOwner());
	// 테이블을 쓰지 않는 기존 테스트 액터는 자체 RequiredQuestIds 설정을 유지한다.
	if (NPC == nullptr || NPC->GetNpcId() <= 0) { return true; }
	const UGameInstance* Instance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUNPCDataSubsystem* NpcData = Instance != nullptr ? Instance->GetSubsystem<UNUNPCDataSubsystem>() : nullptr;
	const FNUNPCDefinition* Definition = NpcData != nullptr ? NpcData->FindDefinition(NPC->GetNpcId()) : nullptr;
	const ANUPlayerState* PS = FindPlayerState(Interactor);
	const UNUWorldLocationComponent* Locations = PS != nullptr ? PS->FindComponentByClass<UNUWorldLocationComponent>() : nullptr;
	return Definition != nullptr && (Definition->LocationId <= 0 || (Locations != nullptr && Locations->IsLocationUnlocked(Definition->LocationId)));
}

UNUDialogueDefinition* UNURecoveryProviderComponent::GetRecoveryPromptDialogue(AActor* Interactor) const
{
	if (!IsInstantRecoveryUnlocked(Interactor)) { return RestOnlyDialogue; }
	int32 Cost = 0;
	if (!TryGetRecoveryCost(Interactor, Cost)) { return RestOnlyDialogue; }
	return Cost > 0 ? PaidRecoveryDialogue.Get() : FreeRecoveryDialogue.Get();
}

bool UNURecoveryProviderComponent::IsContextValid(const FNUInteractionContext& Context) const
{
	const ANUPlayerState* PS = FindPlayerState(Context.Interactor);
	const APawn* Pawn = IsValid(PS) ? PS->GetPawn() : nullptr;
	return Context.IsValid() && Context.Target.Get() == GetOwner() && IsValid(Pawn) && IsLocationServiceUnlocked(Context.Interactor) && FMath::IsFinite(ServiceRadius) && ServiceRadius > 0.0f
		&& FVector::DistSquared(Pawn->GetActorLocation(), GetOwner()->GetActorLocation()) <= FMath::Square(ServiceRadius);
}

bool UNURecoveryProviderComponent::IsInstantRecoveryHandle(const FNUInteractionOptionHandle& Handle) const
{
	return Handle.OptionType == NU::Interaction::Option::Recovery && Handle.OptionId == NURecoveryProvider::InstantRecoveryOptionId;
}


bool UNURecoveryProviderComponent::TryGetRecoveryCost(AActor* Interactor, int32& OutCost) const
{
	OutCost = 0;
	const ANUPlayerState* PlayerState = FindPlayerState(Interactor);
	const UNURecoveryComponent* Recovery = IsValid(PlayerState) ? PlayerState->FindComponentByClass<UNURecoveryComponent>() : nullptr;
	if (!Recovery) { return false; }
	if (!Recovery->IsPaidRecovery())
	{
		return IsValid(PlayerState);
	}

	const UNUProgressionComponent* Progression = PlayerState->GetProgressionComponent();
	const UNURecoverySettings* Settings = GetDefault<UNURecoverySettings>();
	FNURecoveryCostRow Row;
	if (!IsValid(Progression) || !IsValid(Settings) || !Settings->TryGetRecoveryCostRow(Progression->GetLevel(), Row))
	{
		return false;
	}

	OutCost = Row.RecoveryCost;
	return true;
}


FText UNURecoveryProviderComponent::BuildOptionText(AActor* Interactor) const
{
	const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();
	const int32 RecoveryCost = GetRecoveryCost(Interactor);
	return RecoveryCost > 0 ? FText::Format(Settings->PaidRecoveryOptionFormat, FText::AsNumber(RecoveryCost)) : Settings->InstantRecoveryOptionText;
}

FText UNURecoveryProviderComponent::GetUnavailableReason(AActor* Interactor) const
{
	const ANUPlayerState* PlayerState = FindPlayerState(Interactor);
	const UNUQuestComponent* Quest = IsValid(PlayerState) ? PlayerState->GetQuestComponent() : nullptr;
	if (!RequiredQuestIds.IsEmpty() && (Quest == nullptr || !Quest->HasCompletedQuests(RequiredQuestIds)))
	{
		return NSLOCTEXT("NURecovery", "QuestLocked", "선행 퀘스트를 완료해야 이용할 수 있습니다.");
	}
	if (!IsValid(PlayerState))
	{
		return NSLOCTEXT("NURecovery", "PlayerStateUnavailable", "플레이어 상태를 확인할 수 없습니다.");
	}
	if (!IsValid(PlayerState->GetNUASC()) || !PlayerState->GetNUASC()->IsAlive())
	{
		return NSLOCTEXT("NURecovery", "DeadPlayer", "전투불능 상태에서는 부활 절차를 이용해야 합니다.");
	}

	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleNetSubsystem* BattleNet = IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
	if ((IsValid(BattleNet) && BattleNet->IsInBattle()) || IsValid(PlayerState->GetNUASC()->GetBattleEncounter()))
	{
		return NSLOCTEXT("NURecovery", "BattleBlocked", "전투 중에는 이용할 수 없습니다.");
	}

	const UEmberServerManager* ServerManager = IsValid(GameInstance) ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
	if (IsValid(ServerManager) && ServerManager->IsLoggedIn())
	{
		return NSLOCTEXT("NURecovery", "ServerContractMissing", "회복 서버 연동이 준비되지 않았습니다.");
	}

	const UNUInventoryComponent* Inventory = PlayerState->GetInventoryComponent();
	int32 RecoveryCost = 0;
	if (!TryGetRecoveryCost(Interactor, RecoveryCost))
	{
		return NSLOCTEXT("NURecovery", "CostDataUnavailable", "회복 비용 정보를 확인할 수 없습니다.");
	}
	if (IsValid(Inventory) && Inventory->GetGold() < RecoveryCost)
	{
		return NSLOCTEXT("NURecovery", "InsufficientGold", "골드가 부족합니다.");
	}
	return NSLOCTEXT("NURecovery", "Unavailable", "현재 회복 서비스를 이용할 수 없습니다.");
}

ANUPlayerController* UNURecoveryProviderComponent::FindPlayerController(AActor* Interactor) const
{
	if (ANUPlayerController* PlayerController = Cast<ANUPlayerController>(Interactor))
	{
		return PlayerController;
	}

	const APawn* Pawn = Cast<APawn>(Interactor);
	return IsValid(Pawn) ? Cast<ANUPlayerController>(Pawn->GetController()) : nullptr;
}

ANUPlayerState* UNURecoveryProviderComponent::FindPlayerState(AActor* Interactor) const
{
	ANUPlayerController* PlayerController = FindPlayerController(Interactor);
	return IsValid(PlayerController) ? PlayerController->GetPlayerState<ANUPlayerState>() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UNURecoveryProviderComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();
	if (!IsValid(Settings) || Settings->InstantRecoveryOptionText.IsEmpty() || Settings->PaidRecoveryOptionFormat.IsEmpty() || Settings->RecoveryCategoryText.IsEmpty())
	{
		Context.AddError(NSLOCTEXT("NURecovery", "MissingInteractionText", "NU Interaction 설정의 회복 문구가 비어 있습니다."));
		Result = EDataValidationResult::Invalid;
	}
	if (!RecoveryEffectClass)
	{
		Context.AddError(NSLOCTEXT("NURecovery", "MissingRecoveryEffect", "회복 GameplayEffect 클래스가 비어 있습니다."));
		Result = EDataValidationResult::Invalid;
	}
	const UNURecoverySettings* RecoverySettings = GetDefault<UNURecoverySettings>();
	FString RecoveryTableError;
	if (!IsValid(RecoverySettings) || !RecoverySettings->ValidateRecoveryCostTable(RecoveryTableError))
	{
		Context.AddError(FText::FromString(RecoveryTableError));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
