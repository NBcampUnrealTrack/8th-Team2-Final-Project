#include "Dialogue/UI/NUDialogueWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Dialogue/Data/NUDialogueDataSubsystem.h"
#include "Field/NPC/Data/NUNPCDataSubsystem.h"
#include "Interaction/Data/NUInteractionSettings.h"
#include "Interaction/UI/NUInteractionOptionEntryWidget.h"
#include "NUGAS/NUGameplayTags.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/NUUISettings.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUDialogueWidget, Log, All);

namespace NUDialogueWidget
{
	const FName ExitOptionId(TEXT("UI.Exit"));

	FNUInteractionOption MakeExitOption()
	{
		const UNUInteractionSettings* Settings = GetDefault<UNUInteractionSettings>();
		FNUInteractionOption Option;
		Option.Handle.OptionType = NU::Interaction::Option::Dialogue;
		Option.Handle.OptionId = ExitOptionId;
		Option.DisplayText = Settings->ExitOptionText;
		Option.CategoryText = Settings->ExitOptionCategoryText;
		Option.Group = ENUInteractionOptionGroup::System;
		Option.bEnabled = true;
		return Option;
	}

	bool IsExitOption(const FNUInteractionOptionHandle& Handle)
	{
		return Handle.OptionType == NU::Interaction::Option::Dialogue
			&& Handle.OptionId == ExitOptionId;
	}
}

bool UNUDialogueWidget::OpenDialogue(UNUDialogueDefinition* Definition)
{
	const UNUDialogueDataSubsystem* Data = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UNUDialogueDataSubsystem>() : nullptr;
	TArray<FNUDialogueLine> Lines;
	return Data != nullptr && Data->GetDefinitionLines(Definition, Lines) && OpenDialogueInternal(&Lines, nullptr);
}

bool UNUDialogueWidget::OpenDialogueLines(const TArray<FNUDialogueLine>& Lines)
{
	return OpenDialogueInternal(&Lines, nullptr);
}

bool UNUDialogueWidget::OpenInteractionConversation(UNUDialogueDefinition* OpeningDefinition, const TArray<FNUInteractionOption>& Options)
{
	if (!IsValid(OpeningDefinition)) { return OpenDialogueInternal(nullptr, &Options); }
	const UNUDialogueDataSubsystem* Data = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UNUDialogueDataSubsystem>() : nullptr;
	TArray<FNUDialogueLine> Lines;
	return Data != nullptr && Data->GetDefinitionLines(OpeningDefinition, Lines) && OpenDialogueInternal(&Lines, &Options);
}

void UNUDialogueWidget::AdvanceDialogue()
{
	if (!bDialogueOpen || ActiveLines.IsEmpty())
	{
		return;
	}

	if (IsTextRevealing())
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Dialogue::Advance);
		FinishTextReveal();
		return;
	}

	if (CurrentLineIndex + 1 < ActiveLines.Num())
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Dialogue::Advance);
		++CurrentLineIndex;
		RefreshCurrentLine();
		RefreshNavigation();
		return;
	}

	// 선택지는 마지막 대사에서도 유지하고 실제 선택이나 닫기를 기다린다.
	if (!bOptionMode)
	{
		CloseDialogue(true);
	}
}

void UNUDialogueWidget::CloseDialogue(bool bCompleted)
{
	if (!bDialogueOpen)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UNUSFXSubsystem::Play(this, NU::SFX::Dialogue::Close);
	ResetDialogueState();
	SetVisibility(ESlateVisibility::Collapsed);
	OnDialogueClosed.Broadcast(bCompleted);
}

bool UNUDialogueWidget::HandleInputKey(const FKey& Key)
{
	return HandleDialogueKey(Key).IsEventHandled();
}

#if WITH_EDITOR
void UNUDialogueWidget::SetOptionEntryWidgetClass(TSubclassOf<UNUInteractionOptionEntryWidget> InClass)
{
	OptionEntryWidgetClass = InClass;
}
#endif

void UNUDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Collapsed);
	InteractionOptionPanel->SetVisibility(ESlateVisibility::Collapsed);
	DialoguePanelRestVisibility = DialoguePanel->GetVisibility();

	NextButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleNextClicked);
	NextButton->OnClicked.AddDynamic(this, &ThisClass::HandleNextClicked);
	if (IsValid(CloseButton))
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
		CloseButton->OnCloseClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}
}

void UNUDialogueWidget::NativeDestruct()
{
	if (IsValid(NextButton))
	{
		NextButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleNextClicked);
	}
	if (IsValid(CloseButton))
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}

	ResetDialogueState();
	Super::NativeDestruct();
}

void UNUDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (OpenMotion.IsRunning())
	{
		OpenMotion.Advance(InDeltaTime);
	}

	// 문장이 다 나와 기다리는 동안만 다음 표시가 천천히 아래로 내려갔다 돌아온다
	if (IsValid(NextIndicator))
	{
		float Offset = 0.f;
		if (bDialogueOpen && !IsTextRevealing() && NextBobPeriod > KINDA_SMALL_NUMBER)
		{
			NextBobTime = FMath::Fmod(NextBobTime + InDeltaTime, NextBobPeriod);
			Offset = NextBobAmplitude * 0.5f * (1.f - FMath::Cos(2.f * PI * NextBobTime / NextBobPeriod));
		}
		else
		{
			NextBobTime = 0.f;
		}
		NextIndicator->SetRenderTranslation(FVector2D(0.f, Offset));
	}
}

FReply UNUDialogueWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FReply Reply = HandleDialogueKey(InKeyEvent.GetKey());
	return Reply.IsEventHandled()
		? Reply
		: Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UNUDialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FReply Reply = HandleDialogueKey(InKeyEvent.GetKey());
	return Reply.IsEventHandled()
		? Reply
		: Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UNUDialogueWidget::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	const FKey Key = InAnalogEvent.GetKey();
	if (!bDialogueOpen || (Key != EKeys::Gamepad_LeftY && Key != EKeys::Gamepad_LeftX))
	{
		return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
	}

	// 좌우는 삼키기만 한다 (Slate 기본 포커스 이동이 대화창 밖으로 튀지 않게). 위/아래는 D패드 위/아래와 같다
	if (Key == EKeys::Gamepad_LeftY)
	{
		const float Value = InAnalogEvent.GetAnalogValue();
		if (FMath::Abs(Value) < 0.5f)
		{
			bStickCentered = true;
		}
		else
		{
			const double Now = FPlatformTime::Seconds();
			if (bStickCentered || Now - LastStickMoveTime > 0.3)
			{
				bStickCentered = false;
				LastStickMoveTime = Now;
				if (bOptionMode)
				{
					MoveOptionFocus(Value > 0.0f ? -1 : 1); // 위(+Y) = 이전 항목
				}
			}
		}
	}
	return FReply::Handled();
}

bool UNUDialogueWidget::OpenDialogueInternal(const TArray<FNUDialogueLine>* Lines, const TArray<FNUInteractionOption>* Options)
{
	const bool bOpenAsOptionMode = Options != nullptr;
	const bool bHasDialogue = Lines != nullptr && !Lines->IsEmpty();
	if ((!bOpenAsOptionMode && !bHasDialogue) || (bOpenAsOptionMode && Options->IsEmpty()))
	{
		return false;
	}

	TArray<FNUDialogueLine> ResolvedLines;
	FString Error;
	if (bHasDialogue && !FNUDialogueLine::ResolveLines(*Lines, ResolvedLines, Error))
	{
		UE_LOG(LogNUDialogueWidget, Error, TEXT("대사를 읽을 수 없습니다: %s"), *Error);
		return false;
	}

	// NPC 이름은 정적 정의가 원본이며, 문장별 초상화가 비어 있으면 NPC 초상화를 쓴다.
	// 독백은 머리 위 이름표와 같은 출처의 캐릭터 이름을 쓰고, 나레이션은 이름 칸을 비운다.
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUNPCDataSubsystem* NPCData = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUNPCDataSubsystem>() : nullptr;
	const UNUCharacterManager* Characters = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;
	FNUCharacterSummary SelectedCharacter;
	const bool bHasCharacterName = Characters != nullptr && Characters->GetSelectedCharacter(SelectedCharacter) && !SelectedCharacter.Name.IsEmpty();
	for (FNUDialogueLine& Line : ResolvedLines)
	{
		if (Line.Speaker == ENUDialogueSpeaker::Player)
		{
			Line.SpeakerName = bHasCharacterName ? FText::FromString(SelectedCharacter.Name) : FText::GetEmpty();
			continue;
		}

		if (Line.Speaker == ENUDialogueSpeaker::Narration)
		{
			Line.SpeakerName = FText::GetEmpty();
			continue;
		}

		const FNUNPCDefinition* NPC = NPCData != nullptr ? NPCData->FindDefinition(Line.SpeakerNpcId) : nullptr;
		if (NPC != nullptr)
		{
			Line.SpeakerName = NPC->DisplayName;
			if (Line.Portrait.IsNull()) { Line.Portrait = NPC->Portrait; }
		}
	}

	ResetDialogueState();
	ActiveLines = MoveTemp(ResolvedLines);
	ActiveOptions = bOpenAsOptionMode ? *Options : TArray<FNUInteractionOption>();
	if (bOpenAsOptionMode)
	{
		ActiveOptions.Add(NUDialogueWidget::MakeExitOption());
	}
	CurrentLineIndex = bHasDialogue ? 0 : INDEX_NONE;
	bDialogueOpen = true;
	bOptionMode = bOpenAsOptionMode;

	SetVisibility(ESlateVisibility::Visible);
	InteractionOptionPanel->SetVisibility(bOptionMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCollapseDialoguePanelWithoutLines)
	{
		DialoguePanel->SetVisibility(bHasDialogue ? DialoguePanelRestVisibility : ESlateVisibility::Collapsed);
	}

	if (bHasDialogue)
	{
		RefreshCurrentLine();
	}
	else
	{
		SpeakerNameText->SetText(FText::GetEmpty());
		RefreshSpeakerRow(FText::GetEmpty());
		DialogueText->SetText(FText::GetEmpty());
		if (IsValid(PortraitImage))
		{
			PortraitImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (!RebuildOptionEntries())
	{
		UE_LOG(LogNUDialogueWidget, Error, TEXT("기존 대화 WBP의 위쪽 선택지 구성을 만들 수 없습니다. OptionList와 OptionEntryWidgetClass를 확인하세요."));
		ResetDialogueState();
		SetVisibility(ESlateVisibility::Collapsed);
		return false;
	}

	UNUSFXSubsystem::Play(this, NU::SFX::Dialogue::Open);
	RefreshNavigation();
	PlayOpenMotion();
	RequestDialogueFocus();
	return true;
}

void UNUDialogueWidget::PlayOpenMotion()
{
	OpenMotion.Finish();
	if (!bPlayOpenMotion)
	{
		return;
	}
	const FVector2D Rise(0.f, OpenRise);
	OpenMotion.Add(DialoguePanel, 0.f, OpenDuration, Rise);
	if (bOptionMode)
	{
		OpenMotion.Add(InteractionOptionPanel, 0.06f, OpenDuration, Rise);
	}
	OpenMotion.Start();
}

bool UNUDialogueWidget::RebuildOptionEntries()
{
	ClearOptionEntries();
	if (!bOptionMode)
	{
		return true;
	}
	if (!IsValid(OptionList) || OptionEntryWidgetClass == nullptr)
	{
		return false;
	}

	for (const FNUInteractionOption& Option : ActiveOptions)
	{
		UNUInteractionOptionEntryWidget* Entry = CreateWidget<UNUInteractionOptionEntryWidget>(GetOwningPlayer(), OptionEntryWidgetClass);
		if (!IsValid(Entry))
		{
			continue;
		}

		Entry->SetOption(Option);
		Entry->OnSelected.AddDynamic(this, &ThisClass::HandleOptionSelected);
		Entry->OnFocused.AddDynamic(this, &ThisClass::HandleOptionFocused);
		OptionEntryWidgets.Add(Entry);
		if (UVerticalBoxSlot* EntrySlot = OptionList->AddChildToVerticalBox(Entry))
		{
			EntrySlot->SetPadding(FMargin(0.0f, 3.0f));
		}
	}

	return OptionEntryWidgets.Num() == ActiveOptions.Num();
}

void UNUDialogueWidget::ClearOptionEntries()
{
	for (UNUInteractionOptionEntryWidget* Entry : OptionEntryWidgets)
	{
		if (IsValid(Entry))
		{
			Entry->OnSelected.RemoveDynamic(this, &ThisClass::HandleOptionSelected);
			Entry->OnFocused.RemoveDynamic(this, &ThisClass::HandleOptionFocused);
		}
	}

	OptionEntryWidgets.Reset();
	FocusedOptionIndex = INDEX_NONE;
	if (IsValid(OptionList))
	{
		OptionList->ClearChildren();
	}
}

FReply UNUDialogueWidget::HandleDialogueKey(const FKey& Key)
{
	if (!bDialogueOpen)
	{
		return FReply::Unhandled();
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	if (Settings == nullptr)
	{
		return FReply::Unhandled();
	}

	const bool bAdvanceKey = Settings->DialogueAdvanceKeys.Contains(Key);
	const bool bConfirmOptionKey = Settings->DialogueConfirmOptionKeys.Contains(Key);
	const bool bHasNextLine = CurrentLineIndex + 1 < ActiveLines.Num();

	// 출력 중 첫 진행 입력은 문장만 완성하고, 다음 입력부터 진행·선택한다.
	if ((bAdvanceKey || bConfirmOptionKey) && IsTextRevealing())
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Dialogue::Advance);
		FinishTextReveal();
		return FReply::Handled();
	}

	if (bOptionMode && Settings->DialogueNavigateUpKeys.Contains(Key))
	{
		MoveOptionFocus(-1);
		return FReply::Handled();
	}
	if (bOptionMode && Settings->DialogueNavigateDownKeys.Contains(Key))
	{
		MoveOptionFocus(1);
		return FReply::Handled();
	}
	// 패드 D패드 좌우는 여기서 삼킨다 --- 안 삼키면 Slate 포커스 이동이 대화창 밖(채팅 버튼)으로 튄다 (9/15 패드)
	if (Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right)
	{
		return FReply::Handled();
	}
	// 질문 대사가 남아 있으면 먼저 진행하고, 마지막 줄에서 F/Enter로 현재 선택지를 확정한다.
	if (bOptionMode && !bHasNextLine && bConfirmOptionKey)
	{
		ActivateFocusedOption();
		return FReply::Handled();
	}

	if (bAdvanceKey)
	{
		AdvanceDialogue();
		return FReply::Handled();
	}
	if (Settings->DialogueCloseKeys.Contains(Key))
	{
		CloseDialogue(false);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

bool UNUDialogueWidget::FocusFirstEnabledOption()
{
	FocusedOptionIndex = INDEX_NONE;
	for (int32 Index = 0; Index < OptionEntryWidgets.Num(); ++Index)
	{
		if (FocusOption(Index))
		{
			return true;
		}
	}

	return false;
}

bool UNUDialogueWidget::MoveOptionFocus(int32 Direction)
{
	if (!bOptionMode || OptionEntryWidgets.IsEmpty() || Direction == 0)
	{
		return false;
	}

	const int32 EntryCount = OptionEntryWidgets.Num();
	const int32 PreviousIndex = FocusedOptionIndex;
	const int32 StartIndex = OptionEntryWidgets.IsValidIndex(FocusedOptionIndex)
		? FocusedOptionIndex
		: (Direction > 0 ? EntryCount - 1 : 0);

	for (int32 Offset = 1; Offset <= EntryCount; ++Offset)
	{
		const int32 CandidateIndex = (StartIndex + Direction * Offset + EntryCount * 2) % EntryCount;
		if (FocusOption(CandidateIndex))
		{
			// 선택지가 하나뿐이라 제자리로 돌아온 경우는 소리를 내지 않는다
			if (FocusedOptionIndex != PreviousIndex)
			{
				UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
			}
			return true;
		}
	}

	return false;
}

bool UNUDialogueWidget::FocusOption(int32 Index)
{
	if (!OptionEntryWidgets.IsValidIndex(Index))
	{
		return false;
	}

	UNUInteractionOptionEntryWidget* Entry = OptionEntryWidgets[Index];
	if (!IsValid(Entry) || !Entry->IsOptionEnabled())
	{
		return false;
	}

	if (OptionEntryWidgets.IsValidIndex(FocusedOptionIndex) && FocusedOptionIndex != Index)
	{
		if (UNUInteractionOptionEntryWidget* Previous = OptionEntryWidgets[FocusedOptionIndex])
		{
			Previous->SetFocused(false);
		}
	}

	FocusedOptionIndex = Index;
	if (!Entry->FocusEntry())
	{
		FocusedOptionIndex = INDEX_NONE;
		return false;
	}
	if (IsValid(InteractionOptionScroll))
	{
		InteractionOptionScroll->ScrollWidgetIntoView(Entry, true);
	}
	return true;
}

bool UNUDialogueWidget::ActivateFocusedOption()
{
	if (!OptionEntryWidgets.IsValidIndex(FocusedOptionIndex))
	{
		return false;
	}

	UNUInteractionOptionEntryWidget* Entry = OptionEntryWidgets[FocusedOptionIndex];
	if (!IsValid(Entry) || !Entry->IsOptionEnabled())
	{
		return false;
	}

	HandleOptionSelected(Entry->GetOption().Handle);
	return true;
}

void UNUDialogueWidget::RefreshCurrentLine()
{
	if (!ActiveLines.IsValidIndex(CurrentLineIndex))
	{
		return;
	}

	const FNUDialogueLine& Line = ActiveLines[CurrentLineIndex];
	SpeakerNameText->SetText(Line.SpeakerName);
	RefreshSpeakerRow(Line.SpeakerName);
	StartTextReveal();
	OnLinePresented.Broadcast(Line.SourceRowName);

	if (IsValid(PortraitImage))
	{
		UTexture2D* Portrait = Line.Portrait.IsNull() ? nullptr : Line.Portrait.LoadSynchronous();
		PortraitImage->SetVisibility(IsValid(Portrait) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (IsValid(Portrait))
		{
			PortraitImage->SetBrushFromTexture(Portrait, true);
		}
	}
}

void UNUDialogueWidget::RefreshSpeakerRow(const FText& SpeakerName)
{
	if (IsValid(SpeakerNameRow))
	{
		SpeakerNameRow->SetVisibility(SpeakerName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UNUDialogueWidget::StartTextReveal()
{
	StopTextReveal();
	TextReveal.SetText(ActiveLines[CurrentLineIndex].DialogueText);
	UWorld* World = GetWorld();
	if (TextCharactersPerSecond <= 0.0f || !TextReveal.IsRevealing() || World == nullptr)
	{
		FinishTextReveal();
		return;
	}
	TextReveal.ApplyTo(*DialogueText, 0);
	World->GetTimerManager().SetTimer(TextRevealTimer, this, &ThisClass::AdvanceTextReveal, 1.0f / TextCharactersPerSecond, true);
}

void UNUDialogueWidget::AdvanceTextReveal()
{
	if (!bDialogueOpen || !ActiveLines.IsValidIndex(CurrentLineIndex))
	{
		StopTextReveal();
		return;
	}

	++RevealedCharacterCount;
	if (RevealedCharacterCount >= TextReveal.GetCharacterCount())
	{
		FinishTextReveal();
		return;
	}
	TextReveal.ApplyTo(*DialogueText, RevealedCharacterCount);
}

void UNUDialogueWidget::FinishTextReveal()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TextRevealTimer);
	}
	RevealedCharacterCount = TextReveal.GetCharacterCount();
	if (ActiveLines.IsValidIndex(CurrentLineIndex))
	{
		TextReveal.ApplyTo(*DialogueText, TextReveal.GetCharacterCount());
	}
	RefreshNavigation();
}

void UNUDialogueWidget::StopTextReveal()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TextRevealTimer);
	}
	TextReveal.Reset();
	RevealedCharacterCount = 0;
}

void UNUDialogueWidget::RefreshNavigation()
{
	const bool bHasNextLine = CurrentLineIndex + 1 < ActiveLines.Num();
	NextButton->SetVisibility(!bOptionMode || bHasNextLine || IsTextRevealing() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UNUDialogueWidget::ResetDialogueState()
{
	OpenMotion.Finish();
	StopTextReveal();
	ClearOptionEntries();
	ActiveLines.Reset();
	ActiveOptions.Reset();
	CurrentLineIndex = INDEX_NONE;
	bDialogueOpen = false;
	bOptionMode = false;
	if (IsValid(InteractionOptionPanel))
	{
		InteractionOptionPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNUDialogueWidget::RequestDialogueFocus()
{
	ApplyDialogueFocus();

	// 선택 버튼을 닫고 같은 위젯으로 일반 대화를 즉시 다시 열면
	// Slate 재부착 뒤에 한 번 더 포커스를 잡아야 다음 키 입력을 받을 수 있다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::ApplyDialogueFocus));
	}
}

void UNUDialogueWidget::ApplyDialogueFocus()
{
	if (!bDialogueOpen)
	{
		return;
	}

	SetKeyboardFocus();
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		SetUserFocus(PlayerController);
	}

	if (bOptionMode)
	{
		FocusFirstEnabledOption();
	}
}

void UNUDialogueWidget::HandleNextClicked()
{
	AdvanceDialogue();
}

void UNUDialogueWidget::HandleCloseClicked()
{
	CloseDialogue(false);
}

void UNUDialogueWidget::HandleOptionSelected(FNUInteractionOptionHandle Handle)
{
	if (bDialogueOpen && bOptionMode && Handle.IsValid())
	{
		if (NUDialogueWidget::IsExitOption(Handle))
		{
			CloseDialogue(false);
			return;
		}

		if (IsTextRevealing())
		{
			UNUSFXSubsystem::Play(this, NU::SFX::Dialogue::Advance);
			FinishTextReveal();
			return;
		}

		OnOptionSelected.Broadcast(Handle);
	}
}

void UNUDialogueWidget::HandleOptionFocused(FNUInteractionOptionHandle Handle)
{
	if (!bDialogueOpen || !bOptionMode || !Handle.IsValid())
	{
		return;
	}

	const int32 OptionIndex = ActiveOptions.IndexOfByPredicate(
		[&Handle](const FNUInteractionOption& Option)
		{
			return Option.Handle == Handle;
		});
	if (OptionIndex != INDEX_NONE)
	{
		FocusOption(OptionIndex);
	}
}
