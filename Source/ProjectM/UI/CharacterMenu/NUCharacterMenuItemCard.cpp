#include "UI/CharacterMenu/NUCharacterMenuItemCard.h"

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUItemUI.h"

namespace
{
	template <typename TWidget>
	TWidget* FindNamed(const UUserWidget& Page, const FString& Name)
	{
		return Cast<TWidget>(Page.GetWidgetFromName(FName(*Name)));
	}

	void SetShown(UWidget* Widget, bool bShown)
	{
		if (Widget != nullptr)
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	void SetTextOf(UTextBlock* Widget, const FText& Value)
	{
		if (Widget != nullptr)
		{
			Widget->SetText(Value);
		}
	}

	// 아이콘 이미지는 시안에서 그리지 않는 브러시(NoDrawType)로 비워 둔 것이라 텍스처만 꽂으면 안 그려진다
	void SetImageTexture(UImage* Image, UTexture2D* Texture)
	{
		if (Image == nullptr)
		{
			return;
		}
		FSlateBrush Brush = Image->GetBrush();
		Brush.SetResourceObject(Texture);
		Brush.DrawAs = Texture != nullptr ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		Image->SetBrush(Brush);
		Image->SetVisibility(Texture != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

/////////////////////////////////////////////////////////////////
// FNUCharacterMenuSlotStyle
/////////////////////////////////////////////////////////////////

FNUCharacterMenuSlotStyle::FNUCharacterMenuSlotStyle()
{
	GradeColors.Add(ENUItemGrade::Common, FLinearColor(0.30f, 0.33f, 0.35f, 1.f));
	GradeColors.Add(ENUItemGrade::Uncommon, FLinearColor(0.10f, 0.36f, 0.22f, 1.f));
	GradeColors.Add(ENUItemGrade::Rare, FLinearColor(0.025f, 0.23f, 0.36f, 1.f));
	GradeColors.Add(ENUItemGrade::Heroic, FLinearColor(0.40f, 0.23f, 0.49f, 1.f));
	GradeColors.Add(ENUItemGrade::Legendary, FLinearColor(0.73f, 0.59f, 0.36f, 0.92f));
}

/////////////////////////////////////////////////////////////////
// UNUIndexedButtonRelay
/////////////////////////////////////////////////////////////////

UNUIndexedButtonRelay* UNUIndexedButtonRelay::Bind(UObject* Outer, UButton* Button, int32 InIndex, FOnNUIndexedButtonClicked InOnClicked)
{
	if (Button == nullptr)
	{
		return nullptr;
	}
	UNUIndexedButtonRelay* Relay = NewObject<UNUIndexedButtonRelay>(Outer);
	Relay->Index = InIndex;
	Relay->OnClicked = MoveTemp(InOnClicked);
	Button->OnClicked.AddUniqueDynamic(Relay, &UNUIndexedButtonRelay::HandleClicked);
	return Relay;
}

void UNUIndexedButtonRelay::HandleClicked()
{
	OnClicked.ExecuteIfBound(Index);
}

/////////////////////////////////////////////////////////////////
// FNUCharacterMenuSlot
/////////////////////////////////////////////////////////////////

bool FNUCharacterMenuSlot::Bind(const UUserWidget& Page, const FString& Prefix)
{
	Button = FindNamed<UButton>(Page, Prefix + TEXT("Button"));
	if (!Button.IsValid())
	{
		return false;
	}
	Icon = FindNamed<UImage>(Page, Prefix + TEXT("Icon"));
	Rarity = FindNamed<UImage>(Page, Prefix + TEXT("Rarity"));
	Level = FindNamed<UTextBlock>(Page, Prefix + TEXT("Level"));
	Empty = FindNamed<UWidget>(Page, Prefix + TEXT("Empty"));
	EmptyMark = FindNamed<UWidget>(Page, Prefix + TEXT("EmptyMark"));
	Held = FindNamed<UWidget>(Page, Prefix + TEXT("Held"));
	for (const TCHAR* Corner : { TEXT("TLH"), TEXT("TLV"), TEXT("TRH"), TEXT("TRV"), TEXT("BLH"), TEXT("BLV"), TEXT("BRH"), TEXT("BRV") })
	{
		if (UImage* Image = FindNamed<UImage>(Page, Prefix + Corner))
		{
			Corners.Add(Image);
		}
	}

	IdleStyle = Button->GetStyle();
	if (!Corners.IsEmpty())
	{
		IdleCornerTint = Corners[0]->GetBrush().TintColor;
	}
	return true;
}

void FNUCharacterMenuSlot::SetIcon(UTexture2D* Texture) const
{
	SetImageTexture(Icon.Get(), Texture);
}

void FNUCharacterMenuSlot::SetEmptyMarkVisible(bool bVisible) const
{
	SetShown(Empty.Get(), bVisible);
	SetShown(EmptyMark.Get(), bVisible);
}

void FNUCharacterMenuSlot::SetGrade(ENUItemGrade Grade, const FNUCharacterMenuSlotStyle& Style) const
{
	const FLinearColor* Color = Style.GradeColors.Find(Grade);
	if (Rarity.IsValid() && Color != nullptr)
	{
		Rarity->SetBrushTintColor(FSlateColor(*Color));
	}
	SetShown(Rarity.Get(), Color != nullptr);
}

void FNUCharacterMenuSlot::SetHeld(bool bHeld) const
{
	SetShown(Held.Get(), bHeld);
}

void FNUCharacterMenuSlot::SetSelected(bool bSelected, const FNUCharacterMenuSlotStyle& Style) const
{
	if (!Button.IsValid())
	{
		return;
	}
	FButtonStyle ButtonStyle = IdleStyle;
	if (bSelected)
	{
		ButtonStyle.Normal.OutlineSettings.Color = FSlateColor(Style.SelectedOutlineColor);
		for (FSlateBrush* State : { &ButtonStyle.Normal, &ButtonStyle.Hovered, &ButtonStyle.Pressed, &ButtonStyle.Disabled })
		{
			State->OutlineSettings.Width = Style.SelectedOutlineWidth;
		}
	}
	Button->SetStyle(ButtonStyle);

	const FSlateColor CornerTint = bSelected ? FSlateColor(Style.SelectedCornerTint) : IdleCornerTint;
	for (const TWeakObjectPtr<UImage>& Corner : Corners)
	{
		if (Corner.IsValid())
		{
			Corner->SetBrushTintColor(CornerTint);
		}
	}
}

/////////////////////////////////////////////////////////////////
// FNUCharacterMenuItemCard
/////////////////////////////////////////////////////////////////

void FNUCharacterMenuItemCard::Bind(const UUserWidget& Page)
{
	Icon = FindNamed<UImage>(Page, TEXT("SelectedItemIcon"));
	EmptyMark = FindNamed<UWidget>(Page, TEXT("SelectedItemEmptyMark"));
	Name = FindNamed<UTextBlock>(Page, TEXT("SelectedItemName"));
	Level = FindNamed<UTextBlock>(Page, TEXT("SelectedItemLevel"));
	Kind = FindNamed<UTextBlock>(Page, TEXT("SelectedItemRarity"));
	Description = FindNamed<UTextBlock>(Page, TEXT("SelectedItemDescription"));
	StatsHeading = FindNamed<UWidget>(Page, TEXT("ItemStatsHeading"));
	ActionButton = FindNamed<UButton>(Page, TEXT("EquipActionButton"));
	ActionLabel = FindNamed<UTextBlock>(Page, TEXT("EquipActionLabel"));

	const auto TextOf = [](const TWeakObjectPtr<UTextBlock>& Widget) { return Widget.IsValid() ? Widget->GetText() : FText::GetEmpty(); };
	DefaultName = TextOf(Name);
	DefaultLevel = TextOf(Level);
	DefaultKind = TextOf(Kind);
	DefaultKindColor = Kind.IsValid() ? Kind->GetColorAndOpacity() : FSlateColor();
	DefaultDescription = TextOf(Description);
	DefaultActionLabel = TextOf(ActionLabel);
	DisabledActionOpacity = ActionButton.IsValid() ? ActionButton->GetRenderOpacity() : 1.f;

	StatRows.Reset();
	for (const TCHAR* Prefix : { TEXT("ItemAttack"), TEXT("ItemStrength"), TEXT("ItemCritical"), TEXT("ItemSkill") })
	{
		FStatRow Row;
		Row.Label = FindNamed<UTextBlock>(Page, FString(Prefix) + TEXT("Label"));
		Row.Value = FindNamed<UTextBlock>(Page, FString(Prefix) + TEXT("Value"));
		Row.Rule = FindNamed<UWidget>(Page, FString(Prefix) + TEXT("Rule"));
		if (Row.Label.IsValid() && Row.Value.IsValid())
		{
			Row.DefaultLabel = Row.Label->GetText();
			Row.DefaultValue = Row.Value->GetText();
			StatRows.Add(MoveTemp(Row));
		}
	}
}

void FNUCharacterMenuItemCard::SetStatRow(const FStatRow& Row, const FText& Label, const FText& Value, bool bVisible) const
{
	SetTextOf(Row.Label.Get(), Label);
	SetTextOf(Row.Value.Get(), Value);
	SetShown(Row.Label.Get(), bVisible);
	SetShown(Row.Value.Get(), bVisible);
	SetShown(Row.Rule.Get(), bVisible);
}

void FNUCharacterMenuItemCard::SetActionEnabled(bool bEnabled) const
{
	if (ActionButton.IsValid())
	{
		ActionButton->SetIsEnabled(bEnabled);
		ActionButton->SetRenderOpacity(bEnabled ? 1.f : DisabledActionOpacity);
	}
}

void FNUCharacterMenuItemCard::Clear() const
{
	SetImageTexture(Icon.Get(), nullptr);
	SetShown(EmptyMark.Get(), true);
	SetTextOf(Name.Get(), DefaultName);
	SetTextOf(Level.Get(), DefaultLevel);
	SetTextOf(Kind.Get(), DefaultKind);
	if (Kind.IsValid())
	{
		Kind->SetColorAndOpacity(DefaultKindColor);
	}
	SetTextOf(Description.Get(), DefaultDescription);
	SetShown(StatsHeading.Get(), true);
	for (const FStatRow& Row : StatRows)
	{
		SetStatRow(Row, Row.DefaultLabel, Row.DefaultValue, true);
	}
	SetTextOf(ActionLabel.Get(), DefaultActionLabel);
	SetActionEnabled(false);
}

void FNUCharacterMenuItemCard::Show(const FNUItemDefRow& Def, FName RowName, int32 Count, const FText& InActionLabel, bool bActionEnabled,
	const FNUCharacterMenuSlotStyle& Style) const
{
	const bool bEquip = Def.ItemType == ENUItemType::Equipment;

	SetImageTexture(Icon.Get(), NUItemUI::LoadItemIcon(RowName, true)); // 이름과 등급은 카드 글자가 알려 준다
	SetShown(EmptyMark.Get(), false);
	SetTextOf(Name.Get(), Def.DisplayName.IsEmpty() ? FText::FromName(RowName) : Def.DisplayName);

	// 둘째 줄: 장비는 필요 레벨, 나머지는 가진 개수
	SetTextOf(Level.Get(), bEquip ? NUItemUI::GetRequiredLevelText(Def)
		: Count > 0 ? FText::Format(NSLOCTEXT("NUCharacterMenu", "OwnedCount", "보유 {0}개"), FText::AsNumber(Count))
		: FText::GetEmpty());

	// 셋째 줄: 등급이 있으면 "희귀 · 무기" / "희귀 · 소모품"을 등급 색으로, 없으면 분류("장비 · 무기" / "소모품" ...)를 WBP 색으로.
	// 등급 뒤에는 장비면 부위만, 나머지(부위 없는 장비 포함)는 분류를 잇는다.
	// 전용 직업·쓸 수 있는 곳까지 이으면 칸 폭을 넘어 잘린다 --- 설명 칸에 적는다
	const FLinearColor* GradeColor = Style.GradeColors.Find(Def.Grade);
	SetTextOf(Kind.Get(), GradeColor != nullptr
		? FText::Format(NSLOCTEXT("NUCharacterMenu", "GradeAndPart", "{0} · {1}"), NUEquip::GetGradeDisplayName(Def.Grade),
			bEquip && Def.EquipType != ENUEquipType::None ? NUEquip::GetEquipTypeDisplayName(Def.EquipType) : NUItemUI::GetCategoryText(Def))
		: NUItemUI::GetCategoryText(Def));
	if (Kind.IsValid())
	{
		Kind->SetColorAndOpacity(GradeColor != nullptr ? FSlateColor(*GradeColor) : DefaultKindColor);
	}

	// 설명 --- 장비는 설명문 대신 전용 직업: 기획 설명문이 스탯 요약이라 아래 스탯 줄과 같은 말이 두 번 보인다 (툴팁과 같은 규칙, 8/24).
	// 소모품은 설명 아래 줄에 쓸 수 있는 곳을 잇는다
	FText Body = bEquip ? NUItemUI::GetRequiredJobText(Def) : Def.Description;
	if (Def.ItemType == ENUItemType::Consumable)
	{
		const FText Usage = NUItemUI::GetUsageText(Def);
		Body = Body.IsEmpty() ? Usage : FText::Format(NSLOCTEXT("NUCharacterMenu", "DescriptionWithUsage", "{0}\n{1}"), Body, Usage);
	}
	SetTextOf(Description.Get(), Body);

	// 스탯 줄 --- 장비는 0이 아닌 스탯, 소모품은 효과 수치 한 줄
	TArray<TPair<FText, FText>, TInlineAllocator<4>> Lines;
	if (bEquip)
	{
		for (const FNUEquipStatEntry& Entry : NUItemUI::GetEquipStatEntries(Def.EquipStats))
		{
			if (!FMath::IsNearlyZero(Entry.Value))
			{
				Lines.Emplace(Entry.Label, FText::FromString(NUItemUI::FormatEquipStatValue(Entry.Value, Entry.bPercent)));
			}
		}
	}
	else if (const FString EffectValues = NUItemUI::GetEffectValuesText(Def); !EffectValues.IsEmpty())
	{
		Lines.Emplace(NSLOCTEXT("NUCharacterMenu", "EffectValue", "효과 수치"), FText::FromString(EffectValues));
	}
	for (int32 Index = 0; Index < StatRows.Num(); ++Index)
	{
		const bool bUsed = Lines.IsValidIndex(Index);
		SetStatRow(StatRows[Index], bUsed ? Lines[Index].Key : FText::GetEmpty(), bUsed ? Lines[Index].Value : FText::GetEmpty(), bUsed);
	}
	SetShown(StatsHeading.Get(), !Lines.IsEmpty());

	SetTextOf(ActionLabel.Get(), InActionLabel.IsEmpty() ? DefaultActionLabel : InActionLabel);
	SetActionEnabled(bActionEnabled);
}
