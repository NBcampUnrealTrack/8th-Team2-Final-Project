// Fill out your copyright notice in the Description page of Project Settings.

#include "Battle/Data/NUJobResourceViews.h"

#include "Battle/NUBattleTypes.h"

bool NUJobResourceView::IsShownResource(const FNUJobResourceDefinition& Def)
{
	return !Def.IsDerived() && !Def.IsResolvedByJobNode() && !Def.IsCommonToAllJobs();
}

bool NUJobResourceView::ShouldShowGauge(bool bHasStoredAmount, bool bOwnsJobNode)
{
	return bHasStoredAmount || bOwnsJobNode;
}

bool NUJobResourceView::ShouldShowBadge(int32 Amount)
{
	return Amount > 0;
}

int32 NUJobResourceView::ResolveRemainingTurns(const FNUJobResourceDefinition& Def,
	const FNUJobResourceInstance* Instance)
{
	if (Def.DecayType == ENUJobResourceDecay::TimeoutEach && Instance && Instance->StackTurns.Num() > 0)
	{
		// 중첩마다 따로 사라지므로 가장 먼저 사라질 것까지
		return FMath::Min(Instance->StackTurns);
	}

	if (Def.DecayType == ENUJobResourceDecay::TimeoutAll && Instance)
	{
		// 더 안 쌓이면 전량이 사라질 때까지 (마크 3턴)
		return FMath::Max(0, Def.DecayInterval - Instance->IdleTurns);
	}

	return 0;
}

FNUJobResourceView NUJobResourceView::MakeResourceView(const FNUJobResourceDefinition& Def, int32 Amount,
	int32 MaxAmount, int32 RemainingTurns)
{
	FNUJobResourceView View;
	View.ResourceId = Def.ResourceId;
	View.ResourceName = Def.KoreanName;
	View.ResourceKey = Def.ResourceKey;
	View.DisplayName = FText::FromName(Def.KoreanName);
	View.Description = Def.Description;
	View.Amount = Amount;
	View.MaxAmount = MaxAmount;
	View.RemainingTurns = RemainingTurns;

	return View;
}

bool NUJobResourceView::MakeStateView(const FNUJobResourceDefinition* Def, FName ResourceName, FName StateName,
	int32 StartTurn, int32 EndTurn, int32 Turn, FNUJobResourceView& OutView)
{
	if (EndTurn < Turn)
	{
		return false;
	}

	OutView = FNUJobResourceView();
	OutView.bIsState = true;
	OutView.ResourceId = Def ? Def->ResourceId : 0;
	OutView.ResourceName = ResourceName;
	OutView.ResourceKey = Def ? Def->ResourceKey : NAME_None;
	OutView.DisplayName = FText::FromName(StateName);
	OutView.bStatePending = StartTurn > Turn;
	// 턴 제한이 없는 상태는 남은 턴이 없음 (0)
	OutView.RemainingTurns = EndTurn >= FNUJobStateInstance::OpenEnded
		? 0
		: EndTurn - FMath::Max(StartTurn, Turn) + 1;

	return true;
}

void NUJobResourceView::SortViews(TArray<FNUJobResourceView>& InOutGauges, TArray<FNUJobResourceView>& InOutBadges,
	TArray<FNUJobResourceView>& InOutStates)
{
	const auto ByResourceId = [](const FNUJobResourceView& Left, const FNUJobResourceView& Right)
	{
		return Left.ResourceId < Right.ResourceId;
	};

	InOutGauges.Sort(ByResourceId);
	InOutBadges.Sort(ByResourceId);

	InOutStates.Sort(ByResourceId);
	InOutBadges.Append(InOutStates);
}
