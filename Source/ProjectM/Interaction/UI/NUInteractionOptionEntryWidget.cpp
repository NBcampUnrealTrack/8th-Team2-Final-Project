#include "Interaction/UI/NUInteractionOptionEntryWidget.h"

#include "Interaction/Data/NUInteractionSettings.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UNUInteractionOptionEntryWidget::SetOption(const FNUInteractionOption& InOption)
{
	Option = InOption;
	bFocused = false;

	if (IsConstructed())
	{
		RefreshEntry();
		BP_OnOptionChanged(Option);
	}
}

bool UNUInteractionOptionEntryWidget::FocusEntry()
{
	if (IsValid(OptionButton) && OptionButton->GetIsEnabled())
	{
		SetFocused(true);
		return true;
	}

	return false;
}

void UNUInteractionOptionEntryWidget::SetFocused(bool bInFocused)
{
	if (bFocused == bInFocused)
	{
		return;
	}

	bFocused = bInFocused;
	RefreshEntry();
	BP_OnFocusChanged(bFocused);
}

void UNUInteractionOptionEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OptionButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	OptionButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	OptionButton->OnHovered.RemoveDynamic(this, &ThisClass::HandleHovered);
	OptionButton->OnHovered.AddDynamic(this, &ThisClass::HandleHovered);
	RefreshEntry();
	BP_OnOptionChanged(Option);
	BP_OnFocusChanged(bFocused);
}

void UNUInteractionOptionEntryWidget::NativeDestruct()
{
	if (IsValid(OptionButton))
	{
		OptionButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
		OptionButton->OnHovered.RemoveDynamic(this, &ThisClass::HandleHovered);
	}

	bFocused = false;
	Super::NativeDestruct();
}

namespace NUInteractionOptionEntry
{
	// 접두어는 퀘스트 상태([진행 가능] 등)에만 쓴다. 상점·창고·회복 같은 기능 줄은 문구가 기능을 말해 접두어가 중복이고,
	// 줄마다 접두어 유무가 달라 보기 어렵다.
	bool UsesCategoryPrefix(ENUInteractionOptionGroup Group)
	{
		switch (Group)
		{
		case ENUInteractionOptionGroup::QuestTurnIn:
		case ENUInteractionOptionGroup::QuestOffer:
		case ENUInteractionOptionGroup::QuestTarget:
		case ENUInteractionOptionGroup::QuestInProgress:
			return true;
		default:
			return false;
		}
	}
}

FText UNUInteractionOptionEntryWidget::ResolveCategoryText() const
{
	if (!bShowCategoryPrefix || !NUInteractionOptionEntry::UsesCategoryPrefix(Option.Group))
	{
		return FText::GetEmpty();
	}
	if (!Option.CategoryText.IsEmpty())
	{
		return Option.CategoryText;
	}

	return GetDefault<UNUInteractionSettings>()->QuestCategoryText;
}

FText UNUInteractionOptionEntryWidget::BuildDisplayText() const
{
	const FText Category = ResolveCategoryText();

	return Category.IsEmpty()
		? Option.DisplayText
		: FText::Format(NSLOCTEXT("NUInteraction", "CategorizedOption", "[{0}] {1}"), Category, Option.DisplayText);
}

void UNUInteractionOptionEntryWidget::RefreshEntry()
{
	if (IsValid(OptionText))
	{
		OptionText->SetText(BuildDisplayText());
		const FLinearColor TextColor = !Option.bEnabled
			? DisabledTextColor
			: (bFocused ? FocusedTextColor : NormalTextColor);
		OptionText->SetColorAndOpacity(FSlateColor(TextColor));
	}

	if (IsValid(OptionButton))
	{
		OptionButton->SetIsEnabled(Option.bEnabled && Option.IsValid());
	}

	if (IsValid(GroupIcon))
	{
		const TObjectPtr<UTexture2D>* Found = GroupIcons.Find(Option.Group);
		UTexture2D* Icon = Found != nullptr && *Found != nullptr ? Found->Get() : DefaultGroupIcon.Get();
		if (Option.Group == ENUInteractionOptionGroup::System && SystemGroupIcon != nullptr)
		{
			Icon = SystemGroupIcon;
		}
		GroupIcon->SetVisibility(Icon != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Icon != nullptr)
		{
			GroupIcon->SetBrushResourceObject(Icon);      // 그림만 바꾸고 크기는 WBP 값 그대로
		}
		GroupIcon->SetRenderOpacity(Option.bEnabled ? 1.f : 0.45f);
	}

	if (IsValid(FocusHighlight))
	{
		FocusHighlight->SetVisibility(bFocused && IsOptionEnabled() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNUInteractionOptionEntryWidget::HandleClicked()
{
	if (Option.bEnabled && Option.IsValid())
	{
		OnSelected.Broadcast(Option.Handle);
	}
}

void UNUInteractionOptionEntryWidget::HandleHovered()
{
	if (IsOptionEnabled())
	{
		OnFocused.Broadcast(Option.Handle);
	}
}
