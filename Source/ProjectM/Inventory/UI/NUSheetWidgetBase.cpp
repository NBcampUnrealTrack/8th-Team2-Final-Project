// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/UI/NUSheetWidgetBase.h"
#include "Inventory/UI/NUItemCellWidget.h"

#include "Inventory/UI/NUItemTooltipWidget.h"
#include "Inventory/UI/NUItemUI.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Field/NUCharacterPreviewActor.h"
#include "Field/NUCharacterPreviewSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUInventoryWindowWidget.h" // UNUItemDragOperation
#include "Inventory/UI/NUSplitStackDialog.h"
#include "Shop/NUShopTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUUIGeometry.h"
#include "UI/NUUIManager.h" // NUUILayer
#include "UI/NUUISettings.h"
#include "PlayerController/NUPlayerController.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "TimerManager.h"

/////////////////////////////////////////////////////////////////
// UNUDragVisualWidget --- 커서를 따라다니는 담개
/////////////////////////////////////////////////////////////////

UNUDragVisualWidget* UNUDragVisualWidget::Show(APlayerController* Player, UWidget* InContent, float Size, bool bDrawFrame)
{
	// 엔진 데코레이터(DefaultDragVisual)는 감지 위젯 원점에서 커서까지 150ms 날아오는
	// 보간이 하드코딩돼 있어 쓰지 않는다. 뷰포트 최상단에 따로 올리고 틱으로 커서에 고정.
	// 창 안에 두면 캔버스 요구 크기가 커서를 따라 변해 창이 흔들린다
	UNUDragVisualWidget* Visual = InContent != nullptr ? CreateWidget<UNUDragVisualWidget>(Player, StaticClass()) : nullptr;
	if (Visual == nullptr)
	{
		return nullptr;
	}
	Visual->PendingContent = InContent;
	Visual->bDrawFrame = bDrawFrame;
	Visual->SetVisibility(ESlateVisibility::HitTestInvisible); // 드롭 판정을 가리지 않게
	Visual->AddToViewport(NUUILayer::Menu);                    // 툴팁과 같은 최상단 밴드
	// 뷰포트 슬롯은 크기를 스스로 잡지 않는다 --- 안 주면 폭 0으로 붙어 보이지 않는다
	Visual->SetDesiredSizeInViewport(FVector2D(Size, Size));
	Visual->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));     // 커서가 비주얼 가운데
	Visual->FollowCursor();
	UNUSFXSubsystem::Play(Player, NU::SFX::Item::PickUp);
	return Visual;
}

TSharedRef<SWidget> UNUDragVisualWidget::RebuildWidget()
{
	// WBP 없이 쓰는 위젯이라 루트를 코드로 만든다 (공용 딤과 같은 방식)
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrushColor(bDrawFrame ? FLinearColor(0.22f, 0.24f, 0.38f, 0.9f) : FLinearColor::Transparent);
		Frame->SetPadding(FMargin(0.f));
		WidgetTree->RootWidget = Frame;
	}

	// 루트가 생긴 지금이 맡아둔 내용을 붙일 시점이다 (그 전에 붙이면 슬레이트에 반영되지 않는다).
	// 내용은 제 크기로 배치한 뒤 담개 크기에 맞춰 비율대로 그린다 (크기가 같으면 그대로)
	if (Frame != nullptr && PendingContent != nullptr && PendingContent->GetParent() == nullptr)
	{
		UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
		Fit->SetStretch(EStretch::ScaleToFit);
		Fit->AddChild(PendingContent);
		Frame->ClearChildren();
		Frame->AddChild(Fit);
	}
	return Super::RebuildWidget();
}

void UNUDragVisualWidget::NativeDestruct()
{
	// 끌기가 어떻게 끝나든(칸에 놓기·밖에 놓기·취소) 그림은 여기서 내려간다 --- 놓는 소리를 한 곳에서 낸다
	UNUSFXSubsystem::Play(this, NU::SFX::Item::Drop);
	Super::NativeDestruct();
}

void UNUDragVisualWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 끌기가 끝났는데 남아 있으면 스스로 내린다 (놓은 곳이 부른 쪽이 아닌 다른 위젯이면 부른 쪽은 끝난 줄 모를 수 있다)
	if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsDragDropping())
	{
		RemoveFromParent();
		return;
	}
	FollowCursor();
}

void UNUDragVisualWidget::FollowCursor()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	// UMG 드래그 중에는 뷰포트가 마우스 좌표를 갱신하지 않아 PC->GetMousePosition이 멎는다.
	// 슬레이트 데스크톱 좌표를 받아 뷰포트 픽셀 좌표로 바꿔 넘긴다
	FVector2D PixelPos = FVector2D::ZeroVector;
	FVector2D ViewportPos = FVector2D::ZeroVector;
	USlateBlueprintLibrary::AbsoluteToViewport(this, FSlateApplication::Get().GetCursorPos(), PixelPos, ViewportPos);
	SetPositionInViewport(PixelPos); // 기본 인자가 픽셀 좌표에서 DPI를 걷어낸다
}

/////////////////////////////////////////////////////////////////
// UNUSheetWidgetBase
/////////////////////////////////////////////////////////////////

void UNUSheetWidgetBase::RequestClose()
{
	CloseSelf();
}

void UNUSheetWidgetBase::ShowDragVisual(UWidget* InContent, float InCellSize)
{
	ClearDragVisual();
	DragVisualWidget = UNUDragVisualWidget::Show(GetOwningPlayer(), InContent, InCellSize);
}

void UNUSheetWidgetBase::ClearDragVisual()
{
	if (DragVisualWidget != nullptr)
	{
		DragVisualWidget->RemoveFromParent(); // 뷰포트에 직접 올렸으므로 여기서 내린다
		DragVisualWidget = nullptr;
	}
}

void UNUSheetWidgetBase::UpdateGoldText()
{
	UTextBlock* GoldText = FindTextByName(TEXT("GoldText"));
	if (GoldText == nullptr)
	{
		return;
	}
	GoldText->SetText(FText::AsNumber(Inventory.IsValid() ? Inventory->GetGold() : 0));
}

void UNUSheetWidgetBase::OpenSellCountDialog(const FGuid& InstanceId, int32 ShopId)
{
	const FNUShopRow* Shop = NUShop::FindShopRow(ShopId);
	const FNUItemInstance* Instance = Inventory.IsValid() ? Inventory->FindInstance(InstanceId) : nullptr;
	const FNUItemDefRow* Def = Instance != nullptr ? Inventory->FindItemDef(Instance->ItemRowName) : nullptr;
	if (Shop == nullptr || Instance == nullptr || Def == nullptr)
	{
		return;
	}

	// 못 파는 품목은 수량을 묻기 전에 알린다 (실행부 검사와 별개 --- 창 열고 거절하면 헛걸음)
	const int32 UnitPrice = NUShop::GetSellPrice(*Def, *Shop);
	if (!NUItem::IsSellable(Def->ItemType) || UnitPrice <= 0)
	{
		if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
		{
			UI->ShowNotice(NSLOCTEXT("NUShop", "CannotSell", "판매할 수 없는 아이템입니다."));
		}
		return;
	}

	UNUSplitStackDialog::ShowCount(this, Instance->Count, 1,
		FText::Format(NSLOCTEXT("NUShop", "SellPrompt", "[{0}] 몇 개를 판매할까요? (보유 {1}개)"),
			Def->DisplayName, FText::AsNumber(Instance->Count)),
		NSLOCTEXT("NUShop", "SellAction", "판매"), UnitPrice,
		FOnNUSplitConfirmed::CreateUObject(this, &UNUSheetWidgetBase::HandleSellCountConfirmed, InstanceId, ShopId));
}

void UNUSheetWidgetBase::HandleSellCountConfirmed(int32 Count, FGuid InstanceId, int32 ShopId)
{
	const FNUShopRow* Shop = NUShop::FindShopRow(ShopId);
	if (Shop != nullptr && Inventory.IsValid())
	{
		Inventory->ShopSellItem(InstanceId, Count, *Shop);
	}
}

bool UNUSheetWidgetBase::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// 아이템 드래그를 삼켜 "아무도 안 받음 = 창 밖 = 버리기" 판정에서 뺀다.
	// 창 위에 놓은 것은 배치 취소이지 버리기 의사가 아니다
	if (Cast<UNUItemDragOperation>(InOperation) != nullptr)
	{
		return true;
	}
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

FReply UNUSheetWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 여기까지 버블됐다 = 버튼/스크롤이 소비하지 않은 창 영역 클릭 (빈 캔버스는 히트 테스트 안 됨)
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDraggingWindow = true;
		DragStartScreenPos = InMouseEvent.GetScreenSpacePosition();
		DragStartTranslation = WindowTranslation;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UNUSheetWidgetBase::SetWindowTranslation(const FVector2D& InTranslation)
{
	WindowTranslation = InTranslation;
	SetRenderTranslation(InTranslation);
}

FReply UNUSheetWidgetBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingWindow)
	{
		// 스크린 좌표 델타를 DPI 스케일로 나눠 위젯 로컬 이동량으로 변환
		const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
		WindowTranslation = DragStartTranslation + (InMouseEvent.GetScreenSpacePosition() - DragStartScreenPos) / Scale;
		SetRenderTranslation(WindowTranslation);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UNUSheetWidgetBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingWindow && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDraggingWindow = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UNUSheetWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 닫기 버튼은 창마다 타입이 다르다 --- 순수 Button(레거시), UNU_CloseButtonWidget, UNU_CommonButtonWidget
	if (UButton* CloseButtonPure = Cast<UButton>(CloseButton))
	{
		CloseButtonPure->OnClicked.RemoveDynamic(this, &UNUSheetWidgetBase::HandleCloseClicked);
		CloseButtonPure->OnClicked.AddDynamic(this, &UNUSheetWidgetBase::HandleCloseClicked);
	}
	else if (UNU_CloseButtonWidget* CloseButtonWidget = Cast<UNU_CloseButtonWidget>(CloseButton))
	{
		CloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &UNUSheetWidgetBase::HandleCloseClicked);
		CloseButtonWidget->OnCloseClicked.AddDynamic(this, &UNUSheetWidgetBase::HandleCloseClicked);
	}
	else if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(CloseButton))
	{
		CommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &UNUSheetWidgetBase::HandleCloseClicked);
		CommonButtonWidget->OnCommonClicked.AddDynamic(this, &UNUSheetWidgetBase::HandleCloseClicked);
	}

	if (!ResolveComponents())
	{
		return;
	}

	// 재오픈 중복 구독 방지 — 해제 선행 후 구독
	Inventory->OnInventoryChanged.RemoveDynamic(this, &UNUSheetWidgetBase::HandleInventoryChanged);
	Inventory->OnInventoryChanged.AddDynamic(this, &UNUSheetWidgetBase::HandleInventoryChanged);
	Equipment->OnEquipmentChanged.RemoveDynamic(this, &UNUSheetWidgetBase::HandleEquipmentChanged);
	Equipment->OnEquipmentChanged.AddDynamic(this, &UNUSheetWidgetBase::HandleEquipmentChanged);

	for (const FGameplayAttribute& Attribute : GetWatchedAttributes())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
		ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UNUSheetWidgetBase::HandleAttributeChanged);
	}
	Inventory->OnOwnerBattleChanged.RemoveAll(this);
	Inventory->OnOwnerBattleChanged.AddUObject(this, &UNUSheetWidgetBase::HandleOwnerBattleChanged);

	// 구독 직후 1회 수동 동기화 (팀 관례)
	bInBattle = Inventory->IsOwnerInBattle();
	RefreshData();

}

void UNUSheetWidgetBase::NativeDestruct()
{
	if (Inventory.IsValid())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UNUSheetWidgetBase::HandleInventoryChanged);
		Inventory->OnOwnerBattleChanged.RemoveAll(this);
	}
	if (Equipment.IsValid())
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &UNUSheetWidgetBase::HandleEquipmentChanged);
	}
	if (ASC.IsValid())
	{
		for (const FGameplayAttribute& Attribute : GetWatchedAttributes())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
		}
	}

	// 창이 사라지면 미리보기 액터를 대여소에 돌려준다 (파괴하지 않는다 --- 다음에 다시 빌려 쓴다)
	if (PreviewActor != nullptr)
	{
		if (const UWorld* World = GetWorld())
		{
			if (UNUCharacterPreviewSubsystem* Pool = World->GetSubsystem<UNUCharacterPreviewSubsystem>())
			{
				Pool->Release(PreviewActor);
			}
		}
		PreviewActor = nullptr;
	}

	ClearDragVisual(); // 창과 별개로 뷰포트에 떠 있으므로 창이 닫힐 때 같이 내린다
	HideFocusTooltip();
	Super::NativeDestruct();
}

void UNUSheetWidgetBase::PopulateCharacterPreview()
{
	UPanelWidget* PreviewHost = FindPanelByName(TEXT("CharacterPreviewBG"));
	UWorld* World = GetWorld();
	if (PreviewHost == nullptr || WidgetTree == nullptr || World == nullptr)
	{
		return;
	}

	// 액터는 대여소에서 빌린다 --- 창을 여닫을 때마다 새로 만들지 않고,
	// 동시에 뜨는 창 수만큼만 존재하도록 상한이 걸려 있다.
	// 복제하지 않으므로 남의 화면에는 안 보이고, 캡처도 자기 자신만 찍는다
	if (!IsValid(PreviewActor))
	{
		if (UNUCharacterPreviewSubsystem* Pool = World->GetSubsystem<UNUCharacterPreviewSubsystem>())
		{
			PreviewActor = Pool->Acquire();
		}
	}
	if (!IsValid(PreviewActor))
	{
		return; // 못 빌렸으면 WBP의 자리표시 글자를 그대로 둔다
	}

	PreviewActor->RefreshEquipment(Equipment.Get(), Inventory.Get());

	UTextureRenderTarget2D* Target = PreviewActor->GetRenderTarget();
	if (Target == nullptr)
	{
		return;
	}

	// 이미지는 한 번만 만든다 --- Border는 자식이 하나뿐이라 이때 자리표시 글자가 빠진다.
	// 칸과 렌더 타깃의 비율이 달라도 캐릭터가 눌리지 않게 ScaleBox로 비율을 지킨다
	if (!IsValid(PreviewImage))
	{
		PreviewHost->ClearChildren();

		UScaleBox* AspectKeeper = WidgetTree->ConstructWidget<UScaleBox>();
		AspectKeeper->SetStretch(EStretch::ScaleToFit);
		PreviewHost->AddChild(AspectKeeper);

		PreviewImage = WidgetTree->ConstructWidget<UImage>();

		// 씬 캡처의 알파는 뒤집혀 있다(1 - 불투명도) --- 그대로 그리면 캐릭터가 구멍이 된다.
		// 알파를 뒤집어 읽는 UI 머티리얼을 거친다. 머티리얼이 없으면 RT를 직접 --- 배경이 검게 나온다
		FSlateBrush Brush;
		UMaterialInterface* PreviewMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Developers/dbals/UI/M_NUCharacterPreview.M_NUCharacterPreview"));
		if (PreviewMaterial != nullptr)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(PreviewMaterial, this);
			Mid->SetTextureParameterValue(TEXT("PreviewTexture"), Target);
			Brush.SetResourceObject(Mid);
		}
		else
		{
			UE_LOG(LogNUInventory, Warning,
				TEXT("미리보기 머티리얼 없음 (M_NUCharacterPreview) --- 렌더 타깃을 직접 그립니다"));
			Brush.SetResourceObject(Target);
		}
		Brush.ImageSize = FVector2D(Target->SizeX, Target->SizeY);
		PreviewImage->SetBrush(Brush);

		AspectKeeper->AddChild(PreviewImage);
	}
}

bool UNUSheetWidgetBase::ResolveComponents()
{
	if (Inventory.IsValid() && Equipment.IsValid() && ASC.IsValid())
	{
		return true;
	}

	const APlayerController* PC = GetOwningPlayer();
	const ANUPlayerState* PS = PC ? Cast<ANUPlayerState>(PC->PlayerState) : nullptr;
	if (PS == nullptr)
	{
		return false;
	}

	Inventory = PS->GetInventoryComponent();
	Equipment = PS->GetEquipmentComponent();
	ASC = PS->GetNUASC();
	return Inventory.IsValid() && Equipment.IsValid() && ASC.IsValid();
}

void UNUSheetWidgetBase::RequestRefresh()
{
	if (bRefreshQueued || GetWorld() == nullptr)
	{
		return;
	}
	bRefreshQueued = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bRefreshQueued = false;
		RefreshData();
	}));
}

UPanelWidget* UNUSheetWidgetBase::FindPanelByName(FName WidgetName) const
{
	return Cast<UPanelWidget>(GetWidgetFromName(WidgetName));
}

UTextBlock* UNUSheetWidgetBase::FindTextByName(FName WidgetName) const
{
	return Cast<UTextBlock>(GetWidgetFromName(WidgetName));
}

namespace
{
	// 0이 아닌 장비 스탯만 "이름  +값" 줄로 (스탯 목록과 표기 규칙은 NUItemUI).
	// Worn을 주면 차이를 함께 만든다. 이때는 한쪽만 0이어도 줄을 남긴다 (잃는 스탯도 보여야 한다)
	void CollectEquipStatLines(const FNUEquipStats& Stats, const FNUEquipStats* Worn, TArray<FNUTipStat>& OutLines)
	{
		const TArray<FNUEquipStatEntry> Entries = NUItemUI::GetEquipStatEntries(Stats);
		const TArray<FNUEquipStatEntry> WornEntries = NUItemUI::GetEquipStatEntries(Worn != nullptr ? *Worn : FNUEquipStats());
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const FNUEquipStatEntry& Entry = Entries[Index];
			const float WornValue = WornEntries[Index].Value;
			const bool bHasValue = !FMath::IsNearlyZero(Entry.Value);
			const bool bHasWorn = Worn != nullptr && !FMath::IsNearlyZero(WornValue);
			if (!bHasValue && !bHasWorn)
			{
				continue;
			}

			FNUTipStat& Line = OutLines.AddDefaulted_GetRef();
			Line.Text = FString::Printf(TEXT("%s  %s"), *Entry.Label.ToString(), *NUItemUI::FormatEquipStatValue(Entry.Value, Entry.bPercent));

			const float Diff = Entry.Value - WornValue;
			if (Worn != nullptr && !FMath::IsNearlyZero(Diff))
			{
				Line.Delta = FString::Printf(TEXT("(%s)"), *NUItemUI::FormatEquipStatValue(Diff, Entry.bPercent));
				Line.bGain = Diff > 0.f;
			}
		}
	}
}

int32 UNUSheetWidgetBase::GetMyLevel() const
{
	const ANUPlayerState* OwnerState = GetOwningPlayerState<ANUPlayerState>();
	const UNUProgressionComponent* Progression = OwnerState != nullptr ? OwnerState->GetProgressionComponent() : nullptr;
	return Progression != nullptr ? Progression->GetLevel() : 0;
}

int32 UNUSheetWidgetBase::GetMyJobId() const
{
	const ANUPlayerState* OwnerState = GetOwningPlayerState<ANUPlayerState>();
	const UNUJobComponent* Job = OwnerState != nullptr ? OwnerState->GetJobComponent() : nullptr;
	return Job != nullptr ? Job->GetJobId() : 0;
}

UWidget* UNUSheetWidgetBase::BuildItemTooltip(FName ItemRowName) const
{
	if (!Inventory.IsValid() || ItemRowName.IsNone())
	{
		return nullptr;
	}
	const FNUItemDefRow* Def = Inventory->FindItemDef(ItemRowName);
	const TSubclassOf<UNUItemTooltipWidget> TooltipClass = GetDefault<UNUUISettings>()->ItemTooltipClass.LoadSynchronous();
	if (Def == nullptr || TooltipClass == nullptr)
	{
		return nullptr;
	}

	// 장비 스탯 --- 같은 부위에 낀 것이 있으면 차이를 같이 만든다.
	// 이미 끼고 있는 그 장비를 볼 때는 비교할 대상이 자기 자신이라 뺀다
	TArray<FNUTipStat> Stats;
	FText WornName;
	if (Def->ItemType == ENUItemType::Equipment)
	{
		FName WornRowName;
		const FNUItemDefRow* Worn = FindWornEquipDef(Def->EquipType, WornRowName);
		const bool bCompare = Worn != nullptr && Worn != Def;
		CollectEquipStatLines(Def->EquipStats, bCompare ? &Worn->EquipStats : nullptr, Stats);
		if (bCompare)
		{
			WornName = Worn->DisplayName.IsEmpty() ? FText::FromName(WornRowName) : Worn->DisplayName;
		}
	}

	UNUItemTooltipWidget* Tooltip = CreateWidget<UNUItemTooltipWidget>(GetOwningPlayer(), TooltipClass);
	Tooltip->SetItem(*Def, ItemRowName, GetMyLevel(), GetMyJobId(), Stats, WornName);
	return Tooltip;
}

const FNUItemDefRow* UNUSheetWidgetBase::FindWornEquipDef(ENUEquipType EquipType, FName& OutRowName) const
{
	OutRowName = NAME_None;
	if (!Equipment.IsValid() || !Inventory.IsValid() || EquipType == ENUEquipType::None)
	{
		return nullptr;
	}

	// 실제로 이 아이템이 들어갈 칸을 묻는다 --- 반지처럼 후보가 둘인 부위도 장착 경로와 같은 답이 나온다
	const TArray<FNUEquippedItem>& WornItems = Equipment->GetEquippedItems();
	const ENUEquipSlot TargetSlot = NUEquip::ResolveTargetSlot(EquipType, WornItems);

	const int32 Index = static_cast<int32>(TargetSlot);
	if (TargetSlot == ENUEquipSlot::MAX || !WornItems.IsValidIndex(Index) || !WornItems[Index].InstanceId.IsValid())
	{
		return nullptr; // 그 부위가 비어 있다 --- 비교할 것이 없다
	}
	OutRowName = WornItems[Index].ItemRowName;
	return Inventory->FindItemDef(OutRowName);
}

UTexture2D* UNUSheetWidgetBase::LoadItemIconTexture(FName ItemRowName) const
{
	return NUItemUI::LoadItemIcon(ItemRowName);
}

UNUItemCellWidget* UNUSheetWidgetBase::MakeItemCell(float InCellSize)
{
	const TSubclassOf<UNUItemCellWidget> CellClass = GetDefault<UNUUISettings>()->ItemCellClass.LoadSynchronous();
	if (CellClass == nullptr)
	{
		return nullptr;
	}
	UNUItemCellWidget* Cell = CreateWidget<UNUItemCellWidget>(this, CellClass);
	Cell->SetCellSize(InCellSize);
	Cell->OnCellFocusChanged.BindUObject(this, &UNUSheetWidgetBase::HandleCellFocusChanged);
	return Cell;
}

void UNUSheetWidgetBase::RequestConfirm(const FText& InMessage, const FText& InConfirmLabel, FSimpleDelegate OnConfirmed)
{
	NUItemUI::RequestConfirm(GetOwningPlayer(), ConfirmDialogClass, InMessage, InConfirmLabel, MoveTemp(OnConfirmed));
}

void UNUSheetWidgetBase::HandleCloseClicked()
{
	CloseSelf();
}

void UNUSheetWidgetBase::HandleInventoryChanged()
{
	RequestRefresh();
}

void UNUSheetWidgetBase::HandleEquipmentChanged()
{
	RequestRefresh();
}

void UNUSheetWidgetBase::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	RequestRefresh();
}

void UNUSheetWidgetBase::HandleOwnerBattleChanged()
{
	bInBattle = Inventory.IsValid() && Inventory->IsOwnerInBattle();
	OnBattleStateChanged();
	RequestRefresh();
}

TArray<FGameplayAttribute> UNUSheetWidgetBase::GetWatchedAttributes()
{
	return {
		UNUVitalAttributeSet::GetHealthAttribute(),
		UNUVitalAttributeSet::GetMaxHealthAttribute(),
		UNUVitalAttributeSet::GetSPAttribute(),
		UNUVitalAttributeSet::GetMaxSPAttribute(),
		UNUCombatAttributeSet::GetAttackPowerAttribute(),
		UNUCombatAttributeSet::GetMagicAttackPowerAttribute(),
		UNUCombatAttributeSet::GetDefenseAttribute(),
		UNUCombatAttributeSet::GetSpeedAttribute(),
		UNUCombatAttributeSet::GetCriticalRateAttribute(),
		UNUCombatAttributeSet::GetCriticalDamageAttribute()
	};
}

void UNUSheetWidgetBase::FocusForPad()
{
	UNUUIManager::FocusFirstButtonNow(this);
}

bool UNUSheetWidgetBase::IsGamepadMode() const
{
	const ANUPlayerController* NUPlayer = Cast<ANUPlayerController>(GetOwningPlayer());
	return NUPlayer != nullptr && NUPlayer->IsGamepadMode();
}

UNUItemCellWidget* UNUSheetWidgetBase::FindFocusedCell() const
{
	UNUItemCellWidget* Found = nullptr;
	// 칸은 저장소 패널(중첩 UserWidget) 안에 있다 --- 매니저의 깊은 순회가 그 안까지 들어간다 (WidgetTree 순회는 안 들어간다)
	UNUUIManager::ForEachWidgetDeep(const_cast<UNUSheetWidgetBase*>(this), [&Found](UWidget* Widget) // 순회는 읽기만 한다
	{
		UNUItemCellWidget* Cell = Cast<UNUItemCellWidget>(Widget);
		if (Found == nullptr && Cell != nullptr && Cell->HasKeyboardFocus())
		{
			Found = Cell;
		}
	});
	return Found;
}

FReply UNUSheetWidgetBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if ((Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
		&& FocusAdjacentWindow(Key == EKeys::Gamepad_RightShoulder ? 1 : -1))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UNUSheetWidgetBase::FocusAdjacentWindow(int32 Step)
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	if (UI == nullptr)
	{
		return false;
	}
	TArray<UUserWidget*> Windows = UI->GetShownWindows(NUUILayer::Window);
	if (Windows.Num() < 2 || !Windows.Contains(this))
	{
		return false;
	}
	// 화면 왼쪽부터 (창고 | 소지품 | 장비). 창 루트는 화면 전체라 첫 조작부가 그려진 자리(렌더 이동 포함)로 잰다
	const auto BodyX = [](UUserWidget* Window)
	{
		const UWidget* First = UNUUIManager::FindFirstFocusableIn(Window);
		return First != nullptr ? First->GetPaintSpaceGeometry().GetAbsolutePosition().X : TNumericLimits<float>::Max();
	};
	Windows.Sort([&BodyX](UUserWidget& A, UUserWidget& B) { return BodyX(&A) < BodyX(&B); });
	const int32 MyIndex = Windows.IndexOfByKey(this);
	UUserWidget* Next = Windows[(MyIndex + Step + Windows.Num()) % Windows.Num()];
	if (UNUSheetWidgetBase* Sheet = Cast<UNUSheetWidgetBase>(Next))
	{
		Sheet->FocusForPad();
	}
	else
	{
		UNUUIManager::FocusFirstFocusableIn(Next);
	}
	return true;
}

void UNUSheetWidgetBase::HandleCellFocusChanged(UNUItemCellWidget* Cell, bool bFocused)
{
	HideFocusTooltip();
	if (bFocused && Cell != nullptr)
	{
		ShowFocusTooltip(Cell, GetCellItemRowName(Cell->GetContainer(), Cell->GetSlotNo()));
	}
}

void UNUSheetWidgetBase::ShowFocusTooltip(UWidget* Anchor, FName RowName, bool bPreferLeft)
{
	if (Anchor == nullptr || RowName.IsNone() || !IsGamepadMode() || !Anchor->GetCachedWidget().IsValid())
	{
		return;
	}
	FVector2D AnchorPos = FVector2D::ZeroVector, AnchorSize = FVector2D::ZeroVector;
	if (!NUUIGeometry::GetViewportRect(this, *Anchor->GetCachedWidget(), AnchorPos, AnchorSize))
	{
		// 창이 열린 직후나 목록을 새로 지은 직후엔 아직 안 그려져 자리를 모른다 --- 몇 틱까지 다시 본다 (그때도 포커스가 그 안이면)
		if (UWorld* World = GetWorld(); World != nullptr && FocusTooltipRetries < 3)
		{
			++FocusTooltipRetries;
			TWeakObjectPtr<UNUSheetWidgetBase> WeakThis(this);
			TWeakObjectPtr<UWidget> WeakAnchor(Anchor);
			World->GetTimerManager().SetTimerForNextTick([WeakThis, WeakAnchor, RowName, bPreferLeft]()
			{
				UNUSheetWidgetBase* Sheet = WeakThis.Get();
				UWidget* Retry = WeakAnchor.Get();
				if (Sheet != nullptr && Retry != nullptr && (Retry->HasKeyboardFocus() || Retry->HasFocusedDescendants()))
				{
					Sheet->ShowFocusTooltip(Retry, RowName, bPreferLeft);
				}
			});
		}
		return;
	}
	FocusTooltipRetries = 0;
	// 창마다 포커스 툴팁은 하나 --- 앞의 것이 남아 있으면(재시도가 겹치는 등) 먼저 내린다.
	// 안 내리면 그 툴팁은 추적에서 빠져 줄을 옮겨도, 창을 닫아도 화면에 남았다 (공방 창, 9/28)
	if (FocusTooltipWidget != nullptr)
	{
		FocusTooltipWidget->RemoveFromParent();
		FocusTooltipWidget = nullptr;
	}
	UUserWidget* Tooltip = Cast<UUserWidget>(BuildItemTooltip(RowName));
	if (Tooltip == nullptr)
	{
		return;
	}
	// 칸 오른쪽에 붙이되 화면 밖으로 나가면 왼쪽으로 (bPreferLeft면 왼쪽 먼저), 아래로 넘치면 위로 당긴다
	Tooltip->AddToViewport(NUUILayer::Menu);
	Tooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
	Tooltip->SetAlignmentInViewport(FVector2D::ZeroVector);
	Tooltip->ForceLayoutPrepass();
	const FVector2D TipSize = Tooltip->GetDesiredSize();
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(Scale, KINDA_SMALL_NUMBER);
	const float RightX = AnchorPos.X + AnchorSize.X + 8.f;
	const float LeftX = AnchorPos.X - TipSize.X - 8.f;
	const bool bUseLeft = bPreferLeft ? LeftX >= 0.f : RightX + TipSize.X > ViewportSize.X;
	FVector2D Position(bUseLeft ? FMath::Max(0.f, LeftX) : RightX, AnchorPos.Y);
	Position.Y = FMath::Clamp(Position.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - TipSize.Y));
	Tooltip->SetPositionInViewport(Position, false);
	FocusTooltipWidget = Tooltip;
}

void UNUSheetWidgetBase::HideFocusTooltip()
{
	FocusTooltipRetries = 0; // 다음 포커스는 재시도 횟수를 새로 센다
	if (FocusTooltipWidget != nullptr)
	{
		FocusTooltipWidget->RemoveFromParent();
		FocusTooltipWidget = nullptr;
	}
}

void UNUSheetWidgetBase::EquipItemChecked(const FGuid& InstanceId)
{
	NUItemUI::EquipChecked(GetOwningPlayer(), InstanceId);
}

void UNUSheetWidgetBase::ShowNoticeMessage(const FText& Message) const
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->ShowNotice(Message);
	}
}
