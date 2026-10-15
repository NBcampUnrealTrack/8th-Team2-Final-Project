#include "NUPlayerController.h"

#include "UI/Common/NU_CommonButtonWidget.h"

#include "Audio/NUBGMSubsystem.h"
#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Field/Presence/NUActorPresenceTransitionSubsystem.h"
#include "Battle/Avatar/NUBattleParryFX.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Encounter/NUBattleEncounterSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Component/NUBattleCommandComponent.h"
#include "Component/NUBattleCommandComponentBase.h"
#include "Component/NUBattleNetCommandComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/UI/NUProgressionUIController.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Battle/UI/NUActionBannerWidget.h"
#include "Battle/UI/NUCombatPopupLayer.h"
#include "Battle/Net/NUBattleNetActionQueueSubsystem.h"
#include "Battle/UI/NUParryCueWidget.h"
#include "Component/NUParryComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Field/FieldPlayerCharacter.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Interaction/Component/NUInteractionRouterComponent.h"
#include "Interaction/Component/NUInteractorComponent.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Recovery/Component/NURecoveryProviderComponent.h"
#include "Field/NPC/NUNPCBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Inventory/NUInventoryComponent.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/UI/NUStarterNaiaSelectionUIController.h"
#include "Chat/NUChatSubsystem.h"
#include "Chat/UI/NUChatWidget.h"
#include "Party/NUPartySubsystem.h"
#include "Trade/NUTradeSubsystem.h"
#include "Party/UI/NUPartyListWidget.h"
#include "Party/UI/NUPartyPromptWidget.h"
#include "WorldTransition/Net/NULocationRestoreSubsystem.h"
#include "WorldTransition/World/NUFieldLoadingSubsystem.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Dialogue/Data/NUQuestDialogueDefinition.h"
#include "Dialogue/Data/NUDialogueDataSubsystem.h"
#include "Dialogue/UI/NUDialogueUIController.h"
#include "UI/HUD/NU_BattleHUDWidget.h"
#include "Battle/UI/NUBattleResultWidget.h"
#include "UI/HUD/NU_BattleSwitcherWidget.h"
#include "UI/HUD/NU_FieldHUDWidget.h"
#include "UI/Base/NU_BaseFieldWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/CharacterMenu/NUCharacterMenuBase.h"
#include "UI/Controller/NUSpeechBubbleUIController.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUPlayerContextMenu.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UI/NUUIManager.h"
#include "UI/NUOptionsWindowWidget.h"
#include "UI/Map/NU_WorldMapWindowWidget.h"
#include "UI/NUUISettings.h"
#include "UI/Marker/NUWorldMarkerComponent.h"
#include "WorldLocation/UI/NUWorldLocationUIController.h"
#include "Util/NUCollisionChannel.h"
#include "Quest/Component/NUQuestProviderComponent.h"
#include "Quest/Component/NUQuestMarkerComponent.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Component/NUQuestSceneComponent.h"
#include "Quest/Data/NUQuestDataSettings.h"
#include "Quest/Data/NUQuestTypes.h"
#include "Quest/UI/NUQuestJournalWidget.h"
#include "Quest/UI/NUQuestWidget.h"
#include "Recovery/Component/NURecoveryComponent.h"
#include "Recovery/Data/NURecoverySettings.h"
#include "Net/EmberServerManager.h"
#include "UObject/UObjectIterator.h"



// 마지막 입력 장치 판정 --- 패드 키·스틱이면 패드, 키보드 키·마우스 버튼·휠이면 키보드·마우스.
// 마우스 이동은 짧은 시간 안에 이어질 때만 센다: 창이 열리고 닫힐 때 엔진이 커서를 되돌리는 한 번짜리 점프도 이동 이벤트로 오기 때문
class FNUInputDeviceWatcher : public IInputProcessor
{
public:
	explicit FNUInputDeviceWatcher(ANUPlayerController* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		Report(InKeyEvent.GetKey().IsGamepadKey());
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InEvent) override
	{
		if (InEvent.GetKey().IsGamepadKey() && FMath::Abs(InEvent.GetAnalogValue()) > 0.25f)
		{
			Report(true);
		}
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		Report(false);
		return false;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override
	{
		Report(false);
		return false;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetCursorDelta().IsNearlyZero())
		{
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		MouseMoveCount = (Now - LastMouseMoveTime < 0.25) ? MouseMoveCount + 1 : 1;
		LastMouseMoveTime = Now;
		if (MouseMoveCount >= 3)
		{
			Report(false);
		}
		return false;
	}

private:
	void Report(bool bGamepad)
	{
		if (ANUPlayerController* PC = Owner.Get())
		{
			PC->SetGamepadMode(bGamepad);
		}
	}

	TWeakObjectPtr<ANUPlayerController> Owner;
	double LastMouseMoveTime = 0.0;
	int32 MouseMoveCount = 0;
};

namespace  
{
	const FName CursorReason_Battle(TEXT("Battle"));
	const FName CursorReason_PlayerSelect(TEXT("PlayerSelect"));
	const FName CursorReason_Chat(TEXT("Chat"));
	const FName CursorReason_PartyPrompt(TEXT("PartyPrompt"));
	const FName CursorReason_BattleResult(TEXT("BattleResult"));

}

ANUPlayerController::ANUPlayerController()
{
	CreateDefaultSubobject<UNUQuestSceneComponent>(TEXT("QuestSceneComponent"));
	BattleCommandComponent = CreateDefaultSubobject<UNUBattleCommandComponent>(TEXT("BattleCommandComponent"));
	NetBattleCommandComponent = CreateDefaultSubobject<UNUBattleNetCommandComponent>(TEXT("NetBattleCommandComponent"));
	ParryComponent = CreateDefaultSubobject<UNUParryComponent>(TEXT("ParryComponent"));
}

void ANUPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		UI->GetController<UNUWorldLocationUIController>();
		// 근접 채팅 말풍선 --- 채팅 알림을 구독만 하는 상주 컨트롤러라 여기서 한 번 만들어 두면 된다
		UI->GetController<UNUSpeechBubbleUIController>();
		// 패드 모드에서 창이 열려 있는 동안 이동을 막는다 --- 창 목록이 바뀔 때마다 다시 판단
		UI->OnWindowsChanged.AddUObject(this, &ThisClass::RefreshGamepadMoveBlock);
	}

	// 패드 <=> 키보드·마우스 전환 감지
	if (FSlateApplication::IsInitialized() && !InputDeviceWatcher.IsValid())
	{
		InputDeviceWatcher = MakeShared<FNUInputDeviceWatcher>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputDeviceWatcher);
	}

	// 강제로 입력모드에 남은 것을 초기화 함
	// 이전 레벨에서 UIOnly가 남으면 필드에서 조작이 안됨
	ApplyCursorState(true);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UNUFieldLoadingSubsystem* Loading = GameInstance->GetSubsystem<UNUFieldLoadingSubsystem>())
		{
			Loading->HandleFieldControllerReady(this);
		}
	}
	
	CreateQuestWidget();
	CreateQuestJournalWidget();

	CreateInteractionIndicatorWidget();
	BindInteractionComponent(GetPawn());
	
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (IsValid(InventoryMappingContext))
			{
				InputSubsystem->AddMappingContext(InventoryMappingContext, 1);
			}

			if (IsValid(ProgressionMappingContext))
			{
				InputSubsystem->AddMappingContext(ProgressionMappingContext, 2);
			}

			if (IsValid(ChatMappingContext))
			{
				InputSubsystem->AddMappingContext(ChatMappingContext, 1);
			}
		}
	}
	
	if (IsValid(BattleCommandComponent))
	{
		BattleCommandComponent->SetTargetIndicatorClass(TargetIndicatorClass);
		BattleCommandComponent->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleBattleCommandSelectionChanged);
		BattleCommandComponent->OnBattleCommandSessionChanged.AddUObject(this, &ThisClass::HandleBattleCommandSessionChanged);
	}
	if (IsValid(NetBattleCommandComponent))
	{
		NetBattleCommandComponent->SetTargetIndicatorClass(TargetIndicatorClass);
		NetBattleCommandComponent->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleBattleCommandSelectionChanged);
		NetBattleCommandComponent->OnBattleCommandSessionChanged.AddUObject(this, &ThisClass::HandleBattleCommandSessionChanged);

		// 컴포넌트 BeginPlay에서 전투 세트 준비가 완료되면 처리함 
		if (NetBattleCommandComponent->IsSessionActive())
		{
			HandleBattleCommandSessionChanged(NetBattleCommandComponent, true);
		}
	}

	BindBattleEncounterDelegate();
	HandleBattleCommandSelectionChanged();

	// 로그인이 폰보다 늦게 끝나는 경우를 위한 델리게이트
	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->OnStateChanged.AddDynamic(this, &ThisClass::HandleServerConnStateChanged);
	}

	// 이미 로그인된 상태로 필드에 들어온 경우, 로그인 레벨 거쳐 진입
	TryEnterField();
}

void ANUPlayerController::SetPawn(APawn* InPawn)
{
	if (GetPawn() != InPawn) { CancelStarterNaiaGathering(); }
	UnbindInteractionComponent();
	Super::SetPawn(InPawn);

	if (IsLocalController())
	{
		BindInteractionComponent(InPawn);
	}
}

void ANUPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	// 로컬 폰이 확정된 시점
	// AFieldPlayerCharacter::BeginPlay는 Possess 전이라 GetPawn()이 아직 null이므로
	// 필드 진입은 반드시 여기(또는 그 이후)에서 시도해야 함
	TryEnterField();
}

void ANUPlayerController::TryEnterField()
{
	if (!IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// FieldSyncSubsystem은 WorldSubsystem이라 레벨마다 새로 생성
	// 필드가 아닌 레벨에선 아직 필드에 들어가지 않은 상태로 시작함
	UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>();
	if (!Sync || Sync->IsInField())
	{
		return;
	}

	UEmberServerManager* Server = GetServerManager();
	if (!Server || !Server->IsLoggedIn())
	{
		return;
	}

	AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(GetPawn());

	if (!IsValid(FieldCharacter))
	{
		UE_LOG(LogTemp, Warning, TEXT("내 캐릭터가 아직 존재하지 않습니다!"));
		return;
	}

	// 서버의 마지막 확정 위치를 먼저 적용해야 FieldEnter에 복원된 좌표와 장소가 실린다.
	UGameInstance* GameInstance = World->GetGameInstance();
	UNULocationRestoreSubsystem* LocationRestore = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNULocationRestoreSubsystem>() : nullptr;

	if (LocationRestore != nullptr
		&& !LocationRestore->PrepareFieldEntry(FieldCharacter,
			FSimpleDelegate::CreateUObject(this, &ThisClass::TryEnterField)))
	{
		return;
	}

	Sync->EnterField();

	// 로그인 레벨에는 채팅이 없어야 하기 때문에 채팅창은 필드에 들어온 뒤에 만들어야 함
	CreateChatWidget();

	// 파티 초대도 필드에 들어온 뒤에만 올 수 있음
	CreatePartyPromptWidget();
	CreatePartyListWidget();
}

void ANUPlayerController::HandleServerConnStateChanged(EEmberConnState NewState)
{
	if (NewState == EEmberConnState::LoggedIn)
	{
		TryEnterField();
	}
}

UEmberServerManager* ANUPlayerController::GetServerManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}

void ANUPlayerController::UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents)
{
	Super::UpdateHiddenComponents(ViewLocation, HiddenComponents);

	const UNUUIManager* UI = UNUUIManager::Get(this);
	const UNUQuestSceneComponent* Scene = FindComponentByClass<UNUQuestSceneComponent>();
	if ((UI == nullptr || !UI->IsAnyWindowOpen(NUUILayer::Dialogue))
		&& (Scene == nullptr || !Scene->IsScenePlaying()))
	{
		return;
	}

	// 대화·연출 중에는 이 플레이어의 화면에서만 숨긴다. 퀘스트 태그와 지도 표시는 유지한다.
	for (TObjectIterator<UNUQuestMarkerComponent> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->IsRegistered())
		{
			HiddenComponents.Add(It->GetPrimitiveSceneId());
		}
	}
}

void ANUPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (IsLocalController())
	{
		UpdateInteractionIndicatorPosition();
		UpdateRecoveryCover();
	}
}

bool ANUPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	UNUUIManager* UI = UNUUIManager::Get(this);
	UNUDialogueUIController* DialogueUI = UI != nullptr ? UI->GetController<UNUDialogueUIController>() : nullptr;
	if (DialogueUI != nullptr && DialogueUI->IsDialogueOpen())
	{
		if (Params.Event == IE_Pressed)
		{
			DialogueUI->HandleInputKey(Params.Key);
		}
		return true;
	}

	return Super::InputKey(Params);
}

void ANUPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(EnhancedInput))
	{
		return;
	}

	if (IsValid(ToggleCharacterStatsAction))
	{
		EnhancedInput->BindAction(
			ToggleCharacterStatsAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::ToggleCharacterStatsUI);
	}
	if (IsValid(ToggleProgressionGraphAction))
	{
		EnhancedInput->BindAction(
			ToggleProgressionGraphAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::ToggleProgressionGraphUI);
	}
	if (IsValid(ToggleOwnedSkillsAction))
	{
		EnhancedInput->BindAction(
			ToggleOwnedSkillsAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::ToggleOwnedSkillsUI);
	}
	if (IsValid(NavigateAction))
	{
		EnhancedInput->BindAction(NavigateAction, ETriggerEvent::Started, this, &ThisClass::HandleBattleNavigate);
	}
	if (IsValid(ConfirmAction))
	{
		EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Started, this, &ThisClass::HandleBattleConfirm);
	}
	if (IsValid(CancelAction))
	{
		EnhancedInput->BindAction(CancelAction, ETriggerEvent::Started, this, &ThisClass::HandleBattleCancel);
	}
	if (IsValid(ToggleInventoryAction))
	{
		EnhancedInput->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &ThisClass::HandleInventoryKey);
	}
	if (IsValid(ToggleCharacterInfoAction))
	{
		EnhancedInput->BindAction(ToggleCharacterInfoAction, ETriggerEvent::Started, this, &ThisClass::HandleToggleCharacterInfo);
	}
	if (IsValid(ToggleQuestJournalAction))
	{
		EnhancedInput->BindAction(ToggleQuestJournalAction, ETriggerEvent::Started, this, &ThisClass::HandleToggleQuestJournal);
	}
	if (IsValid(ToggleWorldMapAction))
	{
		EnhancedInput->BindAction(ToggleWorldMapAction, ETriggerEvent::Started, this, &ThisClass::HandleToggleWorldMap);
	}
	if (IsValid(ToggleNaiaRosterAction))
	{
		EnhancedInput->BindAction(ToggleNaiaRosterAction, ETriggerEvent::Started, this, &ThisClass::HandleToggleNaiaRoster);
	}
	if (IsValid(ParryAction))
	{
		EnhancedInput->BindAction(ParryAction, ETriggerEvent::Started, this, &ThisClass::HandleBattleParry);
	}
	if (IsValid(ShowMouseAction))
	{
		EnhancedInput->BindAction(ShowMouseAction, ETriggerEvent::Started, this, &ThisClass::StartShowMouse);
		EnhancedInput->BindAction(ShowMouseAction, ETriggerEvent::Completed, this, &ThisClass::CompleteShowMouse);
		EnhancedInput->BindAction(ShowMouseAction, ETriggerEvent::Canceled, this, &ThisClass::CompleteShowMouse);
	}
	if (IsValid(MouseClickAction))
	{
		EnhancedInput->BindAction(MouseClickAction, ETriggerEvent::Started, this, &ThisClass::HandleMouseClick);
	}
	if (IsValid(ContextMenuAction))
	{
		EnhancedInput->BindAction(ContextMenuAction, ETriggerEvent::Started, this, &ThisClass::HandleContextMenuClick);
	}
	if (IsValid(UIEscapeAction))
	{
		EnhancedInput->BindAction(UIEscapeAction, ETriggerEvent::Started, this, &ThisClass::HandleUIEscape);
	}
	if (IsValid(UIBackAction))
	{
		EnhancedInput->BindAction(UIBackAction, ETriggerEvent::Started, this, &ThisClass::HandleUIBack);
	}
	if (IsValid(ChatOpenAction))
	{
		EnhancedInput->BindAction(ChatOpenAction, ETriggerEvent::Started, this, &ThisClass::HandleChatOpen);
	}
}

void ANUPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();
	BindBattleEncounterDelegate();
	RefreshQuestWidget();
	RefreshQuestJournalWidget();

	// PlayerState 도착 순서가 경로마다 다르므로 여기와 OnRep 양쪽에서 시도한다.
	// 이미 만들어져 있으면 즉시 반환하므로 중복 생성은 없다.
	EnsureInGameHUDWidgets();

	if (IsLocalController())
	{
		RegisterGameMenuEntries();
	}
}

void ANUPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (bWaitingForStarterNaiaGathering && StarterNaiaGatheringPlayerState.Get() != GetPlayerState<ANUPlayerState>())
	{
		CancelStarterNaiaGathering();
	}
	if (IsLocalController())
	{
		if (UNUUIManager* UI = UNUUIManager::Get(this))
		{
			UI->GetController<UNUWorldLocationUIController>()->RefreshPlayerStateBinding();
		}
	}
	BindBattleEncounterDelegate();
	RefreshQuestWidget();
	RefreshQuestJournalWidget();
	EnsureInGameHUDWidgets();
}

void ANUPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelStarterNaiaGathering();
	// 컨트롤러 종료 전 출입 시퀀스 정리.
	if (IsValid(WorldTransitionPlayer))
	{
		ClearWorldTransitionFade();
	}
	GetWorldTimerManager().ClearTimer(RecoveryRevealTimer);
	if (UNUUIManager* UI = UNUUIManager::Get(this)) { UI->CloseWindow(RecoveryCover); }
	RecoveryCover = nullptr;
	UnbindInteractionComponent();
	PendingDialogueSource = nullptr;
	PendingInteractionSource = nullptr;

	if (IsValid(BattleCommandComponent))
	{
		BattleCommandComponent->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleBattleCommandSelectionChanged);
		BattleCommandComponent->OnBattleCommandSessionChanged.RemoveAll(this);
	}
	if (IsValid(NetBattleCommandComponent))
	{
		NetBattleCommandComponent->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleBattleCommandSelectionChanged);
		NetBattleCommandComponent->OnBattleCommandSessionChanged.RemoveAll(this);
	}
	if (IsValid(BoundASC))
	{
		BoundASC->OnBattleEncounterChanged.RemoveAll(this);
		BoundASC = nullptr;
	}
	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleServerConnStateChanged);
	}

	// 메뉴 항목은 Owner weak로도 걸러지지만, 레벨 전환 잔재가 남지 않게 명시적으로 지운다
	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		UI->UnregisterGameMenuEntries(this);
		UI->OnWindowsChanged.RemoveAll(this);
	}
	if (InputDeviceWatcher.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputDeviceWatcher);
		}
		InputDeviceWatcher.Reset();
	}

	ClearInputMappingContexts();
	DestroyInteractionIndicatorWidget();
	DestroyQuestJournalWidget();
	DestroyQuestWidget();
	DestroyBattleCommandWidget();
	CloseParryWindow();
	CloseDodgeWindow();
	if (IsValid(ParryComponent))
	{
		ParryComponent->SetCueWidget(nullptr);
	}
	if (IsValid(ParryCueWidget))
	{
		ParryCueWidget->RemoveFromParent();
		ParryCueWidget = nullptr;
	}
	CloseBattleResult();
	DestroyNetBattleHUD();
	DestroyBattleHUDWidget();
	DestroyBattleSwitcherWidget();
	DestroyFieldHUDWidget();
	DestroyPlayerContextMenuWidget();
	DestroyChatWidget();
	DestroyPartyPromptWidget();
	DestroyPartyListWidget();
	
	Super::EndPlay(EndPlayReason);
}

UNUBattleCommandComponentBase* ANUPlayerController::GetBattleCommandComponent() const
{
	return IsValid(ActiveCommandComponent) ? ActiveCommandComponent.Get() : BattleCommandComponent.Get();
}

UNUParryComponent* ANUPlayerController::GetParryComponent() const
{
	return ParryComponent;
}

ENUBattleRootAction ANUPlayerController::GetSelectedRootAction() const
{
	const UNUBattleCommandComponentBase* CommandComponent = GetBattleCommandComponent();
	return IsValid(CommandComponent)
		? CommandComponent->GetSelectedRootAction()
		: ENUBattleRootAction::Attack;
}

UUserWidget* ANUPlayerController::GetBattleCommandWidget() const
{
	return BattleCommandWidget;
}

void ANUPlayerController::HandleBattleCommandSessionChanged(UNUBattleCommandComponentBase* Component, bool bActive)
{
	if (!IsLocalController() || !IsValid(Component))
	{
		return;
	}

	if (bActive)
	{
		ActiveCommandComponent = Component;

		// 로컬 전투, 넷 전투 모두 같은 필드 상태를 공유함
		// 넷 전투는 전투 세트가 준비된 뒤에만 이 통지가 오므로 이 시점에 처리해도 됨
		if (UWorld* World = GetWorld())
		{
			if (UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>())
			{
				Sync->SetActivity(EFieldActivity::Battle);
			}
		}
		if (IsValid(PartyListWidget))
		{
			PartyListWidget->SetSuppressed(true);
		}

		CloseDialogue(false);
		CloseQuestJournal();
		if (IsValid(QuestWidget))
		{
			QuestWidget->CloseQuestOffer();
		}
		if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
		{
			PartySubsystem->DeclinePendingPrompt();
		}
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UNUTradeSubsystem* TradeSubsystem = GI->GetSubsystem<UNUTradeSubsystem>())
			{
				TradeSubsystem->DeclinePendingInvite();
			}
		}

		SetBattleInputActive(true);
		CreateBattleCommandWidget();
		HandleBattleCommandSelectionChanged();
		return;
	}

	// 비활성 컴포넌트의 늦은 통지가 현재 진행 중인 다른 전투 UI를 닫으면 안 됨
	if (ActiveCommandComponent != Component)
	{
		return;
	}

	ActiveCommandComponent = nullptr;
	DestroyBattleCommandWidget();
	SetBattleInputActive(false);
	if (UWorld* World = GetWorld())
	{
		if (UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>())
		{
			Sync->SetActivity(EFieldActivity::Idle);
		}
	}
	if (IsValid(PartyListWidget))
	{
		PartyListWidget->SetSuppressed(false);
	}
}

void ANUPlayerController::BindBattleEncounterDelegate()
{
	if (!IsLocalController())
	{
		return;
	}

	ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>();
	UNUAbilitySystemComponent* NewASC = NUPlayerState ? NUPlayerState->GetNUASC() : nullptr;

	if (BoundASC == NewASC)
	{
		HandleBattleEncounterChanged(
			IsValid(BoundASC)
				? BoundASC->GetBattleEncounter()
				: nullptr
				);
		return;
	}

	if (IsValid(BoundASC))
	{
		BoundASC->OnBattleEncounterChanged.RemoveAll(this);
	}

	BoundASC = NewASC;

	if (!IsValid(BoundASC))
	{
		HandleBattleEncounterChanged(nullptr);
		return;
	}

	BoundASC->OnBattleEncounterChanged.AddUObject(this, &ThisClass::HandleBattleEncounterChanged);

	HandleBattleEncounterChanged(BoundASC->GetBattleEncounter());
}

void ANUPlayerController::HandleBattleEncounterChanged(ANUBattleEncounter* NewEncounter)
{
	if (!IsLocalController())
	{
		return;
	}

	// 넷 전투 중에는 로컬 ASC의 nullptr 통지가 정상
	// 이를 로컬 전투 종료로 처리하면 넷 전투의 카메라/HUD를 잘못 정리함
	if (!IsValid(NewEncounter))
	{
		if (const UGameInstance* GI = GetGameInstance())
		{
			const UNUBattleNetSubsystem* BattleNet = GI->GetSubsystem<UNUBattleNetSubsystem>();
			if (IsValid(BattleNet) && BattleNet->IsInBattle())
			{
				return;
			}
		}
	}
	
	// 다른 사람 화면에서 전투중으로 멈춰보이게 gRPC 서버에 알림
	if (UWorld* World = GetWorld())
	{
		if (UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>())
		{
			// Encounter가 유효할 경우에만 전투 진입이므로 Battle
			// Encounter가 nullptr이면 전투 종료이므로 Idle
			Sync->SetActivity(IsValid(NewEncounter) ? EFieldActivity::Battle : EFieldActivity::Idle);
		}
	}

	// 로컬 전투 BGM 층 - 넷 전투 중 로컬 ASC의 가짜 nullptr 통지는 위에서 이미 걸러진 뒤라 안전함
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNUBGMSubsystem* BGM = GI->GetSubsystem<UNUBGMSubsystem>())
		{
			BGM->SetLocalBattleActive(IsValid(NewEncounter));
		}
	}

	// 전투 중엔 파티 목록을 가림
	if (IsValid(PartyListWidget))
	{
		PartyListWidget->SetSuppressed(IsValid(NewEncounter));
	}
	
	if (IsValid(NewEncounter))
	{
		// 전투 진입과 대화 모달이 겹치지 않도록 로컬 대화를 취소한다.
		CloseDialogue(false);
		CloseQuestJournal();

		if (IsValid(QuestWidget))
		{
			QuestWidget->CloseQuestOffer();
		}
		
		// 팝업이 떠 있는 채로 전투가 시작되면 초대를 거절 처리
		if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
		{
			PartySubsystem->DeclinePendingPrompt();
		}

		// 거래 신청 팝업도 같다 --- 전투 중에는 답할 수 없어 그냥 만료된다
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UNUTradeSubsystem* TradeSubsystem = GI->GetSubsystem<UNUTradeSubsystem>())
			{
				TradeSubsystem->DeclinePendingInvite();
			}
		}

		// 중복 바인딩 방지 (BindBattleEncounterDelegate가 여러 경로에서 호출되므로 미리 제거하고 바인딩)
		NewEncounter->OnTransitionCue.RemoveAll(this);
		NewEncounter->OnTransitionCue.AddUObject(this, &ThisClass::HandleEncounterTransition);

		// TransitionCue 를 받으려면 먼저 설정해야 함
		// 카메라 전환은 암전 뒤에 처리
		ViewedEncounter = NewEncounter;
		RefreshInteractionIndicator();
		
		CreateParryCueWidget();
		CreateCombatPopupLayer();
		CreateActionBannerWidget();

		if (IsValid(BattleCommandComponent))
		{
			BattleCommandComponent->SetEncounter(NewEncounter);
		}

		// OnRep을 놓쳤을 수 있으니 현재 큐로 한 번 실행
		HandleEncounterTransition(NewEncounter->GetTransitionCue());
		return;
	}

	// 전투 참조 복제가 부활 RPC보다 먼저 도착해도 사망자의 필드를 노출하지 않는다.
	if (IsValid(ViewedEncounter))
	{
		const ANUPlayerState* PS = GetPlayerState<ANUPlayerState>();
		const UNURecoveryComponent* Recovery = PS ? PS->FindComponentByClass<UNURecoveryComponent>() : nullptr;
		if (Recovery && !Recovery->IsUsingExternalServer() && PS->GetNUASC() && !PS->GetNUASC()->IsAlive()) { ClientHoldRecoveryFade_Implementation(); }
	}
	// 전환 델리게이트 해제
	// RestoreFieldCamera가 ViewedEncounter를 비우기 전에 해야함
	if (IsValid(ViewedEncounter))
	{
		ViewedEncounter->OnTransitionCue.RemoveAll(this);
	}
	StopTransitionSequence();
	
	if (IsValid(BattleCommandComponent))
	{
		BattleCommandComponent->SetEncounter(nullptr);
	}

	// 필드 입력이 켜지지 않는 상황이 있어 여기서 한 번 더 보장함
	// BindBattleEncounterDelegate의 최초 동기화(nullptr -> nullptr)는 SetEncounter 안에서
	// Encounter == NewEncounter 로 조기 반환돼 세션 브로드캐스트 자체가 안 나가기 때문
	SetBattleInputActive(false);

	// 로컬 전투 전용 연출 UI 정리. 명령 위젯은 세션 통지가 닫음
	CloseParryWindow();
	CloseDodgeWindow();
	if (IsValid(ParryComponent))
	{
		ParryComponent->SetCueWidget(nullptr);
	}
	if (IsValid(ParryCueWidget))
	{
		ParryCueWidget->RemoveFromParent();
		ParryCueWidget = nullptr;
	}
	DestroyNetBattleHUD();
	RestoreFieldCamera();
	RefreshInteractionIndicator();
}

// 서버가 지정한 전환 큐를 그대로 재생하는 함수
void ANUPlayerController::HandleEncounterTransition(ENUTransitionCue Cue)
{
	if (!IsLocalController() || !IsValid(ViewedEncounter))
	{
		return;
	}

	if (Cue == ENUTransitionCue::None)
	{
		StopTransitionSequence();
		return;
	}

	// 암전이 끝난 시점이므로 카메라 전환이 화면에 보이지 않음
	// 외부 구역으로 좌표를 옮기게 되면 이 순서가 필수
	if (Cue == ENUTransitionCue::FadeIn)
	{
		EnterBattleCamera(ViewedEncounter);
		UNUSFXSubsystem::Play(this, NU::SFX::Battle::Ready);
	}
	else if (ViewedEncounter->GetPhase() == ENUBattlePhase::Ending)
	{
		// 진입 암전은 필드 쪽 소리가 맡으므로, 전투가 끝나 돌아가는 암전만 소리를 냄
		UNUSFXSubsystem::Play(this, NU::SFX::Battle::Exit);
	}

	PlayTransitionSequence(ViewedEncounter->GetTransitionSequence(Cue));
}

void ANUPlayerController::EnterBattleCamera(ANUBattleEncounter* Encounter)
{
	if (!IsLocalController() || !IsValid(Encounter))
	{
		return;
	}

	if (!IsValid(FieldViewTargetBeforeBattle))
	{
		FieldViewTargetBeforeBattle = Cast<APawn>(GetViewTarget());
	}

	// 배경 세트가 자기 카메라를 들고 있으면 그 구도로 보고, 없으면 인카운터 카메라 사용
	// 세트 카메라의 Yaw 를 크게 돌리면 좌우 배치 규약이 깨지므로 주의해야함
	AActor* StageCamera = ResolveBattleViewTarget();

	if (IsValid(StageCamera))
	{
		AdoptStageCamera(StageCamera);
		SetViewTarget(StageCamera);
		return;
	}

	SetViewTarget(Encounter);
}

void ANUPlayerController::AdoptStageCamera(AActor* StageCamera)
{
	UCameraComponent* CameraComponent =
		IsValid(StageCamera) ? StageCamera->FindComponentByClass<UCameraComponent>() : nullptr;

	if (!IsValid(CameraComponent))
	{
		return;
	}

	// ACameraActor 는 기본값이 16:9 고정이라 그대로 쓰면 넓은 화면에서 좌우가 검게 잘림
	// 인카운터 카메라도 같은 이유로 생성자에서 제약을 껐음
	// 배치할 때마다 체크를 해제하게 두면 빠뜨리기 쉬워 코드에서 강제함
	CameraComponent->SetConstraintAspectRatio(false);
}

AActor* ANUPlayerController::ResolveBattleViewTarget() const
{
	UWorld* World = GetWorld();
	UNUBattleEncounterSubsystem* EncounterSubsystem = IsValid(World)
		? World->GetSubsystem<UNUBattleEncounterSubsystem>()
		: nullptr;

	return IsValid(EncounterSubsystem) ? EncounterSubsystem->GetStageViewTarget() : nullptr;
}

void ANUPlayerController::RestoreFieldCamera()
{
	if (!IsLocalController())
	{
		return;
	}

	AActor* RestoreTarget = FieldViewTargetBeforeBattle;

	if (!IsValid(RestoreTarget))
	{
		RestoreTarget = GetPawn();
	}

	if (IsValid(RestoreTarget))
	{
		SetViewTarget(RestoreTarget);
	}

	ViewedEncounter = nullptr;
	FieldViewTargetBeforeBattle = nullptr;
}

void ANUPlayerController::SetBattleInputActive(bool bActive)
{
	if (!IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer))
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	if (!IsValid(InputSubsystem))
	{
		return;
	}

	FModifyContextOptions Options;
	Options.bIgnoreAllPressedKeysUntilRelease = true;

	if (IsValid(FieldMappingContext))
	{
		InputSubsystem->RemoveMappingContext(FieldMappingContext);
	}

	if (IsValid(BattleCommandMappingContext))
	{
		InputSubsystem->RemoveMappingContext(BattleCommandMappingContext);
	}

	if (bActive)
	{
		if (IsValid(BattleCommandMappingContext))
		{
			InputSubsystem->AddMappingContext(BattleCommandMappingContext, 100, Options);
		}

		RequestCursor(CursorReason_Battle, true, this);
		
		// 다른 플레이어 컨텍스트창을 열고 전투 진입하면 닫히도록 처리
		if (IsValid(PlayerContextMenuWidget))
		{
			PlayerContextMenuWidget->Close();
		}
		return;
	}

	if (IsValid(FieldMappingContext))
	{
		InputSubsystem->AddMappingContext(FieldMappingContext, 0, Options);
	}

	RequestCursor(CursorReason_Battle, false, this);
}

void ANUPlayerController::ClearInputMappingContexts()
{
	if (!IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer))
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	if (!IsValid(InputSubsystem))
	{
		return;
	}

	if (IsValid(FieldMappingContext))
	{
		InputSubsystem->RemoveMappingContext(FieldMappingContext);
	}

	if (IsValid(BattleCommandMappingContext))
	{
		InputSubsystem->RemoveMappingContext(BattleCommandMappingContext);
	}
}

void ANUPlayerController::HandleBattleNavigate(const FInputActionValue& Value)
{
	if (IsValid(ActiveCommandComponent))
	{
		ActiveCommandComponent->NavigateSelection(Value.Get<FVector2D>());
	}
}

void ANUPlayerController::HandleBattleConfirm()
{
	if (IsValid(ActiveCommandComponent))
	{
		ActiveCommandComponent->ConfirmSelection();
	}
}

void ANUPlayerController::HandleBattleCancel()
{
	if (IsValid(ActiveCommandComponent))
	{
		ActiveCommandComponent->CancelSelection();
	}
}

void ANUPlayerController::HandleBattleParry()
{
	// 패링 불가 공격이면 패링과 같은 키가 회피로 동작함
	// 한 공격은 둘 중 하나라 창이 동시에 열리는 일이 없음
	if (DodgeWindowActionSequence != INDEX_NONE)
	{
		const int32 SentActionSequence = DodgeWindowActionSequence;

		// 한 번 요청하면 끝, 연타로 SP가 두 번 나가지 않게 보내기 전에 창을 닫음
		// 성공 여부는 아바타 머리 위 팝업으로 알려줌
		CloseDodgeWindow();
		RouteDodgePress(SentActionSequence);
		return;
	}

	if (IsValid(ParryComponent))
	{
		ParryComponent->RegisterPress();
	}
}

void ANUPlayerController::StartShowMouse()
{
	RequestCursor(CursorReason_PlayerSelect, true, this);
	SetWorldClickEnabled(true);
}

void ANUPlayerController::CompleteShowMouse()
{
	RequestCursor(CursorReason_PlayerSelect, false, this);
	SetWorldClickEnabled(false);
}

void ANUPlayerController::HandleMouseClick()
{
	// 지금 좌클릭은 컨텍스트 메뉴창 닫기만 처리
	ClosePlayerContextMenu();
}

void ANUPlayerController::HandleContextMenuClick()
{
	// 다른 대상에 이미 열려 있으면 닫기만 하고 끝냄
	if (IsValid(PlayerContextMenuWidget) && PlayerContextMenuWidget->IsOpen())
	{
		PlayerContextMenuWidget->Close();
		return;
	}

	// 패드(X)는 커서가 없어 가까운 사람을 고른다
	if (bGamepadMode)
	{
		OpenNearbyPlayerContextMenu(0);
		return;
	}

	FHitResult HitResult;
	
	const bool bHit = GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(NUCollisionChannel::PlayerSelectionTrace),
		false,
		HitResult
		);
	
	if (!bHit)
	{
		return;
	}
	
	AFieldRemoteCharacter* Remote = Cast<AFieldRemoteCharacter>(HitResult.GetActor());
	if (!IsValid(Remote))
	{
		return;
	}
	
	FVector2D MousePos;
	if (!GetMousePosition(MousePos.X, MousePos.Y))
	{
		return;
	}

	OpenFieldPlayerContextMenu(Remote, MousePos + FVector2D(12.f, 0.f));
}

void ANUPlayerController::OpenFieldPlayerContextMenu(AFieldRemoteCharacter* Remote, const FVector2D& ScreenPos)
{
	FNUPlayerContextTarget ContextTarget;
	ContextTarget.CharacterIdx = Remote->CharacterIdx;
	ContextTarget.PlayerId     = Remote->PlayerId;
	ContextTarget.DisplayName  = Remote->GetDisplayName();
	ContextTarget.ActionMask   = NUPlayerContext::FieldActions();

	// 전투 중인 사람은 초대를 받아도 팝업을 누를 수 없음
	//  -> 항목 자체를 가리도록 처리
	if (Remote->Activity == EFieldActivity::Battle)
	{
		ContextTarget.ActionMask &= ~NUPlayerContext::ActionBit(ENUPlayerContextAction::PartyInvite);
	}

	// 메뉴가 그 캐릭터를 따라다니게 액터를 넘긴다 (마우스로 열면 커서와 캐릭터 사이 간격을 지킨다)
	OpenPlayerContextMenu(ContextTarget, ScreenPos, Remote);
}

void ANUPlayerController::OpenNearbyPlayerContextMenu(int32 Step)
{
	const APawn* MyPawn = GetPawn();
	if (!IsLocalController() || MyPawn == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	// 거래 가능 거리 (gRPC 가이드 9/16: 800cm, 서버가 수락 때 다시 본다) --- 파티 초대·귓속말도 같은 반경에서 고른다
	constexpr double NearbyRadius = 800.0;
	const FVector MyLocation = MyPawn->GetActorLocation();
	TArray<AFieldRemoteCharacter*> Nearby;
	for (TActorIterator<AFieldRemoteCharacter> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared2D(It->GetActorLocation(), MyLocation) <= FMath::Square(NearbyRadius))
		{
			Nearby.Add(*It);
		}
	}
	if (Nearby.IsEmpty())
	{
		if (UNUUIManager* UI = UNUUIManager::Get(this))
		{
			UI->ShowNotice(NSLOCTEXT("NUPlayerContext", "NoNearbyPlayer", "주변에 플레이어가 없습니다."));
		}
		return;
	}
	Nearby.Sort([&MyLocation](const AFieldRemoteCharacter& A, const AFieldRemoteCharacter& B)
	{
		return FVector::DistSquared2D(A.GetActorLocation(), MyLocation) < FVector::DistSquared2D(B.GetActorLocation(), MyLocation);
	});

	// 열린 대상에서 Step만큼 옮긴 사람 (닫혀 있거나 대상이 범위를 벗어났으면 가장 가까운 사람)
	int32 Index = 0;
	if (IsValid(PlayerContextMenuWidget) && PlayerContextMenuWidget->IsOpen())
	{
		const int64 CurrentId = PlayerContextMenuWidget->GetTargetPlayerId();
		const int32 Current = Nearby.IndexOfByPredicate([CurrentId](const AFieldRemoteCharacter* Remote) { return Remote->PlayerId == CurrentId; });
		Index = Current == INDEX_NONE ? 0 : (Current + Step % Nearby.Num() + Nearby.Num()) % Nearby.Num();
	}

	// 메뉴는 그 캐릭터 자리에 연다 (열리는 방향은 메뉴가 화면 절반 기준으로 정한다)
	FVector2D ScreenPos = FVector2D::ZeroVector;
	ProjectWorldLocationToScreen(Nearby[Index]->GetActorLocation(), ScreenPos);
	OpenFieldPlayerContextMenu(Nearby[Index], ScreenPos);
}

void ANUPlayerController::OpenPlayerContextMenu(const FNUPlayerContextTarget& Target, const FVector2D& ScreenPos, AActor* FollowActor)
{
	if (!IsLocalController())
	{
		return;
	}

	// 필드 우클릭과 파티 목록 우클릭이 같이 사용함
	// 비트마스킹으로 필요한 항목만 Vertical Box에 넣어둠
	CreatePlayerContextMenuWidget();
	if (!IsValid(PlayerContextMenuWidget)) 
	{
		return;
	}

	PlayerContextMenuWidget->Open(Target, ScreenPos, FollowActor);
}

void ANUPlayerController::HandleUIEscape()
{
	UNUUIManager* UI = UNUUIManager::Get(this);
	if (UI == nullptr)
	{
		return;
	}
	if (UI->CloseTopWindow())
	{
		return; // 창 하나를 닫았으므로 이번 ESC는 소비됨
	}

	// 닫을 창이 없음 — 전체 메뉴. 대화·전투 중에는 열지 않는다 (퀘스트 저널과 같은 조건)
	if (IsGameInputBlocked() || IsValid(ViewedEncounter))
	{
		return;
	}
	UI->OpenGameMenu();
}

void ANUPlayerController::HandleUIBack()
{
	// 퀘스트 제안 패널은 매니저 창이 아니라 여기서 먼저 닫는다 (거절 버튼과 같다)
	UE_LOG(LogTemp, Verbose, TEXT("[패드] B --- 퀘스트 제안 %s"), IsValid(QuestWidget) && QuestWidget->IsQuestOfferOpen() ? TEXT("열림 => 닫기") : TEXT("없음 => 창 닫기"));
	if (IsValid(QuestWidget) && QuestWidget->IsQuestOfferOpen())
	{
		QuestWidget->CloseQuestOffer();
		return;
	}
	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		UI->CloseTopWindow();
	}
}

void ANUPlayerController::SetGamepadMode(bool bGamepad)
{
	if (!IsLocalController() || bGamepad == bGamepadMode)
	{
		return;
	}
	bGamepadMode = bGamepad;
	UE_LOG(LogTemp, Log, TEXT("[입력] 장치 전환 --- %s"), bGamepad ? TEXT("패드") : TEXT("키보드·마우스"));

	ApplyCursorState(); // 커서 표시만 다시 계산 (입력 모드는 그대로 --- 다시 걸면 위젯 포커스가 초기화된다)
	RefreshGamepadMoveBlock();
	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		UI->HandleGamepadModeChanged(bGamepad);
	}
}

void ANUPlayerController::RefreshGamepadMoveBlock()
{
	const UNUUIManager* UI = UNUUIManager::Get(this);
	// HUD·트래커 층의 상주 창은 세지 않는다 --- 여닫는 창(Window 층 이상)이 있을 때만 막는다
	const bool bWant = bGamepadMode && UI != nullptr && UI->IsAnyWindowOpen(NUUILayer::Window);
	if (bWant == bGamepadMoveIgnored)
	{
		return;
	}
	bGamepadMoveIgnored = bWant;
	SetIgnoreMoveInput(bWant);
}

void ANUPlayerController::RegisterGameMenuEntries()
{
	UNUUIManager* UI = UNUUIManager::Get(this);
	if (UI == nullptr)
	{
		return;
	}

	// 재진입(BeginPlayingState 재호출)에도 멱등하도록 내 것을 지우고 다시 등록한다
	UI->UnregisterGameMenuEntries(this);

	// 단축키는 동작 이름만 넘긴다 --- 실제 키는 메뉴가 그릴 때 사용자 설정에서 읽으므로
	// 키 설정에서 바꾸면 표기가 따라온다. 성장 계열은 개발용 바인딩이라 표기하지 않는다
	UI->RegisterGameMenuEntry(TEXT("CharacterInfo"), ENUGameMenuCategory::Character, FText::FromString(TEXT("캐릭터 정보")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleToggleCharacterInfo), this, 0, TEXT("ToggleCharacterInfo"));
	UI->RegisterGameMenuEntry(TEXT("Equipment"), ENUGameMenuCategory::Character, FText::FromString(TEXT("장비")),
		FSimpleDelegate::CreateWeakLambda(this, [this]()
		{
			if (ANUPlayerState* PS = GetPlayerState<ANUPlayerState>())
			{
				if (UNUInventoryComponent* Inventory = PS->GetInventoryComponent()) Inventory->ToggleEquipmentMenuUI();
			}
		}), this, 5);
	UI->RegisterGameMenuEntry(TEXT("Stats"), ENUGameMenuCategory::Character, FText::FromString(TEXT("스탯")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::ToggleCharacterStatsUI), this, 10);
	UI->RegisterGameMenuEntry(TEXT("Skills"), ENUGameMenuCategory::Character, FText::FromString(TEXT("스킬")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::ToggleOwnedSkillsUI), this, 20);
	UI->RegisterGameMenuEntry(TEXT("GrowthTree"), ENUGameMenuCategory::Character, FText::FromString(TEXT("성장 트리")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::ToggleProgressionGraphUI), this, 30);

	UI->RegisterGameMenuEntry(TEXT("Inventory"), ENUGameMenuCategory::Item, FText::FromString(TEXT("인벤토리")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleToggleInventory), this, 0, TEXT("ToggleInventory"));

	UI->RegisterGameMenuEntry(TEXT("NaiaRoster"), ENUGameMenuCategory::Naia, FText::FromString(TEXT("나이아 편성")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleToggleNaiaRoster), this, 0, TEXT("ToggleNaiaRoster"));

	UI->RegisterGameMenuEntry(TEXT("QuestJournal"), ENUGameMenuCategory::Adventure, FText::FromString(TEXT("퀘스트 일지")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleToggleQuestJournal), this, 0, TEXT("ToggleQuestJournal"));
	UI->RegisterGameMenuEntry(TEXT("WorldMap"), ENUGameMenuCategory::Adventure, FText::FromString(TEXT("지도")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleToggleWorldMap), this, 10, TEXT("ToggleWorldMap"));

	// 하단 줄 — 거래(대상 지정 필요)와 창고(NPC 전용)는 메뉴에 넣지 않는다
	UI->RegisterGameMenuEntry(TEXT("Options"), ENUGameMenuCategory::System, FText::FromString(TEXT("옵션")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleMenuOptions), this, 0);
	UI->RegisterGameMenuEntry(TEXT("CharacterSelect"), ENUGameMenuCategory::System, FText::FromString(TEXT("캐릭터 선택으로")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleMenuCharacterSelect), this, 10);
	UI->RegisterGameMenuEntry(TEXT("QuitGame"), ENUGameMenuCategory::System, FText::FromString(TEXT("게임 종료")),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleMenuQuitGame), this, 20);
}

void ANUPlayerController::HandleMenuOptions()
{
	UNUOptionsWindowWidget::Show(this);
}

void ANUPlayerController::HandleToggleWorldMap()
{
	if (WorldMapWindow.IsValid())
	{
		WorldMapWindow->Close();
		WorldMapWindow.Reset();
		return;
	}
	WorldMapWindow = UNU_WorldMapWindowWidget::Show(this);
}

void ANUPlayerController::HandleMenuCharacterSelect()
{
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	if (Settings == nullptr || Settings->CharacterSelectLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[메뉴] CharacterSelectLevel 미지정 --- 프로젝트 세팅 > 게임 > NU UI"));
		return;
	}

	FSimpleDelegate Travel = FSimpleDelegate::CreateWeakLambda(this, [this]()
	{
		const UNUUISettings* InnerSettings = GetDefault<UNUUISettings>();
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, InnerSettings->CharacterSelectLevel);
	});

	// 확인 창이 없으면(클래스 미지정) 그냥 이동한다 --- 호출부가 분기하지 않는 공용 확인 창 원칙
	TSubclassOf<UNUConfirmDialog> DialogClass = Settings->ConfirmDialogClass.LoadSynchronous();
	if (UNUConfirmDialog::Show(this, DialogClass,
		FText::FromString(TEXT("캐릭터 선택 화면으로 돌아갈까요?")),
		FText::FromString(TEXT("돌아가기")), Travel) == nullptr)
	{
		Travel.ExecuteIfBound();
	}
}

void ANUPlayerController::HandleMenuQuitGame()
{
	FSimpleDelegate Quit = FSimpleDelegate::CreateWeakLambda(this, [this]()
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	});

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	TSubclassOf<UNUConfirmDialog> DialogClass = Settings ? Settings->ConfirmDialogClass.LoadSynchronous() : nullptr;
	if (UNUConfirmDialog::Show(this, DialogClass,
		FText::FromString(TEXT("게임을 종료할까요?")),
		FText::FromString(TEXT("종료")), Quit) == nullptr)
	{
		Quit.ExecuteIfBound();
	}
}

void ANUPlayerController::ClosePlayerContextMenu()
{
	if (IsValid(PlayerContextMenuWidget) && PlayerContextMenuWidget->IsOpen())
	{
		PlayerContextMenuWidget->Close();
	}
}

bool ANUPlayerController::IsBlockedOnlyByCharacterMenu() const
{
	if (bRecoveryTransitionActive || GameInputBlockRequesters.IsEmpty())
	{
		return false;
	}
	for (const auto& Request : GameInputBlockRequesters)
	{
		if (Request.Value.IsValid() && !Request.Value->IsA<UNUCharacterMenuBase>())
		{
			return false;
		}
	}
	return true;
}

void ANUPlayerController::HandleToggleInventory()
{
	// 통합 캐릭터 메뉴는 자신이 입력 차단 이유라 막는 것이 그 메뉴뿐이면 키를 받는다 (퀘스트 일지와 같은 방식)
	if (IsGameInputBlocked() && !IsBlockedOnlyByCharacterMenu())
	{
		return;
	}

	if (ANUPlayerState* PS = GetPlayerState<ANUPlayerState>())
	{
		if (UNUInventoryComponent* Inventory = PS->GetInventoryComponent())
		{
			Inventory->ToggleInventoryUI();
		}
	}
}

void ANUPlayerController::HandleInventoryKey()
{
	if (!bGamepadMode)
	{
		HandleToggleInventory();
		return;
	}
	if (IsGameInputBlocked() && !IsBlockedOnlyByCharacterMenu())
	{
		return;
	}

	if (ANUPlayerState* PS = GetPlayerState<ANUPlayerState>())
	{
		if (UNUInventoryComponent* Inventory = PS->GetInventoryComponent())
		{
			Inventory->ToggleCharacterMenuUI();
		}
	}
}

void ANUPlayerController::HandleToggleCharacterInfo()
{
	if (IsGameInputBlocked() && !IsBlockedOnlyByCharacterMenu())
	{
		return;
	}

	if (ANUPlayerState* PS = GetPlayerState<ANUPlayerState>())
	{
		if (UNUInventoryComponent* Inventory = PS->GetInventoryComponent())
		{
			Inventory->ToggleCharacterInfoUI();
		}
	}
}

void ANUPlayerController::HandleToggleNaiaRoster()
{
	if (IsGameInputBlocked())
	{
		return;
	}

	if (ANUPlayerState* PS = GetPlayerState<ANUPlayerState>())
	{
		if (UNUNaiaRosterComponent* Roster = PS->GetNaiaRosterComponent())
		{
			Roster->ToggleRosterUI();
		}
	}
}

void ANUPlayerController::HandleToggleQuestJournal()
{
	// 저널 자신이 입력 차단 이유이므로 닫기는 차단 검사보다 먼저 처리한다.
	if (IsValid(QuestJournalWidget) && QuestJournalWidget->IsJournalOpen())
	{
		if (QuestJournalWidget->IsJournalClosing())
		{
			// Reopen smoothly only while the journal is the sole input-blocking owner.
			bool bOtherBlocker = bRecoveryTransitionActive || IsValid(ViewedEncounter);
			for (const auto& Request : GameInputBlockRequesters)
			{
				bOtherBlocker |= Request.Value.IsValid() && Request.Value.Get() != QuestJournalWidget.Get();
			}
			if (!bOtherBlocker) QuestJournalWidget->OpenJournal();
		}
		else QuestJournalWidget->CloseJournal();
		return;
	}

	if (IsGameInputBlocked() || IsValid(ViewedEncounter))
	{
		return;
	}

	CreateQuestJournalWidget();
	if (IsValid(QuestJournalWidget))
	{
		QuestJournalWidget->OpenJournal();
		if (UNUUIManager* UI = UNUUIManager::Get(this))
		{
			FNUUIWindowParams Params;
			Params.Layer = NUUILayer::Window;
			Params.bRequestCursor = false;
			Params.bBlockGameInput = false;
			Params.bVisibilityDriven = true;
			Params.bCloseOnSessionEnd = true;
			UI->OpenWindow(
				QuestJournalWidget,
				Params,
				FSimpleDelegate::CreateUObject(QuestJournalWidget.Get(), &UNUQuestJournalWidget::CloseJournal));
		}
	}
}

void ANUPlayerController::SelectBattleRootAction(
	ENUBattleRootAction Action)
{
	if (!IsValid(ActiveCommandComponent)
		|| ActiveCommandComponent->GetSelectionPhase() != ENUCommandSelectionPhase::RootMenu)
	{
		return;
	}

	ActiveCommandComponent->SelectRootAction(Action);
}

void ANUPlayerController::HandleBattleCommandSelectionChanged()
{
	if (!IsValid(ActiveCommandComponent))
	{
		if (IsValid(BattleCommandWidget))
		{
			BattleCommandWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		OnBattleCommandUIChanged.Broadcast();
		return;
	}
	
	const ENUCommandSelectionPhase SelectionPhase = ActiveCommandComponent->GetSelectionPhase();
	
	if (IsValid(BattleCommandWidget))
	{
		BattleCommandWidget->SetVisibility(
			SelectionPhase == ENUCommandSelectionPhase::Inactive
				? ESlateVisibility::Collapsed
				: ESlateVisibility::Visible
				);
	}

	OnBattleCommandUIChanged.Broadcast();
	// 공용 전투 WBP를 그대로 사용하고 현재 명령의 표시 이름만 반영한다.
	if (IsValid(BattleCommandWidget))
	{
		if (UNU_CommonButtonWidget* AttackButton = Cast<UNU_CommonButtonWidget>(BattleCommandWidget->GetWidgetFromName(TEXT("Button_Attack")))) { AttackButton->SetLabelText(ActiveCommandComponent->GetAttackDisplayName()); }
		if (UNU_CommonButtonWidget* SkillButton = Cast<UNU_CommonButtonWidget>(BattleCommandWidget->GetWidgetFromName(TEXT("Button_Skill")))) { SkillButton->SetLabelText(ActiveCommandComponent->GetSkillDisplayName()); }
	}
}

void ANUPlayerController::CreateBattleCommandWidget()
{
	if (!IsLocalController()
		|| !BattleCommandWidgetClass
		|| IsValid(BattleCommandWidget))
	{
		return;
	}

	BattleCommandWidget = CreateWidget<UUserWidget>(this, BattleCommandWidgetClass);
	if (!IsValid(BattleCommandWidget))
	{
		return;
	}

	BattleCommandWidget->AddToViewport(NUUILayer::HUD);
	BattleCommandWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ANUPlayerController::CreateParryCueWidget()
{
	if (!IsLocalController()
		|| !ParryCueWidgetClass
		|| IsValid(ParryCueWidget))
	{
		return;
	}

	ParryCueWidget = CreateWidget<UNUParryCueWidget>(this, ParryCueWidgetClass);
	if (!IsValid(ParryCueWidget))
	{
		return;
	}

	ParryCueWidget->AddToViewport(NUUILayer::HUD);
	ParryCueWidget->SetVisibility(ESlateVisibility::Collapsed);

	if (IsValid(ParryComponent))
	{
		ParryComponent->SetCueWidget(ParryCueWidget);
	}
}

void ANUPlayerController::EnsureInGameHUDWidgets()
{
    if (!IsLocalController())
    {
    	return;
    }

    // UNU_BaseInGameWidget 은 NativeConstruct 시점에 PlayerState/ASC 를 해석하고 그 ASC 로 구독을 건다.
    // 즉 PlayerState 복제 전에 만들면 아무것도 구독하지 못한 채 살아있게 되므로,
    // 준비되기 전에는 만들지 않고 다음 호출(OnRep_PlayerState)로 미룬다.
    const ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>();
    if (NUPlayerState == nullptr || NUPlayerState->GetNUASC() == nullptr)
    {
        return;
    }

    CreateBattleHUDWidget();
    CreateBattleSwitcherWidget();
    CreateFieldHUDWidget();

    // 참고: 만약 아래 위젯들도 ASC나 PlayerState가 준비된 이후 생성되어야 한다면 
    // 기존에 호출하던 위치(예: BeginPlay 등)에서 제거하고 이곳에서 호출하도록 수정해야 합니다.
    // CreateCombatPopupLayer();
    // CreateActionBannerWidget();
}

void ANUPlayerController::CreateCombatPopupLayer()
{
    if (!IsLocalController()
        || !CombatPopupLayerClass
        || IsValid(CombatPopupLayer))
    {
        return;
    }

    CombatPopupLayer = CreateWidget<UNUCombatPopupLayer>(this, CombatPopupLayerClass);
    if (!IsValid(CombatPopupLayer))
    {
    	return;
    }

    // 팝업이 없을 때도 레이어는 떠 있음, 루트가 SelfHitTestInvisible이라 클릭을 막지 않음
    CombatPopupLayer->AddToViewport(NUUILayer::HUD);
}

void ANUPlayerController::ShowCombatPopup(AActor* AnchorActor, const FNUCombatPopupData& PopupData)
{
    // NUA: 패링·회피 성공 연출 --- 로컬(GAS) 전투와 넷 전투가 모두 지나는 자리 (nu.VFX.Parry.Enabled / nu.VFX.Dodge.Enabled 로 끔)
    if (IsLocalController())
    {
        NUBattleParryFX::PlayForPopup(AnchorActor, PopupData.Type);

        // 팝업 하나에 소리 하나 --- 이 자리가 로컬·넷 전투 공통이라 여기서만 냄
        switch (PopupData.Type)
        {
        case ENUCombatPopupType::Damage:
            UNUSFXSubsystem::Play(this, PopupData.Affinity == ENUElementAffinity::Weak ? NU::SFX::Battle::Weak
                : PopupData.Affinity == ENUElementAffinity::Resist ? NU::SFX::Battle::Resist
                : NU::SFX::Battle::Hit);
            break;
        case ENUCombatPopupType::Critical:
            UNUSFXSubsystem::Play(this, NU::SFX::Battle::Critical);
            break;
        case ENUCombatPopupType::Heal:
            UNUSFXSubsystem::Play(this, NU::SFX::Battle::Heal);
            break;
        case ENUCombatPopupType::Parry:
            UNUSFXSubsystem::Play(this, NU::SFX::Battle::ParrySuccess);
            break;
        case ENUCombatPopupType::Dodge:
            UNUSFXSubsystem::Play(this, NU::SFX::Battle::DodgeSuccess);
            break;
        case ENUCombatPopupType::DodgeFail:
            UNUSFXSubsystem::Play(this, NU::SFX::Battle::DodgeFail);
            break;
        case ENUCombatPopupType::ParryFail:
            UNUSFXSubsystem::Play(this, NU::SFX::Battle::ParryFail);
            break;
        case ENUCombatPopupType::CaptureSuccess:
            UNUSFXSubsystem::Play(this, NU::SFX::Battle::CaptureSuccess);
            break;
        case ENUCombatPopupType::CaptureFail:
            UNUSFXSubsystem::Play(this, NU::SFX::Battle::CaptureFail);
            break;
        case ENUCombatPopupType::ActionFail:
            UNUSFXSubsystem::Play(this, NU::SFX::Battle::ActionFail);
            break;
        case ENUCombatPopupType::NotEnoughSP:
            UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
            break;
        default:
            break;
        }
    }

    if (!IsLocalController() || !IsValid(CombatPopupLayer))
    {
    	return;
    }

    CombatPopupLayer->AddPopup(AnchorActor, PopupData);
}

void ANUPlayerController::CreateActionBannerWidget()
{
    if (!IsLocalController()
        || !ActionBannerWidgetClass
        || IsValid(ActionBannerWidget))
    {
        return;
    }

    ActionBannerWidget = CreateWidget<UNUActionBannerWidget>(this, ActionBannerWidgetClass);
    if (!IsValid(ActionBannerWidget))
    {
    	return;
    }

    ActionBannerWidget->AddToViewport(NUUILayer::HUD);

    // 첫 행동이 나올 때까지는 숨김
    ActionBannerWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ANUPlayerController::ShowActionBanner(const FNUBattleActionBanner& Banner)
{
    if (!IsLocalController() || !IsValid(ActionBannerWidget))
    {
    	return;
    }

    ActionBannerWidget->Setup(Banner);
}

void ANUPlayerController::EnsureNetBattleHUD()
{
	CreateCombatPopupLayer();
	CreateActionBannerWidget();

	// 패링 창의 바는 로컬 전투와 같은 위젯·같은 판정기를 사용함
	// 넷 전투는 액션 큐가 이 위젯을 열고 판정 결과를 서버에 보고함
	CreateParryCueWidget();
}

void ANUPlayerController::DestroyNetBattleHUD()
{
	// 창이 열린 채 무대가 철거되면 바가 화면에 남음
	CloseParryWindow();
	CloseDodgeWindow();

	if (IsValid(ParryComponent))
	{
		ParryComponent->SetCueWidget(nullptr);
	}

	if (IsValid(ParryCueWidget))
	{
		ParryCueWidget->RemoveFromParent();
		ParryCueWidget = nullptr;
	}

	if (IsValid(CombatPopupLayer))
	{
		CombatPopupLayer->RemoveFromParent();
		CombatPopupLayer = nullptr;
	}

	if (IsValid(ActionBannerWidget))
	{
		ActionBannerWidget->RemoveFromParent();
		ActionBannerWidget = nullptr;
	}
}

void ANUPlayerController::CreateBattleHUDWidget()
{
    if (!IsLocalController()
        || !BattleHUDWidgetClass
        || IsValid(BattleHUDWidget))
    {
        return;
    }

    BattleHUDWidget = CreateWidget<UNU_BattleHUDWidget>(this, BattleHUDWidgetClass);
    if (!IsValid(BattleHUDWidget))
    {
    	return;
    }

    // 가시성은 위젯이 Encounter 유무를 보고 스스로 정한다 (ApplyBattleVisibility)
    BattleHUDWidget->AddToViewport(BattleHUDZOrder);
}

void ANUPlayerController::DestroyBattleHUDWidget()
{
    if (!IsValid(BattleHUDWidget))
    {
    	return;
    }

    BattleHUDWidget->RemoveFromParent();
    BattleHUDWidget = nullptr;
}

void ANUPlayerController::CreateBattleSwitcherWidget()
{
    if (!IsLocalController()
        || !BattleSwitcherWidgetClass
        || IsValid(BattleSwitcherWidget))
    {
        return;
    }

    BattleSwitcherWidget = CreateWidget<UNU_BattleSwitcherWidget>(this, BattleSwitcherWidgetClass);
    if (!IsValid(BattleSwitcherWidget))
    {
    	return;
    }

    // 가시성은 위젯이 페이즈를 보고 스스로 정한다 (ApplyScreen)
    BattleSwitcherWidget->AddToViewport(BattleSwitcherZOrder);
}

void ANUPlayerController::DestroyBattleSwitcherWidget()
{
    if (!IsValid(BattleSwitcherWidget))
    {
    	return;
    }

    BattleSwitcherWidget->RemoveFromParent();
    BattleSwitcherWidget = nullptr;
}

void ANUPlayerController::CreateFieldHUDWidget()
{
    if (!IsLocalController()
        || !FieldHUDWidgetClass
        || IsValid(FieldHUDWidget))
    {
        return;
    }

    FieldHUDWidget = CreateWidget<UNU_FieldHUDWidget>(this, FieldHUDWidgetClass);
    if (!IsValid(FieldHUDWidget))
    {
    	return;
    }

    // 가시성은 위젯이 전투 여부를 보고 스스로 정한다 (ApplyFieldVisibility)
    FieldHUDWidget->AddToViewport(FieldHUDZOrder);
}

void ANUPlayerController::DestroyFieldHUDWidget()
{
    if (!IsValid(FieldHUDWidget))
    {
    	return;
    }

    FieldHUDWidget->RemoveFromParent();
    FieldHUDWidget = nullptr;
}

void ANUPlayerController::DestroyBattleCommandWidget()
{
	if (IsValid(BattleCommandWidget))
	{
		BattleCommandWidget->RemoveFromParent();
		BattleCommandWidget = nullptr;
	}
}

void ANUPlayerController::CreatePlayerContextMenuWidget()
{
	if (!IsLocalController()
		|| !PlayerContextMenuClass
		|| IsValid(PlayerContextMenuWidget))
	{
		return;
	}

	PlayerContextMenuWidget = CreateWidget<UNUPlayerContextMenu>(this, PlayerContextMenuClass);
	if (!IsValid(PlayerContextMenuWidget))
	{
		return;
	}

	// Menu 밴드 — 인벤(Window=100) 등 모든 창 위. 0이면 창에 깔려 안 보이던 버그
	PlayerContextMenuWidget->AddToViewport(NUUILayer::Menu);
	PlayerContextMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ANUPlayerController::DestroyPlayerContextMenuWidget()
{
	if (IsValid(PlayerContextMenuWidget))
	{
		PlayerContextMenuWidget->RemoveFromParent();
		PlayerContextMenuWidget = nullptr;
	}
}

void ANUPlayerController::CreateChatWidget()
{
	if (!IsLocalController()
		|| !ChatWidgetClass
		|| IsValid(ChatWidget))
	{
		return;
	}

	ChatWidget = CreateWidget<UNUChatWidget>(this, ChatWidgetClass);
	if (!IsValid(ChatWidget))
	{
		return;
	}

	// 채팅창은 항상 떠 있어야 함
	ChatWidget->AddToViewport(NUUILayer::HUD);

	ChatWidget->OnFocusChanged.BindUObject(this, &ThisClass::HandleChatFocusChanged);
	ChatWidget->OnChatSubmitted.BindUObject(this, &ThisClass::HandleChatSubmitted);

	// 수신은 서브시스템이 모아서 넘겨준다 
	// 근접·전체·귓속말이 한 창으로 들어옴
	if (UNUChatSubsystem* Chat = GetChatSubsystem())
	{
		Chat->OnChatMessage.AddUObject(ChatWidget.Get(), &UNUChatWidget::AddMessage);
		Chat->OnWhisperTargetChanged.AddUObject(this, &ThisClass::HandleWhisperTargetChanged);
	}
}

void ANUPlayerController::DestroyChatWidget()
{
	if (!IsValid(ChatWidget))
	{
		return;
	}

	if (UNUChatSubsystem* Chat = GetChatSubsystem())
	{
		Chat->OnChatMessage.RemoveAll(ChatWidget.Get());
		Chat->OnWhisperTargetChanged.RemoveAll(this);
	}

	ChatWidget->OnFocusChanged.Unbind();
	ChatWidget->OnChatSubmitted.Unbind();

	ChatWidget->RemoveFromParent();
	ChatWidget = nullptr;
}

void ANUPlayerController::CreatePartyListWidget()
{
	if (!IsLocalController() || IsValid(PartyListWidget))
	{
		return;
	}

	if (!PartyListWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PartyListWidgetClass가 지정되지 않아 파티 목록이 뜨지 않습니다"));
		return;
	}

	PartyListWidget = CreateWidget<UNUPartyListWidget>(this, PartyListWidgetClass);
	if (!IsValid(PartyListWidget))
	{
		return;
	}

	// 파티가 없으면 위젯이 스스로 접음
	PartyListWidget->AddToViewport(NUUILayer::HUD);
}

void ANUPlayerController::DestroyPartyListWidget()
{
	if (!IsValid(PartyListWidget))
	{
		return;
	}

	PartyListWidget->RemoveFromParent();
	PartyListWidget = nullptr;
}

void ANUPlayerController::CreatePartyPromptWidget()
{
	if (!IsLocalController() || IsValid(PartyPromptWidget))
	{
		return;
	}

	if (!PartyPromptWidgetClass)
	{
		// 팝업이 없어도 초대 자체는 동작함, 시스템 채팅과 콘솔로 확인할 수 있음
		UE_LOG(LogTemp, Warning, TEXT("PartyPromptWidgetClass가 지정되지 않아 초대 팝업이 뜨지 않습니다"));
		return;
	}

	PartyPromptWidget = CreateWidget<UNUPartyPromptWidget>(this, PartyPromptWidgetClass);
	if (!IsValid(PartyPromptWidget))
	{
		return;
	}

	// 전투 커맨드(0), 대화 커맨드(40) 사이
	// Popup 밴드 — 창고 딤(Window-1=99)보다 위. 30이면 딤에 가려 안 보인 채 만료되던 버그
	PartyPromptWidget->AddToViewport(NUUILayer::Popup);
	PartyPromptWidget->SetVisibility(ESlateVisibility::Collapsed);

	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		PartySubsystem->OnPartyPrompt.AddUObject(this, &ThisClass::HandlePartyPrompt);
		PartySubsystem->OnPartyPromptClosed.AddUObject(this, &ThisClass::HandlePartyPromptClosed);
	}
}

void ANUPlayerController::DestroyPartyPromptWidget()
{
	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		PartySubsystem->OnPartyPrompt.RemoveAll(this);
		PartySubsystem->OnPartyPromptClosed.RemoveAll(this);
	}

	if (!IsValid(PartyPromptWidget))
	{
		return;
	}

	// 팝업이 뜬 채로 레벨이 바뀌면 커서 요청이 남는다
	RequestCursor(CursorReason_PartyPrompt, false, PartyPromptWidget);

	PartyPromptWidget->RemoveFromParent();
	PartyPromptWidget = nullptr;
}

void ANUPlayerController::HandlePartyPrompt(bool bIsInvite, const FString& FromName, int32 ExpireSec)
{
	if (!IsLocalController() || !IsValid(PartyPromptWidget))
	{
		return;
	}

	PartyPromptWidget->Open(bIsInvite, FromName, ExpireSec);

	// 커서만 띄운다. 초대가 왔다고 캐릭터가 멈추면 안 되므로 게임 입력은 막지 않음
	RequestCursor(CursorReason_PartyPrompt, true, PartyPromptWidget);
}

void ANUPlayerController::HandlePartyPromptClosed()
{
	if (!IsLocalController())
	{
		return;
	}

	// 수락·거절·만료가 전부 여기로 온다. 정리가 한 곳뿐이라 커서를 놓칠 일이 없음
	if (IsValid(PartyPromptWidget))
	{
		PartyPromptWidget->Close();
	}

	RequestCursor(CursorReason_PartyPrompt, false, PartyPromptWidget);
}

UNUPartySubsystem* ANUPlayerController::GetPartySubsystem() const
{
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNUPartySubsystem>() : nullptr;
}

UNUChatSubsystem* ANUPlayerController::GetChatSubsystem() const
{
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNUChatSubsystem>() : nullptr;
}

void ANUPlayerController::HandleChatOpen()
{
	if (IsDialogueOpen())
	{
		return;
	}

	if (!IsValid(ChatWidget) || ChatWidget->IsFocusing())
	{
		return;
	}

	// 실제 입력 모드 전환은 HandleChatFocusChanged가 처리함
	ChatWidget->BeginFocus();
}

void ANUPlayerController::HandleChatFocusChanged(bool bFocusing)
{
	if (!IsLocalController())
	{
		return;
	}

	// IMC_Field를 빼면 이동·Alt·인벤토리 키가 전부 함께 불가능함
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = IsValid(LocalPlayer)
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer)
		: nullptr;

	if (IsValid(InputSubsystem))
	{
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = true;

		if (bFocusing)
		{
			// Field, Battle IMC 둘 다 제거
			// 전투 중에 채팅을 치면 전투 커맨드 입력이 안되도록 빼는 것
			if (IsValid(FieldMappingContext))
			{
				InputSubsystem->RemoveMappingContext(FieldMappingContext);
			}
			if (IsValid(BattleCommandMappingContext))
			{
				InputSubsystem->RemoveMappingContext(BattleCommandMappingContext);
			}
		}
		else if (IsValid(ActiveCommandComponent))
		{
			// 전투 중이었으면 전투 입력으로 돌아간다
			// ViewedEncounter 는 로컬 전투 액터라 넷 전투에서는 비어 있다
			// 전투 IMC 를 켠 것과 같은 것으로 물어야 두 전투가 같이 돌아온다
			if (IsValid(BattleCommandMappingContext))
			{
				InputSubsystem->AddMappingContext(BattleCommandMappingContext, 100, Options);
			}
		}
		else if (IsValid(FieldMappingContext))
		{
			// 전투중이 아니면 Field IMC 복구
			InputSubsystem->AddMappingContext(FieldMappingContext, 0, Options);
		}
	}

	// 채팅창 열리면 커서 보이게 처리
	RequestCursor(CursorReason_Chat, bFocusing, ChatWidget);
}

void ANUPlayerController::HandleChatSubmitted(ENUChatChannel Channel, const FString& Text)
{
	if (!IsValid(ChatWidget))
	{
		return;
	}

	UNUChatSubsystem* Chat = GetChatSubsystem();
	if (!IsValid(Chat))
	{
		return;
	}

	// 명령어 해석과 채널별 전송은 서브시스템에서 처리
	// PlayerController는 위젯 입력을 넘기기만 함
	Chat->SubmitChat(Channel, Text);
}

void ANUPlayerController::HandleWhisperTargetChanged(int64 TargetIdx, const FString& TargetName)
{
	if (!IsLocalController() || !IsValid(ChatWidget))
	{
		return;
	}

	// 컨텍스트 메뉴에서 귓속말을 선택했을 때 바로 칠 수 있게 입력창을 열음
	// BeginFocus가 입력창을 비우므로 "/w 이름 " 은 그 뒤에 채워야 함
	// BeginWhisperCommand가 전송 채널도 귓속말로 맞춰줌
	ChatWidget->BeginFocus();
	ChatWidget->BeginWhisperCommand(TargetName);
}

void ANUPlayerController::CreateInteractionIndicatorWidget()
{
	if (!IsLocalController()
		|| !InteractionIndicatorWidgetClass
		|| IsValid(InteractionIndicatorWidget))
	{
		return;
	}

	InteractionIndicatorWidget = CreateWidget<UUserWidget>(this, InteractionIndicatorWidgetClass);

	if (!IsValid(InteractionIndicatorWidget))
	{
		return;
	}

	InteractionIndicatorWidget->AddToViewport(NUUILayer::Indicator);
	InteractionIndicatorWidget->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));
	InteractionIndicatorWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ANUPlayerController::DestroyInteractionIndicatorWidget()
{
	if (!IsValid(InteractionIndicatorWidget))
	{
		return;
	}

	InteractionIndicatorWidget->RemoveFromParent();
	InteractionIndicatorWidget = nullptr;
}

void ANUPlayerController::CreateQuestWidget()
{
	if (!IsLocalController() || QuestWidgetClass == nullptr || IsValid(QuestWidget))
	{
		return;
	}

	QuestWidget = CreateWidget<UNUQuestWidget>(this, QuestWidgetClass);
	if (!IsValid(QuestWidget))
	{
		return;
	}

	QuestWidget->AddToViewport(NUUILayer::Tracker);
	QuestWidget->RefreshQuestComponent();
}

void ANUPlayerController::DestroyQuestWidget()
{
	if (!IsValid(QuestWidget))
	{
		return;
	}

	QuestWidget->RemoveFromParent();
	QuestWidget = nullptr;
}

void ANUPlayerController::RefreshQuestWidget()
{
	CreateQuestWidget();

	if (IsValid(QuestWidget))
	{
		QuestWidget->RefreshQuestComponent();
	}
}

void ANUPlayerController::CreateQuestJournalWidget()
{
	if (!IsLocalController() || QuestJournalWidgetClass == nullptr || IsValid(QuestJournalWidget))
	{
		return;
	}

	QuestJournalWidget = CreateWidget<UNUQuestJournalWidget>(this, QuestJournalWidgetClass);
	if (!IsValid(QuestJournalWidget))
	{
		return;
	}

	QuestJournalWidget->AddToViewport(NUUILayer::Window);
	QuestJournalWidget->RefreshQuestComponent();
}

void ANUPlayerController::DestroyQuestJournalWidget()
{
	if (!IsValid(QuestJournalWidget))
	{
		return;
	}

	CloseQuestJournal();
	QuestJournalWidget->RemoveFromParent();
	QuestJournalWidget = nullptr;
}

void ANUPlayerController::RefreshQuestJournalWidget()
{
	CreateQuestJournalWidget();

	if (IsValid(QuestJournalWidget))
	{
		QuestJournalWidget->RefreshQuestComponent();
	}
}

void ANUPlayerController::TrackQuest(FName QuestId)
{
	if (QuestId.IsNone() || TrackedQuestIds.Contains(QuestId))
	{
		return;
	}

	if (IsTrackedQuestFull())
	{
		UE_LOG(LogTemp, Log, TEXT("Quest: 트래커가 가득 차 고정하지 못했습니다 QuestId=%s (%d/%d)"),
			*QuestId.ToString(), TrackedQuestIds.Num(), MaxTrackedQuests);
		return;
	}

	TrackedQuestIds.Add(QuestId);

	if (IsValid(QuestWidget))
	{
		QuestWidget->RefreshTracker();
	}
	if (IsValid(QuestJournalWidget) && QuestJournalWidget->IsJournalOpen())
	{
		QuestJournalWidget->RefreshJournal();
	}
}

void ANUPlayerController::UntrackQuest(FName QuestId)
{
	if (TrackedQuestIds.Remove(QuestId) <= 0)
	{
		return;
	}

	if (IsValid(QuestWidget))
	{
		QuestWidget->RefreshTracker();
	}
	if (IsValid(QuestJournalWidget) && QuestJournalWidget->IsJournalOpen())
	{
		QuestJournalWidget->RefreshJournal();
	}
}

void ANUPlayerController::ToggleTrackedQuest(FName QuestId)
{
	if (IsQuestTracked(QuestId))
	{
		UntrackQuest(QuestId);
		return;
	}

	TrackQuest(QuestId);
}

void ANUPlayerController::CloseQuestJournal()
{
	if (IsValid(QuestJournalWidget))
	{
		if (UNUUIManager* UI = UNUUIManager::Get(this))
		{
			UI->CloseWindow(QuestJournalWidget);
		}
		QuestJournalWidget->CloseJournalImmediately();
	}
}

void ANUPlayerController::ClientOpenDialogue_Implementation(UNUDialogueDefinition* DialogueDefinition, AActor* SourceActor, const FNUQuestInteractionOption& QuestOption)
{
	if (!IsLocalController() || !IsValid(SourceActor))
	{
		return;
	}

	// 하나의 로컬 모달만 유지한다. 대화 중에는 F/Space/Enter로 현재 대사를 진행한다.
	if (IsDialogueOpen())
	{
		return;
	}

	CloseQuestJournal();

	// 대화 위젯 생성과 ESC, 커서, 입력 차단은 UIController와 UIManager에 맡긴다.
	UNUUIManager* UI = UNUUIManager::Get(this);
	UNUDialogueUIController* DialogueUI = UI != nullptr
		? UI->GetController<UNUDialogueUIController>()
		: nullptr;
	PendingDialogueSource = SourceActor;
	PendingDialogueOption = QuestOption;
	if (DialogueUI == nullptr || !DialogueUI->OpenDialogue(
		DialogueDefinition,
		FOnNUDialogueSessionClosed::CreateUObject(this, &ThisClass::HandleDialogueClosed)))
	{
		PendingDialogueSource = nullptr;
		PendingDialogueOption = FNUQuestInteractionOption();
		UE_LOG(LogNUQuest, Error, TEXT("Dialogue: 대화를 열 수 없어 후속 처리를 중단합니다. Dialogue=%s"), *GetNameSafe(DialogueDefinition));
		return;
	}

	LastDialogueSource = SourceActor;
	RefreshInteractionIndicator();
}

void ANUPlayerController::ClientOpenQuestDialogue_Implementation(UNUQuestDialogueDefinition* DialogueDefinition, AActor* SourceActor, const FNUQuestInteractionOption& QuestOption)
{
	if (!IsLocalController() || !IsValid(DialogueDefinition) || !IsValid(SourceActor))
	{
		return;
	}

	const FName Dialogue = DialogueDefinition->FindDialogue(QuestOption.Phase);
	if (!Dialogue.IsNone())
	{
		ClientOpenQuestDialogueSequence_Implementation(Dialogue, SourceActor, QuestOption);
		return;
	}
	const TArray<FNUDialogueLine>* Lines = DialogueDefinition->FindLines(QuestOption.Phase);
	if (Lines != nullptr) { OpenQuestDialogueLines(*Lines, SourceActor, QuestOption); }
}

void ANUPlayerController::ClientOpenQuestDialogueSequence_Implementation(FName DialogueId, AActor* SourceActor, const FNUQuestInteractionOption& QuestOption)
{
	const UNUDialogueDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUDialogueDataSubsystem>();
	TArray<FNUDialogueLine> Lines;
	if (Data == nullptr || !Data->GetLines(DialogueId, Lines))
	{
		UE_LOG(LogNUQuest, Error, TEXT("대사 연결을 읽을 수 없습니다. Dialogue=%s"), *DialogueId.ToString());
		return;
	}
	OpenQuestDialogueLines(Lines, SourceActor, QuestOption);
}

void ANUPlayerController::OpenQuestDialogueLines(const TArray<FNUDialogueLine>& Lines, AActor* SourceActor, const FNUQuestInteractionOption& QuestOption)
{
	if (!IsLocalController() || !IsValid(SourceActor) || Lines.IsEmpty() || IsDialogueOpen())
	{
		return;
	}

	CloseQuestJournal();

	UNUUIManager* UI = UNUUIManager::Get(this);
	UNUDialogueUIController* DialogueUI = UI != nullptr
		? UI->GetController<UNUDialogueUIController>()
		: nullptr;
	PendingDialogueSource = SourceActor;
	PendingDialogueOption = QuestOption;
	if (DialogueUI == nullptr || !DialogueUI->OpenDialogueLines(
		Lines,
		FOnNUDialogueSessionClosed::CreateUObject(this, &ThisClass::HandleDialogueClosed)))
	{
		PendingDialogueSource = nullptr;
		PendingDialogueOption = FNUQuestInteractionOption();
		UE_LOG(LogNUQuest, Error, TEXT("Dialogue: 퀘스트 단계 대화를 열 수 없습니다. QuestId=%s Phase=%s"),
			*QuestOption.QuestId.ToString(),
			*UEnum::GetValueAsString(QuestOption.Phase));
		// 설정된 대화를 읽거나 표시하지 못한 경우 목표와 보상을 진행시키지 않는다.
		return;
	}

	LastDialogueSource = SourceActor;
	RefreshInteractionIndicator();
}

void ANUPlayerController::ClientOpenStandaloneDialogue_Implementation(UNUDialogueDefinition* DialogueDefinition)
{
	if (!IsLocalController() || !IsValid(DialogueDefinition) || IsDialogueOpen())
	{
		return;
	}

	CloseQuestJournal();
	PendingDialogueSource = nullptr;
	PendingDialogueOption = FNUQuestInteractionOption();

	UNUUIManager* UI = UNUUIManager::Get(this);
	UNUDialogueUIController* DialogueUI = UI != nullptr ? UI->GetController<UNUDialogueUIController>() : nullptr;
	if (DialogueUI == nullptr || !DialogueUI->OpenDialogue(DialogueDefinition, FOnNUDialogueSessionClosed::CreateUObject(this, &ThisClass::HandleDialogueClosed)))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("Dialogue: 일반 대화를 열 수 없습니다. Dialogue=%s"), *GetNameSafe(DialogueDefinition));
		return;
	}

	RefreshInteractionIndicator();
}

void ANUPlayerController::ClientOpenInteractionConversation_Implementation(UNUDialogueDefinition* OpeningDefinition, AActor* SourceActor, const TArray<FNUInteractionOption>& Options)
{
	if (!IsLocalController() || !IsValid(SourceActor) || Options.IsEmpty() || IsDialogueOpen())
	{
		return;
	}

	CloseQuestJournal();
	PendingDialogueSource = nullptr;
	PendingDialogueOption = FNUQuestInteractionOption();

	UNUUIManager* UI = UNUUIManager::Get(this);
	UNUDialogueUIController* DialogueUI = UI != nullptr ? UI->GetController<UNUDialogueUIController>() : nullptr;
	PendingInteractionSource = SourceActor;
	if (DialogueUI == nullptr || !DialogueUI->OpenInteractionConversation(
		OpeningDefinition,
		Options,
		FOnNUInteractionConversationOptionSelected::CreateUObject(this, &ThisClass::HandleInteractionConversationOptionSelected),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleInteractionConversationCancelled)))
	{
		PendingInteractionSource = nullptr;
		PendingDialogueSource = nullptr;
		PendingDialogueOption = FNUQuestInteractionOption();
		UE_LOG(LogNUQuest, Warning, TEXT("Interaction: 기존 대화창 위쪽의 선택지를 열 수 없습니다. Actor=%s"), *GetNameSafe(SourceActor));
		return;
	}

	LastDialogueSource = SourceActor;
	RefreshInteractionIndicator();
}

void ANUPlayerController::ClientOpenShop_Implementation(int32 ShopId, AActor* SourceActor)
{
	if (!IsLocalController() || ShopId <= 0)
	{
		return;
	}

	LastDialogueSource = SourceActor;
	ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>();
	UNUInventoryComponent* InventoryComponent = NUPlayerState != nullptr ? NUPlayerState->GetInventoryComponent() : nullptr;
	if (InventoryComponent != nullptr)
	{
		InventoryComponent->OpenShopUI(ShopId);
	}
}

void ANUPlayerController::ClientOpenStorage_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>();
	UNUInventoryComponent* InventoryComponent = NUPlayerState != nullptr ? NUPlayerState->GetInventoryComponent() : nullptr;
	if (InventoryComponent != nullptr && !InventoryComponent->IsStorageOpen())
	{
		InventoryComponent->ToggleStorageUI();
	}
}

bool ANUPlayerController::IsDialogueOpen() const
{
	UNUUIManager* UI = UNUUIManager::Get(this);
	const UNUDialogueUIController* DialogueUI = UI != nullptr
		? UI->GetController<UNUDialogueUIController>()
		: nullptr;
	return DialogueUI != nullptr && DialogueUI->IsDialogueOpen();
}

void ANUPlayerController::AdvanceDialogue()
{
	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		UI->GetController<UNUDialogueUIController>()->AdvanceDialogue();
	}
}

void ANUPlayerController::CloseDialogue(bool bCompleted)
{
	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		UI->GetController<UNUDialogueUIController>()->CloseDialogue(bCompleted);
	}
}

void ANUPlayerController::HandleDialogueClosed(bool bCompleted)
{
	AActor* FinishedSource = PendingDialogueSource.Get();
	const FNUQuestInteractionOption FinishedOption = PendingDialogueOption;

	PendingDialogueSource = nullptr;
	PendingDialogueOption = FNUQuestInteractionOption();

	RefreshInteractionIndicator();

	if (bCompleted && IsValid(FinishedSource))
	{
		ServerFinishDialogue(FinishedSource, FinishedOption);
	}
}

void ANUPlayerController::HandleInteractionConversationOptionSelected(FNUInteractionOptionHandle Handle)
{
	AActor* SourceActor = PendingInteractionSource.Get();
	PendingInteractionSource = nullptr;
	RefreshInteractionIndicator();

	if (IsValid(SourceActor) && Handle.IsValid())
	{
		UNUInteractionRouterComponent* Router = SourceActor->FindComponentByClass<UNUInteractionRouterComponent>();
		FNUInteractionContext Context;
		Context.Interactor = this;
		Context.Target = SourceActor;
		TArray<FNUInteractionOption> Options;
		if (Router) { Router->CollectInteractionOptions(Context, Options); }
		const FNUInteractionOption* Selected = Options.FindByPredicate([&Handle](const FNUInteractionOption& Option) { return Option.Handle == Handle; });
		if (Selected && Selected->ExecutionDialogue)
		{
			UNUDialogueDefinition* Dialogue = NewObject<UNUDialogueDefinition>(this);
			const UNUDialogueDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUDialogueDataSubsystem>();
			if (Data == nullptr || !Data->GetDefinitionLines(Selected->ExecutionDialogue, Dialogue->Lines))
			{
				UE_LOG(LogNUQuest, Error, TEXT("상호작용 안내 대사를 읽을 수 없습니다: %s"), *GetNameSafe(Selected->ExecutionDialogue));
				return;
			}
			if (const UNURecoveryProviderComponent* Recovery = SourceActor->FindComponentByClass<UNURecoveryProviderComponent>())
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Cost"), FText::AsNumber(Recovery->GetRecoveryCost(this)));
				for (FNUDialogueLine& Line : Dialogue->Lines) { Line.DialogueText = FText::Format(Line.DialogueText, Arguments); }
			}
			UNUUIManager* UI = UNUUIManager::Get(this);
			UNUDialogueUIController* DialogueUI = UI ? UI->GetController<UNUDialogueUIController>() : nullptr;
			if (DialogueUI) { DialogueUI->OpenDialogue(Dialogue, FOnNUDialogueSessionClosed::CreateUObject(this, &ThisClass::HandleInteractionActionDialogueClosed, TWeakObjectPtr<AActor>(SourceActor), Handle)); }
			return;
		}
		ServerExecuteInteractionOption(SourceActor, Handle);
	}
}

void ANUPlayerController::HandleInteractionActionDialogueClosed(bool bCompleted, TWeakObjectPtr<AActor> SourceActor, FNUInteractionOptionHandle Handle)
{
	RefreshInteractionIndicator();
	if (bCompleted && SourceActor.IsValid()) { ServerExecuteInteractionOption(SourceActor.Get(), Handle); }
}

void ANUPlayerController::HandleInteractionConversationCancelled()
{
	PendingInteractionSource = nullptr;
	PendingDialogueSource = nullptr;
	PendingDialogueOption = FNUQuestInteractionOption();
	RefreshInteractionIndicator();
}

void ANUPlayerController::ServerFinishDialogue_Implementation(AActor* SourceActor, const FNUQuestInteractionOption& QuestOption)
{
	// 클라이언트가 보낸 대상이 여전히 현재 상호작용 대상인지 서버에서 다시 확인
	AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(GetPawn());
	if (!IsValid(FieldCharacter)
		|| !IsValid(FieldCharacter->InteractionComponent)
		|| !FieldCharacter->InteractionComponent->RevalidateInteractionTarget(SourceActor))
	{
		return;
	}

	UNUQuestProviderComponent* QuestProvider = IsValid(SourceActor)
		? SourceActor->FindComponentByClass<UNUQuestProviderComponent>()
		: nullptr;
	if (IsValid(QuestProvider))
	{
		// 일반 대화는 퀘스트 처리를 건너뛰고, 퀘스트 대화만 선택 옵션을 재검증한다.
		if (!QuestOption.QuestId.IsNone()
			&& !QuestProvider->IsInteractionOptionValid(FieldCharacter, QuestOption))
		{
			return;
		}

		if (!QuestOption.QuestId.IsNone())
		{
			QuestProvider->HandleDialogueFinished(FieldCharacter, QuestOption);
		}

		// 최초 직업 선택은 JobProvider 선택지를 실행한 경우에만 연다.
		// 직업 선택을 끝낸 뒤 같은 NPC에서 이어지는 시작 나이아 선택 흐름만 유지한다.
		ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>();
		UNUJobComponent* JobComponent = NUPlayerState != nullptr
			? NUPlayerState->GetJobComponent()
			: nullptr;
		if (JobComponent != nullptr && JobComponent->HasJob() && JobComponent->IsInitialJobSelectionNpc(QuestProvider->QuestActorId))
		{
			TryAuthorizeStarterNaiaSelection();
		}
	}
}

void ANUPlayerController::ServerExecuteInteractionOption_Implementation(AActor* SourceActor, const FNUInteractionOptionHandle& Handle)
{
	AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(GetPawn());
	if (!IsValid(FieldCharacter)
		|| !IsValid(FieldCharacter->InteractionComponent)
		|| !FieldCharacter->InteractionComponent->RevalidateInteractionTarget(SourceActor))
	{
		return;
	}

	UNUInteractionRouterComponent* Router = SourceActor->FindComponentByClass<UNUInteractionRouterComponent>();
	if (!IsValid(Router))
	{
		return;
	}

	FNUInteractionContext Context;
	Context.Interactor = FieldCharacter;
	Context.Target = SourceActor;
	if (!Router->ExecuteInteractionOption(Context, Handle))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("Interaction: 서버 재검증에서 선택지 실행을 거부했습니다. Actor=%s Type=%s Id=%s"), *GetNameSafe(SourceActor), *Handle.OptionType.ToString(), *Handle.OptionId.ToString());
	}
}

void ANUPlayerController::ClientBeginInitialJobSelection_Implementation()
{
	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		// 실제 창 생성과 중복 열기 방지는 기능 UIController가 담당
		UI->GetController<UNUProgressionUIController>()->OpenJobSelection(FSimpleDelegate::CreateUObject(this, &ThisClass::HandleInitialJobSelectionCompleted));
	}
}

void ANUPlayerController::ClientBeginStarterNaiaSelection_Implementation()
{
	if (!IsLocalController() || IsDialogueOpen() || bWaitingForStarterNaiaGathering)
	{
		return;
	}

	ANUPlayerState* State = GetPlayerState<ANUPlayerState>();
	if (State == nullptr || GetPawn() == nullptr)
	{
		return;
	}
	StarterNaiaGatheringPlayerState = State;
	const UNUQuestComponent* Quest = State->GetQuestComponent();
	StarterNaiaGatheringDebugSerial = Quest != nullptr ? Quest->GetDebugCompletionSerial() : 0;
	bWaitingForStarterNaiaGathering = true;
	CloseQuestJournal();
	RequestGameInputBlock(TEXT("StarterNaiaGathering"), true, this);
	// 직업 변경 알림을 받는 모든 배치 컴포넌트가 갱신된 다음에 이동 여부를 확인한다.
	StarterNaiaGatheringTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::WaitForStarterNaiaGathering);
}

void ANUPlayerController::WaitForStarterNaiaGathering()
{
	StarterNaiaGatheringTimer.Invalidate();
	if (!bWaitingForStarterNaiaGathering)
	{
		return;
	}
	ANUPlayerState* State = GetPlayerState<ANUPlayerState>();
	const UNUJobComponent* Job = State != nullptr ? State->GetJobComponent() : nullptr;
	const UNUNaiaRosterComponent* Roster = State != nullptr ? State->GetNaiaRosterComponent() : nullptr;
	const UNUQuestComponent* Quest = State != nullptr ? State->GetQuestComponent() : nullptr;
	if (!IsLocalController() || GetPawn() == nullptr || State != StarterNaiaGatheringPlayerState.Get()
		|| Job == nullptr || !Job->HasJob() || Roster == nullptr || Roster->HasSelectedStarterNaia()
		|| (Quest != nullptr && Quest->GetDebugCompletionSerial() != StarterNaiaGatheringDebugSerial)
		|| IsDialogueOpen())
	{
		CancelStarterNaiaGathering();
		return;
	}

	// 재배치와 화면 복귀가 모두 끝난 뒤 계약 소개를 연다.
	if (const UNUActorPresenceTransitionSubsystem* Placement = GetWorld()->GetSubsystem<UNUActorPresenceTransitionSubsystem>();
		Placement != nullptr && Placement->IsTransitionPending())
	{
		GetWorldTimerManager().SetTimer(StarterNaiaGatheringTimer, this, &ThisClass::WaitForStarterNaiaGathering, 0.1f, false);
		return;
	}
	CancelStarterNaiaGathering();
	BeginStarterNaiaSelectionPresentation();
}

void ANUPlayerController::CancelStarterNaiaGathering()
{
	if (!bWaitingForStarterNaiaGathering)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(StarterNaiaGatheringTimer);
	StarterNaiaGatheringPlayerState.Reset();
	bWaitingForStarterNaiaGathering = false;
	RequestGameInputBlock(TEXT("StarterNaiaGathering"), false, this);
}

void ANUPlayerController::BeginStarterNaiaSelectionPresentation()
{
	if (!IsLocalController() || IsDialogueOpen())
	{
		return;
	}

	const UNUDialogueDataSubsystem* DialogueData = GetGameInstance()->GetSubsystem<UNUDialogueDataSubsystem>();
	TArray<FNUDialogueLine> Lines;
	if (DialogueData != nullptr && DialogueData->GetLines(DialogueData->FindEventDialogue(TEXT("StarterNaiaIntroduction")), Lines))
	{
		CloseQuestJournal();
		UNUUIManager* UI = UNUUIManager::Get(this);
		UNUDialogueUIController* DialogueUI = UI != nullptr ? UI->GetController<UNUDialogueUIController>() : nullptr;
		if (DialogueUI == nullptr || !DialogueUI->OpenDialogueLines(Lines, FOnNUDialogueSessionClosed::CreateUObject(this, &ThisClass::HandleStarterNaiaSelectionDialogueClosed)))
		{
			UE_LOG(LogNUQuest, Error, TEXT("계약 선택 전 테이블 대화를 열 수 없습니다."));
		}
		return;
	}

	const UNUQuestDataSettings* Settings = GetDefault<UNUQuestDataSettings>();
	UNUDialogueDefinition* DialogueDefinition = Settings != nullptr
		? Settings->StarterNaiaSelectionDialogue.LoadSynchronous()
		: nullptr;
	if (!IsValid(DialogueDefinition))
	{
		UE_LOG(LogNUQuest, Error, TEXT("Q9 계약 나이아 선택 전 대화 데이터가 설정되지 않았습니다"));
		OpenStarterNaiaSelectionUI();
		return;
	}

	CloseQuestJournal();
	UNUUIManager* UI = UNUUIManager::Get(this);
	UNUDialogueUIController* DialogueUI = UI != nullptr ? UI->GetController<UNUDialogueUIController>() : nullptr;
	if (DialogueUI == nullptr || !DialogueUI->OpenDialogue(
		DialogueDefinition,
		FOnNUDialogueSessionClosed::CreateUObject(this, &ThisClass::HandleStarterNaiaSelectionDialogueClosed)))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("Q9 계약 나이아 선택 전 대화를 열 수 없습니다. Dialogue=%s"), *GetNameSafe(DialogueDefinition));
		OpenStarterNaiaSelectionUI();
	}
}

void ANUPlayerController::ServerAuthorizeStarterNaiaSelection_Implementation()
{
	TryAuthorizeStarterNaiaSelection();
}

bool ANUPlayerController::TryAuthorizeStarterNaiaSelection()
{
	ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>();
	const UNUJobComponent* JobComponent = NUPlayerState != nullptr ? NUPlayerState->GetJobComponent() : nullptr;
	UNUNaiaRosterComponent* RosterComponent = NUPlayerState != nullptr ? NUPlayerState->GetNaiaRosterComponent() : nullptr;
	if (JobComponent == nullptr || !JobComponent->HasJob() || RosterComponent == nullptr || !RosterComponent->AuthorizeStarterNaiaSelection())
	{
		return false;
	}

	ClientBeginStarterNaiaSelection();
	return true;
}

void ANUPlayerController::HandleInitialJobSelectionCompleted()
{
	ServerAuthorizeStarterNaiaSelection();
}

void ANUPlayerController::HandleStarterNaiaSelectionDialogueClosed(bool bCompleted)
{
	if (!bCompleted)
	{
		return;
	}

	OpenStarterNaiaSelectionUI();
}

void ANUPlayerController::OpenStarterNaiaSelectionUI()
{
	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		UI->GetController<UNUStarterNaiaSelectionUIController>()->OpenSelection();
	}
}

bool ANUPlayerController::OpenDialogueEvent(FName EventId)
{
	if (!IsLocalController() || IsDialogueOpen()) { return false; }
	const UNUDialogueDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUDialogueDataSubsystem>();
	TArray<FNUDialogueLine> Lines;
	if (Data == nullptr || !Data->GetLines(Data->FindEventDialogue(EventId), Lines)) { return false; }
	UNUUIManager* UI = UNUUIManager::Get(this);
	return UI != nullptr && UI->GetController<UNUDialogueUIController>()->OpenDialogueLines(Lines, FOnNUDialogueSessionClosed());
}

void ANUPlayerController::BindInteractionComponent(APawn* InPawn)
{
	UnbindInteractionComponent();

	const AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(InPawn);
	if (!IsValid(FieldCharacter) || !IsValid(FieldCharacter->InteractionComponent))
	{
		RefreshInteractionIndicator();
		return;
	}

	BoundInteractionComponent = FieldCharacter->InteractionComponent;
	BoundInteractionComponent->OnCurrentInteractableChanged.AddDynamic(this, &ThisClass::HandleCurrentInteractableChanged);

	RefreshInteractionIndicator();
}

void ANUPlayerController::UnbindInteractionComponent()
{
	if (IsValid(BoundInteractionComponent))
	{
		BoundInteractionComponent->OnCurrentInteractableChanged.RemoveDynamic(this, &ThisClass::HandleCurrentInteractableChanged);
	}

	BoundInteractionComponent = nullptr;
	RefreshInteractionIndicator();
}

void ANUPlayerController::RefreshInteractionIndicator()
{
	if (!IsValid(InteractionIndicatorWidget))
	{
		return;
	}

	const bool bShouldShow =
		!IsValid(ViewedEncounter)
		&& !IsGameInputBlocked()
		&& IsValid(BoundInteractionComponent)
		&& BoundInteractionComponent->HasCurrentInteractable();

	if (!bShouldShow)
	{
		InteractionIndicatorWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UpdateInteractionIndicatorPosition();
}

void ANUPlayerController::UpdateInteractionIndicatorPosition()
{
	if (!IsValid(InteractionIndicatorWidget))
	{
		return;
	}

	if (IsValid(ViewedEncounter) || IsGameInputBlocked() || !IsValid(BoundInteractionComponent))
	{
		InteractionIndicatorWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	AActor* CurrentInteractable = BoundInteractionComponent->GetCurrentInteractable();
	if (!IsValid(CurrentInteractable))
	{
		InteractionIndicatorWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FVector BoundsOrigin;
	FVector BoundsExtent;
	CurrentInteractable->GetActorBounds(true, BoundsOrigin, BoundsExtent);

	FVector IndicatorWorldLocation =
		BoundsOrigin
		+ FVector(0.0f, 0.0f, BoundsExtent.Z  + InteractionIndicatorWorldPadding);
	bool bDocked = false;

	if (Cast<ANUNPCBase>(CurrentInteractable) != nullptr && IsValid(PlayerCameraManager))
	{
		const UNUWorldMarkerComponent* Marker = CurrentInteractable->FindComponentByClass<UNUWorldMarkerComponent>();
		if (IsValid(Marker) && Marker->IsVisible() && IsValid(Marker->GetUserWidgetObject()))
		{
			const FRotator CameraRotation = PlayerCameraManager->GetCameraRotation();
			const FVector CameraUp = CameraRotation.RotateVector(FVector::UpVector);
			const double Height = Marker->GetDrawSize().Y * FMath::Abs(Marker->GetComponentScale().Z);
			if (!InteractionIndicatorMarkerAnchor.IsNearlyZero())
			{
				// 표시 판 가운데 → 오른쪽 아래 모서리. 키 칩이 배지 앞(화면 UI)에 겹쳐 붙는다
				const FVector CameraRight = CameraRotation.RotateVector(FVector::RightVector);
				const double Width = Marker->GetDrawSize().X * FMath::Abs(Marker->GetComponentScale().Y);
				const FVector Centre = Marker->GetComponentLocation() + CameraUp * Height * (Marker->GetPivot().Y - 0.5f);
				IndicatorWorldLocation = Centre + CameraRight * Width * InteractionIndicatorMarkerAnchor.X
					- CameraUp * Height * InteractionIndicatorMarkerAnchor.Y;
				bDocked = true;
			}
			else
			{
				IndicatorWorldLocation = Marker->GetComponentLocation() + CameraUp * Height * (Marker->GetPivot().Y - 0.125f);
			}
		}
	}
	// 붙일 때는 키 칩 가운데를 그 점에, 아니면(물건·표시 없는 NPC) 칩 아래를 대상 위에 둔다
	InteractionIndicatorWidget->SetAlignmentInViewport(bDocked ? FVector2D(0.5f, 0.5f) : FVector2D(0.5f, 1.0f));

	FVector2D WidgetPosition;
	const bool bProjected =
		UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			this,
			IndicatorWorldLocation,
			WidgetPosition,
			true
		);

	if (!bProjected)
	{
		InteractionIndicatorWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	InteractionIndicatorWidget->SetPositionInViewport(WidgetPosition, false);
	InteractionIndicatorWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ANUPlayerController::HandleCurrentInteractableChanged(AActor* PreviousInteractable, AActor* CurrentInteractable)
{
	RefreshInteractionIndicator();
}

void ANUPlayerController::PlayNetBattleTransition(ULevelSequence* Sequence)
{
	PlayTransitionSequence(Sequence);
}

void ANUPlayerController::StopNetBattleTransition()
{
	StopTransitionSequence();
}

void ANUPlayerController::PlayTransitionSequence(ULevelSequence* Sequence)
{
	if (!IsLocalController() || !Sequence)
	{
		return;
	}
	
	StopTransitionSequence();
	
	ALevelSequenceActor* SequenceActor = nullptr;
	TransitionPlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		this,
		Sequence, 
		FMovieSceneSequencePlaybackSettings(), 
		SequenceActor
		);
	
	TransitionSequenceActor = SequenceActor;
	
	if (IsValid(TransitionPlayer))
	{
		TransitionPlayer->Play();
	}
}

void ANUPlayerController::StopTransitionSequence()
{
	if (IsValid(TransitionPlayer))
	{
		TransitionPlayer->Stop();
	}
	
	if (IsValid(TransitionSequenceActor))
	{
		TransitionSequenceActor->Destroy();
	}
	
	TransitionPlayer = nullptr;
	TransitionSequenceActor = nullptr;
	
	if (IsValid(PlayerCameraManager))
	{
		if (bRecoveryTransitionActive)
		{
			// 시퀀스 정리가 복원한 화면을 노출하지 않고 부활 쪽으로 암전 유지를 넘긴다.
			PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false);
		}
		else
		{
			PlayerCameraManager->StopCameraFade();
		}
	}
}

void ANUPlayerController::SubmitBattleCommandFromComponent(FGuid EncounterId,
                                                           const FNUBattleCommandRequest& Request)
{
	if (!IsLocalController())
	{
		return;
	}

	ServerSubmitBattleCommand(EncounterId, Request);
}

void ANUPlayerController::ClientOpenParryWindow_Implementation(int32 ActionSequence, float ImpactOffset,
	const FNUParryTimingSpec& Spec, int32 SPCost)
{
	if (!IsValid(ParryComponent))
	{
		return;
	}

	// SP는 서버가 상태이상 보정까지 마친 값이라 그대로 표시하면 됨
	ParryComponent->OpenWindow(ActionSequence, ImpactOffset, Spec, SPCost);
}

void ANUPlayerController::CloseParryWindow()
{
	if (IsValid(ParryComponent))
	{
		ParryComponent->CloseWindow();
	}
}

void ANUPlayerController::RouteParryPress(int32 ForActionSequence, float PressOffset)
{
	UWorld* World = GetWorld();
	UNUBattleNetActionQueueSubsystem* Queue = IsValid(World)
		? World->GetSubsystem<UNUBattleNetActionQueueSubsystem>()
		: nullptr;

	// 넷 전투의 창이 열려 있으면 클라가 판정해서 서버에 보고함
	// 로컬 전투는 창을 인카운터가 열었으므로 이 검사에 걸리지 않음
	if (IsValid(Queue) && Queue->IsReactionWindowOpen())
	{
		Queue->HandleReactionPress(PressOffset);
		return;
	}

	ServerTryParry(ForActionSequence, PressOffset);
}

bool ANUPlayerController::ShowBattleResult(const FNUBattleResultView& InView, FSimpleDelegate OnDismissed)
{
	if (!IsLocalController() || !BattleResultWidgetClass)
	{
		// 위젯 클래스가 없으면 창을 못 띄우므로 false 리턴
		UE_LOG(LogTemp, Warning, TEXT("전투 결과창 클래스가 지정되지 않아 결과창을 건너뜁니다"));
		return false;
	}

	// 앞 전투의 창이 남아 있으면 지우고 새로 만든다
	CloseBattleResult();

	BattleResultWidget = CreateWidget<UNUBattleResultWidget>(this, BattleResultWidgetClass);
	if (!IsValid(BattleResultWidget))
	{
		return false;
	}

	BattleResultWidget->OnDismissed = OnDismissed;
	BattleResultWidget->AddToViewport(BattleResultZOrder);
	BattleResultWidget->ShowResult(InView);

	// 확인 버튼을 누를 수 있어야 하고, 키 입력도 이 창이 직접 받으므로 포커스를 줘야함
	RequestCursor(CursorReason_BattleResult, true, BattleResultWidget);
	BattleResultWidget->SetKeyboardFocus();

	return true;
}

void ANUPlayerController::CloseBattleResult()
{
	RequestCursor(CursorReason_BattleResult, false, BattleResultWidget);

	if (!IsValid(BattleResultWidget))
	{
		return;
	}

	// 정리하면서 델리게이트가 다시 불리지 않게 먼저 끊기
	BattleResultWidget->OnDismissed.Unbind();
	BattleResultWidget->RemoveFromParent();
	BattleResultWidget = nullptr;
}

void ANUPlayerController::RouteDodgePress(int32 ForActionSequence)
{
	UWorld* World = GetWorld();
	UNUBattleNetActionQueueSubsystem* Queue = IsValid(World)
		? World->GetSubsystem<UNUBattleNetActionQueueSubsystem>()
		: nullptr;

	// 넷 전투의 창이 열려 있으면 클라가 굴려서 서버에 보고함
	// 로컬 전투는 창을 인카운터가 열었으므로 이 검사에 걸리지 않음
	if (IsValid(Queue) && Queue->IsReactionWindowOpen())
	{
		Queue->HandleDodgePress();
		return;
	}

	ServerTryDodge(ForActionSequence);
}

void ANUPlayerController::ClientOpenDodgeWindow_Implementation(int32 ActionSequence, int32 SPCost,
	float WindowDuration)
{
	DodgeWindowActionSequence = ActionSequence;
	DodgeWindowSPCost = SPCost;

	if (IsValid(ParryCueWidget))
	{
		ParryCueWidget->SetDodgePrompt(true, SPCost);
	}

	// 안 누르고 지나가도 창이 남지 않게 타격 시점에 스스로 닫음
	GetWorldTimerManager().ClearTimer(DodgeWindowTimer);

	if (WindowDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			DodgeWindowTimer,
			this,
			&ThisClass::CloseDodgeWindow,
			WindowDuration,
			false
			);
	}
}

void ANUPlayerController::CloseDodgeWindow()
{
	DodgeWindowActionSequence = INDEX_NONE;
	DodgeWindowSPCost = 0;

	GetWorldTimerManager().ClearTimer(DodgeWindowTimer);

	if (IsValid(ParryCueWidget))
	{
		ParryCueWidget->SetDodgePrompt(false, 0);
	}
}

void ANUPlayerController::RequestCursor(FName Reason, bool bWant, const UObject* InOwner, bool bForce)
{
	if (Reason.IsNone())
	{
		return;
	}
	
	if (bWant)
	{
		CursorRequesters.Add(Reason, InOwner);
	}
	else
	{
		CursorRequesters.Remove(Reason);
	}
	
	ApplyCursorState(bForce);
}

void ANUPlayerController::RequestGameInputBlock(FName Reason, bool bBlock, const UObject* InOwner)
{
	if (!IsLocalController() || Reason.IsNone())
	{
		return;
	}

	if (bBlock)
	{
		GameInputBlockRequesters.Add(Reason, InOwner);
	}
	else
	{
		GameInputBlockRequesters.Remove(Reason);
	}

	ApplyGameInputBlockState();
}


// 현재 밝기에서 목표 밝기로 페이드한다.
void ANUPlayerController::SetWorldTransitionFade(float Amount, float Duration)
{
	if (!IsLocalController() || !IsValid(PlayerCameraManager))
	{
		return;
	}
	const float Target = FMath::Clamp(Amount, 0.0f, 1.0f);
	const float Current = PlayerCameraManager->bEnableFading ? PlayerCameraManager->FadeAmount : 0.0f;
	PlayerCameraManager->StartCameraFade(Current, Target, FMath::Max(0.0f, Duration), FLinearColor::Black, false, true);
	if (Duration <= 0.0f)
	{
		// 즉시 반영해 텔레포트 순간 화면이 노출되지 않게 한다.
		PlayerCameraManager->SetManualCameraFade(Target, FLinearColor::Black, false);
	}
}

void ANUPlayerController::ClearWorldTransitionFade()
{
	StopWorldTransitionSequence();
	if (IsLocalController() && IsValid(PlayerCameraManager))
	{
		SetWorldTransitionFade(0.0f, 0.0f);
		PlayerCameraManager->StopCameraFade();
	}
}


bool ANUPlayerController::PlayWorldTransitionSequence(ULevelSequence* Sequence, FSimpleDelegate OnFinished, FSimpleDelegate OnInterrupted)
{
	StopWorldTransitionSequence();
	if (!IsLocalController() || !IsValid(Sequence) || !IsValid(PlayerCameraManager))
	{
		return false;
	}

	// 카메라 컷을 끄고 시퀀스 종료 밝기를 유지한다.
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableCameraCuts = true;
	Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceKeepState;
	ALevelSequenceActor* SequenceActor = nullptr;
	WorldTransitionPlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, SequenceActor);
	WorldTransitionSequenceActor = SequenceActor;
	if (!IsValid(WorldTransitionPlayer) || WorldTransitionPlayer->GetDuration().AsSeconds() <= 0.0)
	{
		StopWorldTransitionSequence();
		return false;
	}

	// 완료와 중단을 구분해 이동 또는 복구로 이어간다.
	WorldTransitionSequenceFinished = MoveTemp(OnFinished);
	WorldTransitionSequenceInterrupted = MoveTemp(OnInterrupted);
	WorldTransitionPlayer->OnFinished.AddDynamic(this, &ThisClass::HandleWorldTransitionSequenceFinished);
	WorldTransitionPlayer->OnStop.AddDynamic(this, &ThisClass::HandleWorldTransitionSequenceStopped);
	WorldTransitionPlayer->OnPause.AddDynamic(this, &ThisClass::HandleWorldTransitionSequenceStopped);
	WorldTransitionPlayer->Play();
	return true;
}

void ANUPlayerController::StopWorldTransitionSequence()
{
	// 정리 중 중단 콜백이 실행되지 않도록 먼저 해제한다.
	GetWorldTimerManager().ClearTimer(WorldTransitionSequenceStopTimer);
	WorldTransitionSequenceFinished.Unbind();
	WorldTransitionSequenceInterrupted.Unbind();
	if (IsValid(WorldTransitionPlayer))
	{
		WorldTransitionPlayer->OnFinished.RemoveAll(this);
		WorldTransitionPlayer->OnStop.RemoveAll(this);
		WorldTransitionPlayer->OnPause.RemoveAll(this);
		WorldTransitionPlayer->Stop();
	}

	if (IsValid(WorldTransitionSequenceActor))
	{
		WorldTransitionSequenceActor->Destroy();
	}
	WorldTransitionPlayer = nullptr;
	WorldTransitionSequenceActor = nullptr;
}


void ANUPlayerController::HandleWorldTransitionSequenceFinished()
{
	// 다음 시퀀스 재생 전에 기존 상태를 정리한다.
	FSimpleDelegate Callback = MoveTemp(WorldTransitionSequenceFinished);
	StopWorldTransitionSequence();
	Callback.ExecuteIfBound();
}

void ANUPlayerController::HandleWorldTransitionSequenceStopped()
{
	// OnStop 후 OnFinished가 오면 타이머를 취소해 정상 종료로 처리한다.
	GetWorldTimerManager().ClearTimer(WorldTransitionSequenceStopTimer);
	WorldTransitionSequenceStopTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::HandleWorldTransitionSequenceInterrupted);
}

void ANUPlayerController::HandleWorldTransitionSequenceInterrupted()
{
	FSimpleDelegate Callback = MoveTemp(WorldTransitionSequenceInterrupted);
	StopWorldTransitionSequence();
	Callback.ExecuteIfBound();
}


void ANUPlayerController::SetWorldClickEnabled(bool bEnabled)
{
	bEnableMouseOverEvents = bEnabled;
}

UNUChatWidget* ANUPlayerController::GetChatWidget() const
{
	return ChatWidget;
}

void ANUPlayerController::ToggleCharacterStatsUI()
{
	if (!IsLocalController())
	{
		return;
	}

	UNUUIManager* UI = UNUUIManager::Get(this);
	UNUProgressionUIController* ProgressionUI = UI != nullptr
		? UI->GetController<UNUProgressionUIController>()
		: nullptr;
	if (ProgressionUI == nullptr)
	{
		return;
	}

	if (IsGameInputBlocked() && !ProgressionUI->IsCharacterStatsOpen())
	{
		return;
	}

	ProgressionUI->ToggleCharacterStats();
}

void ANUPlayerController::ToggleProgressionGraphUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		if (UNUProgressionUIController* ProgressionUI = UI->GetController<UNUProgressionUIController>())
		{
			ProgressionUI->ToggleProgressionGraph();
		}
	}
}

void ANUPlayerController::ToggleProgressionDebugUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		if (UNUProgressionUIController* ProgressionUI = UI->GetController<UNUProgressionUIController>())
		{
			ProgressionUI->ToggleProgressionDebug();
		}
	}
}

void ANUPlayerController::ToggleOwnedSkillsUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		if (UNUProgressionUIController* SkillUI = UI->GetController<UNUProgressionUIController>())
		{
			SkillUI->ToggleSkills();
		}
	}
}

void ANUPlayerController::ApplyCursorState(bool bForce)
{
	for (auto It = CursorRequesters.CreateIterator(); It; ++It)
	{
		// 요청한 소유자가 파괴되었는지 확인하고, 파괴되었다면 삭제 
		// 이전에 nullptr로 요청한건 여기서 걸러지지않고 명시적으로 지워야 함
		if (It.Value().IsStale())
		{
			It.RemoveCurrent();
		}
	}
	
	bool bWantCursor = CursorRequesters.Num() > 0;
	// 패드를 쓰는 동안은 창이 열려도 커서를 숨긴다 (입력 모드는 그대로 GameAndUI --- 포커스 이동에 필요)
	bShowMouseCursor = bWantCursor && !bGamepadMode;
	
	// 모드가 그대로면 변경 X
	// GameAndUI 에서 또 GameAndUI 로 변경하면 위젯 포커스가 초기화됨
	if (!bForce && bCursorModeApplied == bWantCursor)
	{
		return;
	}
	bCursorModeApplied = bWantCursor;
	
	if (bWantCursor)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);	// 커서 클릭해도 유지
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ANUPlayerController::ApplyGameInputBlockState()
{
	for (auto It = GameInputBlockRequesters.CreateIterator(); It; ++It)
	{
		if (It.Value().IsStale())
		{
			It.RemoveCurrent();
		}
	}

	const bool bShouldBlock = GameInputBlockRequesters.Num() > 0;
	if (bGameInputBlockApplied == bShouldBlock)
	{
		return;
	}

	// SetIgnoreMoveInput은 카운터 방식이므로 게이트 상태가 바뀔 때 한 번씩 호출
	SetIgnoreMoveInput(bShouldBlock);
	bGameInputBlockApplied = bShouldBlock;
}

void ANUPlayerController::ClientParryResult_Implementation(bool bSuccess)
{
	if (IsValid(ParryComponent))
	{
		ParryComponent->OnParryJudged.Broadcast(bSuccess);
	}
}

void ANUPlayerController::ServerTryParry_Implementation(int32 ForActionSequence, float PressOffset)
{
	// ActionTimeOut이 지금 10.f 라 10초 넘으면 무효
	// 최소한의 범위 검사
	if (PressOffset < 0.f || PressOffset > 10.f)
	{
		return;
	}
	
	ANUPlayerState* NUPlayerState = Cast<ANUPlayerState>(PlayerState);
	UNUAbilitySystemComponent* ASC = IsValid(NUPlayerState) ? NUPlayerState->GetNUASC() : nullptr;
	ANUBattleEncounter* Encounter = IsValid(ASC) ? ASC->GetBattleEncounter() : nullptr;
	if (!IsValid(Encounter))
	{
		return;
	}
	
	Encounter->RegisterParryPress(NUPlayerState, ForActionSequence, PressOffset);
}

void ANUPlayerController::ServerTryDodge_Implementation(int32 ForActionSequence)
{
	ANUPlayerState* NUPlayerState = Cast<ANUPlayerState>(PlayerState);
	UNUAbilitySystemComponent* ASC = IsValid(NUPlayerState) ? NUPlayerState->GetNUASC() : nullptr;
	ANUBattleEncounter* Encounter = IsValid(ASC) ? ASC->GetBattleEncounter() : nullptr;
	if (!IsValid(Encounter))
	{
		return;
	}

	// SP 검사와 차감은 판정 시점(ResolveDodgeWindow)에 서버가 함
	Encounter->RegisterDodgeRequest(NUPlayerState, ForActionSequence);
}

void ANUPlayerController::ServerRequestNaiaSwap_Implementation(int32 ToPartySlot)
{
	ANUPlayerState* NUPlayerState = Cast<ANUPlayerState>(PlayerState);
	UNUAbilitySystemComponent* ASC = IsValid(NUPlayerState) ? NUPlayerState->GetNUASC() : nullptr;
	ANUBattleEncounter* Encounter = IsValid(ASC) ? ASC->GetBattleEncounter() : nullptr;
	if (!IsValid(Encounter))
	{
		return;
	}

	// 페이즈·슬롯·생사 검증은 전부 인카운터가 함
	// 클라이언트는 스왑 요청만
	const ENUNaiaSwapResult Result = Encounter->ApplyNaiaSwap(NUPlayerState, ToPartySlot);

	// 성공하면 복제로 결과가 보이므로 실패할때만 사유를 보여주면 됨
	if (Result != ENUNaiaSwapResult::Success)
	{
		ClientNaiaSwapResult(Result);
	}
}

void ANUPlayerController::ClientNaiaSwapResult_Implementation(ENUNaiaSwapResult Result)
{
	if (IsValid(BattleCommandComponent))
	{
		BattleCommandComponent->HandleNaiaSwapResult(Result);
	}
}

void ANUPlayerController::ToggleNaiaSummon()
{
	if (!IsLocalController())
	{
		return;
	}

	// 입력으로 부른 경우만, 실내 진입 자동 해제 같은 서버 경로는 여길 지나지 않음.
	// 실내에서는 서버가 소환을 받아 주지 않으므로(ANUPlayerState::SetNaiaSummoned) 소환 소리도 내지 않음
	if (const ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>())
	{
		const bool bSummoned = NUPlayerState->IsNaiaSummoned();
		const UNUInteriorParticipantComponent* Participant = IsValid(GetPawn())
			? GetPawn()->FindComponentByClass<UNUInteriorParticipantComponent>() : nullptr;
		if (bSummoned || !IsValid(Participant) || Participant->GetCurrentInterior() == nullptr)
		{
			UNUSFXSubsystem::Play(this, bSummoned ? NU::SFX::Field::NaiaUnsummon : NU::SFX::Field::NaiaSummon);
		}
	}

	ServerToggleNaiaSummon();
}

void ANUPlayerController::ServerToggleNaiaSummon_Implementation()
{
	// 값을 클라가 계산해 보내지 않고 서버가 뒤집음, 복제본과 어긋날 여지를 없앰
	if (ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>())
	{
		NUPlayerState->SetNaiaSummoned(!NUPlayerState->IsNaiaSummoned());
	}
}

void ANUPlayerController::ServerSubmitBattleCommand_Implementation(FGuid EncounterId,
                                                                   const FNUBattleCommandRequest& Request)
{
	ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>();
	ANUBattleEncounter* Encounter = NUPlayerState ? NUPlayerState->GetBattleEncounter() : nullptr;

	if (!IsValid(Encounter)
		|| Encounter->GetEncounterId() != EncounterId)
	{
		ClientBattleCommandResult(
			EncounterId,
			Request.TurnNumber,
			Request.SourceCombatantId,
			ENUBattleCommandSubmitResult::WrongEncounter
			);
		return;
	}

	// 거절 사유를 그대로 흘려보냄, 클라가 OnSubmissionFailed에서 사유를 Broadcast
	ENUBattleCommandSubmitResult Result = Encounter->TrySubmitCommand(this, Request);

	ClientBattleCommandResult(
		EncounterId,
		Request.TurnNumber,
		Request.SourceCombatantId,
		Result
		);
}

void ANUPlayerController::ClientBattleCommandResult_Implementation(FGuid EncounterId,
	int32 TurnNumber, int32 SourceCombatantId, ENUBattleCommandSubmitResult Result)
{
	if (IsValid(BattleCommandComponent))
	{
		BattleCommandComponent->HandleSubmissionResult(EncounterId, TurnNumber, SourceCombatantId, Result);
	}
}


void ANUPlayerController::ClientRecoveryNotice_Implementation(const FText& Message)
{
	if (bRecoveryTransitionActive) { PendingRecoveryNotice = Message; return; }
	if (UNUUIManager* UI = UNUUIManager::Get(this))
	{
		UI->ShowNotice(Message);
	}
}

void ANUPlayerController::ClientRecoveryFade_Implementation(bool bFadeOut)
{
	if (bFadeOut && bRecoveryTransitionActive) { return; }
	GetWorldTimerManager().ClearTimer(RecoveryRevealTimer);
	if (bFadeOut != bRecoveryTransitionActive)
	{
		SetIgnoreMoveInput(bFadeOut);
	}
	bRecoveryTransitionActive = bFadeOut;
	UpdateRecoveryCover();
	if (bFadeOut) { PendingRecoveryNotice = FText::GetEmpty(); OnRecoveryStarted(); }
	else if (!PendingRecoveryNotice.IsEmpty())
	{
		const FText Message = PendingRecoveryNotice;
		PendingRecoveryNotice = FText::GetEmpty();
		ClientRecoveryNotice(Message);
	}
	if (PlayerCameraManager != nullptr)
	{
		const float CurrentAlpha = PlayerCameraManager->bEnableFading ? PlayerCameraManager->FadeAmount : 0.0f;
		PlayerCameraManager->StartCameraFade(CurrentAlpha, bFadeOut ? 1.0f : 0.0f, 0.25f, FLinearColor::Black, false, bFadeOut);
	}
}

void ANUPlayerController::ClientCompleteRecovery_Implementation(FVector Destination)
{
	RecoveryDestination = Destination;
	RecoveryRevealStarted = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(RecoveryRevealTimer, this, &ThisClass::TryRevealRecovery, 0.01f, true);
	TryRevealRecovery();
}

void ANUPlayerController::TryRevealRecovery()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - RecoveryRevealStarted > 15.0)
	{
		ClientRecoveryFade(false);
		ClientRecoveryNotice(NSLOCTEXT("NURecovery", "RevealTimeout", "부활 화면 갱신에 실패했습니다. 상태를 확인해 주세요."));
		return;
	}

	APawn* RecoveryPawn = GetPawn();
	const ANUPlayerState* PS = GetPlayerState<ANUPlayerState>();
	// 성공 RPC와 Pawn/ASC 복제의 수신 순서는 다를 수 있다. 실제 적용 전에는 화면을 열지 않는다.
	if (RecoveryPawn == nullptr || PS == nullptr || PS->GetNUASC() == nullptr || !PS->GetNUASC()->IsAlive()
		|| PS->GetNUASC()->GetBattleEncounter() != nullptr || IsValid(ViewedEncounter)
		|| !RecoveryPawn->GetActorLocation().Equals(RecoveryDestination, 50.0f))
	{
		return;
	}
	if (PlayerCameraManager != nullptr && bRecoveryTransitionActive
		&& (!PlayerCameraManager->bEnableFading || PlayerCameraManager->FadeAmount < 0.99f))
	{
		return;
	}
	// 기존 필드 카메라 설정은 유지하고 순간이동 이전의 위치 지연만 비운다.
	if (USpringArmComponent* Arm = RecoveryPawn->FindComponentByClass<USpringArmComponent>())
	{
		const bool bLocationLag = Arm->bEnableCameraLag;
		const bool bRotationLag = Arm->bEnableCameraRotationLag;
		Arm->bEnableCameraLag = false;
		Arm->bEnableCameraRotationLag = false;
		Arm->TickComponent(0.0f, LEVELTICK_All, nullptr);
		Arm->bEnableCameraLag = bLocationLag;
		Arm->bEnableCameraRotationLag = bRotationLag;
	}
	if (PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->SetGameCameraCutThisFrame();
		PlayerCameraManager->UpdateCamera(0.0f);
	}
	const float WakeSeconds = FMath::Max(0.1f, GetDefault<UNURecoverySettings>()->WakeUpSeconds);
	GetWorldTimerManager().SetTimer(RecoveryRevealTimer, FTimerDelegate::CreateUObject(this, &ThisClass::ClientRecoveryFade, false), WakeSeconds, false);
	OnRecoveryWaking(WakeSeconds);
	if (PlayerCameraManager) { PlayerCameraManager->StartCameraFade(1.0f, 0.0f, WakeSeconds, FLinearColor::Black, false, false); }
}

void ANUPlayerController::ClientHoldRecoveryFade_Implementation()
{
	ClientRecoveryFade_Implementation(true);
	if (PlayerCameraManager) { PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false); }
	if (RecoveryCover) { RecoveryCover->SetRenderOpacity(1.0f); }
}

void ANUPlayerController::UpdateRecoveryCover()
{
	UNUUIManager* UI = UNUUIManager::Get(this);
	if (!UI) { return; }
	if (bRecoveryTransitionActive && !RecoveryCover)
	{
		RecoveryCover = CreateWidget<UNUTransitionLayerWidget>(this);
		if (!RecoveryCover) { return; }
		RecoveryCover->SetRenderOpacity(PlayerCameraManager && PlayerCameraManager->bEnableFading ? PlayerCameraManager->FadeAmount : 0.0f);
		FNUUIWindowParams Params;
		Params.Layer = NUUILayer::Transition;
		Params.bCloseOnEscape = false;
		Params.bRequestCursor = false;
		UI->OpenWindow(RecoveryCover, Params);
	}
	else if (!bRecoveryTransitionActive && RecoveryCover && (!PlayerCameraManager || !PlayerCameraManager->bEnableFading || PlayerCameraManager->FadeAmount <= KINDA_SMALL_NUMBER))
	{
		UI->CloseWindow(RecoveryCover);
		RecoveryCover = nullptr;
	}
}

#if !UE_BUILD_SHIPPING
// PIE는 ESC를 에디터가 재생 중지로 가져가 전체 메뉴를 못 연다 --- 옵션창(nu.Options.Open)과 같은 테스트 경로.
// ESC와 달리 "열린 창 먼저 닫기"는 하지 않는다. 메뉴만 바로 연다
static FAutoConsoleCommandWithWorld GNUGameMenuOpenCommand(
	TEXT("nu.Menu.Open"),
	TEXT("전체 메뉴 열기 (PIE에서 ESC 대신)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		APlayerController* PC = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		if (UNUUIManager* UI = UNUUIManager::Get(PC))
		{
			UI->OpenGameMenu();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[nu.Menu] UI 매니저를 못 찾음 (필드에 들어간 뒤에 쓸 것)"));
		}
	}));
#endif
