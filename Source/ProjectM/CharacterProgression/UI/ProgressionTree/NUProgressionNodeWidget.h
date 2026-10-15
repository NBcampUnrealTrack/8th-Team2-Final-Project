#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterProgression/Data/NUProgressionNodeTypes.h"
#include "NUProgressionNodeWidget.generated.h"

class UButton;
class UImage;
class UTexture2D;
class UWidget;

DECLARE_DELEGATE_OneParam(FOnNUProgressionNodeActivated, int32);
DECLARE_DELEGATE_TwoParams(FOnNUProgressionNodeHoverChanged, int32, bool);
DECLARE_DELEGATE_RetVal_OneParam(UWidget*, FOnNUProgressionNodeTooltipRequested, const FNUProgressionNodeDefinitionRow&);

UCLASS(Abstract)
class PROJECTM_API UNUProgressionNodeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeNode(
		const FNUProgressionNodeDefinitionRow& InDefinition,
		const FVector2D& InRenderSize,
		FOnNUProgressionNodeActivated InOnActivated,
		FOnNUProgressionNodeHoverChanged InOnHoverChanged,
		FOnNUProgressionNodeTooltipRequested InOnTooltipRequested);

	void SetSelected(bool bInSelected);
	void SetNodeState(ENUProgressionNodeState InNodeState);
	int32 GetNodeId() const { return Definition.NodeId; }
	ENUProgressionNodeState GetNodeState() const { return NodeState; }

	UFUNCTION(BlueprintCallable, Category = "NU|Progression|Node")
	void ActivateNode();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> NodeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	// 화면 선택 또는 획득 완료 노드의 노란 테두리
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectionIndicator;

	// 테이블에서 읽은 정적 노드 정보
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	FNUProgressionNodeDefinitionRow Definition;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	bool bSelected = false;

	// 플레이어 상태에 따라 바뀌는 런타임 표시 상태
	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|Node")
	ENUProgressionNodeState NodeState = ENUProgressionNodeState::Locked;

	// WBP에서 상태별 색상이나 테두리를 바꿀 때 사용
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Progression|Node")
	void OnNodeStateChanged(ENUProgressionNodeState NewState);

private:
	void RefreshContent();
	void RefreshState();
	void RefreshVisualStyle();
	void RefreshSelectionIndicator();
	void RefreshIcon();
	static UTexture2D* ResolveNodeIcon(const FNUProgressionNodeDefinitionRow& Node);

	FOnNUProgressionNodeActivated OnActivated;
	FOnNUProgressionNodeHoverChanged OnHoverChanged;
	FOnNUProgressionNodeTooltipRequested OnTooltipRequested;
	FVector2D RenderSize = FVector2D::ZeroVector;
};
