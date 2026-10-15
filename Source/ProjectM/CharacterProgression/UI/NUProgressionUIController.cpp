#include "CharacterProgression/UI/NUProgressionUIController.h"

#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUPlayerSkillComponent.h"
#include "CharacterProgression/UI/Job/NUJobSelectionWidget.h"
#include "CharacterProgression/UI/Skill/NUPlayerSkillWindowWidget.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/UI/Job/NUAdvancementSelectionWidget.h"
#include "CharacterProgression/UI/ProgressionTree/NUProgressionScreenWidget.h"
#include "CharacterProgression/UI/Stats/NUCharacterStatsWidget.h"
#include "CharacterProgression/UI/Stats/NUProgressionDebugWidget.h"
#include "CharacterState/NUPlayerState.h"
#include "Inventory/NUEquipmentComponent.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

void UNUProgressionUIController::ToggleCharacterStats()
{
	if (IsCharacterStatsOpen())
	{
		CloseCharacterStats();
		return;
	}

	OpenCharacterStats();
}

bool UNUProgressionUIController::IsCharacterStatsOpen() const
{
	const UNUUIManager* UI = GetManager();
	return UI != nullptr && CharacterStatsWidget != nullptr && UI->IsWindowOpen(CharacterStatsWidget);
}

void UNUProgressionUIController::ToggleProgressionGraph()
{
	if (IsProgressionGraphOpen() || bOpenProgressionGraphAfterProgressionResponse)
	{
		CloseProgressionGraph();
		return;
	}

	OpenProgressionGraph();
}

void UNUProgressionUIController::ToggleProgressionDebug()
{
#if !UE_BUILD_SHIPPING
	if (IsProgressionDebugOpen())
	{
		CloseProgressionDebug();
		return;
	}

	OpenProgressionDebug();
#endif
}

void UNUProgressionUIController::OnPlayerControllerChanged(APlayerController*)
{
	ResetWindows();
}

void UNUProgressionUIController::OnDeinitialize()
{
	ResetWindows();
}

void UNUProgressionUIController::ResetWindows()
{
	CloseJobSelection();
	CloseSkills();
	CloseCharacterStats();
	CloseProgressionGraph();
	CloseProgressionDebug();
	JobSelectionWidget = nullptr;
	SkillWindowWidget = nullptr;
	CharacterStatsWidget = nullptr;
	ProgressionScreenWidget = nullptr;
	AdvancementSelectionWidget = nullptr;
	ProgressionDebugWidget = nullptr;
}

UNUProgressionComponent* UNUProgressionUIController::ResolveProgression() const
{
	const ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayerController());
	const ANUPlayerState* PlayerState = PlayerController != nullptr
		? PlayerController->GetPlayerState<ANUPlayerState>()
		: nullptr;
	return PlayerState != nullptr ? PlayerState->GetProgressionComponent() : nullptr;
}

UNUEquipmentComponent* UNUProgressionUIController::ResolveEquipment() const
{
	const ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayerController());
	const ANUPlayerState* PlayerState = PlayerController != nullptr
		? PlayerController->GetPlayerState<ANUPlayerState>()
		: nullptr;
	return PlayerState != nullptr ? PlayerState->GetEquipmentComponent() : nullptr;
}

void UNUProgressionUIController::OpenCharacterStats()
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayerController());
	UNUUIManager* UI = GetManager();
	UNUProgressionComponent* Progression = ResolveProgression();
	if (PlayerController == nullptr || UI == nullptr || Progression == nullptr
		|| PlayerController->IsGameInputBlocked())
	{
		return;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUCharacterStatsWidget> WidgetClass = Settings != nullptr
		? Settings->CharacterStatsWidgetClass.LoadSynchronous()
		: nullptr;
	if (WidgetClass == nullptr)
	{
		return;
	}

	GetOrCreateWidget(CharacterStatsWidget, WidgetClass);
	if (CharacterStatsWidget == nullptr)
	{
		return;
	}

	CharacterStatsWidget->InitializeStats(
		Progression,
		FSimpleDelegate::CreateUObject(this, &ThisClass::CloseCharacterStats));
	// 장비 변경으로 스탯이 달라지면 열린 창의 수치를 다시 요청한다.
	BindEquipment(ResolveEquipment());
	Progression->RequestCharacterStats();
	if (!Progression->HasServerProgressionSnapshot())
	{
		Progression->RequestProgressionState();
	}

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Window;
	Params.bDimBackground = false;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = false;
	Params.bCloseOnSessionEnd = true;
	UI->OpenWindow(CharacterStatsWidget, Params);
}

void UNUProgressionUIController::CloseCharacterStats()
{
	UnbindEquipment();
	if (UNUUIManager* UI = GetManager(); UI != nullptr && CharacterStatsWidget != nullptr)
	{
		UI->CloseWindow(CharacterStatsWidget);
	}
}

void UNUProgressionUIController::OpenProgressionGraph()
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayerController());
	UNUUIManager* UI = GetManager();
	UNUProgressionComponent* Progression = ResolveProgression();
	ANUPlayerState* PlayerState = PlayerController != nullptr
		? PlayerController->GetPlayerState<ANUPlayerState>()
		: nullptr;
	UNUJobComponent* Job = PlayerState != nullptr ? PlayerState->GetJobComponent() : nullptr;
	if (PlayerController == nullptr || UI == nullptr || Progression == nullptr
		|| PlayerController->IsGameInputBlocked())
	{
		return;
	}

	// 성장 그래프는 직업 선택이 완료된 캐릭터만 사용할 수 있다.
	if (Job == nullptr || !Job->HasJob())
	{
		UI->ShowNotice(NSLOCTEXT(
			"NUProgression",
			"JobSelectionRequired",
			"전직 NPC와 대화해 직업을 먼저 선택하세요"));
		return;
	}

	// 노드 상태는 18번 최종 전투 스탯이 아니라 170번 성장 상태를 기준으로 계산한다.
	if (!Progression->HasServerProgressionSnapshot())
	{
		bOpenProgressionGraphAfterProgressionResponse = true;
		BindPendingGraphRequest(Progression);
		Progression->RequestProgressionState();
		if (Progression->IsServerProgressionRequestPending())
		{
			return;
		}
		bOpenProgressionGraphAfterProgressionResponse = false;
		UnbindPendingGraphRequest();
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUProgressionScreenWidget> WidgetClass = Settings != nullptr
		? Settings->ProgressionScreenWidgetClass.LoadSynchronous()
		: nullptr;
	if (WidgetClass == nullptr)
	{
		return;
	}

	GetOrCreateWidget(ProgressionScreenWidget, WidgetClass);
	const int32 GraphId = Progression->GetProgressionGraphId();
	if (ProgressionScreenWidget == nullptr || GraphId <= 0)
	{
		return;
	}

	ProgressionScreenWidget->InitializeScreen(
		GraphId,
		Progression,
		FSimpleDelegate::CreateUObject(this, &ThisClass::CloseProgressionGraph),
		FOnNUAdvancementSelectionRequested::CreateUObject(this, &ThisClass::OpenAdvancementSelection));

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Window;
	Params.bDimBackground = true;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = true;
	Params.bCloseOnSessionEnd = true;
	UI->OpenWindow(ProgressionScreenWidget, Params);
}

void UNUProgressionUIController::CloseProgressionGraph()
{
	bOpenProgressionGraphAfterProgressionResponse = false;
	UnbindPendingGraphRequest();
	CloseAdvancementSelection();
	if (UNUUIManager* UI = GetManager(); UI != nullptr && ProgressionScreenWidget != nullptr)
	{
		UI->CloseWindow(ProgressionScreenWidget);
	}
}

bool UNUProgressionUIController::IsProgressionGraphOpen() const
{
	const UNUUIManager* UI = GetManager();
	return UI != nullptr && ProgressionScreenWidget != nullptr && UI->IsWindowOpen(ProgressionScreenWidget);
}

void UNUProgressionUIController::OpenAdvancementSelection(int32 NodeId)
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayerController());
	UNUUIManager* UI = GetManager();
	UNUProgressionComponent* Progression = ResolveProgression();
	ANUPlayerState* PlayerState = PlayerController != nullptr
		? PlayerController->GetPlayerState<ANUPlayerState>()
		: nullptr;
	UNUJobComponent* Job = PlayerState != nullptr ? PlayerState->GetJobComponent() : nullptr;
	// 이미 전직했거나 선택할 수 없는 노드에서는 전직 선택 창을 열지 않는다.
	if (PlayerController == nullptr
		|| UI == nullptr
		|| Progression == nullptr
		|| Job == nullptr
		|| Job->GetAdvancementId() > 0
		|| Progression->GetNodeState(NodeId) != ENUProgressionNodeState::Available)
	{
		return;
	}
	if (AdvancementSelectionWidget != nullptr && UI->IsWindowOpen(AdvancementSelectionWidget))
	{
		return;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUAdvancementSelectionWidget> WidgetClass = Settings != nullptr
		? Settings->AdvancementSelectionWidgetClass.LoadSynchronous()
		: nullptr;
	if (WidgetClass == nullptr)
	{
		return;
	}

	GetOrCreateWidget(AdvancementSelectionWidget, WidgetClass);
	if (AdvancementSelectionWidget == nullptr)
	{
		return;
	}
	AdvancementSelectionWidget->InitializeSelection(
		NodeId,
		Job,
		Progression,
		FSimpleDelegate::CreateUObject(this, &ThisClass::CloseAdvancementSelection));

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Window;
	Params.bDimBackground = true;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = true;
	Params.bCloseOnSessionEnd = true;
	UI->OpenWindow(AdvancementSelectionWidget, Params);
}

void UNUProgressionUIController::CloseAdvancementSelection()
{
	if (UNUUIManager* UI = GetManager(); UI != nullptr && AdvancementSelectionWidget != nullptr)
	{
		UI->CloseWindow(AdvancementSelectionWidget);
	}
}

void UNUProgressionUIController::OpenProgressionDebug()
{
#if !UE_BUILD_SHIPPING
	UNUUIManager* UI = GetManager();
	UNUProgressionComponent* Progression = ResolveProgression();
	if (UI == nullptr || Progression == nullptr)
	{
		return;
	}

	GetOrCreateWidget<UNUProgressionDebugWidget>(
		ProgressionDebugWidget,
		UNUProgressionDebugWidget::StaticClass());
	if (ProgressionDebugWidget == nullptr)
	{
		return;
	}

	ProgressionDebugWidget->InitializeProgression(Progression);

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Tracker;
	Params.bDimBackground = false;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = false;
	Params.bBlockGameInput = false;
	Params.bCloseOnSessionEnd = false;
	UI->OpenWindow(
		ProgressionDebugWidget,
		Params,
		FSimpleDelegate::CreateUObject(this, &ThisClass::CloseProgressionDebug));
#endif
}

void UNUProgressionUIController::CloseProgressionDebug()
{
	if (UNUUIManager* UI = GetManager(); UI != nullptr && ProgressionDebugWidget != nullptr)
	{
		UI->CloseWindow(ProgressionDebugWidget);
	}
}

bool UNUProgressionUIController::IsProgressionDebugOpen() const
{
	const UNUUIManager* UI = GetManager();
	return UI != nullptr && ProgressionDebugWidget != nullptr && UI->IsWindowOpen(ProgressionDebugWidget);
}

void UNUProgressionUIController::BindEquipment(UNUEquipmentComponent* Equipment)
{
	UnbindEquipment();
	BoundEquipment = Equipment;
	if (Equipment != nullptr)
	{
		Equipment->OnEquipmentChanged.AddDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
}

void UNUProgressionUIController::UnbindEquipment()
{
	if (BoundEquipment.IsValid())
	{
		BoundEquipment->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	BoundEquipment.Reset();
}

void UNUProgressionUIController::BindPendingGraphRequest(UNUProgressionComponent* Progression)
{
	UnbindPendingGraphRequest();
	PendingGraphProgression = Progression;
	if (Progression != nullptr)
	{
		Progression->OnProgressionRequestStateChanged.AddDynamic(this, &ThisClass::HandleProgressionRequestStateChanged);
	}
}

void UNUProgressionUIController::UnbindPendingGraphRequest()
{
	if (PendingGraphProgression.IsValid())
	{
		PendingGraphProgression->OnProgressionRequestStateChanged.RemoveDynamic(this, &ThisClass::HandleProgressionRequestStateChanged);
	}
	PendingGraphProgression.Reset();
}

void UNUProgressionUIController::HandleEquipmentChanged()
{
	// 닫힌 스탯 창을 위해 불필요한 스탯 요청을 보내지 않는다.
	if (IsCharacterStatsOpen())
	{
		if (UNUProgressionComponent* Progression = ResolveProgression())
		{
			Progression->RequestCharacterStats();
		}
	}
}

void UNUProgressionUIController::HandleProgressionRequestStateChanged()
{
	UNUProgressionComponent* Progression = PendingGraphProgression.Get();
	if (Progression == nullptr || Progression->IsServerProgressionRequestPending())
	{
		return;
	}

	// 응답 대기가 끝났고 유효한 스탯을 받았을 때만 예약된 그래프를 연다.
	const bool bShouldOpen = bOpenProgressionGraphAfterProgressionResponse
		&& Progression->HasServerProgressionSnapshot();
	bOpenProgressionGraphAfterProgressionResponse = false;
	UnbindPendingGraphRequest();
	if (bShouldOpen)
	{
		OpenProgressionGraph();
	}
}

void UNUProgressionUIController::OpenJobSelection(FSimpleDelegate InSelectionCompleted)
{
	// 컨트롤러는 레벨 전환보다 오래 살아 현재 PlayerController와 PlayerState를 매번 다시 찾는다.
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayerController());
	UNUUIManager* UI = GetManager();
	ANUPlayerState* PlayerState = PlayerController != nullptr
		? PlayerController->GetPlayerState<ANUPlayerState>()
		: nullptr;
	UNUJobComponent* JobComponent = PlayerState != nullptr ? PlayerState->GetJobComponent() : nullptr;
	UNUProgressionComponent* Progression = PlayerState != nullptr ? PlayerState->GetProgressionComponent() : nullptr;
	if (PlayerController == nullptr || UI == nullptr || JobComponent == nullptr || Progression == nullptr)
	{
		return;
	}
	if (JobSelectionWidget != nullptr && UI->IsWindowOpen(JobSelectionWidget))
	{
		return;
	}
	if (JobComponent->HasJob())
	{
		UI->ShowNotice(NSLOCTEXT("NUJob", "AlreadySelected", "이미 직업을 선택했습니다"));
		return;
	}
	if (PlayerController->IsGameInputBlocked())
	{
		return;
	}

	// 위젯 클래스는 공용 UI 설정에서 읽고 같은 월드에서는 생성한 인스턴스를 재사용
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUJobSelectionWidget> WidgetClass = Settings != nullptr
		? Settings->JobSelectionWidgetClass.LoadSynchronous()
		: nullptr;
	if (WidgetClass == nullptr)
	{
		return;
	}

	GetOrCreateWidget(JobSelectionWidget, WidgetClass);
	if (JobSelectionWidget == nullptr)
	{
		return;
	}

	JobSelectionWidget->InitializeSelection(
		JobComponent,
		Progression,
		FSimpleDelegate::CreateUObject(this, &ThisClass::CloseJobSelection),
		MoveTemp(InSelectionCompleted));

	// ESC, 커서, 입력 차단은 개별 위젯이 아니라 UIManager가 일괄 처리
	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Window;
	Params.bDimBackground = true;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = true;
	Params.bCloseOnSessionEnd = true;
	Params.bHideFieldHUD = JobSelectionWidget->ShouldHideFieldHUD();
	UI->OpenWindow(JobSelectionWidget, Params);
}

void UNUProgressionUIController::CloseJobSelection()
{
	if (UNUUIManager* UI = GetManager(); UI != nullptr && JobSelectionWidget != nullptr)
	{
		UI->CloseWindow(JobSelectionWidget);
	}
}


void UNUProgressionUIController::ToggleSkills()
{
	if (IsSkillsOpen())
	{
		CloseSkills();
		return;
	}

	OpenSkills();
}

void UNUProgressionUIController::OpenSkills()
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwningPlayerController());
	UNUUIManager* UI = GetManager();
	ANUPlayerState* PlayerState = PlayerController != nullptr
		? PlayerController->GetPlayerState<ANUPlayerState>()
		: nullptr;
	UNUPlayerSkillComponent* SkillComponent = PlayerState != nullptr
		? PlayerState->GetPlayerSkillComponent()
		: nullptr;
	if (PlayerController == nullptr || UI == nullptr || SkillComponent == nullptr
		|| PlayerController->IsGameInputBlocked())
	{
		return;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUPlayerSkillWindowWidget> WidgetClass = Settings != nullptr
		? Settings->PlayerSkillWindowClass.LoadSynchronous()
		: nullptr;
	if (WidgetClass == nullptr)
	{
		return;
	}

	GetOrCreateWidget(SkillWindowWidget, WidgetClass);
	if (SkillWindowWidget == nullptr)
	{
		return;
	}

	SkillWindowWidget->InitializeSkillWindow(
		SkillComponent,
		FSimpleDelegate::CreateUObject(this, &ThisClass::CloseSkills));

	// 커서와 ESC 처리는 UIManager가 맡는다.
	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Window;
	Params.bDimBackground = false;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = false;
	Params.bCloseOnSessionEnd = true;
	UI->OpenWindow(SkillWindowWidget, Params);
}

void UNUProgressionUIController::CloseSkills()
{
	if (UNUUIManager* UI = GetManager(); UI != nullptr && SkillWindowWidget != nullptr)
	{
		UI->CloseWindow(SkillWindowWidget);
	}
}

bool UNUProgressionUIController::IsSkillsOpen() const
{
	const UNUUIManager* UI = GetManager();
	return UI != nullptr && SkillWindowWidget != nullptr && UI->IsWindowOpen(SkillWindowWidget);
}
