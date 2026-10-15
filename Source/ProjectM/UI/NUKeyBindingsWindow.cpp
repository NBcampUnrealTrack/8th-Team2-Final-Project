#include "UI/NUKeyBindingsWindow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/NUInputUserSettings.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/IConsoleManager.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "NUKeyBindingRowWidget.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUKeyBindingGroupWidget.h"
#include "UI/NUUIManager.h" // NUUILayer
#include "UI/NUUISettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUKeyBindings, Log, All);

namespace
{
	// 한 매핑이 패드 칸인지 --- 기본 키(IMC)로 가른다. 지금 키로 가르면 비운 패드 슬롯(둘 다 없음)이 키보드 칸으로 넘어가 표시가 깨진다.
	// 기본 키가 없는 매핑은 창이 패드 키를 위해 새로 만든 슬롯이다 (엔진은 슬롯에 장치 표시를 안 붙인다)
	bool IsGamepadMapping(const FPlayerKeyMapping& Mapping)
	{
		return !Mapping.GetDefaultKey().IsValid() || Mapping.GetDefaultKey().IsGamepadKey();
	}

	// 행에서 그 장치의 매핑 (없으면 nullptr). 슬롯 번호는 등록 순서에 따라 흔들릴 수 있어 키 종류로 찾는다
	const FPlayerKeyMapping* FindMapping(const FKeyMappingRow& Row, bool bGamepad)
	{
		for (const FPlayerKeyMapping& Mapping : Row.Mappings)
		{
			if (IsGamepadMapping(Mapping) == bGamepad)
			{
				return &Mapping;
			}
		}
		return nullptr;
	}

	const UEnhancedPlayerMappableKeyProfile* FindProfile(const APlayerController* OwningPlayer)
	{
		const ULocalPlayer* LocalPlayer = OwningPlayer != nullptr ? OwningPlayer->GetLocalPlayer() : nullptr;
		const UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		const UEnhancedInputUserSettings* Settings = Subsystem != nullptr ? Subsystem->GetUserSettings() : nullptr;
		return Settings != nullptr ? Settings->GetActiveKeyProfile() : nullptr;
	}

	// 반영할 슬롯: 그 장치 매핑이 있으면 그 슬롯, 없으면 행에서 비어 있는 첫 슬롯 (MapPlayerKey가 새로 만든다)
	EPlayerMappableKeySlot FindSlotFor(const UEnhancedInputUserSettings& Settings, FName MappingName, bool bGamepad)
	{
		const UEnhancedPlayerMappableKeyProfile* Profile = Settings.GetActiveKeyProfile();
		const FKeyMappingRow* Row = Profile != nullptr ? Profile->FindKeyMappingRow(MappingName) : nullptr;
		if (Row == nullptr)
		{
			return bGamepad ? EPlayerMappableKeySlot::Second : EPlayerMappableKeySlot::First;
		}
		if (const FPlayerKeyMapping* Existing = FindMapping(*Row, bGamepad))
		{
			return Existing->GetSlot();
		}
		for (int32 Index = 0; Index < static_cast<int32>(EPlayerMappableKeySlot::Unspecified); ++Index)
		{
			const EPlayerMappableKeySlot Slot = static_cast<EPlayerMappableKeySlot>(Index);
			bool bUsed = false;
			for (const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				bUsed |= Mapping.GetSlot() == Slot;
			}
			if (!bUsed)
			{
				return Slot;
			}
		}
		return EPlayerMappableKeySlot::Second;
	}
}

UNUKeyBindingsWindow* UNUKeyBindingsWindow::Show(APlayerController* OwningPlayer)
{
	if (OwningPlayer == nullptr)
	{
		return nullptr;
	}

	const TSubclassOf<UNUKeyBindingsWindow> WindowClass = GetDefault<UNUUISettings>()->KeyBindingsWindowClass.LoadSynchronous();
	if (WindowClass == nullptr)
	{
		UE_LOG(LogNUKeyBindings, Warning, TEXT("키 설정 창 WBP 미지정 --- 프로젝트 세팅(NU UI)의 KeyBindingsWindowClass를 지정하세요"));
		return nullptr;
	}

	UNUKeyBindingsWindow* Window = CreateWidget<UNUKeyBindingsWindow>(OwningPlayer, WindowClass);
	if (Window == nullptr)
	{
		return nullptr;
	}
	// 매니저에 올려야 ESC 한 번에 이 창이 먼저 닫힌다 (옵션창 위에 떠 있다)
	if (UNUUIManager* UI = UNUUIManager::Get(OwningPlayer))
	{
		FNUUIWindowParams Params;
		Params.Layer = NUUILayer::Modal;
		Params.bDimBackground = true;
		Params.bBlockGameInput = true;
		UI->OpenWindow(Window, Params, FSimpleDelegate::CreateUObject(Window, &UNUKeyBindingsWindow::CloseSelf));
	}
	else
	{
		Window->AddToViewport(NUUILayer::Modal);
	}
	return Window;
}

void UNUKeyBindingsWindow::NativeConstruct()
{
	Super::NativeConstruct();

	// 키 입력을 직접 받아야 해서 포커스를 가질 수 있어야 한다
	SetIsFocusable(true);

	ApplyButton->OnCommonClicked.RemoveDynamic(this, &UNUKeyBindingsWindow::HandleApplyClicked);
	ApplyButton->OnCommonClicked.AddDynamic(this, &UNUKeyBindingsWindow::HandleApplyClicked);
	DefaultsButton->OnCommonClicked.RemoveDynamic(this, &UNUKeyBindingsWindow::HandleDefaultsClicked);
	DefaultsButton->OnCommonClicked.AddDynamic(this, &UNUKeyBindingsWindow::HandleDefaultsClicked);
	// Optional footer cancel follows the same manager/capture cleanup as the header close.
	if (UButton* FooterClose = Cast<UButton>(GetWidgetFromName(TEXT("FooterCloseButton"))))
	{
		FooterClose->OnClicked.RemoveDynamic(this, &UNUKeyBindingsWindow::HandleCloseClicked);
		FooterClose->OnClicked.AddDynamic(this, &UNUKeyBindingsWindow::HandleCloseClicked);
	}

	// 닫기 버튼은 WBP마다 타입이 다르다 --- 순정 Button(레거시), 공용 닫기 버튼, 공용 일반 버튼
	if (UButton* ClosePure = Cast<UButton>(CloseButton))
	{
		ClosePure->OnClicked.RemoveDynamic(this, &UNUKeyBindingsWindow::HandleCloseClicked);
		ClosePure->OnClicked.AddDynamic(this, &UNUKeyBindingsWindow::HandleCloseClicked);
	}
	else if (UNU_CloseButtonWidget* CloseWidget = Cast<UNU_CloseButtonWidget>(CloseButton))
	{
		CloseWidget->OnCloseClicked.RemoveDynamic(this, &UNUKeyBindingsWindow::HandleCloseClicked);
		CloseWidget->OnCloseClicked.AddDynamic(this, &UNUKeyBindingsWindow::HandleCloseClicked);
	}
	else if (UNU_CommonButtonWidget* CloseCommon = Cast<UNU_CommonButtonWidget>(CloseButton))
	{
		CloseCommon->OnCommonClicked.RemoveDynamic(this, &UNUKeyBindingsWindow::HandleCloseClicked);
		CloseCommon->OnCommonClicked.AddDynamic(this, &UNUKeyBindingsWindow::HandleCloseClicked);
	}

	if (Cast<UNUInputUserSettings>(GetUserSettings()) == nullptr)
	{
		UE_LOG(LogNUKeyBindings, Warning, TEXT("사용자 설정 클래스가 NUInputUserSettings가 아닙니다 --- DefaultInput.ini의 UserSettingsClass 확인 (행이 비고 패드 키가 안 붙는다)"));
	}
	Rebuild();
}

void UNUKeyBindingsWindow::NativeDestruct()
{
	EndCapture();
	Super::NativeDestruct();
}

UEnhancedInputUserSettings* UNUKeyBindingsWindow::GetUserSettings() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (LocalPlayer == nullptr)
	{
		return nullptr;
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	return Subsystem != nullptr ? Subsystem->GetUserSettings() : nullptr;
}

FKey NUKeyBindings::GetMappedKey(const APlayerController* OwningPlayer, FName MappingName, bool bGamepad)
{
	const UEnhancedPlayerMappableKeyProfile* Profile = FindProfile(OwningPlayer);
	if (Profile == nullptr)
	{
		return FKey();
	}

	const FKeyMappingRow* Row = Profile->FindKeyMappingRow(MappingName);
	const FPlayerKeyMapping* Mapping = Row != nullptr ? FindMapping(*Row, bGamepad) : nullptr;
	return Mapping != nullptr ? Mapping->GetCurrentKey() : FKey();
}

FText NUKeyBindings::GetMappedKeyText(const APlayerController* OwningPlayer, FName MappingName)
{
	const ANUPlayerController* NUPlayer = Cast<ANUPlayerController>(OwningPlayer);
	const FKey Key = GetMappedKey(OwningPlayer, MappingName, NUPlayer != nullptr && NUPlayer->IsGamepadMode());
	return Key.IsValid() ? GetKeyText(Key) : FText::GetEmpty();
}

FText NUKeyBindings::GetKeyText(const FKey& Key)
{
	// Xbox 이름 기준 (엔진 키 이름이 자리 기준이라 다른 패드도 동작은 같다). PS·닌텐도 표기는 나중에 옵션으로
	static const TMap<FKey, FText> PadNames = {
		{ EKeys::Gamepad_FaceButton_Bottom, FText::FromString(TEXT("A")) },
		{ EKeys::Gamepad_FaceButton_Right, FText::FromString(TEXT("B")) },
		{ EKeys::Gamepad_FaceButton_Left, FText::FromString(TEXT("X")) },
		{ EKeys::Gamepad_FaceButton_Top, FText::FromString(TEXT("Y")) },
		{ EKeys::Gamepad_LeftShoulder, FText::FromString(TEXT("LB")) },
		{ EKeys::Gamepad_RightShoulder, FText::FromString(TEXT("RB")) },
		{ EKeys::Gamepad_LeftTrigger, FText::FromString(TEXT("LT")) },
		{ EKeys::Gamepad_RightTrigger, FText::FromString(TEXT("RT")) },
		{ EKeys::Gamepad_LeftTriggerAxis, FText::FromString(TEXT("LT")) },
		{ EKeys::Gamepad_RightTriggerAxis, FText::FromString(TEXT("RT")) },
		{ EKeys::Gamepad_LeftThumbstick, FText::FromString(TEXT("L3")) },
		{ EKeys::Gamepad_RightThumbstick, FText::FromString(TEXT("R3")) },
		{ EKeys::Gamepad_Special_Right, FText::FromString(TEXT("Start")) },
		{ EKeys::Gamepad_Special_Left, FText::FromString(TEXT("Back")) },
		{ EKeys::Gamepad_DPad_Up, NSLOCTEXT("NUKeyBindings", "PadDPadUp", "D패드 위") },
		{ EKeys::Gamepad_DPad_Down, NSLOCTEXT("NUKeyBindings", "PadDPadDown", "D패드 아래") },
		{ EKeys::Gamepad_DPad_Left, NSLOCTEXT("NUKeyBindings", "PadDPadLeft", "D패드 왼쪽") },
		{ EKeys::Gamepad_DPad_Right, NSLOCTEXT("NUKeyBindings", "PadDPadRight", "D패드 오른쪽") },
		{ EKeys::Gamepad_Left2D, NSLOCTEXT("NUKeyBindings", "PadLeftStick", "왼쪽 스틱") },
		{ EKeys::Gamepad_LeftX, NSLOCTEXT("NUKeyBindings", "PadLeftStick", "왼쪽 스틱") },
		{ EKeys::Gamepad_LeftY, NSLOCTEXT("NUKeyBindings", "PadLeftStick", "왼쪽 스틱") },
		{ EKeys::Gamepad_Right2D, NSLOCTEXT("NUKeyBindings", "PadRightStick", "오른쪽 스틱") },
		{ EKeys::Gamepad_RightX, NSLOCTEXT("NUKeyBindings", "PadRightStick", "오른쪽 스틱") },
		{ EKeys::Gamepad_RightY, NSLOCTEXT("NUKeyBindings", "PadRightStick", "오른쪽 스틱") },
	};
	if (const FText* Found = PadNames.Find(Key))
	{
		return *Found;
	}
	return Key.GetDisplayName(false);
}

FKey UNUKeyBindingsWindow::GetSavedKey(FName MappingName, bool bGamepad) const
{
	return NUKeyBindings::GetMappedKey(GetOwningPlayer(), MappingName, bGamepad);
}

FKey UNUKeyBindingsWindow::GetEffectiveKey(FName MappingName, bool bGamepad) const
{
	if (const FKey* Pending = GetPendingMap(bGamepad).Find(MappingName))
	{
		return *Pending;
	}
	return GetSavedKey(MappingName, bGamepad);
}

void UNUKeyBindingsWindow::Rebuild()
{
	RowBox->ClearChildren();
	RowByName.Reset();

	if (RowWidgetClass == nullptr)
	{
		UE_LOG(LogNUKeyBindings, Warning, TEXT("RowWidgetClass 미지정(WBP_KeyBindingsWindow 기본값) --- 목록을 그릴 수 없습니다"));
		return;
	}

	const UEnhancedInputUserSettings* Settings = GetUserSettings();
	if (Settings == nullptr)
	{
		// 프로젝트 세팅에서 사용자 설정을 켜지 않으면 여기로 온다 (bEnableUserSettings)
		UE_LOG(LogNUKeyBindings, Warning, TEXT("입력 사용자 설정이 꺼져 있습니다 --- DefaultInput.ini의 bEnableUserSettings 확인"));
	}

	FText CurrentGroup;
	for (const FNUKeyBindingEntry& Entry : GetDefault<UNUUISettings>()->KeyBindingEntries)
	{
		if (Entry.MappingName.IsNone())
		{
			continue;
		}

		// 묶음이 바뀌면 제목 줄을 하나 끼운다 (제목 WBP를 안 준 창은 줄만 이어 그린다)
		if (GroupHeaderClass != nullptr && !Entry.GroupName.EqualTo(CurrentGroup))
		{
			CurrentGroup = Entry.GroupName;
			if (UNUKeyBindingGroupWidget* Header = CreateWidget<UNUKeyBindingGroupWidget>(this, GroupHeaderClass))
			{
				Header->SetTitle(CurrentGroup);
				RowBox->AddChild(Header);
			}
		}

		UNUKeyBindingRowWidget* Row = CreateWidget<UNUKeyBindingRowWidget>(this, RowWidgetClass);
		if (Row == nullptr)
		{
			continue;
		}
		Row->SetBinding(Entry.MappingName, Entry.DisplayName,
			GetEffectiveKey(Entry.MappingName, false), PendingKeys.Contains(Entry.MappingName),
			GetEffectiveKey(Entry.MappingName, true), PendingPadKeys.Contains(Entry.MappingName), Entry.GamepadFixedText);
		Row->OnChangeRequested.BindUObject(this, &UNUKeyBindingsWindow::BeginCapture);
		RowBox->AddChild(Row);
		RowByName.Add(Entry.MappingName, Row);
	}

	if (GuideText != nullptr && CapturingName.IsNone())
	{
		GuideText->SetText(PendingKeys.IsEmpty() && PendingPadKeys.IsEmpty()
			? NSLOCTEXT("NUKeyBindings", "Guide", "바꿀 항목의 [키보드] 또는 [패드]를 누르세요.")
			: NSLOCTEXT("NUKeyBindings", "GuideDirty", "바뀐 항목이 있습니다. [적용]을 눌러야 반영됩니다."));
	}
}

void UNUKeyBindingsWindow::BeginCapture(FName MappingName, bool bGamepad)
{
	CapturingName = MappingName;
	bCapturingGamepad = bGamepad;
	if (TObjectPtr<UNUKeyBindingRowWidget>* Row = RowByName.Find(MappingName))
	{
		(*Row)->SetWaiting(true, bGamepad);
	}
	if (GuideText != nullptr)
	{
		GuideText->SetText(bGamepad
			? NSLOCTEXT("NUKeyBindings", "GuideCapturingPad", "패드 버튼을 누르세요. (B = 취소)")
			: NSLOCTEXT("NUKeyBindings", "GuideCapturing", "새 키를 누르세요. (ESC = 취소)"));
	}
	SetKeyboardFocus();
}

void UNUKeyBindingsWindow::EndCapture()
{
	if (CapturingName.IsNone())
	{
		return;
	}
	if (TObjectPtr<UNUKeyBindingRowWidget>* Row = RowByName.Find(CapturingName))
	{
		(*Row)->SetWaiting(false, bCapturingGamepad);
	}
	CapturingName = NAME_None;
}

void UNUKeyBindingsWindow::ApplyCapturedKey(const FKey& NewKey)
{
	const FName Target = CapturingName;
	const bool bGamepad = bCapturingGamepad;
	EndCapture();
	if (Target.IsNone() || !NewKey.IsValid())
	{
		return;
	}

	// 같은 장치에서 같은 키를 이미 쓰는 동작은 비운다 --- 한 키에 두 동작이 걸리면 둘 다 발동한다.
	// 목록에 없는(고정) 동작과 겹치는 것은 막지 않는다. UI 조작과 게임 조작은 상황(IMC)이 달라 같이 눌리지 않는다
	TMap<FName, FKey>& Pending = GetPendingMap(bGamepad);
	for (const FNUKeyBindingEntry& Entry : GetDefault<UNUUISettings>()->KeyBindingEntries)
	{
		if (Entry.MappingName != Target && GetEffectiveKey(Entry.MappingName, bGamepad) == NewKey)
		{
			Pending.Add(Entry.MappingName, FKey());
		}
	}

	Pending.Add(Target, NewKey);
	Rebuild();
	RestoreRowFocus(Target, bGamepad);
}

void UNUKeyBindingsWindow::RestoreRowFocus(FName MappingName, bool bGamepad)
{
	const ANUPlayerController* NUPlayer = Cast<ANUPlayerController>(GetOwningPlayer());
	if (NUPlayer == nullptr || !NUPlayer->IsGamepadMode())
	{
		return;
	}
	if (TObjectPtr<UNUKeyBindingRowWidget>* Row = RowByName.Find(MappingName))
	{
		(*Row)->FocusChangeButton(bGamepad);
	}
}

FReply UNUKeyBindingsWindow::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (CapturingName.IsNone())
	{
		// 패드 LB/RB = 목록 처음 <=> [적용] 건너뛰기 (줄이 많아 아래 버튼까지 D패드로 내려가지 않게)
		if (Key == EKeys::Gamepad_RightShoulder)
		{
			UNUUIManager::FocusFirstButtonIn(ApplyButton);
			return FReply::Handled();
		}
		if (Key == EKeys::Gamepad_LeftShoulder)
		{
			// FocusFirstButtonIn은 목록 안에 이미 포커스가 있으면 두므로(창 첫 포커스용) 여기선 첫 조작부로 바로 옮긴다.
			// 스크롤은 포커스 따라가기(RowScroll의 NavigationScrollPadding으로 묶음 제목까지 보인다)에 맡긴다
			UNUUIManager::FocusFirstFocusableIn(RowBox);
			return FReply::Handled();
		}
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	// ESC와 패드 B는 어느 장치를 기다리든 취소 --- 그래서 둘은 지정할 수 없다
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		const FName Cancelled = CapturingName;
		const bool bCancelledGamepad = bCapturingGamepad;
		EndCapture();
		Rebuild();
		RestoreRowFocus(Cancelled, bCancelledGamepad);
		return FReply::Handled();
	}

	// 기다리는 장치의 키만 받는다. 다른 장치 키는 삼키기만 한다 (창 밖으로 새면 게임 조작이 된다)
	if (Key.IsGamepadKey() != bCapturingGamepad)
	{
		return FReply::Handled();
	}

	ApplyCapturedKey(Key);
	return FReply::Handled();
}

FReply UNUKeyBindingsWindow::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CapturingName.IsNone())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	// 마우스 버튼은 키보드 칸에만 (패드 칸을 기다릴 때는 삼킨다)
	if (!bCapturingGamepad)
	{
		ApplyCapturedKey(InMouseEvent.GetEffectingButton());
	}
	return FReply::Handled();
}

FReply UNUKeyBindingsWindow::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	if (CapturingName.IsNone())
	{
		return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
	}
	return FReply::Handled();
}

void UNUKeyBindingsWindow::HandleApplyClicked()
{
	EndCapture();

	// 키보드 칸이 빈 채로 남은 동작을 모은다 (겹쳐서 떼인 것 포함). 패드 칸은 없어도 된다 --- 키보드로 다 되니까
	TArray<FText> RequiredEmpty;
	TArray<FText> OptionalEmpty;
	for (const FNUKeyBindingEntry& Entry : GetDefault<UNUUISettings>()->KeyBindingEntries)
	{
		if (Entry.MappingName.IsNone() || GetEffectiveKey(Entry.MappingName, false).IsValid())
		{
			continue;
		}
		(Entry.bRequired ? RequiredEmpty : OptionalEmpty).Add(Entry.DisplayName);
	}

	const TSubclassOf<UNUConfirmDialog> DialogClass = GetDefault<UNUUISettings>()->ConfirmDialogClass.LoadSynchronous();

	// 꼭 필요한 키가 비었으면 적용하지 않는다 --- 고를 것이 없으므로 확인 버튼만 있는 안내
	if (!RequiredEmpty.IsEmpty())
	{
		const FText Message = FText::Format(
			NSLOCTEXT("NUKeyBindings", "RequiredEmpty", "{0}에 키를 지정해야 합니다.\n지정하지 않으면 적용할 수 없습니다."),
			FText::Join(NSLOCTEXT("NUKeyBindings", "Comma", ", "), RequiredEmpty));
		UNUConfirmDialog::Show(GetOwningPlayer(), DialogClass, Message,
			NSLOCTEXT("NUKeyBindings", "OK", "확인"), FSimpleDelegate(), FSimpleDelegate(), FText(),
			ENUConfirmIntent::Destructive, /*bNoticeOnly*/ true);
		return;
	}

	// 없어도 되는 키만 비었으면 그대로 둘지 물어본다
	if (!OptionalEmpty.IsEmpty())
	{
		const FText Message = FText::Format(
			NSLOCTEXT("NUKeyBindings", "OptionalEmpty", "{0}에 키가 지정되어 있지 않습니다.\n이대로 적용하시겠습니까?"),
			FText::Join(NSLOCTEXT("NUKeyBindings", "Comma", ", "), OptionalEmpty));
		UNUConfirmDialog::Show(GetOwningPlayer(), DialogClass, Message,
			NSLOCTEXT("NUKeyBindings", "Apply", "적용"),
			FSimpleDelegate::CreateWeakLambda(this, [this]() { CommitChanges(); }),
			FSimpleDelegate(), NSLOCTEXT("NUKeyBindings", "Cancel", "취소"), ENUConfirmIntent::Positive);
		return;
	}

	CommitChanges();
}

void UNUKeyBindingsWindow::CommitChanges()
{
	UEnhancedInputUserSettings* Settings = GetUserSettings();
	if (Settings == nullptr)
	{
		UE_LOG(LogNUKeyBindings, Warning, TEXT("입력 사용자 설정이 없어 적용하지 못했습니다"));
		return;
	}

	for (const bool bGamepad : { false, true })
	{
		for (const TPair<FName, FKey>& Pair : GetPendingMap(bGamepad))
		{
			FMapPlayerKeyArgs Args;
			Args.MappingName = Pair.Key;
			Args.Slot = FindSlotFor(*Settings, Pair.Key, bGamepad);
			Args.NewKey = Pair.Value;
			// 패드 기본 키가 없던 동작은 그 슬롯이 아직 없다 --- 만들어서 넣는다
			Args.bCreateMatchingSlotIfNeeded = true;

			// 빈 키도 MapPlayerKey로 넣는다 --- UnMapPlayerKey는 "플레이어가 바꾼 것을 지우기"라
			// 기본 키(IMC에 적힌 키)로 되돌아간다. 우리가 원하는 건 "이 동작에 키가 없음"이다
			FGameplayTagContainer FailureReason;
			Settings->MapPlayerKey(Args, FailureReason);
			if (!FailureReason.IsEmpty())
			{
				UE_LOG(LogNUKeyBindings, Warning, TEXT("키 적용 실패 %s(%s): %s"), *Pair.Key.ToString(), bGamepad ? TEXT("패드") : TEXT("키보드"), *FailureReason.ToString());
			}
		}
	}

	Settings->ApplySettings();
	Settings->SaveSettings();
	PendingKeys.Reset();
	PendingPadKeys.Reset();
	Rebuild();

	// 메뉴는 표기만 키를 참조하므로 다시 그리기만 하면 된다 (항목 등록 내용은 그대로)
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->RefreshGameMenu();
	}
}

void UNUKeyBindingsWindow::HandleDefaultsClicked()
{
	EndCapture();
	UEnhancedInputUserSettings* Settings = GetUserSettings();
	if (Settings == nullptr)
	{
		return;
	}

	// 되돌리기는 되돌릴 수 없으므로 한 번 물어본다
	const TSubclassOf<UNUConfirmDialog> DialogClass = GetDefault<UNUUISettings>()->ConfirmDialogClass.LoadSynchronous();
	UNUConfirmDialog::Show(GetOwningPlayer(), DialogClass,
		NSLOCTEXT("NUKeyBindings", "ResetConfirm", "키 설정을 기본값으로 되돌리시겠습니까?"),
		NSLOCTEXT("NUKeyBindings", "Reset", "되돌리기"),
		FSimpleDelegate::CreateWeakLambda(this, [this]()
		{
			UEnhancedInputUserSettings* InnerSettings = GetUserSettings();
			if (InnerSettings == nullptr)
			{
				return;
			}
			// 행 전체(키보드·패드 슬롯 모두)를 되돌린다
			for (const FNUKeyBindingEntry& Entry : GetDefault<UNUUISettings>()->KeyBindingEntries)
			{
				FMapPlayerKeyArgs Args;
				Args.MappingName = Entry.MappingName;
				Args.Slot = EPlayerMappableKeySlot::First;

				FGameplayTagContainer FailureReason;
				InnerSettings->ResetAllPlayerKeysInRow(Args, FailureReason);
			}
			InnerSettings->ApplySettings();
			InnerSettings->SaveSettings();
			PendingKeys.Reset();
			PendingPadKeys.Reset();
			Rebuild();
		}));
}

void UNUKeyBindingsWindow::HandleCloseClicked()
{
	CloseSelf();
}

void UNUKeyBindingsWindow::CloseSelf()
{
	// 적용하지 않은 변경은 버린다 (엔진에는 아직 아무것도 쓰지 않았다)
	EndCapture();
	PendingKeys.Reset();
	PendingPadKeys.Reset();
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->CloseWindow(this); // 미등록이면 무시 --- 아래 RemoveFromParent와 겹쳐도 안전
	}
	RemoveFromParent();
}

#if !UE_BUILD_SHIPPING
// 개발용: 옵션창 > 조작 탭을 거치지 않고 키 설정 창을 바로 연다 (PIE에서 패드 칸 확인용). 사용: nu.Keys.Open
static FAutoConsoleCommandWithWorldAndArgs GNUKeysOpenCommand(
	TEXT("nu.Keys.Open"),
	TEXT("키 설정 창 열기 (개발용)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UNUKeyBindingsWindow::Show(World != nullptr ? UGameplayStatics::GetPlayerController(World, 0) : nullptr);
	}));
#endif
