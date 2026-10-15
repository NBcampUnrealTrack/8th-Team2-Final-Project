#include "CharacterProgression/UI/ProgressionTree/NUProgressionGraphWidget.h"

#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterProgression/Data/NUProgressionTypes.h"
#include "CharacterProgression/UI/ProgressionTree/NUProgressionGraphLinkWidget.h"
#include "CharacterProgression/UI/ProgressionTree/NUProgressionNodeTooltipWidget.h"
#include "CharacterProgression/UI/ProgressionTree/NUProgressionNodeWidget.h"
#include "UI/NUUISettings.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"

namespace
{
	constexpr float MinZoomLevel = 0.2f;
	constexpr float MaxZoomLevel = 1.6f;
	constexpr float ZoomStep = 0.1f;
	constexpr int32 LinkLayerZOrder = -10;
	constexpr int32 NodeLayerZOrder = 10;

	struct FResolvedNodeLayout
	{
		const FNUProgressionNodeDefinitionRow* Node = nullptr;
		FVector2D RawCenter = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
	};

	FVector2D GetNodeRenderSize(
		const FNUProgressionNodeDefinitionRow& Node,
		float InStatNodeSize,
		float InSkillNodeSize,
		float InLargeNodeSize,
		float InAdvancementNodeScale)
	{
		if (Node.NodeType == ENUProgressionNodeType::Stat)
		{
			return FVector2D(InStatNodeSize);
		}
		if (Node.NodeType == ENUProgressionNodeType::Skill)
		{
			return FVector2D(InSkillNodeSize);
		}
		if (Node.NodeType == ENUProgressionNodeType::Advancement)
		{
			return FVector2D(InLargeNodeSize * InAdvancementNodeScale);
		}
		return FVector2D(InLargeNodeSize);
	}

	FVector2D ResolveRawNodeCenter(const FNUProgressionNodeDefinitionRow& Node, float InColumnSpacing, float InRowSpacing)
	{
		if (Node.LayoutMode == ENUProgressionNodeLayoutMode::Radial)
		{
			const float AngleRadians = FMath::DegreesToRadians(Node.LayoutAngleDegrees);
			return FVector2D(FMath::Cos(AngleRadians), -FMath::Sin(AngleRadians)) * Node.LayoutRadius
				+ FVector2D(Node.LayoutOffsetX, Node.LayoutOffsetY);
		}

		return FVector2D(
			Node.LayoutColumn * InColumnSpacing + Node.LayoutOffsetX,
			-Node.LayoutRow * InRowSpacing + Node.LayoutOffsetY);
	}

}

void UNUProgressionGraphWidget::InitializeGraph(int32 InGraphId, UNUProgressionComponent* InProgression, FOnNUProgressionGraphNodeSelected InOnNodeSelected)
{
	GraphId = InGraphId;
	Progression = InProgression;
	OnNodeSelected = MoveTemp(InOnNodeSelected);
}

void UNUProgressionGraphWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 빈 영역도 드래그와 휠 입력을 받을 수 있게 한다.
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	SetClipping(EWidgetClipping::ClipToBounds);
	RefreshGraph();
}

void UNUProgressionGraphWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const FVector2D ViewportSize = MyGeometry.GetLocalSize();
	if (ViewportSize.X <= 0.f || ViewportSize.Y <= 0.f)
	{
		return;
	}

	const bool bViewportSizeChanged = !ViewportSize.Equals(LastViewportSize);
	const bool bShouldCenterGraph = bCenterGraphOnOpen;
	if (bShouldCenterGraph)
	{
		// 처음 열 때 트리 전체의 중심을 화면 가운데에 배치
		ViewOffset = GraphContentSize * ZoomLevel * 0.5f - ViewportSize * 0.5f;
		bCenterGraphOnOpen = false;
	}

	if (bShouldCenterGraph || bViewportSizeChanged)
	{
		ApplyViewOffset(ViewportSize);
	}
	LastViewportSize = ViewportSize;
}

FReply UNUProgressionGraphWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton
		|| IsPointerOverNode(InMouseEvent.GetScreenSpacePosition()))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	bIsPanning = true;
	return FReply::Handled()
		.CaptureMouse(TakeWidget())
		.SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

FReply UNUProgressionGraphWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bIsPanning || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	bIsPanning = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UNUProgressionGraphWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bIsPanning || !InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		bIsPanning = false;
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	// 빈 배경을 잡아당긴 방향으로 그래프 이동
	ViewOffset -= InMouseEvent.GetCursorDelta();
	ApplyViewOffset(InGeometry.GetLocalSize());
	return FReply::Handled();
}

FReply UNUProgressionGraphWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ViewportSize = InGeometry.GetLocalSize();
	if (ViewportSize.X <= 0.f || ViewportSize.Y <= 0.f)
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}

	const float NewZoomLevel = FMath::Clamp(
		ZoomLevel + InMouseEvent.GetWheelDelta() * ZoomStep,
		MinZoomLevel,
		MaxZoomLevel);
	if (FMath::IsNearlyEqual(NewZoomLevel, ZoomLevel))
	{
		return FReply::Handled();
	}

	// 커서가 가리키던 그래프 좌표를 확대 전후 같은 위치에 유지
	const FVector2D CursorPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D GraphPositionAtCursor = (ViewOffset + CursorPosition) / ZoomLevel;
	ZoomLevel = NewZoomLevel;
	ViewOffset = GraphPositionAtCursor * ZoomLevel - CursorPosition;
	ApplyViewOffset(ViewportSize);
	return FReply::Handled();
}

void UNUProgressionGraphWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bIsPanning = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UNUProgressionGraphWidget::RefreshGraph()
{
	// 다시 만들기 전에 이전 노드 위젯과 연결선 정보 제거
	ClearGraph();
	GraphContentSize = FVector2D::ZeroVector;

	UGameInstance* GameInstance = GetGameInstance();
	UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	// 노드 하나를 표시할 WBP 클래스 조회
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	TSubclassOf<UNUProgressionNodeWidget> NodeClass = Settings != nullptr
		? Settings->ProgressionNodeWidgetClass.LoadSynchronous()
		: nullptr;
	if (GraphId <= 0 || Data == nullptr || !Data->IsReady()
		|| GraphCanvas == nullptr || NodeClass == nullptr)
	{
		UE_LOG(LogNUProgression, Warning, TEXT("성장 그래프 위젯 설정이 완료되지 않음"));
		return;
	}

	const TArray<const FNUProgressionNodeDefinitionRow*> Nodes = Data->GetNodesByGraph(GraphId);
	RenderedFinalAnchorNodeId = ResolveFinalAnchorNodeId(*Data);
	const FNUProgressionNodeDefinitionRow* FinalAnchorNode = Data->FindNode(RenderedFinalAnchorNodeId);
	TMap<int32, FVector2D> FinalNodeCenters;
	if (FinalAnchorNode != nullptr && FinalAnchorNode->NodeType == ENUProgressionNodeType::Main && Progression.IsValid())
	{
		TArray<const FNUProgressionNodeDefinitionRow*> FinalCandidates;
		const FNUProgressionState& State = Progression->GetState();
		const FNUProgressionNodeDefinitionRow* SelectedFinalNode = Data->FindNode(State.SelectedFinalNodeId);
		for (const FNUProgressionNodeDefinitionRow* Node : Nodes)
		{
			if (Node == nullptr || Node->NodeType != ENUProgressionNodeType::Final)
			{
				continue;
			}

			const bool bMatchesSelectedFinal = SelectedFinalNode != nullptr
				&& Node->RequiredAdvancementId == SelectedFinalNode->RequiredAdvancementId;
			const bool bCanUseAnchor = State.SelectedFinalNodeId <= 0
				&& Progression->ResolveAvailableParentNodeId(Node->NodeId) == RenderedFinalAnchorNodeId;
			if (bMatchesSelectedFinal || bCanUseAnchor)
			{
				FinalCandidates.Add(Node);
			}
		}
		FinalCandidates.Sort([](const FNUProgressionNodeDefinitionRow& Left, const FNUProgressionNodeDefinitionRow& Right)
		{
			return Left.NodeId < Right.NodeId;
		});

		const FVector2D AnchorCenter = ResolveRawNodeCenter(*FinalAnchorNode, LayoutColumnSpacing, LayoutRowSpacing);
		const FVector2D RadialDirection = AnchorCenter.GetSafeNormal();
		const FVector2D TangentDirection(-RadialDirection.Y, RadialDirection.X);
		const FVector2D FinalBaseCenter = AnchorCenter + RadialDirection * FinalNodeRadialOffset;
		for (int32 Index = 0; Index < FinalCandidates.Num(); ++Index)
		{
			const float CenteredIndex = static_cast<float>(Index) - static_cast<float>(FinalCandidates.Num() - 1) * 0.5f;
			FinalNodeCenters.Add(
				FinalCandidates[Index]->NodeId,
				FinalBaseCenter + TangentDirection * FinalNodeTangentSpacing * CenteredIndex);
		}
	}
	TArray<FResolvedNodeLayout> NodeLayouts;
	NodeLayouts.Reserve(Nodes.Num());
	FVector2D MinBounds = FVector2D::ZeroVector;
	FVector2D MaxBounds = FVector2D::ZeroVector;
	bool bHasBounds = false;

	// 논리 행과 열을 실제 좌표로 바꾼 뒤 전체 그래프가 양수 캔버스 안에 들어오게 범위를 구한다.
	for (const FNUProgressionNodeDefinitionRow* Node : Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		const FVector2D* FinalCenter = Node->NodeType == ENUProgressionNodeType::Final
			? FinalNodeCenters.Find(Node->NodeId)
			: nullptr;
		if (Node->NodeType == ENUProgressionNodeType::Final && FinalCenter == nullptr)
		{
			continue;
		}
		const FVector2D RawCenter = FinalCenter != nullptr
			? *FinalCenter
			: ResolveRawNodeCenter(*Node, LayoutColumnSpacing, LayoutRowSpacing);

		const FVector2D NodeSize = GetNodeRenderSize(
			*Node,
			StatNodeSize,
			SkillNodeSize,
			LargeNodeSize,
			AdvancementNodeScale);
		const FVector2D NodeMin = RawCenter - NodeSize * 0.5f;
		const FVector2D NodeMax = RawCenter + NodeSize * 0.5f;
		if (!bHasBounds)
		{
			MinBounds = NodeMin;
			MaxBounds = NodeMax;
			bHasBounds = true;
		}
		else
		{
			MinBounds.X = FMath::Min(MinBounds.X, NodeMin.X);
			MinBounds.Y = FMath::Min(MinBounds.Y, NodeMin.Y);
			MaxBounds.X = FMath::Max(MaxBounds.X, NodeMax.X);
			MaxBounds.Y = FMath::Max(MaxBounds.Y, NodeMax.Y);
		}
		NodeLayouts.Add({ Node, RawCenter, NodeSize });
	}

	if (!bHasBounds)
	{
		UE_LOG(LogNUProgression, Warning, TEXT("성장 그래프에 배치할 수 있는 노드가 없음: GraphId=%d"), GraphId);
		return;
	}

	const FVector2D LayoutTranslation = FVector2D(LayoutContentPadding) - MinBounds;
	GraphContentSize = MaxBounds - MinBounds + FVector2D(LayoutContentPadding * 2.f);

	TSubclassOf<UNUProgressionGraphLinkWidget> ResolvedLinkLayerClass = LinkLayerWidgetClass;
	if (ResolvedLinkLayerClass == nullptr)
	{
		ResolvedLinkLayerClass = UNUProgressionGraphLinkWidget::StaticClass();
	}
	LinkLayerWidget = CreateWidget<UNUProgressionGraphLinkWidget>(GetOwningPlayer(), ResolvedLinkLayerClass);
	if (LinkLayerWidget != nullptr)
	{
		LinkLayerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* LinkSlot = GraphCanvas->AddChildToCanvas(LinkLayerWidget))
		{
			LinkSlot->SetPosition(FVector2D::ZeroVector);
			LinkSlot->SetSize(GraphContentSize);
			LinkSlot->SetZOrder(LinkLayerZOrder);
		}
	}

	// 계산된 좌표에 같은 노드 WBP를 생성하고 DataTable 행의 내용을 채운다.
	for (const FResolvedNodeLayout& Layout : NodeLayouts)
	{
		const FNUProgressionNodeDefinitionRow* Node = Layout.Node;
		if (Node == nullptr)
		{
			continue;
		}
		const FVector2D NodePosition = Layout.RawCenter + LayoutTranslation - Layout.Size * 0.5f;

		// 노드마다 같은 WBP 클래스를 생성하고 행 데이터만 다르게 전달
		UNUProgressionNodeWidget* NodeWidget = CreateWidget<UNUProgressionNodeWidget>(
			GetOwningPlayer(),
			NodeClass);
		if (NodeWidget == nullptr)
		{
			continue;
		}

		NodeWidget->InitializeNode(
			*Node,
			Layout.Size,
			FOnNUProgressionNodeActivated::CreateUObject(this, &ThisClass::HandleNodeActivated),
			FOnNUProgressionNodeHoverChanged::CreateUObject(this, &ThisClass::HandleNodeHoverChanged),
			FOnNUProgressionNodeTooltipRequested::CreateUObject(this, &ThisClass::HandleNodeTooltipRequested));

		if (UCanvasPanelSlot* CanvasSlot = GraphCanvas->AddChildToCanvas(NodeWidget))
		{
			CanvasSlot->SetPosition(NodePosition);
			CanvasSlot->SetSize(Layout.Size);
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetZOrder(NodeLayerZOrder);
		}

		NodeWidgetsById.Add(Node->NodeId, NodeWidget);

		if (Node->NodeType == ENUProgressionNodeType::Final && RenderedFinalAnchorNodeId > 0)
		{
			Links.Emplace(RenderedFinalAnchorNodeId, Node->NodeId);
		}
		else if (Node->ParentNodeId1 > 0)
		{
			Links.Emplace(Node->ParentNodeId1, Node->NodeId);
		}
		if (Node->NodeType != ENUProgressionNodeType::Final && Node->ParentNodeId2 > 0)
		{
			Links.Emplace(Node->ParentNodeId2, Node->NodeId);
		}
		if (Node->NodeType != ENUProgressionNodeType::Final && Node->ParentNodeId3 > 0)
		{
			Links.Emplace(Node->ParentNodeId3, Node->NodeId);
		}
	}
	RebuildLinkCache();

	// 생성 직후 현재 플레이어 기준 상태 반영
	RefreshNodeStates();

	// 처음 열 때 Root 노드를 선택 상태로 표시
	for (const FNUProgressionNodeDefinitionRow* Node : Nodes)
	{
		if (Node != nullptr && Node->NodeType == ENUProgressionNodeType::Root
			&& NodeWidgetsById.Contains(Node->NodeId))
		{
			SelectNode(Node->NodeId, false);
			break;
		}
	}

	InvalidateLayoutAndVolatility();
	bCenterGraphOnOpen = true;
}

void UNUProgressionGraphWidget::RefreshNodeStates()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	if (Data != nullptr && ResolveFinalAnchorNodeId(*Data) != RenderedFinalAnchorNodeId)
	{
		RefreshGraph();
		return;
	}

	// 같은 정적 노드라도 플레이어 State에 따라 잠김, 획득 가능, 완료가 달라짐
	for (const TPair<int32, TObjectPtr<UNUProgressionNodeWidget>>& Pair : NodeWidgetsById)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->SetNodeState(
				Progression.IsValid()
					? Progression->GetNodeState(Pair.Key)
					: ENUProgressionNodeState::Locked);
		}
	}
	RefreshLinkStates();
}

int32 UNUProgressionGraphWidget::ResolveFinalAnchorNodeId(const UNUProgressionDataSubsystem& Data) const
{
	if (!Progression.IsValid())
	{
		return 0;
	}

	const FNUProgressionState& State = Progression->GetState();
	if (FNUProgressionNodeRules::GetCompletedRadialStemCount(State) != 4 || State.ActiveStemId > 0)
	{
		return 0;
	}
	if (State.SelectedFinalNodeId > 0)
	{
		for (const FNUProgressionNodeLinkState& Link : State.AcquiredNodeLinks)
		{
			if (Link.ChildNodeId == State.SelectedFinalNodeId)
			{
				const FNUProgressionNodeDefinitionRow* Parent = Data.FindNode(Link.ParentNodeId);
				return Parent != nullptr && Parent->NodeType == ENUProgressionNodeType::Main
					? Parent->NodeId
					: 0;
			}
		}
		return 0;
	}

	const FNUProgressionNodeDefinitionRow* CurrentNode = Data.FindNode(State.CurrentNodeId);
	return CurrentNode != nullptr
		&& CurrentNode->NodeType == ENUProgressionNodeType::Main
		&& State.AcquiredNodeIds.Contains(CurrentNode->NodeId)
		? CurrentNode->NodeId
		: 0;
}

void UNUProgressionGraphWidget::RebuildLinkCache()
{
	if (LinkLayerWidget == nullptr)
	{
		return;
	}

	TArray<FNUProgressionGraphLinkDefinition> LinkDefinitions;
	LinkDefinitions.Reserve(Links.Num());
	for (const TPair<int32, int32>& Link : Links)
	{
		const TObjectPtr<UNUProgressionNodeWidget>* ParentWidget = NodeWidgetsById.Find(Link.Key);
		const TObjectPtr<UNUProgressionNodeWidget>* ChildWidget = NodeWidgetsById.Find(Link.Value);
		const UCanvasPanelSlot* ParentSlot = ParentWidget != nullptr && ParentWidget->Get() != nullptr
			? Cast<UCanvasPanelSlot>(ParentWidget->Get()->Slot)
			: nullptr;
		const UCanvasPanelSlot* ChildSlot = ChildWidget != nullptr && ChildWidget->Get() != nullptr
			? Cast<UCanvasPanelSlot>(ChildWidget->Get()->Slot)
			: nullptr;
		if (ParentSlot == nullptr || ChildSlot == nullptr)
		{
			continue;
		}

		FNUProgressionGraphLinkDefinition& Definition = LinkDefinitions.AddDefaulted_GetRef();
		Definition.ParentNodeId = Link.Key;
		Definition.ChildNodeId = Link.Value;
		const UGameInstance* GameInstance = GetGameInstance();
		const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
			? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
			: nullptr;
		const FNUProgressionNodeDefinitionRow* ChildNode = Data != nullptr
			? Data->FindNode(Link.Value)
			: nullptr;
		const FNUProgressionNodeDefinitionRow* ParentNode = Data != nullptr
			? Data->FindNode(Link.Key)
			: nullptr;
		Definition.ParentStemId = ParentNode != nullptr ? ParentNode->StemId : 0;
		Definition.ChildStemId = ChildNode != nullptr ? ChildNode->StemId : 0;
		Definition.bParentIsAdvancement = ParentNode != nullptr
			&& ParentNode->NodeType == ENUProgressionNodeType::Advancement;
		Definition.bChildIsAdvancement = ChildNode != nullptr
			&& ChildNode->NodeType == ENUProgressionNodeType::Advancement;
		Definition.ParentCenter = ParentSlot->GetPosition() + ParentSlot->GetSize() * 0.5f;
		Definition.ParentSize = ParentSlot->GetSize();
		Definition.ChildCenter = ChildSlot->GetPosition() + ChildSlot->GetSize() * 0.5f;
		Definition.ChildSize = ChildSlot->GetSize();
	}

	LinkLayerWidget->SetLinks(MoveTemp(LinkDefinitions));
}

void UNUProgressionGraphWidget::RefreshLinkStates()
{
	if (LinkLayerWidget == nullptr)
	{
		return;
	}

	const FNUProgressionState* State = Progression.IsValid()
		? &Progression->GetState()
		: nullptr;
	const TObjectPtr<UNUProgressionNodeWidget>* HoveredNodeWidget = NodeWidgetsById.Find(HoveredNodeId);
	const bool bHoveredNodeAvailable = HoveredNodeWidget != nullptr
		&& HoveredNodeWidget->Get() != nullptr
		&& HoveredNodeWidget->Get()->GetNodeState() == ENUProgressionNodeState::Available;

	int32 HoveredParentNodeId = 0;
	if (bHoveredNodeAvailable && Progression.IsValid())
	{
		HoveredParentNodeId = Progression->ResolveAvailableParentNodeId(HoveredNodeId);
	}

	static const TArray<FNUProgressionNodeLinkState> EmptyAcquiredNodeLinks;
	LinkLayerWidget->RefreshLinkStates(
		State != nullptr ? State->AcquiredNodeLinks : EmptyAcquiredNodeLinks,
		HoveredParentNodeId,
		HoveredNodeId,
		bHoveredNodeAvailable);
}

void UNUProgressionGraphWidget::HandleNodeActivated(int32 NodeId)
{
	UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	if (Data != nullptr)
	{
		if (const FNUProgressionNodeDefinitionRow* Node = Data->FindNode(NodeId))
		{
			OnNodeSelected.ExecuteIfBound(*Node, true);
		}
	}
}

void UNUProgressionGraphWidget::HandleNodeHoverChanged(int32 NodeId, bool bHovered)
{
	if (bHovered)
	{
		HoveredNodeId = NodeId;
	}
	else if (HoveredNodeId == NodeId)
	{
		HoveredNodeId = 0;
	}

	RefreshLinkStates();
}

UWidget* UNUProgressionGraphWidget::HandleNodeTooltipRequested(const FNUProgressionNodeDefinitionRow& Node)
{
	if (SharedNodeTooltipWidget == nullptr)
	{
		const UNUUISettings* Settings = GetDefault<UNUUISettings>();
		const TSubclassOf<UNUProgressionNodeTooltipWidget> TooltipClass = Settings != nullptr
			? Settings->ProgressionNodeTooltipWidgetClass.LoadSynchronous()
			: nullptr;
		if (TooltipClass != nullptr)
		{
			SharedNodeTooltipWidget = CreateWidget<UNUProgressionNodeTooltipWidget>(GetOwningPlayer(), TooltipClass);
		}
	}

	if (SharedNodeTooltipWidget != nullptr)
	{
		SharedNodeTooltipWidget->SetNodeDefinition(Node);
	}
	return SharedNodeTooltipWidget;
}

void UNUProgressionGraphWidget::SelectNode(int32 NodeId, bool bAcquireRequested)
{
	// 현재 화면에 생성되지 않은 노드는 선택할 수 없음
	if (!NodeWidgetsById.Contains(NodeId))
	{
		return;
	}

	// 선택 표시를 새 노드 하나에만 적용
	SelectedNodeId = NodeId;
	for (const TPair<int32, TObjectPtr<UNUProgressionNodeWidget>>& Pair : NodeWidgetsById)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->SetSelected(Pair.Key == SelectedNodeId);
		}
	}

	// 선택한 NodeId의 원본 행을 찾아 화면으로 전달
	UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	if (Data != nullptr)
	{
		if (const FNUProgressionNodeDefinitionRow* Node = Data->FindNode(SelectedNodeId))
		{
			OnNodeSelected.ExecuteIfBound(*Node, bAcquireRequested);
		}
	}

	InvalidateLayoutAndVolatility();
}

void UNUProgressionGraphWidget::ClearGraph()
{
	for (const TPair<int32, TObjectPtr<UNUProgressionNodeWidget>>& Pair : NodeWidgetsById)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	NodeWidgetsById.Reset();
	if (LinkLayerWidget != nullptr)
	{
		LinkLayerWidget->RemoveFromParent();
		LinkLayerWidget = nullptr;
	}
	Links.Reset();
	SelectedNodeId = 0;
	HoveredNodeId = 0;
	SharedNodeTooltipWidget = nullptr;
	LastViewportSize = FVector2D::ZeroVector;
}

bool UNUProgressionGraphWidget::IsPointerOverNode(const FVector2D& ScreenPosition) const
{
	for (const TPair<int32, TObjectPtr<UNUProgressionNodeWidget>>& Pair : NodeWidgetsById)
	{
		if (Pair.Value != nullptr && Pair.Value->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return true;
		}
	}
	return false;
}

void UNUProgressionGraphWidget::ApplyViewOffset(const FVector2D& ViewportSize)
{
	const FVector2D ScaledContentSize = GraphContentSize * ZoomLevel;
	const FVector2D MinViewOffset(
		ScaledContentSize.X < ViewportSize.X ? (ScaledContentSize.X - ViewportSize.X) * 0.5f : 0.f,
		ScaledContentSize.Y < ViewportSize.Y ? (ScaledContentSize.Y - ViewportSize.Y) * 0.5f : 0.f);
	const FVector2D MaxViewOffset(
		ScaledContentSize.X < ViewportSize.X ? MinViewOffset.X : ScaledContentSize.X - ViewportSize.X,
		ScaledContentSize.Y < ViewportSize.Y ? MinViewOffset.Y : ScaledContentSize.Y - ViewportSize.Y);
	ViewOffset.X = FMath::Clamp(ViewOffset.X, MinViewOffset.X, MaxViewOffset.X);
	ViewOffset.Y = FMath::Clamp(ViewOffset.Y, MinViewOffset.Y, MaxViewOffset.Y);
	if (GraphCanvas != nullptr)
	{
		GraphCanvas->SetRenderTransformPivot(FVector2D::ZeroVector);
		GraphCanvas->SetRenderScale(FVector2D(ZoomLevel));
		GraphCanvas->SetRenderTranslation(-ViewOffset);
	}
}
