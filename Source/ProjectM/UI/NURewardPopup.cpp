#include "UI/NURewardPopup.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/Controller/NUNotificationUIController.h"
#include "UI/NUUIManager.h"

UNURewardPopup* UNURewardPopup::Show(
	APlayerController* OwningPlayer,
	TSubclassOf<UNURewardPopup> PopupClass,
	const FText& InTitle,
	const TArray<FNURewardEntry>& InEntries)
{
	if (OwningPlayer == nullptr || PopupClass == nullptr || InEntries.IsEmpty())
	{
		return nullptr;
	}

	UNUUIManager* UI = UNUUIManager::Get(OwningPlayer);
	UNUNotificationUIController* Controller = UI ? UI->GetController<UNUNotificationUIController>() : nullptr;
	return Controller ? Controller->ShowReward(PopupClass, InTitle, InEntries) : nullptr;
}

void UNURewardPopup::SetupReward(const FText& InTitle, const TArray<FNURewardEntry>& InEntries)
{
	Title = InTitle;
	Entries = InEntries;
	BindCloseButton();
}

UTexture2D* UNURewardPopup::ResolveRewardIcon(const FNURewardEntry& Entry)
{
	return Entry.Icon.IsNull() ? nullptr : Entry.Icon.LoadSynchronous();
}

UNUNotificationUIController* UNURewardPopup::GetNotificationController() const
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	return UI ? UI->GetController<UNUNotificationUIController>() : nullptr;
}

void UNURewardPopup::NativeConstruct()
{
	Super::NativeConstruct();

	// 이름 계약 — WBP에 해당 위젯이 있으면 C++가 채우고 묶는다 (그래프 배선 불필요)
	if (UTextBlock* TitleText = Cast<UTextBlock>(GetWidgetFromName(TEXT("TitleText"))))
	{
		TitleText->SetText(Title);
	}
	if (UTextBlock* EntryText = Cast<UTextBlock>(GetWidgetFromName(TEXT("EntryText"))))
	{
		EntryText->SetText(BuildEntrySummary());
		if (BuildEntryRows())
		{
			EntryText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	// CloseButton 은 순정 Button(레거시)과 UNU_CloseButtonWidget(WBP_CloseButton, 신규 공용 부품) 둘 다 지원한다.
	UWidget* CloseWidget = GetWidgetFromName(TEXT("CloseButton"));
	if (UButton* CloseButton = Cast<UButton>(CloseWidget))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UNURewardPopup::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &UNURewardPopup::HandleCloseClicked);
	}
	else if (UNU_CloseButtonWidget* CloseButtonWidget = Cast<UNU_CloseButtonWidget>(CloseWidget))
	{
		CloseButtonWidget->OnCloseClicked.RemoveDynamic(this, &UNURewardPopup::HandleCloseClicked);
		CloseButtonWidget->OnCloseClicked.AddDynamic(this, &UNURewardPopup::HandleCloseClicked);
	}
	else if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(CloseWidget))
	{
		CommonButtonWidget->OnCommonClicked.RemoveDynamic(this, &UNURewardPopup::HandleCloseClicked);
		CommonButtonWidget->OnCommonClicked.AddDynamic(this, &UNURewardPopup::HandleCloseClicked);
	}

	OnRewardSet();

	// 커서·입력 차단은 매니저가 창 등록으로 처리한다 (ESC/Enter/Space는 아래 포커스로 직접 받는다)
	SetIsFocusable(true);
	SetKeyboardFocus();
}

void UNURewardPopup::BindCloseButton()
{
	UWidget* CloseWidget = GetWidgetFromName(TEXT("CloseButton"));
	if (UButton* CloseButton = Cast<UButton>(CloseWidget))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
		return;
	}
	if (UNU_CloseButtonWidget* CloseButton = Cast<UNU_CloseButtonWidget>(CloseWidget))
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
		CloseButton->OnCloseClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}
}

FReply UNURewardPopup::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	// 패드는 A·B 어느 쪽이든 닫는다
	if (Key == EKeys::Escape || Key == EKeys::Enter || Key == EKeys::SpaceBar
		|| Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Gamepad_FaceButton_Right)
	{
		Close();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// 이름 계약(선택): RewardRows(VerticalBox)가 있으면 항목마다 [아이콘] 이름 ... +수량 줄을 만든다.
// 모양은 숨겨 둔 견본에서 복사한다 --- RewardLabelTemplate·RewardAmountTemplate(TextBlock: 글꼴·색),
// RewardBulletTemplate(Image: 아이콘이 없는 항목의 표시), RewardDividerTemplate(Image: 줄 사이 1px 선)
bool UNURewardPopup::BuildEntryRows()
{
	UVerticalBox* Rows = Cast<UVerticalBox>(GetWidgetFromName(TEXT("RewardRows")));
	const UTextBlock* LabelTemplate = Cast<UTextBlock>(GetWidgetFromName(TEXT("RewardLabelTemplate")));
	const UTextBlock* AmountTemplate = Cast<UTextBlock>(GetWidgetFromName(TEXT("RewardAmountTemplate")));
	if (Rows == nullptr || LabelTemplate == nullptr || AmountTemplate == nullptr || WidgetTree == nullptr)
	{
		return false;
	}
	const UImage* BulletTemplate = Cast<UImage>(GetWidgetFromName(TEXT("RewardBulletTemplate")));
	const UImage* DividerTemplate = Cast<UImage>(GetWidgetFromName(TEXT("RewardDividerTemplate")));
	constexpr float IconSize = 28.f;
	constexpr float RowHeight = 48.f;

	Rows->ClearChildren();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FNURewardEntry& Entry = Entries[Index];
		if (Index > 0 && DividerTemplate != nullptr)
		{
			UImage* Divider = WidgetTree->ConstructWidget<UImage>();
			Divider->SetBrush(DividerTemplate->GetBrush());
			Divider->SetDesiredSizeOverride(FVector2D(1.f, 1.f));
			Rows->AddChildToVerticalBox(Divider)->SetHorizontalAlignment(HAlign_Fill);
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, (RowHeight - IconSize) * 0.5f));

		UImage* Icon = WidgetTree->ConstructWidget<UImage>();
		if (UTexture2D* Texture = ResolveRewardIcon(Entry))
		{
			Icon->SetBrushFromTexture(Texture);
		}
		else if (BulletTemplate != nullptr)
		{
			Icon->SetBrush(BulletTemplate->GetBrush());
		}
		Icon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
		UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(Icon);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(4.f, 0.f, 16.f, 0.f));

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetFont(LabelTemplate->GetFont());
		Label->SetColorAndOpacity(LabelTemplate->GetColorAndOpacity());
		Label->SetText(Entry.Label);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		if (Entry.Amount > 0)
		{
			UTextBlock* Amount = WidgetTree->ConstructWidget<UTextBlock>();
			Amount->SetFont(AmountTemplate->GetFont());
			Amount->SetColorAndOpacity(AmountTemplate->GetColorAndOpacity());
			Amount->SetText(FText::Format(NSLOCTEXT("NUReward", "RowAmount", "+{0}"), FText::AsNumber(Entry.Amount)));
			UHorizontalBoxSlot* AmountSlot = Row->AddChildToHorizontalBox(Amount);
			AmountSlot->SetVerticalAlignment(VAlign_Center);
			AmountSlot->SetPadding(FMargin(12.f, 0.f, 4.f, 0.f));
		}
	}
	return true;
}

FText UNURewardPopup::BuildEntrySummary() const
{
	TArray<FString> Lines;
	Lines.Reserve(Entries.Num());

	for (const FNURewardEntry& Entry : Entries)
	{
		Lines.Add(Entry.Amount > 0
			? FString::Printf(TEXT("%s  x%d"), *Entry.Label.ToString(), Entry.Amount)
			: Entry.Label.ToString());
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

void UNURewardPopup::Close()
{
	// 등록 해제와 다음 대기 창 표시는 컨트롤러가 한다
	if (UNUNotificationUIController* Controller = GetNotificationController())
	{
		Controller->NotifyPopupClosed(this);
		return;
	}
	RemoveFromParent();
}

void UNURewardPopup::HandleCloseClicked()
{
	Close();
}
