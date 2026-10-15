#pragma once

#include "CoreMinimal.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Interaction/Data/NUInteractionTypes.h"
#include "UI/Controller/NUUIController.h"
#include "NUDialogueUIController.generated.h"

class UNUDialogueDefinition;
class UNUDialogueWidget;

DECLARE_DELEGATE_OneParam(FOnNUDialogueSessionClosed, bool);
DECLARE_DELEGATE_OneParam(FOnNUInteractionConversationOptionSelected, FNUInteractionOptionHandle);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUDialoguePresentation, FName);

// NPC 대화 위젯을 소유하고 UIManager 창 규칙과 대화 종료 콜백을 연결합니다.
UCLASS()
class PROJECTM_API UNUDialogueUIController : public UNUUIController
{
	GENERATED_BODY()

public:
	FOnNUDialoguePresentation OnLinePresented;
	bool OpenDialogue(UNUDialogueDefinition* Definition, FOnNUDialogueSessionClosed OnClosed);
	bool OpenDialogueLines(const TArray<FNUDialogueLine>& Lines, FOnNUDialogueSessionClosed OnClosed);
	bool OpenInteractionConversation(UNUDialogueDefinition* OpeningDefinition, const TArray<FNUInteractionOption>& Options, FOnNUInteractionConversationOptionSelected OnSelected, FSimpleDelegate OnCancelled = FSimpleDelegate());
	void AdvanceDialogue();
	void CloseDialogue(bool bCompleted = false);
	bool HandleInputKey(const FKey& Key);
	bool IsDialogueOpen() const;
	bool IsInteractionConversationOpen() const;

protected:
	virtual void OnPlayerControllerChanged(APlayerController* NewPC) override;
	virtual void OnDeinitialize() override;

private:
	void HandleLinePresented(FName SourceRowName);
	bool PrepareDialogueWidget();
	void FocusDialogueInput();
	void HandleCloseRequested();
	void FinishInteractionConversation(const FNUInteractionOptionHandle& SelectedHandle, bool bNotifyCancelled);

	UFUNCTION()
	void HandleDialogueClosed(bool bCompleted);

	UFUNCTION()
	void HandleInteractionOptionSelected(FNUInteractionOptionHandle Handle);

	UPROPERTY(Transient)
	TObjectPtr<UNUDialogueWidget> DialogueWidget;

	FOnNUDialogueSessionClosed OnSessionClosed;
	FOnNUInteractionConversationOptionSelected OnInteractionOptionSelected;
	FSimpleDelegate OnInteractionCancelled;
	bool bInteractionConversationOpen = false;
};
