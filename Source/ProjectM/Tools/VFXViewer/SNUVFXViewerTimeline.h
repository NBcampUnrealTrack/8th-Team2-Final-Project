// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class ANUVFXViewerDirector;

// 전투 VFX 뷰어 3D 화면 아래 진행바, 동영상 플레이어처럼 눌러서 · 끌어서 재생 위치를 옮김
// 막대는 단계 구간(예고 · 접근 · 행동 · 복귀 · 잔상)으로 칠하고 위에 시전 · 타격 눈금을 그림
// 값은 매 프레임 감독(ANUVFXViewerDirector)에게서 읽음
class SNUVFXViewerTimeline : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SNUVFXViewerTimeline) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ANUVFXViewerDirector>, Director)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	ANUVFXViewerDirector* GetDirector() const { return Director.Get(); }

	// 막대 위 위치 ↔ 시각, 양 끝은 손잡이가 잘리지 않게 여백을 둠
	float TimeAtLocalX(float LocalX, float Width) const;
	float LocalXAtTime(float Time, float Width) const;

	void SeekToMouse(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	TWeakObjectPtr<ANUVFXViewerDirector> Director;
	bool bDragging = false;

	// 마우스를 올린 곳의 시각, 없으면 음수
	float HoverTime = -1.f;
};

// 재생 조작 버튼 그림, 글꼴에 없을 수 있는 기호(⏮ 등) 대신 직접 그림
enum class ENUVFXViewerTransportIcon : uint8
{
	ToStart,
	BackSecond,
	BackFrame,
	Play,
	Pause,
	ForwardFrame,
	ForwardSecond
};

class SNUVFXViewerTransportIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SNUVFXViewerTransportIcon)
		: _Icon(ENUVFXViewerTransportIcon::Play)
		{}
		SLATE_ATTRIBUTE(ENUVFXViewerTransportIcon, Icon)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	TAttribute<ENUVFXViewerTransportIcon> Icon;
};
