// Fill out your copyright notice in the Description page of Project Settings.


#include "NUParryCueBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"

TSharedRef<SWidget> UNUParryCueBarWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return Super::RebuildWidget();
}

void UNUParryCueBarWidget::BuildWidgetTree()
{
	UCanvasPanel* RootCanvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	// 안쪽 Canvas가 도전 구간 0~1의 좌표계 역할을 한다.
	// 바깥 Canvas는 화면 배치만 담당해서, 존 앵커가 해상도와 무관해진다
	BarCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BarCanvas"));

	if (UCanvasPanelSlot* BarSlot = RootCanvas->AddChildToCanvas(BarCanvas))
	{
		BarSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));   // 하단 중앙
		BarSlot->SetAlignment(FVector2D(0.5f, 1.f));
		BarSlot->SetPosition(BarScreenOffset);
		BarSlot->SetSize(BarSize);
	}

	// Miss는 바 전체를 덮는 배경
	MissZone = AddBarImage(TEXT("MissZone"), MissColor);
	ApplyZoneAnchors(MissZone, 0.f, 1.f);

	// SuccessZone은 성공 판정에 해당하는 구역
	SuccessZone = AddBarImage(TEXT("PerfectZone"), SuccessColor);

	// 바 전체를 훑고 지나가는 세로선
	Marker = AddBarImage(TEXT("Marker"), MarkerMissColor);

	if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
	{
		// X 앵커의 Min == Max일 때 UMG는 Offsets.Right를 폭으로 사용
		// Alignment 0.5로 앵커 위치에 기둥의 중심이 오게
		MarkerSlot->SetAlignment(FVector2D(0.5f, 0.f));
		MarkerSlot->SetOffsets(FMargin(0.f, 0.f, MarkerWidth, 0.f));
	}
}

UImage* UNUParryCueBarWidget::AddBarImage(FName WidgetName, const FLinearColor& Color)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), WidgetName);
	Image->SetColorAndOpacity(Color);
	
	BarCanvas->AddChildToCanvas(Image);

	return Image;
}

void UNUParryCueBarWidget::ApplyZoneAnchors(UImage* Zone, float Start, float End)
{
	if (!Zone) return;

	if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Zone->Slot))
	{
		Slot->SetAnchors(FAnchors(Start, 0.f, End, 1.f));
		Slot->SetOffsets(FMargin(0.f));
	}
}

FLinearColor UNUParryCueBarWidget::GetMarkerColor(bool bInSuccess) const
{
	return bInSuccess ? MarkerSuccessColor : MarkerMissColor;
}

void UNUParryCueBarWidget::InitializeZones(const FNUParryTimingSpec& InTiming, int32 InSPCost)
{
	Super::InitializeZones(InTiming, InSPCost);

	ApplyZoneAnchors(SuccessZone, SuccessStart, SuccessEnd);

	SetProgress(0.f);
}

void UNUParryCueBarWidget::SetProgress(float InProgress)
{
	Super::SetProgress(InProgress);

	if (!Marker) return;

	if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
	{
		MarkerSlot->SetAnchors(FAnchors(Progress, 0.f, Progress, 1.f));
	}

	Marker->SetColorAndOpacity(GetMarkerColor(bSuccess));
}
