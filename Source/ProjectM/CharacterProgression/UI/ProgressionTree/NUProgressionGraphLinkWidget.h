#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterProgression/Data/NUProgressionTypes.h"
#include "NUProgressionGraphLinkWidget.generated.h"

// 그래프가 한 번 계산한 노드 중심과 크기를 연결선 레이어에 전달
struct PROJECTM_API FNUProgressionGraphLinkDefinition
{
	int32 ParentNodeId = 0;
	int32 ChildNodeId = 0;
	int32 ParentStemId = 0;
	int32 ChildStemId = 0;
	bool bParentIsAdvancement = false;
	bool bChildIsAdvancement = false;
	FVector2D ParentCenter = FVector2D::ZeroVector;
	FVector2D ParentSize = FVector2D::ZeroVector;
	FVector2D ChildCenter = FVector2D::ZeroVector;
	FVector2D ChildSize = FVector2D::ZeroVector;
};

// 성장 그래프의 모든 연결선을 한 번에 그리는 전용 레이어
UCLASS(Blueprintable)
class PROJECTM_API UNUProgressionGraphLinkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 그래프 생성 시 연결 선 좌표를 전달 받아 캐싱
	void SetLinks(TArray<FNUProgressionGraphLinkDefinition> InLinks);

	// 획득 경로와 호버 경로의 표시 상태 갱신
	void RefreshLinkStates(
		const TArray<FNUProgressionNodeLinkState>& AcquiredNodeLinks,
		int32 HoveredParentNodeId,
		int32 HoveredNodeId,
		bool bHoveredNodeAvailable);

protected:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link")
	FLinearColor InactiveColor = FLinearColor(0.25f, 0.38f, 0.62f, 0.42f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link")
	FLinearColor HoverColor = FLinearColor(0.42f, 0.70f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link")
	FLinearColor ConfirmedColor = FLinearColor(0.95f, 0.68f, 0.18f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link", meta = (ClampMin = "0.1"))
	float InactiveThickness = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link", meta = (ClampMin = "0.1"))
	float HighlightThickness = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link", meta = (ClampMin = "0.0"))
	float GlowExtraThickness = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link", meta = (ClampMin = "0.0"))
	float NodeGap = 0.f;

	// 테스트용: 각 줄기의 선을 2차 전직 이미지 외곽의 한 점에 모은다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link|Test")
	bool bUseThreeWayJunctions = true;

	// 전직 위젯 전체 크기에서 실제 원형 이미지 반지름이 차지하는 비율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link|Test", meta = (ClampMin = "0.1", ClampMax = "0.5"))
	float AdvancementImageRadiusScale = 0.36f;

private:
	// 연결선 표시 상태
	enum class EVisualState : uint8
	{
		Inactive,	// 선택되지 않음
		Hovered,	// 다음 선택 경로
		Confirmed	// 실제 획득 경로
	};

	// 렌더링할 연결선 하나의 캐시 데이터
	struct FCachedLink
	{
		int32 ParentNodeId = 0;		//	시작 노드
		int32 ChildNodeId = 0;		//	도착 노드
		FVector2D Start = FVector2D::ZeroVector;
		FVector2D End = FVector2D::ZeroVector;
		EVisualState VisualState = EVisualState::Inactive;
	};

	// 연결선 렌더링
	void DrawLink(
		const FCachedLink& Link, 
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FLinearColor& Color,
		float Thickness) const;

	// 그래프 재생성 전, 재사용할 데이터
	TArray<FCachedLink> CachedLinks;
};
