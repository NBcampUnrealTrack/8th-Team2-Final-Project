// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Common/NU_CommonButtonWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBox.h"

namespace
{
	// M_Button_Hovered(Custom HLSL 9-slice 노드)가 읽는 파라미터 이름 --- 머티리얼 쪽 이름과 반드시 일치해야 함
	const FName MaterialParam_MarginPx(TEXT("MarginPx"));
	const FName MaterialParam_BrushSize(TEXT("BrushSize"));
	const FName MaterialParam_WidgetSizePx(TEXT("WidgetSizePx"));

	// 머티리얼이 참조하는 원본 텍스처의 논리적 기준 해상도 --- WBP_CommonButton 그래프가 예전에
	// Slate ImageSize로 하드코딩했던 32x32와 동일. 일부러 인스턴스 프로퍼티로 안 열어뒀다 ---
	// EditAnywhere였다면 ButtonSize와 같은 값으로 잘못 설정해서 9-slice 효과를 꺼버릴 위험이 있다
	// (실제로 한 번 그랬다). 모든 WBP_CommonButton 인스턴스가 항상 같은 텍스처를 공유하므로 상수면 충분하다.
	const FVector2D BrushNativeSize(32.f, 32.f);
}

void UNU_CommonButtonWidget::NativeConstruct()
{
	// Super 호출 안에서 WBP 자체 그래프의 Event Construct(WidgetStyle 재생성)가 먼저 실행된다.
	// ApplyButtonMargin은 그 뒤에 호출해야 그래프가 하드코딩한 Margin(0)을 덮어쓸 수 있다.
	Super::NativeConstruct();

	ApplyButtonMargin();
	ApplyIconOnlyMode();

	if (IsValid(Btn_Common))
	{
		Btn_Common->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
		Btn_Common->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}
}

void UNU_CommonButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetLabelText(LabelText);
	SetLabelColor(LabelColor);
	SetLabelFontSize(LabelFontSize);
	SetIconTexture(IconTexture);
	SetIconSize(IconSize);
	SetShortcutText(ShortcutText);
	ApplyButtonMargin();
	ApplyIconOnlyMode();
}

void UNU_CommonButtonWidget::SetLabelText(const FText& InText)
{
	LabelText = InText;
	if (!IsValid(Txt_Label))
	{
		return;
	}

	Txt_Label->SetText(InText);
	// 아이콘 전용 버튼(필드 HUD 등)은 라벨을 아예 안 쓴다 --- 비어 있으면 자리를 차지하지 않게 접어서
	// Fill 슬롯에 빈 라벨이 아이콘을 밀어내는 일이 없게 한다. 기존 사용처는 전부 텍스트를 채워서 쓰므로 영향 없다
	Txt_Label->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UNU_CommonButtonWidget::SetLabelColor(const FLinearColor& InColor)
{
	LabelColor = InColor;
	if (IsValid(Txt_Label))
	{
		Txt_Label->SetColorAndOpacity(FSlateColor(InColor));
	}
}

void UNU_CommonButtonWidget::SetLabelFontSize(int32 InSize)
{
	LabelFontSize = InSize;
	if (!IsValid(Txt_Label) || InSize <= 0)
	{
		return;
	}

	FSlateFontInfo Font = Txt_Label->GetFont();
	Font.Size = InSize;
	Txt_Label->SetFont(Font);
}

void UNU_CommonButtonWidget::SetIconTexture(UTexture2D* InTexture)
{
	IconTexture = InTexture;
	if (!IsValid(Img_Icon))
	{
		return;
	}

	if (InTexture != nullptr)
	{
		Img_Icon->SetBrushFromTexture(InTexture);
		// HitTestInvisible --- 아이콘이 버튼 위에 그려지지만 클릭은 밑의 Btn_Common이 받아야 한다
		Img_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Img_Icon->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNU_CommonButtonWidget::SetIconSize(const FVector2D& InSize)
{
	IconSize = InSize;
	if (!IsValid(Img_Icon))
	{
		return;
	}

	if (InSize.X > 0.f && InSize.Y > 0.f)
	{
		Img_Icon->SetDesiredSizeOverride(InSize);
	}
}

void UNU_CommonButtonWidget::SetShortcutText(const FText& InText)
{
	ShortcutText = InText;
	if (!IsValid(Txt_Shortcut))
	{
		return;
	}

	Txt_Shortcut->SetText(InText);
	Txt_Shortcut->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UNU_CommonButtonWidget::SetBackgroundColor(const FLinearColor& InColor)
{
	if (IsValid(Btn_Common))
	{
		Btn_Common->SetBackgroundColor(InColor);
	}
}

void UNU_CommonButtonWidget::ApplyButtonMargin()
{
	// Btn_Common의 배경은 텍스처가 아니라 머티리얼(Dynamic Material Instance)이라 Slate의
	// FSlateBrush::Margin(9-slice 자동 슬라이싱)은 셰이더가 안 읽어서 시각적으로 아무 효과가
	// 없다(실측 확인). 그래서 진짜 9-slice는 머티리얼 안의 Custom HLSL 노드가 MarginPx/BrushSize
	// 파라미터를 직접 읽어 UV를 재계산하는 방식으로 처리한다 --- 여기선 그 파라미터만 채운다.
	UMaterialInstanceDynamic* Material = GetBrushMaterial();
	if (Material == nullptr)
	{
		return;
	}

	Material->SetVectorParameterValue(MaterialParam_MarginPx,
		FLinearColor(ButtonMargin.Left, ButtonMargin.Top, ButtonMargin.Right, ButtonMargin.Bottom));
	Material->SetVectorParameterValue(MaterialParam_BrushSize,
		FLinearColor(BrushNativeSize.X, BrushNativeSize.Y, 0.f, 0.f));
}

void UNU_CommonButtonWidget::ApplyIconOnlyMode()
{
	// SetBackgroundColor와 같은 이유(머티리얼이 VertexColor를 안 읽음)로 SlateColor 틴트로는
	// 배경을 못 지운다. DrawAs=NoDrawType은 머티리얼과 무관하게 항상 먹히므로 이 방법을 쓴다.
	if (!bIconOnly || !IsValid(Btn_Common))
	{
		return;
	}

	FButtonStyle Style = Btn_Common->GetStyle();
	Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	Btn_Common->SetStyle(Style);
}

UMaterialInstanceDynamic* UNU_CommonButtonWidget::GetBrushMaterial() const
{
	if (!IsValid(Btn_Common))
	{
		return nullptr;
	}

	return Cast<UMaterialInstanceDynamic>(Btn_Common->GetStyle().Normal.GetResourceObject());
}

void UNU_CommonButtonWidget::ApplyWidgetSizeToMaterial(const FVector2D& SizePx)
{
	UMaterialInstanceDynamic* Material = GetBrushMaterial();
	if (Material == nullptr)
	{
		return;
	}

	Material->SetVectorParameterValue(MaterialParam_WidgetSizePx, FLinearColor(SizePx.X, SizePx.Y, 0.f, 0.f));
	LastAppliedWidgetSize = SizePx;
}

void UNU_CommonButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Fill 배치라 크기가 레이아웃마다 바뀔 수 있는 버튼(채팅 탭 등)도 9-slice 비율이 항상 맞도록,
	// 실제 렌더 크기를 매 프레임 확인한다. 크기가 그대로면(대부분의 프레임) 다시 안 씀.
	const FVector2D CurrentSize = MyGeometry.GetLocalSize();
	if (!CurrentSize.Equals(LastAppliedWidgetSize, 0.5f))
	{
		ApplyWidgetSizeToMaterial(CurrentSize);
	}
}

TSharedRef<SWidget> UNU_CommonButtonWidget::RebuildWidget()
{
	TSharedRef<SWidget> Content = Super::RebuildWidget();
	if (!bOverrideButtonSize)
	{
		return Content;
	}

	return SNew(SBox)
		.WidthOverride(ButtonSize.X)
		.HeightOverride(ButtonSize.Y)
		[
			Content
		];
}

void UNU_CommonButtonWidget::HandleClicked()
{
	OnCommonClicked.Broadcast();
	OnCommonClickedWithTag.Broadcast(Tag);
}
