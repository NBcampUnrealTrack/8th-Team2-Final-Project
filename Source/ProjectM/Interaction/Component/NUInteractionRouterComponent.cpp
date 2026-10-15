#include "Interaction/Component/NUInteractionRouterComponent.h"

#include "Interaction/Interface/NUInteractionOptionProviderInterface.h"
#include "Interaction/NUInteractionSpatialRules.h"
#include "NUGAS/NUGameplayTags.h"

namespace NUInteractionRouter
{
	// UI에 표시할 선택지와 실행 담당 Provider 내부에서 연결
	struct FResolvedInteractionOption
	{
		FNUInteractionOption Option;
		TWeakObjectPtr<UActorComponent> Provider;
	};

	bool IsContextValid(const UActorComponent* Router, const FNUInteractionContext& Context)
	{
		return IsValid(Router)
			&& Context.IsValid()
			&& Context.Target.Get() == Router->GetOwner()
			&& NUInteractionSpatialRules::CanInteract(Context.Interactor.Get(), Context.Target.Get());
	}

	int32 GetGroupSortOrder(ENUInteractionOptionGroup Group)
	{
		// 퀘스트 > 서비스 > 대화 > 기타 > 시스템(나가기). 목적을 갖고 온 플레이어가 서비스를 먼저 만나도록 대화는 서비스 뒤에 둔다.
		// 서비스 그룹 안 기본 SortOrder: 직업 0, 나이아 계약 10, 상점 20, 공방 30, 창고 40, 회복 50(무료 휴식은 +1)
		switch (Group)
		{
		case ENUInteractionOptionGroup::QuestTurnIn:	return 0;
		case ENUInteractionOptionGroup::QuestOffer:		return 1;
		case ENUInteractionOptionGroup::QuestTarget:		return 2;
		case ENUInteractionOptionGroup::QuestInProgress:	return 3;
		case ENUInteractionOptionGroup::Service:		return 4;
		case ENUInteractionOptionGroup::Dialogue:		return 5;
		case ENUInteractionOptionGroup::Other:			return 6;
		case ENUInteractionOptionGroup::System:			return 7;
		default:									return 7;
		}
	}

	bool SortOptions(const FResolvedInteractionOption& Left, const FResolvedInteractionOption& Right)
	{
		const int32 LeftGroupOrder = GetGroupSortOrder(Left.Option.Group);
		const int32 RightGroupOrder = GetGroupSortOrder(Right.Option.Group);
		if (LeftGroupOrder != RightGroupOrder)
		{
			return LeftGroupOrder < RightGroupOrder;
		}
		if (Left.Option.SortOrder != Right.Option.SortOrder)
		{
			return Left.Option.SortOrder < Right.Option.SortOrder;
		}

		const int32 TypeOrder = Left.Option.Handle.OptionType.ToString().Compare(
			Right.Option.Handle.OptionType.ToString());
		if (TypeOrder != 0)
		{
			return TypeOrder < 0;
		}

		const int32 IdOrder = Left.Option.Handle.OptionId.ToString().Compare(
			Right.Option.Handle.OptionId.ToString());
		if (IdOrder != 0)
		{
			return IdOrder < 0;
		}

		return GetNameSafe(Left.Provider.Get()) < GetNameSafe(Right.Provider.Get());
	}

	void CollectResolvedOptions(const UActorComponent* Router, const FNUInteractionContext& Context, TArray<FResolvedInteractionOption>& OutResolvedOptions)
	{
		OutResolvedOptions.Reset();
		if (!IsContextValid(Router, Context))
		{
			return;
		}

		// 대상 액터에 붙은 모든 컴포넌트에서 Provider를 찾는다.
		TArray<UActorComponent*> Components;
		Router->GetOwner()->GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (!IsValid(Component)
				|| !Component->GetClass()->ImplementsInterface(UNUInteractionOptionProviderInterface::StaticClass()))
			{
				continue;
			}

			// Provider별 선택지를 따로 받아 실행할 Provider와 함께 보관
			TArray<FNUInteractionOption> ProviderOptions;
			INUInteractionOptionProviderInterface::Execute_CollectInteractionOptions(
				Component,
				Context,
				ProviderOptions);

			for (FNUInteractionOption& Option : ProviderOptions)
			{
				if (!Option.IsValid()
					|| Option.Handle.OptionType == NU::Interaction::Option::Root
					|| !Option.Handle.OptionType.MatchesTag(NU::Interaction::Option::Root))
				{
					continue;
				}

				FResolvedInteractionOption& Resolved = OutResolvedOptions.AddDefaulted_GetRef();
				Resolved.Option = MoveTemp(Option);
				Resolved.Provider = Component;
			}
		}

		// 의미 그룹, 기획 SortOrder 순으로 정렬한 뒤 Handle로 결과를 고정한다.
		OutResolvedOptions.StableSort(&SortOptions);

		// 같은 Handle이 여러 번 제공되면 정렬 결과의 첫 번째 항목만 유지
		TSet<FNUInteractionOptionHandle> SeenHandles;
		TArray<FResolvedInteractionOption> UniqueOptions;
		UniqueOptions.Reserve(OutResolvedOptions.Num());

		for (FResolvedInteractionOption& Resolved : OutResolvedOptions)
		{
			if (SeenHandles.Contains(Resolved.Option.Handle))
			{
				continue;
			}

			SeenHandles.Add(Resolved.Option.Handle);
			UniqueOptions.Add(MoveTemp(Resolved));
		}

		OutResolvedOptions = MoveTemp(UniqueOptions);
	}

	bool ExecuteResolvedOption(const FNUInteractionContext& Context, const FResolvedInteractionOption& Resolved)
	{
		UActorComponent* Provider = Resolved.Provider.Get();
		if (!IsValid(Provider) || !Resolved.Option.bEnabled)
		{
			return false;
		}

		if (!INUInteractionOptionProviderInterface::Execute_CanExecuteInteraction(Provider, Context, Resolved.Option.Handle))
		{
			return false;
		}

		return INUInteractionOptionProviderInterface::Execute_ExecuteInteraction(Provider, Context, Resolved.Option.Handle);
	}
}

UNUInteractionRouterComponent::UNUInteractionRouterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UNUInteractionRouterComponent::CollectInteractionOptions(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const
{
	OutOptions.Reset();

	TArray<NUInteractionRouter::FResolvedInteractionOption> ResolvedOptions;
	NUInteractionRouter::CollectResolvedOptions(this, Context, ResolvedOptions);

	OutOptions.Reserve(ResolvedOptions.Num());
	for (const NUInteractionRouter::FResolvedInteractionOption& Resolved : ResolvedOptions)
	{
		OutOptions.Add(Resolved.Option);
	}
}

ENUInteractionRouteResult UNUInteractionRouterComponent::RouteInteraction(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions, ENUInteractionRoutePolicy Policy)
{
	OutOptions.Reset();

	TArray<NUInteractionRouter::FResolvedInteractionOption> ResolvedOptions;
	NUInteractionRouter::CollectResolvedOptions(this, Context, ResolvedOptions);

	OutOptions.Reserve(ResolvedOptions.Num());
	for (const NUInteractionRouter::FResolvedInteractionOption& Resolved : ResolvedOptions)
	{
		OutOptions.Add(Resolved.Option);
	}

	if (ResolvedOptions.IsEmpty())
	{
		return ENUInteractionRouteResult::Failed;
	}

	if (Policy == ENUInteractionRoutePolicy::AutoExecuteSingle
		&& ResolvedOptions.Num() == 1
		&& NUInteractionRouter::ExecuteResolvedOption(Context, ResolvedOptions[0]))
	{
		return ENUInteractionRouteResult::Executed;
	}

	// NPC 정책, 여러 선택지, 실행할 수 없는 단일 선택지는 플레이어에게 상태를 보여준다.
	return ENUInteractionRouteResult::SelectionRequired;
}

bool UNUInteractionRouterComponent::ExecuteInteractionOption(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return false;
	}

	// 대화창을 연 시점이 아닌 현재 대상의 선택지를 다시 수집
	TArray<NUInteractionRouter::FResolvedInteractionOption> ResolvedOptions;
	NUInteractionRouter::CollectResolvedOptions(this, Context, ResolvedOptions);

	const NUInteractionRouter::FResolvedInteractionOption* Resolved = ResolvedOptions.FindByPredicate(
		[&Handle](const NUInteractionRouter::FResolvedInteractionOption& Candidate)
		{
			return Candidate.Option.Handle == Handle;
		});

	return Resolved != nullptr
		&& NUInteractionRouter::ExecuteResolvedOption(Context, *Resolved);
}
