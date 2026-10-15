#include "CharacterProgression/UI/ProgressionTree/NUProgressionNodeWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

void UNUProgressionNodeWidget::InitializeNode(const FNUProgressionNodeDefinitionRow& InDefinition, const FVector2D& InRenderSize, FOnNUProgressionNodeActivated InOnActivated, FOnNUProgressionNodeHoverChanged InOnHoverChanged, FOnNUProgressionNodeTooltipRequested InOnTooltipRequested)
{
	// 노드마다 다른 테이블 행과 클릭 콜백을 저장
	Definition = InDefinition;
	RenderSize = InRenderSize;
	OnActivated = MoveTemp(InOnActivated);
	OnHoverChanged = MoveTemp(InOnHoverChanged);
	OnTooltipRequested = MoveTemp(InOnTooltipRequested);
	RefreshContent();
}

void UNUProgressionNodeWidget::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}

	bSelected = bInSelected;
	RefreshSelectionIndicator();
}

void UNUProgressionNodeWidget::SetNodeState(ENUProgressionNodeState InNodeState)
{
	// C++ 기본 표시와 WBP 스타일을 같은 상태로 갱신
	NodeState = InNodeState;
	RefreshState();
	RefreshSelectionIndicator();
	OnNodeStateChanged(NodeState);
}

void UNUProgressionNodeWidget::ActivateNode()
{
	// 잠김이나 획득 완료 상태에서는 서버 요청을 보내지 않는다.
	if (NodeState == ENUProgressionNodeState::Available)
	{
		OnActivated.ExecuteIfBound(Definition.NodeId);
	}
}

void UNUProgressionNodeWidget::HandleHovered()
{
	if (NodeButton != nullptr)
	{
		NodeButton->SetToolTip(OnTooltipRequested.IsBound()
			? OnTooltipRequested.Execute(Definition)
			: nullptr);
	}
	OnHoverChanged.ExecuteIfBound(Definition.NodeId, true);
}

void UNUProgressionNodeWidget::HandleUnhovered()
{
	if (NodeButton != nullptr)
	{
		NodeButton->SetToolTip(nullptr);
	}
	OnHoverChanged.ExecuteIfBound(Definition.NodeId, false);
}

void UNUProgressionNodeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	NodeButton->OnClicked.RemoveDynamic(this, &ThisClass::ActivateNode);
	NodeButton->OnClicked.AddDynamic(this, &ThisClass::ActivateNode);
	NodeButton->OnHovered.RemoveDynamic(this, &ThisClass::HandleHovered);
	NodeButton->OnHovered.AddDynamic(this, &ThisClass::HandleHovered);
	NodeButton->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleUnhovered);
	NodeButton->OnUnhovered.AddDynamic(this, &ThisClass::HandleUnhovered);
	RefreshContent();
	RefreshState();
	RefreshSelectionIndicator();
}

void UNUProgressionNodeWidget::NativeDestruct()
{
	if (NodeButton != nullptr)
	{
		NodeButton->OnClicked.RemoveDynamic(this, &ThisClass::ActivateNode);
		NodeButton->OnHovered.RemoveDynamic(this, &ThisClass::HandleHovered);
		NodeButton->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleUnhovered);
		NodeButton->SetToolTip(nullptr);
	}
	OnTooltipRequested.Unbind();
	Super::NativeDestruct();
}

void UNUProgressionNodeWidget::RefreshContent()
{
	if (IconImage == nullptr)
	{
		return;
	}

	RefreshIcon();
	RefreshVisualStyle();
}

void UNUProgressionNodeWidget::RefreshIcon()
{
	if (IconImage == nullptr)
	{
		return;
	}

	if (UTexture2D* Icon = ResolveNodeIcon(Definition))
	{
		IconImage->SetBrushFromTexture(Icon, false);
		IconImage->SetDesiredSizeOverride(RenderSize);
		IconImage->SetColorAndOpacity(FLinearColor::White);
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	IconImage->SetVisibility(ESlateVisibility::Collapsed);
}

UTexture2D* UNUProgressionNodeWidget::ResolveNodeIcon(const FNUProgressionNodeDefinitionRow& Node)
{
	if (!Node.Icon.IsNull())
	{
		if (UTexture2D* ExplicitIcon = Node.Icon.LoadSynchronous())
		{
			return ExplicitIcon;
		}
	}

	const TCHAR* IconObjectPath =
		TEXT("/Game/Developers/zxcv1/CharacterProgression/UI/Icons/T_ProgressionNode_Generic.T_ProgressionNode_Generic");

	return LoadObject<UTexture2D>(nullptr, IconObjectPath);
}

void UNUProgressionNodeWidget::RefreshState()
{
	// 잠긴 노드도 호버 설명은 볼 수 있고 클릭 획득만 차단한다.
	if (NodeButton != nullptr)
	{
		NodeButton->SetIsEnabled(true);
		NodeButton->SetRenderOpacity(1.f);
	}
	if (IconImage != nullptr)
	{
		IconImage->SetColorAndOpacity(NodeState == ENUProgressionNodeState::Locked
			? FLinearColor(0.28f, 0.31f, 0.38f, 1.f)
			: FLinearColor::White);
	}

	RefreshVisualStyle();
}

void UNUProgressionNodeWidget::RefreshVisualStyle()
{
	if (NodeButton == nullptr)
	{
		return;
	}

	const bool bHighlight = bSelected || NodeState == ENUProgressionNodeState::Acquired;
	const FSlateColor HighlightColor(FLinearColor(1.f, 0.72f, 0.18f, 1.f));
	const FSlateColor HighlightHoverColor(FLinearColor(1.f, 0.84f, 0.42f, 1.f));
	const FSlateColor NodeHoverColor(FLinearColor(0.32f, 0.68f, 1.f, 1.f));
	const FSlateColor TransparentColor(FLinearColor::Transparent);

	auto MakeRoundBrush = [](const FSlateColor& OutlineColor, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::Transparent);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(OutlineColor, OutlineWidth);
		return Brush;
	};

	FButtonStyle Style = NodeButton->GetStyle();
	Style.SetNormal(MakeRoundBrush(bHighlight ? HighlightColor : TransparentColor, bHighlight ? 5.f : 0.f));
	Style.SetHovered(MakeRoundBrush(bHighlight ? HighlightHoverColor : NodeHoverColor, bHighlight ? 6.f : 4.f));
	Style.SetPressed(MakeRoundBrush(bHighlight ? HighlightColor : NodeHoverColor, 6.f));
	Style.SetDisabled(MakeRoundBrush(bHighlight ? HighlightColor : TransparentColor, bHighlight ? 5.f : 0.f));
	Style.SetNormalPadding(FMargin(0.f));
	Style.SetPressedPadding(FMargin(0.f));
	NodeButton->SetStyle(Style);
	NodeButton->SetBackgroundColor(FLinearColor::White);
}

void UNUProgressionNodeWidget::RefreshSelectionIndicator()
{
	// 별도 사각 테두리는 숨기고 원형 노드 자체의 외곽선만 강조한다.
	if (SelectionIndicator != nullptr)
	{
		SelectionIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshVisualStyle();
}
