#include "Inventory/UI/NUSplitStackDialog.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

UNUSplitStackDialog* UNUSplitStackDialog::Show(UUserWidget* OwnerWindow, int32 MaxSplit, FOnNUSplitConfirmed InOnConfirmed)
{
	// 절반을 기본값으로 --- 가장 흔한 선택이라 그대로 확인만 눌러도 되게
	return ShowCount(OwnerWindow, MaxSplit, FMath::Max(1, MaxSplit / 2),
		FText::Format(
			NSLOCTEXT("NUInventory", "SplitPrompt", "몇 개를 나눌까요? (최대 {0}개)"),
			FText::AsNumber(MaxSplit)),
		NSLOCTEXT("NUInventory", "SplitConfirm", "나누기"),
		0, InOnConfirmed);
}

UNUSplitStackDialog* UNUSplitStackDialog::ShowCount(UUserWidget* OwnerWindow, int32 MaxCount, int32 InDefaultCount,
	const FText& InMessage, const FText& InConfirmLabel, int32 InUnitPrice, FOnNUSplitConfirmed InOnConfirmed)
{
	APlayerController* OwningPlayer = OwnerWindow != nullptr ? OwnerWindow->GetOwningPlayer() : nullptr;
	if (OwningPlayer == nullptr || MaxCount < 1)
	{
		return nullptr;
	}

	const TSubclassOf<UNUSplitStackDialog> DialogClass = GetDefault<UNUUISettings>()->CountDialogClass.LoadSynchronous();
	if (DialogClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[수량 창] CountDialogClass 미지정 --- 프로젝트 세팅(NU UI) 확인"));
		return nullptr;
	}

	UNUSplitStackDialog* Dialog = CreateWidget<UNUSplitStackDialog>(OwningPlayer, DialogClass);
	if (Dialog == nullptr)
	{
		return nullptr;
	}

	// NativeConstruct에서 읽으므로 먼저 채운다
	Dialog->MaxSplit = MaxCount;
	Dialog->DefaultCount = FMath::Clamp(InDefaultCount, 1, MaxCount);
	Dialog->DialogMessage = InMessage;
	Dialog->ConfirmLabel = InConfirmLabel;
	Dialog->UnitPrice = InUnitPrice;
	Dialog->OnConfirmed = InOnConfirmed;

	// Modal 레이어 --- ESC는 자체 처리라 매니저 ESC 대상에서 뺀다
	if (UNUUIManager* UI = UNUUIManager::Get(OwningPlayer))
	{
		FNUUIWindowParams Params;
		Params.Layer = NUUILayer::Modal;
		Params.bCloseOnEscape = false;
		Params.OwnerWindow = OwnerWindow; // 연 창이 닫히면 같이 닫힌다
		UI->OpenWindow(Dialog, Params);
	}
	else
	{
		Dialog->AddToViewport(NUUILayer::Modal);
	}
	return Dialog;
}

void UNUSplitStackDialog::NativeConstruct()
{
	Super::NativeConstruct();

	// 패드 모드에선 D패드로 수량을 바꾼다 --- 문구에 한 줄 덧붙인다
	const ANUPlayerController* NUPlayer = Cast<ANUPlayerController>(GetOwningPlayer());
	const bool bGamepad = NUPlayer != nullptr && NUPlayer->IsGamepadMode();
	MessageText->SetText(bGamepad
		? FText::Format(NSLOCTEXT("NUInventory", "CountPadHint", "{0}\n(D패드 위아래: 1씩, 좌우: 10씩)"), DialogMessage)
		: DialogMessage);
	ConfirmButton->SetLabelText(ConfirmLabel);
	if (PriceText != nullptr)
	{
		PriceText->SetVisibility(UnitPrice > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// AddUniqueDynamic이라 창을 다시 올려도 중복 구독되지 않는다
	CountTextBox->OnTextChanged.AddUniqueDynamic(this, &UNUSplitStackDialog::HandleTextChanged);
	CountTextBox->OnTextCommitted.AddUniqueDynamic(this, &UNUSplitStackDialog::HandleTextCommitted);
	ConfirmButton->OnCommonClicked.AddUniqueDynamic(this, &UNUSplitStackDialog::HandleConfirmClicked);
	CancelButton->OnCommonClicked.AddUniqueDynamic(this, &UNUSplitStackDialog::HandleCancelClicked);
	if (DecreaseButton != nullptr) DecreaseButton->OnClicked.AddUniqueDynamic(this, &UNUSplitStackDialog::HandleDecreaseClicked);
	if (IncreaseButton != nullptr) IncreaseButton->OnClicked.AddUniqueDynamic(this, &UNUSplitStackDialog::HandleIncreaseClicked);
	if (MaxButton != nullptr) MaxButton->OnClicked.AddUniqueDynamic(this, &UNUSplitStackDialog::HandleMaxClicked);

	bApplyingClamp = true;
	CountTextBox->SetText(FText::AsNumber(DefaultCount));
	bApplyingClamp = false;
	UpdatePriceText();

	// ESC·Enter를 받으려면 포커스가 필요하다 --- 입력칸으로 바로 보낸다. 패드 모드에선 매니저가 [확인] 버튼에 준다 (A = 확인, D패드 = 수량)
	SetIsFocusable(true);
	if (!bGamepad)
	{
		CountTextBox->SetKeyboardFocus();
	}
}

void UNUSplitStackDialog::HandleTextChanged(const FText& Text)
{
	if (bApplyingClamp)
	{
		return;
	}

	// 숫자만 남긴다 --- 붙여넣기·오타로 들어온 글자를 걸러낸다
	FString Digits;
	for (const TCHAR Char : Text.ToString())
	{
		if (FChar::IsDigit(Char))
		{
			Digits.AppendChar(Char);
		}
	}

	FString Clamped = Digits;
	if (!Digits.IsEmpty())
	{
		// 자릿수가 커도 int32를 넘지 않게 먼저 자른다 (1111111... 같은 입력)
		const int64 Parsed = FCString::Atoi64(*Digits);
		const int64 Limited = FMath::Clamp<int64>(Parsed, 1, MaxSplit);
		Clamped = FString::FromInt(static_cast<int32>(Limited));
	}

	// 비어 있는 동안은 그대로 둔다 --- 지우고 다시 치는 중일 수 있다
	if (Clamped != Text.ToString())
	{
		bApplyingClamp = true;
		CountTextBox->SetText(FText::FromString(Clamped));
		bApplyingClamp = false;
	}
	UpdatePriceText();
}

void UNUSplitStackDialog::UpdatePriceText()
{
	if (PriceText == nullptr || UnitPrice <= 0)
	{
		return;
	}
	PriceText->SetText(FText::Format(
		NSLOCTEXT("NUShop", "CountDialogPrice", "금액: {0}골드"),
		FText::AsNumber(static_cast<int64>(GetEnteredCount()) * UnitPrice)));
}

void UNUSplitStackDialog::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Confirm();
	}
}

int32 UNUSplitStackDialog::GetEnteredCount() const
{
	const int32 Parsed = FCString::Atoi(*CountTextBox->GetText().ToString());
	return FMath::Clamp(Parsed, 1, MaxSplit);
}

void UNUSplitStackDialog::Confirm()
{
	// 콜백이 창을 다시 띄울 수 있으므로 먼저 떼어내고 실행한다
	const int32 Count = GetEnteredCount();
	FOnNUSplitConfirmed Pending = OnConfirmed;
	OnConfirmed.Unbind();
	Close();
	Pending.ExecuteIfBound(Count);
}

void UNUSplitStackDialog::Cancel()
{
	OnConfirmed.Unbind();
	Close();
}

FReply UNUSplitStackDialog::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// ESC 또는 패드 B = 취소
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		Cancel();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Bottom)
	{
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UNUSplitStackDialog::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const int32 Delta = Key == EKeys::Gamepad_DPad_Up ? 1 : Key == EKeys::Gamepad_DPad_Down ? -1 : Key == EKeys::Gamepad_DPad_Right ? 10 : Key == EKeys::Gamepad_DPad_Left ? -10 : 0;
	if (Delta == 0)
	{
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	}
	SetEnteredCount(GetEnteredCount() + Delta);
	return FReply::Handled();
}

void UNUSplitStackDialog::SetEnteredCount(int32 Count)
{
	bApplyingClamp = true;
	CountTextBox->SetText(FText::AsNumber(FMath::Clamp(Count, 1, MaxSplit)));
	bApplyingClamp = false;
	UpdatePriceText();
}

void UNUSplitStackDialog::HandleDecreaseClicked()
{
	SetEnteredCount(GetEnteredCount() - 1);
}

void UNUSplitStackDialog::HandleIncreaseClicked()
{
	SetEnteredCount(GetEnteredCount() + 1);
}

void UNUSplitStackDialog::HandleMaxClicked()
{
	SetEnteredCount(MaxSplit);
}

void UNUSplitStackDialog::HandleConfirmClicked()
{
	Confirm();
}

void UNUSplitStackDialog::HandleCancelClicked()
{
	Cancel();
}

void UNUSplitStackDialog::Close()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->CloseWindow(this); // 미등록이면 무시 --- 아래 RemoveFromParent와 겹쳐도 안전
	}
	RemoveFromParent();
}
