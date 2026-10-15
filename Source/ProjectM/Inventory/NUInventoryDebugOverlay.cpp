// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUInventoryDebugOverlay.h"

#if !UE_BUILD_SHIPPING

#include "Blueprint/WidgetLayoutLibrary.h"
#include "CharacterState/NUPlayerState.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "PlayerController/NUPlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SNUInventoryDebugOverlay::Construct(const FArguments& InArgs, UWorld* InWorld)
{
	World = InWorld;

	ChildSlot
	[
		SNew(SDPIScaler)
		.DPIScale(this, &SNUInventoryDebugOverlay::GetViewportScale)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.f, 80.f, 24.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.85f))
				.Padding(FMargin(16.f, 12.f))
				[
					SNew(SBox)
					.MinDesiredWidth(300.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 0.f, 0.f, 4.f)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
							.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.4f))
							.Text(FText::FromString(TEXT("인벤토리 [I]")))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 0.f, 0.f, 8.f)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Mono", 14))
							.ColorAndOpacity(FLinearColor::White)
							.Text(this, &SNUInventoryDebugOverlay::GetStatusText)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(ItemListBox, SVerticalBox)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 8.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
							.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
							.Text(FText::FromString(TEXT("클릭 = 사용 · 전투 중엔 전투 메뉴로")))
						]
					]
				]
			]
		]
	];
}

void SNUInventoryDebugOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	UNUInventoryComponent* Inventory = NUInventoryDebugUI::FindLocalInventory(World.Get(), /*bWarnIfMissing=*/false);
	const bool bInBattle = NUInventoryDebugUI::IsLocalPlayerInBattle(World.Get());

	// 변경 감지용 시그니처 — 달라진 프레임에만 버튼 목록 재구성
	FString Signature = bInBattle ? TEXT("B|") : TEXT("F|");
	if (Inventory != nullptr)
	{
		for (const FNUItemInstance& Item : Inventory->GetItems())
		{
			Signature += FString::Printf(TEXT("%s:%d;"), *Item.ItemRowName.ToString(), Item.Count);
		}
	}
	if (Signature != CachedSignature)
	{
		CachedSignature = Signature;
		RebuildItemList(Inventory, bInBattle);
	}
}

void SNUInventoryDebugOverlay::RebuildItemList(UNUInventoryComponent* Inventory, bool bInBattle)
{
	if (!ItemListBox.IsValid())
	{
		return;
	}
	ItemListBox->ClearChildren();

	if (Inventory == nullptr)
	{
		ItemListBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 14))
			.ColorAndOpacity(FLinearColor(0.7f, 0.5f, 0.5f))
			.Text(FText::FromString(TEXT("인벤토리 없음 (PlayerState 확인)")))
		];
		return;
	}

	const TArray<FNUItemInstance>& Items = Inventory->GetItems();
	if (Items.IsEmpty())
	{
		ItemListBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 14))
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
			.Text(FText::FromString(TEXT("(비어 있음)")))
		];
		return;
	}

	for (const FNUItemInstance& Item : Items)
	{
		const FNUItemDefRow* Def = Inventory->FindItemDef(Item.ItemRowName);
		const FString Label = FString::Printf(TEXT("%s x%d"),
			Def ? *Def->DisplayName.ToString() : *Item.ItemRowName.ToString(),
			Item.Count);

		// 필드 사용 가능 조건과 동일하게 버튼 활성화 (서버 검증은 ServerUseItem이 다시 함)
		const bool bUsable = !bInBattle
			&& Def != nullptr
			&& Def->ItemType == ENUItemType::Consumable
			&& Def->bUsableInField
			&& Item.Count > 0;

		ItemListBox->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SButton)
			.IsEnabled(bUsable)
			.OnClicked(this, &SNUInventoryDebugOverlay::HandleItemClicked, Item.InstanceId)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 14))
				.ColorAndOpacity(bUsable ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.45f))
				.Text(FText::FromString(Label))
			]
		];
	}
}

FReply SNUInventoryDebugOverlay::HandleItemClicked(FGuid InstanceId)
{
	if (UNUInventoryComponent* Inventory = NUInventoryDebugUI::FindLocalInventory(World.Get(), /*bWarnIfMissing=*/false))
	{
		Inventory->ServerUseItem(InstanceId);
	}
	return FReply::Handled();
}

float SNUInventoryDebugOverlay::GetViewportScale() const
{
	const UWorld* CurrentWorld = World.Get();
	return CurrentWorld ? UWidgetLayoutLibrary::GetViewportScale(CurrentWorld->GetGameViewport()) : 1.f;
}

FText SNUInventoryDebugOverlay::GetStatusText() const
{
	UNUInventoryComponent* Inventory = NUInventoryDebugUI::FindLocalInventory(World.Get(), /*bWarnIfMissing=*/false);
	const ANUPlayerState* OwnerState = Inventory ? Cast<ANUPlayerState>(Inventory->GetOwner()) : nullptr;
	const UNUAbilitySystemComponent* ASC = OwnerState ? OwnerState->GetNUASC() : nullptr;
	const UNUVitalAttributeSet* Attributes = ASC ? ASC->GetVitalAttributeSet() : nullptr;
	if (Attributes == nullptr)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("HP %.0f / %.0f    SP %.0f / %.0f"),
		Attributes->GetHealth(), Attributes->GetMaxHealth(),
		Attributes->GetSP(), Attributes->GetMaxSP()));
}

/////////////////////////////////////////////////////////////////
// 토글 관리 (월드별)
/////////////////////////////////////////////////////////////////

namespace
{
	const FName CursorReason_DebugOverlay(TEXT("DebugOverlay"));
	
	TMap<FObjectKey, TSharedPtr<SNUInventoryDebugOverlay>> GNUInventoryOverlays;
	bool bGNUInventoryCleanupHooked = false;

	// 필드에서 열면 커서·UI 입력 모드로 전환, 닫으면 원복.
	void ApplyFieldCursorMode(UWorld* World, bool bOverlayVisible)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ANUPlayerController* NUPC = PC ? Cast<ANUPlayerController>(PC) : nullptr;
		if (!IsValid(NUPC)) return;
		
		NUPC->RequestCursor(CursorReason_DebugOverlay, bOverlayVisible, nullptr);
	}
}

UNUInventoryComponent* NUInventoryDebugUI::FindLocalInventory(UWorld* World, bool bWarnIfMissing)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UNUInventoryComponent* Inventory = PS ? PS->FindComponentByClass<UNUInventoryComponent>() : nullptr;
	if (Inventory == nullptr && bWarnIfMissing)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv] 인벤토리 컴포넌트 없음 — PlayerState가 ANUPlayerState(+부착 수정)인지 확인"));
	}
	return Inventory;
}

bool NUInventoryDebugUI::IsLocalPlayerInBattle(UWorld* World)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const ANUPlayerState* PS = PC ? Cast<ANUPlayerState>(PC->PlayerState) : nullptr;
	const UNUAbilitySystemComponent* ASC = PS ? PS->GetNUASC() : nullptr;
	return ASC != nullptr && ASC->GetBattleEncounter() != nullptr;
}

void NUInventoryDebugUI::Toggle(UWorld* World)
{
	if (World == nullptr)
	{
		return;
	}
	UGameViewportClient* Viewport = World->GetGameViewport();
	if (Viewport == nullptr)
	{
		UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.UI] 게임 뷰포트 없음 — PIE 중에만 사용 가능"));
		return;
	}

	const FObjectKey WorldKey(World);
	if (const TSharedPtr<SNUInventoryDebugOverlay>* Existing = GNUInventoryOverlays.Find(WorldKey))
	{
		Viewport->RemoveViewportWidgetContent(Existing->ToSharedRef());
		GNUInventoryOverlays.Remove(WorldKey);
		ApplyFieldCursorMode(World, false);
		return;
	}

	const TSharedPtr<SNUInventoryDebugOverlay> Overlay = SNew(SNUInventoryDebugOverlay, World);
	Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 100);
	GNUInventoryOverlays.Add(WorldKey, Overlay);
	ApplyFieldCursorMode(World, true);

	// PIE 종료 시 맵에 남은 위젯 참조 정리
	if (!bGNUInventoryCleanupHooked)
	{
		bGNUInventoryCleanupHooked = true;
		FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* CleanupWorld, bool, bool)
		{
			GNUInventoryOverlays.Remove(FObjectKey(CleanupWorld));
		});
	}
}

#endif // !UE_BUILD_SHIPPING
