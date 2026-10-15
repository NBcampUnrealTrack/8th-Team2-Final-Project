#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterProgression/Data/NUProgressionNodeTypes.h"
#include "NUProgressionGraphWidget.generated.h"

class UCanvasPanel;
class UNUProgressionComponent;
class UNUProgressionDataSubsystem;
class UNUProgressionGraphLinkWidget;
class UNUProgressionNodeTooltipWidget;
class UNUProgressionNodeWidget;

// 노드 선택 정보와 실제 획득 요청 여부를 화면으로 전달
DECLARE_DELEGATE_TwoParams(FOnNUProgressionGraphNodeSelected, const FNUProgressionNodeDefinitionRow&, bool);

// 테이블의 행과 열 좌표로 노드를 자동 배치하고 노드 사이의 연결선을 그린다.
UCLASS(Abstract)
class PROJECTM_API UNUProgressionGraphWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeGraph(int32 InGraphId, UNUProgressionComponent* InProgression, FOnNUProgressionGraphNodeSelected InOnNodeSelected);

	void RefreshGraph();
	void RefreshNodeStates();
	int32 GetRenderedNodeCount() const { return NodeWidgetsById.Num(); }
	int32 GetSelectedNodeId() const { return SelectedNodeId; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> GraphCanvas;

	// 그래프 WBP의 Class Defaults에서 전체 배치 간격과 노드 크기를 조정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "1.0"))
	float LayoutColumnSpacing = 122.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "1.0"))
	float LayoutRowSpacing = 164.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "0.0"))
	float LayoutContentPadding = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "1.0"))
	float StatNodeSize = 72.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "1.0"))
	float SkillNodeSize = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "1.0"))
	float LargeNodeSize = 132.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "1.0"))
	float AdvancementNodeScale = 3.f;

	// 네 번째 완주 줄기 끝에서 최종 2택이 갈라지는 거리다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "1.0"))
	float FinalNodeRadialOffset = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Layout", meta = (ClampMin = "1.0"))
	float FinalNodeTangentSpacing = 180.f;

	// 비워 두면 C++ 기본 연결선 레이어를 사용하고, WBP 클래스를 지정하면 선 스타일을 Class Defaults에서 조정할 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Progression|Link")
	TSubclassOf<UNUProgressionGraphLinkWidget> LinkLayerWidgetClass;

private:
	void HandleNodeActivated(int32 NodeId);
	void HandleNodeHoverChanged(int32 NodeId, bool bHovered);
	UWidget* HandleNodeTooltipRequested(const FNUProgressionNodeDefinitionRow& Node);
	void SelectNode(int32 NodeId, bool bAcquireRequested);
	void ClearGraph();
	void RebuildLinkCache();
	void RefreshLinkStates();
	int32 ResolveFinalAnchorNodeId(const UNUProgressionDataSubsystem& Data) const;
	bool IsPointerOverNode(const FVector2D& ScreenPosition) const;
	void ApplyViewOffset(const FVector2D& ViewportSize);

	int32 GraphId = 0;
	int32 SelectedNodeId = 0;
	int32 HoveredNodeId = 0;
	FOnNUProgressionGraphNodeSelected OnNodeSelected;
	TWeakObjectPtr<UNUProgressionComponent> Progression;

	// 생성한 노드와 연결선 좌표는 다시 그릴 때 재사용
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UNUProgressionNodeWidget>> NodeWidgetsById;

	UPROPERTY(Transient)
	TObjectPtr<UNUProgressionGraphLinkWidget> LinkLayerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNUProgressionNodeTooltipWidget> SharedNodeTooltipWidget;

	TArray<TPair<int32, int32>> Links;
	FVector2D GraphContentSize = FVector2D::ZeroVector;
	FVector2D ViewOffset = FVector2D::ZeroVector;
	FVector2D LastViewportSize = FVector2D::ZeroVector;
	float ZoomLevel = 1.2f;
	int32 RenderedFinalAnchorNodeId = 0;
	bool bIsPanning = false;
	bool bCenterGraphOnOpen = false;
};
