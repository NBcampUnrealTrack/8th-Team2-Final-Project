#include "CharacterProgression/UI/ProgressionTree/NUProgressionGraphLinkWidget.h"

#include "Rendering/DrawElements.h"

namespace
{
	// 이미지 외곽 사이의 외각 사이의 연결선 좌표 계산
	bool ResolveLinkEndpoints(
		const FNUProgressionGraphLinkDefinition& Link,
		float NodeGap,
		FVector2D& OutStart,
		FVector2D& OutEnd)
	{
		// 부모 노드에서 자식 노드까지의 방향과 거리
		const FVector2D Delta = Link.ChildCenter - Link.ParentCenter;
		const float LinkLength = Delta.Size();
		if (LinkLength <= UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}

		// 반지름과 설정된 간격만큼 중심에서 떨어뜨림
		const FVector2D Direction = Delta / LinkLength;
		const float ParentDistance = Link.ParentSize.GetMin() * 0.5f + NodeGap;
		const float ChildDistance = Link.ChildSize.GetMin() * 0.5f + NodeGap;
		if (ParentDistance + ChildDistance >= LinkLength)
		{
			return false;
		}

		OutStart = Link.ParentCenter + Direction * ParentDistance;
		OutEnd = Link.ChildCenter - Direction * ChildDistance;
		return true;
	}

	bool IsConfirmedPathLink(
		const TArray<FNUProgressionNodeLinkState>& AcquiredNodeLinks,
		int32 ParentNodeId,
		int32 ChildNodeId)
	{
		return AcquiredNodeLinks.ContainsByPredicate(
			[ParentNodeId, ChildNodeId](const FNUProgressionNodeLinkState& Link)
			{
				return Link.ParentNodeId == ParentNodeId && Link.ChildNodeId == ChildNodeId;
			});
	}
}

void UNUProgressionGraphLinkWidget::SetLinks(TArray<FNUProgressionGraphLinkDefinition> InLinks)
{
	CachedLinks.Reset(InLinks.Num());
	TArray<int32> SourceDefinitionIndices;
	SourceDefinitionIndices.Reserve(InLinks.Num());
	for (const FNUProgressionGraphLinkDefinition& Link : InLinks)
	{
		FVector2D Start;
		FVector2D End;
		if (!ResolveLinkEndpoints(Link, NodeGap, Start, End))
		{
			continue;
		}

		FCachedLink& CachedLink = CachedLinks.AddDefaulted_GetRef();
		CachedLink.ParentNodeId = Link.ParentNodeId;
		CachedLink.ChildNodeId = Link.ChildNodeId;
		CachedLink.Start = Start;
		CachedLink.End = End;
		SourceDefinitionIndices.Add(&Link - InLinks.GetData());
	}

	if (bUseThreeWayJunctions)
	{
		TMap<int32, TArray<int32>> IncomingGroups;
		TMap<int32, TArray<int32>> OutgoingGroups;
		for (int32 CachedIndex = 0; CachedIndex < CachedLinks.Num(); ++CachedIndex)
		{
			const FNUProgressionGraphLinkDefinition& Definition = InLinks[SourceDefinitionIndices[CachedIndex]];
			if (Definition.bChildIsAdvancement)
			{
				IncomingGroups.FindOrAdd(Definition.ParentStemId).Add(CachedIndex);
			}
			if (Definition.bParentIsAdvancement)
			{
				OutgoingGroups.FindOrAdd(Definition.ChildStemId).Add(CachedIndex);
			}
		}

		auto ApplyJunction = [this, &InLinks, &SourceDefinitionIndices](const TArray<int32>& Group, bool bAdvancementAtStart)
		{
			if (Group.IsEmpty())
			{
				return;
			}

			FVector2D OtherCenterAverage = FVector2D::ZeroVector;
			for (const int32 LinkIndex : Group)
			{
				const FNUProgressionGraphLinkDefinition& Definition = InLinks[SourceDefinitionIndices[LinkIndex]];
				OtherCenterAverage += bAdvancementAtStart ? Definition.ChildCenter : Definition.ParentCenter;
			}
			OtherCenterAverage /= static_cast<float>(Group.Num());

			const FNUProgressionGraphLinkDefinition& FirstDefinition = InLinks[SourceDefinitionIndices[Group[0]]];
			const FVector2D AdvancementCenter = bAdvancementAtStart
				? FirstDefinition.ParentCenter
				: FirstDefinition.ChildCenter;
			const FVector2D AdvancementSize = bAdvancementAtStart
				? FirstDefinition.ParentSize
				: FirstDefinition.ChildSize;
			const FVector2D Direction = (OtherCenterAverage - AdvancementCenter).GetSafeNormal();
			if (Direction.IsNearlyZero())
			{
				return;
			}

			const float ImageRadius = AdvancementSize.GetMin() * AdvancementImageRadiusScale + NodeGap;
			const FVector2D Junction = AdvancementCenter + Direction * ImageRadius;
			for (const int32 LinkIndex : Group)
			{
				if (bAdvancementAtStart)
				{
					CachedLinks[LinkIndex].Start = Junction;
				}
				else
				{
					CachedLinks[LinkIndex].End = Junction;
				}
			}
		};

		for (const TPair<int32, TArray<int32>>& Group : IncomingGroups)
		{
			ApplyJunction(Group.Value, false);
		}
		for (const TPair<int32, TArray<int32>>& Group : OutgoingGroups)
		{
			ApplyJunction(Group.Value, true);
		}
	}

	Invalidate(EInvalidateWidgetReason::Paint);
}

void UNUProgressionGraphLinkWidget::RefreshLinkStates(
	const TArray<FNUProgressionNodeLinkState>& AcquiredNodeLinks,
	int32 HoveredParentNodeId,
	int32 HoveredNodeId,
	bool bHoveredNodeAvailable)
{
	bool bVisualStateChanged = false;
	for (FCachedLink& Link : CachedLinks)
	{
		EVisualState NewState = EVisualState::Inactive;
		
		// 지금까지 실제로 획득한 경로
		if (IsConfirmedPathLink(AcquiredNodeLinks, Link.ParentNodeId, Link.ChildNodeId))
		{
			NewState = EVisualState::Confirmed;
		}
		// 다음으로 선택 가능한 경로
		else if (bHoveredNodeAvailable
			&& Link.ParentNodeId == HoveredParentNodeId
			&& Link.ChildNodeId == HoveredNodeId)
		{
			NewState = EVisualState::Hovered;
		}

		bVisualStateChanged |= Link.VisualState != NewState;
		Link.VisualState = NewState;
	}

	if (bVisualStateChanged)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 UNUProgressionGraphLinkWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// 비활성 선은 한 번, 강조 선은 발광과 본선을 겹쳐서 그린다.
	for (const FCachedLink& Link : CachedLinks)
	{
		// 일반 연결선은 한번에 그림
		if (Link.VisualState == EVisualState::Inactive)
		{
			DrawLink(
				Link,
				AllottedGeometry,
				OutDrawElements,
				LayerId,
				InactiveColor,
				InactiveThickness);
			continue;
		}

		const bool bConfirmed = Link.VisualState == EVisualState::Confirmed;
		const FLinearColor& CoreColor = bConfirmed
			? ConfirmedColor
			: HoverColor;
		FLinearColor GlowColor = CoreColor;
		GlowColor.A *= 0.28f;

		DrawLink(
			Link,
			AllottedGeometry,
			OutDrawElements,
			LayerId + 1,
			GlowColor,
			HighlightThickness + GlowExtraThickness);
		DrawLink(
			Link,
			AllottedGeometry,
			OutDrawElements,
			LayerId + 2,
			CoreColor,
			HighlightThickness);
	}

	return Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId + 3,
		InWidgetStyle,
		bParentEnabled);
}

void UNUProgressionGraphLinkWidget::DrawLink(const FCachedLink& Link, const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Color, float Thickness) const
{
	TArray<FVector2D> Points;
	Points.Reserve(2);
	Points.Add(Link.Start);
	Points.Add(Link.End);
	
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		Color,
		true,
		Thickness);
}
