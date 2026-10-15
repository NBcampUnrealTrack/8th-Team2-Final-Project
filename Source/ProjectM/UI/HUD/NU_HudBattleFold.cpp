#include "UI/HUD/NU_HudBattleFold.h"

#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "CharacterState/NUPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NUGAS/NUAbilitySystemComponent.h"

bool FNUHudBattleFold::IsBattleUIActive(const UUserWidget* Owner)
{
	if (Owner == nullptr)
	{
		return false;
	}
	const APlayerController* PC = Owner->GetOwningPlayer();
	const ANUPlayerState* PS = PC != nullptr ? Cast<ANUPlayerState>(PC->PlayerState) : nullptr;
	const UNUAbilitySystemComponent* ASC = PS != nullptr ? PS->GetNUASC() : nullptr;
	if (ASC != nullptr && ASC->GetBattleEncounter() != nullptr)
	{
		return true;
	}
	const UGameInstance* GameInstance = Owner->GetGameInstance();
	const UNUBattleNetSubsystem* BattleNet = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
	if (BattleNet != nullptr && BattleNet->IsInBattle())
	{
		return true;
	}
	const UWorld* World = Owner->GetWorld();
	const UNUBattleNetStageSubsystem* StageNet = World != nullptr ? World->GetSubsystem<UNUBattleNetStageSubsystem>() : nullptr;
	return StageNet != nullptr && StageNet->IsNetStageReady();
}

void FNUHudBattleFold::Tick(UUserWidget* Owner, float DeltaTime)
{
	UWidget* Root = Owner != nullptr ? Owner->GetRootWidget() : nullptr;
	if (Root == nullptr)
	{
		return;
	}
	if (!bCaptured)
	{
		RestVisibility = Root->GetVisibility();
		bCaptured = true;
	}
	const float Target = IsBattleUIActive(Owner) ? 0.f : 1.f;
	if (Alpha == Target)
	{
		return;
	}
	// 접힐 때는 빨리 (전투 진입 연출을 가리지 않게), 펼 때는 조금 천천히
	Alpha = FMath::FInterpConstantTo(Alpha, Target, DeltaTime, Target < Alpha ? 6.f : 3.f);
	Root->SetRenderOpacity(Alpha);
	Root->SetVisibility(Alpha <= 0.f ? ESlateVisibility::Collapsed : RestVisibility);
}
