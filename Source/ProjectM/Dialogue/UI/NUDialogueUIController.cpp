#include "Dialogue/UI/NUDialogueUIController.h"

#include "Dialogue/UI/NUDialogueWidget.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

#include "GameFramework/PlayerController.h"

bool UNUDialogueUIController::OpenDialogue(UNUDialogueDefinition* Definition, FOnNUDialogueSessionClosed OnClosed)
{
	if (IsDialogueOpen() || !PrepareDialogueWidget())
	{
		return false;
	}

	UNUUIManager* UI = GetManager();
	if (UI == nullptr)
	{
		return false;
	}

	DialogueWidget->OnDialogueClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
	DialogueWidget->OnDialogueClosed.AddDynamic(this, &ThisClass::HandleDialogueClosed);
	DialogueWidget->OnOptionSelected.RemoveDynamic(this, &ThisClass::HandleInteractionOptionSelected);
	OnSessionClosed = MoveTemp(OnClosed);

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Dialogue;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = true;
	Params.bCloseOnSessionEnd = true;
	Params.bOwnsFocus = true; // 대화창이 포커스와 키를 직접 관리한다 (패드 모드에서 매니저가 버튼에 포커스를 주지 않게, 9/15)
	Params.bHideFieldHUD = DialogueWidget->ShouldHideFieldHUD();
	UI->OpenWindow(DialogueWidget, Params, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCloseRequested));

	if (!DialogueWidget->OpenDialogue(Definition))
	{
		OnSessionClosed.Unbind();
		UI->CloseWindow(DialogueWidget);
		return false;
	}
	FocusDialogueInput();
	return true;
}

bool UNUDialogueUIController::OpenDialogueLines(const TArray<FNUDialogueLine>& Lines, FOnNUDialogueSessionClosed OnClosed)
{
	if (Lines.IsEmpty() || IsDialogueOpen() || !PrepareDialogueWidget())
	{
		return false;
	}

	UNUUIManager* UI = GetManager();
	if (UI == nullptr)
	{
		return false;
	}

	DialogueWidget->OnDialogueClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
	DialogueWidget->OnDialogueClosed.AddDynamic(this, &ThisClass::HandleDialogueClosed);
	DialogueWidget->OnOptionSelected.RemoveDynamic(this, &ThisClass::HandleInteractionOptionSelected);
	OnSessionClosed = MoveTemp(OnClosed);

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Dialogue;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = true;
	Params.bCloseOnSessionEnd = true;
	Params.bOwnsFocus = true; // 대화창이 포커스와 키를 직접 관리한다 (패드 모드에서 매니저가 버튼에 포커스를 주지 않게, 9/15)
	Params.bHideFieldHUD = DialogueWidget->ShouldHideFieldHUD();
	UI->OpenWindow(DialogueWidget, Params, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCloseRequested));

	if (!DialogueWidget->OpenDialogueLines(Lines))
	{
		OnSessionClosed.Unbind();
		UI->CloseWindow(DialogueWidget);
		return false;
	}
	FocusDialogueInput();
	return true;
}

bool UNUDialogueUIController::OpenInteractionConversation(UNUDialogueDefinition* OpeningDefinition, const TArray<FNUInteractionOption>& Options, FOnNUInteractionConversationOptionSelected OnSelected, FSimpleDelegate OnCancelled)
{
	if (Options.IsEmpty() || IsDialogueOpen() || !PrepareDialogueWidget())
	{
		return false;
	}

	UNUUIManager* UI = GetManager();
	if (UI == nullptr)
	{
		return false;
	}

	DialogueWidget->OnDialogueClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
	DialogueWidget->OnDialogueClosed.AddDynamic(this, &ThisClass::HandleDialogueClosed);
	DialogueWidget->OnOptionSelected.RemoveDynamic(this, &ThisClass::HandleInteractionOptionSelected);
	DialogueWidget->OnOptionSelected.AddDynamic(this, &ThisClass::HandleInteractionOptionSelected);

	bInteractionConversationOpen = true;
	OnInteractionOptionSelected = MoveTemp(OnSelected);
	OnInteractionCancelled = MoveTemp(OnCancelled);
	OnSessionClosed.Unbind();

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Dialogue;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = true;
	Params.bCloseOnSessionEnd = true;
	Params.bOwnsFocus = true; // 대화창이 포커스와 키를 직접 관리한다 (패드 모드에서 매니저가 버튼에 포커스를 주지 않게, 9/15)
	Params.bHideFieldHUD = DialogueWidget->ShouldHideFieldHUD();
	UI->OpenWindow(DialogueWidget, Params, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCloseRequested));

	// 복수 선택지 질문 대사를 시작하는 순간 기존 대화 WBP의 위쪽에 선택지도 함께 표시합니다.
	if (!DialogueWidget->OpenInteractionConversation(OpeningDefinition, Options))
	{
		FinishInteractionConversation(FNUInteractionOptionHandle(), false);
		return false;
	}
	FocusDialogueInput();
	return true;
}

void UNUDialogueUIController::AdvanceDialogue()
{
	if (DialogueWidget != nullptr)
	{
		DialogueWidget->AdvanceDialogue();
	}
}

void UNUDialogueUIController::CloseDialogue(bool bCompleted)
{
	if (bInteractionConversationOpen)
	{
		FinishInteractionConversation(FNUInteractionOptionHandle(), true);
		return;
	}

	if (DialogueWidget != nullptr && DialogueWidget->IsDialogueOpen())
	{
		DialogueWidget->CloseDialogue(bCompleted);
		return;
	}

	if (UNUUIManager* UI = GetManager(); UI != nullptr && DialogueWidget != nullptr)
	{
		UI->CloseWindow(DialogueWidget);
	}
	FOnNUDialogueSessionClosed Pending = MoveTemp(OnSessionClosed);
	Pending.ExecuteIfBound(bCompleted);
}

bool UNUDialogueUIController::HandleInputKey(const FKey& Key)
{
	return IsDialogueOpen() && DialogueWidget->HandleInputKey(Key);
}

bool UNUDialogueUIController::IsDialogueOpen() const
{
	return DialogueWidget != nullptr && DialogueWidget->IsDialogueOpen();
}

bool UNUDialogueUIController::IsInteractionConversationOpen() const
{
	return bInteractionConversationOpen && IsDialogueOpen();
}

void UNUDialogueUIController::OnPlayerControllerChanged(APlayerController* NewPC)
{
	CloseDialogue(false);
	DialogueWidget = nullptr;
}

void UNUDialogueUIController::OnDeinitialize()
{
	CloseDialogue(false);
	if (DialogueWidget != nullptr)
	{
		DialogueWidget->OnDialogueClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
		DialogueWidget->OnOptionSelected.RemoveDynamic(this, &ThisClass::HandleInteractionOptionSelected);
		DialogueWidget = nullptr;
	}
}

bool UNUDialogueUIController::PrepareDialogueWidget()
{
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUDialogueWidget> WidgetClass = Settings != nullptr
		? Settings->DialogueWidgetClass.LoadSynchronous()
		: nullptr;
	if (WidgetClass == nullptr)
	{
		return false;
	}

	GetOrCreateWidget(DialogueWidget, WidgetClass);
	if (DialogueWidget != nullptr)
	{
		DialogueWidget->OnLinePresented.RemoveAll(this);
		DialogueWidget->OnLinePresented.AddUObject(this, &ThisClass::HandleLinePresented);
	}
	return DialogueWidget != nullptr;
}

void UNUDialogueUIController::HandleLinePresented(FName SourceRowName)
{
	OnLinePresented.Broadcast(SourceRowName);
}

void UNUDialogueUIController::FocusDialogueInput()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !IsValid(DialogueWidget))
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(DialogueWidget->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	DialogueWidget->SetUserFocus(PlayerController);
}

void UNUDialogueUIController::HandleCloseRequested()
{
	CloseDialogue(false);
}

void UNUDialogueUIController::HandleDialogueClosed(bool bCompleted)
{
	if (bInteractionConversationOpen)
	{
		FinishInteractionConversation(FNUInteractionOptionHandle(), true);
		return;
	}

	if (UNUUIManager* UI = GetManager(); UI != nullptr && DialogueWidget != nullptr)
	{
		UI->CloseWindow(DialogueWidget);
	}
	FOnNUDialogueSessionClosed Pending = MoveTemp(OnSessionClosed);
	Pending.ExecuteIfBound(bCompleted);
}

void UNUDialogueUIController::HandleInteractionOptionSelected(FNUInteractionOptionHandle Handle)
{
	FinishInteractionConversation(Handle, false);
}

void UNUDialogueUIController::FinishInteractionConversation(const FNUInteractionOptionHandle& SelectedHandle, bool bNotifyCancelled)
{
	if (!bInteractionConversationOpen)
	{
		return;
	}

	bInteractionConversationOpen = false;
	FOnNUInteractionConversationOptionSelected Selected = MoveTemp(OnInteractionOptionSelected);
	FSimpleDelegate Cancelled = MoveTemp(OnInteractionCancelled);

	if (DialogueWidget != nullptr)
	{
		DialogueWidget->OnDialogueClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
		DialogueWidget->OnOptionSelected.RemoveDynamic(this, &ThisClass::HandleInteractionOptionSelected);
		if (DialogueWidget->IsDialogueOpen())
		{
			DialogueWidget->CloseDialogue(false);
		}
	}

	if (UNUUIManager* UI = GetManager(); UI != nullptr && DialogueWidget != nullptr)
	{
		UI->CloseWindow(DialogueWidget);
	}

	if (SelectedHandle.IsValid())
	{
		Selected.ExecuteIfBound(SelectedHandle);
	}
	else if (bNotifyCancelled)
	{
		Cancelled.ExecuteIfBound();
	}
}
