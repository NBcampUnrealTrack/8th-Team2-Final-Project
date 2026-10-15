// Fill out your copyright notice in the Description page of Project Settings.

#include "Chat/UI/NUChatTabButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/Common/NU_CommonButtonWidget.h"

void UNUChatTabButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 순정 버튼과 공용 부품 둘 다 인자 없는 델리게이트라 핸들러 하나로 받음
	// 둘을 같이 두는 WBP도 막지 않음, 어차피 같은 탭이라 어느 쪽을 눌러도 결과가 같음
	if (IsValid(TabButton))
	{
		TabButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}

	if (IsValid(TabCommonButton))
	{
		TabCommonButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}

	// 클릭을 받을 곳과 글자를 넣을 곳을 따로 확인함
	if (!IsValid(TabButton) && !IsValid(TabCommonButton))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] %s 에 클릭을 받을 위젯이 없음 - TabButton(Button) 이나 TabCommonButton(WBP_CommonButton) 중 하나로 이름을 맞추세요"),
			*GetClass()->GetName());
	}

	if (!IsValid(LabelText) && !IsValid(TabCommonButton))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] %s 에 탭 이름을 넣을 곳이 없음 - LabelText(TextBlock) 이나 TabCommonButton(WBP_CommonButton) 중 하나로 이름을 맞추세요"),
			*GetClass()->GetName());
	}

	if (IsValid(SelectedFrame))
	{
		SelectedFrame->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UNUChatTabButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// SetTab()이 아직 한 번도 호출 안 됐으면(TabIndex가 기본값 INDEX_NONE) 디자이너 프리뷰/기본값을 보여준다.
	// 실제 탭/채널로 SetTab()이 호출된 뒤에는 여길 다시 지나도 손대지 않는다.
	if (TabIndex == INDEX_NONE && !DefaultLabel.IsEmpty())
	{
		SetTab(INDEX_NONE, DefaultLabel);
	}
}

void UNUChatTabButtonWidget::SetTab(int32 InTabIndex, const FText& InLabel)
{
	TabIndex = InTabIndex;
	Label = InLabel;

	ApplyLabel();
}

void UNUChatTabButtonWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;

	if (IsValid(SelectedFrame))
	{
		// HitTestInvisible - 테두리가 탭 클릭을 가로채지 않게
		SelectedFrame->SetVisibility(bInSelected
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
	}

	ApplyLabel();

	// 공용 부품이면 배경 텍스처도 같이 바꿔치움(선택/비선택 텍스처를 안 채웠으면 조용히 무시됨)
	if (IsValid(TabCommonButton))
	{
		TabCommonButton->SetSelected(bInSelected);
	}

	OnSelectedChanged(bInSelected);
}

void UNUChatTabButtonWidget::SetIconTexture(UTexture2D* InTexture)
{
	if (IsValid(TabCommonButton))
	{
		TabCommonButton->SetIconTexture(InTexture);
	}
}

void UNUChatTabButtonWidget::SetIconSize(const FVector2D& InSize)
{
	if (IsValid(TabCommonButton))
	{
		TabCommonButton->SetIconSize(InSize);
	}
}

int32 UNUChatTabButtonWidget::GetTabIndex() const
{
	return TabIndex;
}

void UNUChatTabButtonWidget::ApplyLabel()
{
	const FLinearColor Color = bSelected ? SelectedLabelColor : IdleLabelColor;

	// LabelText가 있으면 이 위젯이 직접 그리고, 없으면 공용 부품의 라벨로 넘긴다.
	// NUChatWidget::RefreshSendChannelVisual이 쓰는 갈림과 같은 모양
	if (IsValid(LabelText))
	{
		LabelText->SetText(Label);
		LabelText->SetColorAndOpacity(FSlateColor(Color));
		return;
	}

	if (IsValid(TabCommonButton))
	{
		// 공용 부품의 세터는 자기 인스턴스 프로퍼티도 같이 갱신하므로,
		// 이후 NativePreConstruct가 다시 돌아도 여기서 넣은 값이 유지된다
		TabCommonButton->SetLabelText(Label);
		TabCommonButton->SetLabelColor(Color);
	}
}

void UNUChatTabButtonWidget::HandleClicked()
{
	OnTabClicked.ExecuteIfBound(TabIndex);
}
