#include "Shop/UI/NUCraftDetailWidget.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "PlayerController/NUPlayerController.h"
#include "Shop/UI/NUCraftMaterialWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"

void UNUCraftDetailWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	QuantitySpin->OnValueChanged.AddDynamic(this, &UNUCraftDetailWidget::HandleQuantityChanged);
	CraftButton->OnCommonClicked.AddDynamic(this, &UNUCraftDetailWidget::HandleCraftClicked);
}

void UNUCraftDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 조작 안내는 장치를 따라간다 --- 키보드로 연 창에서 패드를 잡아도 보이게
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnGamepadModeChanged.RemoveAll(this);
		UI->OnGamepadModeChanged.AddUObject(this, &UNUCraftDetailWidget::HandleGamepadModeChanged);
	}
	const ANUPlayerController* NUPlayer = Cast<ANUPlayerController>(GetOwningPlayer());
	HandleGamepadModeChanged(NUPlayer != nullptr && NUPlayer->IsGamepadMode());
}

void UNUCraftDetailWidget::NativeDestruct()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnGamepadModeChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UNUCraftDetailWidget::SetData(const FNUCraftDetailData& Data)
{
	const bool bSameRecipe = RecipeId == Data.RecipeId;
	RecipeId = Data.RecipeId;
	GoldPerCraft = Data.GoldPerCraft;
	OwnedGold = Data.OwnedGold;
	MaxCount = Data.MaxCount;
	bHardLocked = !Data.Reason.IsEmpty();

	// 결과물 --- 아이콘이 없는 아이템은 이름 글자가 아이콘 노릇
	ResultIcon->SetBrushFromTexture(Data.ResultIcon);
	ResultIcon->SetVisibility(Data.ResultIcon != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (ResultLabelText != nullptr)
	{
		ResultLabelText->SetText(Data.ResultName);
		ResultLabelText->SetVisibility(Data.ResultIcon != nullptr ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (ResultNameText != nullptr)
	{
		ResultNameText->SetText(Data.ResultCount > 1
			? FText::Format(NSLOCTEXT("NUCraft", "ResultCount", "{0} x{1}"), Data.ResultName, FText::AsNumber(Data.ResultCount))
			: Data.ResultName);
	}
	if (ReasonText != nullptr)
	{
		ReasonText->SetText(Data.Reason);
		ReasonText->SetVisibility(Data.Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// 재료 칸 --- 개수만 바뀔 때는 SetCounts로 갱신하므로 여기서 한 번만 만든다
	MaterialBox->ClearChildren();
	MaterialCells.Reset();
	MaterialHave.Reset();
	MaterialNeed.Reset();
	if (MaterialCellClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[제작] MaterialCellClass 미지정 --- 재료 칸을 그릴 수 없습니다 (WBP_CraftDetail 기본값 확인)"));
	}
	for (int32 Index = 0; Index < Data.Materials.Num() && MaterialCellClass != nullptr; ++Index)
	{
		const FNUCraftMaterialView& View = Data.Materials[Index];
		UNUCraftMaterialWidget* Cell = CreateWidget<UNUCraftMaterialWidget>(this, MaterialCellClass);
		Cell->SetMaterial(View.Icon, View.Name, View.Have, View.Need, Index == 0);
		MaterialBox->AddChild(Cell);
		MaterialCells.Add(Cell);
		MaterialHave.Add(View.Have);
		MaterialNeed.Add(View.Need);
	}

	// 만들 개수: 1 ~ 최대. 못 만드는 상태(최대 0)여도 칸은 1로 두고 버튼만 끈다
	const int32 SpinMax = FMath::Max(1, MaxCount);
	const int32 Keep = bSameRecipe ? FMath::Clamp(GetQuantity(), 1, SpinMax) : 1;
	QuantitySpin->SetMinValue(1.f);
	QuantitySpin->SetMinSliderValue(1.f);
	QuantitySpin->SetMaxValue(static_cast<float>(SpinMax));
	QuantitySpin->SetMaxSliderValue(static_cast<float>(SpinMax));
	QuantitySpin->SetValue(static_cast<float>(Keep));

	if (MaxCountText != nullptr)
	{
		MaxCountText->SetText(FText::Format(NSLOCTEXT("NUCraft", "MaxCount", "최대 {0}회"), FText::AsNumber(MaxCount)));
	}
	if (OwnedGoldText != nullptr)
	{
		OwnedGoldText->SetText(FText::AsNumber(OwnedGold));
	}
	RefreshForQuantity();
}

int32 UNUCraftDetailWidget::GetQuantity() const
{
	return FMath::Max(1, FMath::RoundToInt(QuantitySpin->GetValue()));
}

void UNUCraftDetailWidget::RefreshForQuantity()
{
	const int32 Quantity = GetQuantity();
	for (int32 Index = 0; Index < MaterialCells.Num(); ++Index)
	{
		MaterialCells[Index]->SetCounts(MaterialHave[Index], MaterialNeed[Index] * Quantity);
	}
	if (CostGoldText != nullptr)
	{
		CostGoldText->SetText(FText::AsNumber(static_cast<int64>(GoldPerCraft) * Quantity));
	}
	const bool bCanCraft = !bHardLocked && Quantity <= MaxCount;
	CraftButton->SetIsEnabled(bCanCraft);

	// 만들어서 재료가 떨어지면 버튼이 꺼진다 --- 꺼진 버튼에 남은 패드 포커스는 목록의 줄로 돌려보낸다
	if (!bCanCraft && HasFocusedDescendants())
	{
		OnBackRequested.ExecuteIfBound();
	}
}

void UNUCraftDetailWidget::FocusCraftButton()
{
	if (CraftButton->GetIsEnabled())
	{
		UNUUIManager::FocusFirstFocusableIn(CraftButton);
	}
}

FReply UNUCraftDetailWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 터널이라 포커스가 이 패널 안([만들기]·개수 칸)일 때만 온다. 포커스된 버튼이 D패드를 이동으로 먼저 삼키므로 여기서 가로챈다
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Gamepad_FaceButton_Right)
	{
		OnBackRequested.ExecuteIfBound(); // 창을 닫지 않고 목록으로
		return FReply::Handled();
	}

	// 수량 창과 같은 규칙 --- 위아래 1씩, 좌우 10씩
	const int32 Delta = Key == EKeys::Gamepad_DPad_Up ? 1 : Key == EKeys::Gamepad_DPad_Down ? -1
		: Key == EKeys::Gamepad_DPad_Right ? 10 : Key == EKeys::Gamepad_DPad_Left ? -10 : 0;
	if (Delta != 0)
	{
		// 개수 칸에 포커스가 있으면(마우스로 누른 뒤 패드를 잡음) 글자 입력 상태다 --- 입력 중인 칸은 엔진이 표시를 안 바꾸고,
		// 포커스가 떠날 때 칸에 남은 옛 숫자로 값을 되돌린다 (비용·재료만 오르고 숫자는 그대로였다). [만들기]로 옮겨 입력을 끝낸 뒤 바꾼다
		if (QuantitySpin->HasAnyUserFocus() || QuantitySpin->HasFocusedDescendants())
		{
			FocusCraftButton();
		}
		// 값이 바뀌면 칸이 변경 알림을 보내 재료·비용이 다시 그려진다 (HandleQuantityChanged)
		QuantitySpin->SetValue(static_cast<float>(FMath::Clamp(GetQuantity() + Delta, 1, FMath::Max(1, MaxCount))));
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FNavigationReply UNUCraftDetailWidget::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	// 스틱 이동만 여기로 온다 (D패드는 위에서 개수로 먼저 쓴다). 왼쪽 = 목록의 고른 줄로 (목록에서 오른쪽으로 들어온 길),
	// 나머지는 제자리 --- 개수 칸(SpinBox)은 패드 키를 안 받아 거기 서면 막힌다
	if (InNavigationEvent.GetNavigationType() == EUINavigation::Left)
	{
		OnBackRequested.ExecuteIfBound();
	}
	return FNavigationReply::Stop();
}

void UNUCraftDetailWidget::HandleQuantityChanged(float NewValue)
{
	RefreshForQuantity();
}

void UNUCraftDetailWidget::HandleGamepadModeChanged(bool bGamepad)
{
	if (PadHintText != nullptr)
	{
		PadHintText->SetVisibility(bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNUCraftDetailWidget::HandleCraftClicked()
{
	OnCraftRequested.ExecuteIfBound(RecipeId, GetQuantity());
}
