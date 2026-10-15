#include "CharacterProgression/UI/ProgressionTree/NUProgressionScreenWidget.h"

#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterProgression/Data/NUProgressionTypes.h"
#include "CharacterProgression/UI/ProgressionTree/NUProgressionGraphWidget.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void UNUProgressionScreenWidget::InitializeScreen(
	int32 InGraphId,
	UNUProgressionComponent* InProgression,
	FSimpleDelegate InCloseRequested,
	FOnNUAdvancementSelectionRequested InAdvancementSelectionRequested)
{
	GraphId = InGraphId;
	Progression = InProgression;
	SetCloseRequested(MoveTemp(InCloseRequested));
	AdvancementSelectionRequested = MoveTemp(InAdvancementSelectionRequested);
}

void UNUProgressionScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 서버 State가 복제되거나 변경되면 포인트와 노드 상태 갱신
	if (Progression.IsValid())
	{
		Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
		Progression->OnProgressionChanged.AddDynamic(this, &ThisClass::HandleProgressionChanged);
		Progression->OnProgressionRequestStateChanged.RemoveDynamic(this, &ThisClass::HandleProgressionRequestStateChanged);
		Progression->OnProgressionRequestStateChanged.AddDynamic(this, &ThisClass::HandleProgressionRequestStateChanged);
		Progression->OnProgressionRequestFailed.RemoveDynamic(this, &ThisClass::HandleProgressionRequestFailed);
		Progression->OnProgressionRequestFailed.AddDynamic(this, &ThisClass::HandleProgressionRequestFailed);
	}

	// 선택한 GraphId와 플레이어 상태를 그래프 위젯에 전달
	if (GraphWidget != nullptr)
	{
		GraphWidget->InitializeGraph(
			GraphId,
			Progression.Get(),
			FOnNUProgressionGraphNodeSelected::CreateUObject(this, &ThisClass::HandleNodeSelected));
		GraphWidget->RefreshGraph();
	}

	ClearPendingNode();
	RefreshState();
}

void UNUProgressionScreenWidget::NativeDestruct()
{
	// 닫힌 화면이 상태 변경을 계속 받지 않도록 구독 해제
	if (Progression.IsValid())
	{
		Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
		Progression->OnProgressionRequestStateChanged.RemoveDynamic(this, &ThisClass::HandleProgressionRequestStateChanged);
		Progression->OnProgressionRequestFailed.RemoveDynamic(this, &ThisClass::HandleProgressionRequestFailed);
	}
	if (ActiveConfirmWidget != nullptr)
	{
		ActiveConfirmWidget->Dismiss();
		ActiveConfirmWidget = nullptr;
	}
	ClearPendingNode();
	AdvancementSelectionRequested.Unbind();

	Super::NativeDestruct();
}

bool UNUProgressionScreenWidget::CanDragWindow() const
{
	return false;
}

void UNUProgressionScreenWidget::HandleProgressionChanged()
{
	// 서버 상태가 바뀌면 포인트와 노드 상태를 같이 갱신
	RefreshState();
}

void UNUProgressionScreenWidget::HandleProgressionRequestStateChanged()
{
	RefreshState();
}

void UNUProgressionScreenWidget::HandleProgressionRequestFailed(int32 ResultCode, const FString& Message)
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->ShowNotice(FText::FromString(Message));
	}
	RefreshState();
}

void UNUProgressionScreenWidget::HandleNodeSelected(const FNUProgressionNodeDefinitionRow& Node, bool bAcquireRequested)
{
	SelectedNode = Node;
	if (bAcquireRequested)
	{
		if (Node.NodeType == ENUProgressionNodeType::Advancement)
		{
			AdvancementSelectionRequested.ExecuteIfBound(Node.NodeId);
		}
		else
		{
			ShowNodeConfirmation(Node);
		}
	}
}

void UNUProgressionScreenWidget::ShowNodeConfirmation(const FNUProgressionNodeDefinitionRow& Node)
{
	if (ActiveConfirmWidget != nullptr || GetOwningPlayer() == nullptr)
	{
		return;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUConfirmDialog> ConfirmClass = Settings != nullptr
		? Settings->ProgressionNodeConfirmWidgetClass.LoadSynchronous()
		: nullptr;
	if (ConfirmClass == nullptr)
	{
		return;
	}

	PendingNode = Node;
	bHasPendingNode = true;
	ActiveConfirmWidget = UNUConfirmDialog::Show(
		GetOwningPlayer(),
		ConfirmClass,
		GetConfirmationMessage(Node),
		NSLOCTEXT("NUProgression", "ConfirmYes", "예"),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleConfirmYes),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleConfirmNo),
		NSLOCTEXT("NUProgression", "ConfirmNo", "아니오"));
	if (ActiveConfirmWidget == nullptr)
	{
		ClearPendingNode();
	}
}

void UNUProgressionScreenWidget::HandleConfirmYes()
{
	const int32 NodeId = bHasPendingNode ? PendingNode.NodeId : 0;
	ActiveConfirmWidget = nullptr;
	ClearPendingNode();
	if (NodeId > 0 && Progression.IsValid())
	{
		if (!Progression->RequestNodeAcquire(NodeId, 0))
		{
			if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
			{
				UI->ShowNotice(NSLOCTEXT("NUProgression", "RequestUnavailable", "현재 성장 노드를 요청할 수 없습니다."));
			}
		}
	}
}

void UNUProgressionScreenWidget::HandleConfirmNo()
{
	ActiveConfirmWidget = nullptr;
	ClearPendingNode();
}

void UNUProgressionScreenWidget::ClearPendingNode()
{
	bHasPendingNode = false;
	PendingNode = FNUProgressionNodeDefinitionRow();
}

FText UNUProgressionScreenWidget::GetConfirmationMessage(const FNUProgressionNodeDefinitionRow& Node) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	const FText NodeName = Node.DisplayName.IsEmpty()
		? FText::AsNumber(Node.NodeId)
		: Node.DisplayName;
	if (Node.NodeType == ENUProgressionNodeType::Stat && Data != nullptr)
	{
		for (const FNUProgressionNodeRewardRow* Reward : Data->GetRewardsByNode(Node.NodeId))
		{
			if (Reward != nullptr && Reward->RewardType == ENUProgressionNodeRewardType::PrimaryStat)
			{
				return FText::Format(
					NSLOCTEXT("NUProgression", "ConfirmStatNode", "{0} +{1} 스탯을 추가하시겠습니까?"),
					GetStatLabel(Reward->RewardId),
					FText::AsNumber(Reward->Value));
			}
		}
	}

	return FText::Format(
		NSLOCTEXT("NUProgression", "ConfirmNodeSelection", "{0}을(를) 선택하시겠습니까?"),
		NodeName);
}

FText UNUProgressionScreenWidget::GetStatLabel(int32 StatId)
{
	switch (static_cast<ENUPrimaryStatId>(StatId))
	{
	case ENUPrimaryStatId::Strength: return FText::FromString(TEXT("STR"));
	case ENUPrimaryStatId::Dexterity: return FText::FromString(TEXT("DEX"));
	case ENUPrimaryStatId::Intelligence: return FText::FromString(TEXT("INT"));
	case ENUPrimaryStatId::Luck: return FText::FromString(TEXT("LUK"));
	case ENUPrimaryStatId::Vitality: return FText::FromString(TEXT("VIT"));
	default: return FText::AsNumber(StatId);
	}
}

void UNUProgressionScreenWidget::RefreshState()
{
	// 서버가 확정한 남은 포인트 표시
	if (AvailablePointsText != nullptr)
	{
		AvailablePointsText->SetText(FText::Format(
			NSLOCTEXT("NUProgression", "AvailablePoints", "보유 포인트 {0}"),
			FText::AsNumber(Progression.IsValid() ? Progression->GetAvailablePoints() : 0)));
	}
	SetIsEnabled(!Progression.IsValid() || !Progression->IsServerProgressionRequestPending());

	// 포인트나 획득 NodeId가 바뀌면 모든 노드 상태 다시 계산
	if (GraphWidget != nullptr)
	{
		GraphWidget->RefreshNodeStates();
	}
}
