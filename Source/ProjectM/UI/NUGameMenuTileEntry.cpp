#include "UI/NUGameMenuTileEntry.h"

#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/NUGameMenuWidget.h"

void UNUGameMenuTileEntry::NativePreConstruct()
{
	Super::NativePreConstruct();

	// The common button applies LabelColor during PreConstruct. Menu labels must
	// inherit the button state foreground, just like the icon and stat glyph.
	if (IsValid(Txt_Label))
	{
		Txt_Label->SetColorAndOpacity(FSlateColor::UseForeground());
	}
}

void UNUGameMenuTileEntry::NativeConstruct()
{
	Super::NativeConstruct();
	OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleTileClicked);
}

void UNUGameMenuTileEntry::NativeDestruct()
{
	OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleTileClicked);
	Super::NativeDestruct();
}

void UNUGameMenuTileEntry::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	const UNUGameMenuTileItem* Item = Cast<UNUGameMenuTileItem>(ListItemObject);
	SetLabelText(Item ? Item->Label : FText::GetEmpty());
	SetShortcutText(Item ? Item->Shortcut : FText::GetEmpty());
	SetIconTexture(Item ? Item->Icon.Get() : nullptr);
	SetToolTipText(Item ? Item->Label : FText::GetEmpty());
	SetIsEnabled(Item != nullptr);

	// Each recycled row owns its atlas material instances. Hovering one row must
	// never recolor another row, and reused entries must update the atlas index.
	if (Btn_Common && Item)
	{
		const TArray<FName> AtlasRows = { TEXT("CharacterInfo"), TEXT("Equipment"), TEXT("Stats"),
			TEXT("Skills"), TEXT("GrowthTree"), TEXT("NaiaRoster"), TEXT("QuestJournal"), TEXT("WorldMap"), TEXT("Inventory") };
		const int32 RowIndex = AtlasRows.IndexOfByKey(Item->EntryId);
		FButtonStyle Style = Btn_Common->GetStyle();
		for (FSlateBrush* Brush : { &Style.Normal, &Style.Hovered, &Style.Pressed, &Style.Disabled })
		{
			UMaterialInterface* Template = Cast<UMaterialInterface>(Brush->GetResourceObject());
			while (UMaterialInstanceDynamic* Previous = Cast<UMaterialInstanceDynamic>(Template)) Template = Previous->Parent;
			float Unused = 0.f;
			if (Template && Template->GetScalarParameterValue(FMaterialParameterInfo(TEXT("RowIndex")), Unused))
			{
				UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Template, this);
				Dynamic->SetScalarParameterValue(TEXT("RowIndex"), static_cast<float>(RowIndex));
				Brush->SetResourceObject(Dynamic);
			}
		}
		Btn_Common->SetStyle(Style);
	}

	// 스탯은 기존 막대 그래프 장식을 사용한다. 풀에서 재사용될 때도 모두 갱신한다.
	const ESlateVisibility StatVisibility = Item && Item->EntryId == TEXT("Stats")
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	for (UImage* Bar : {HubStatBar1.Get(), HubStatBar2.Get(), HubStatBar3.Get(), HubStatBaseline.Get()})
	{
		if (IsValid(Bar))
		{
			Bar->SetVisibility(StatVisibility);
		}
	}
}

void UNUGameMenuTileEntry::HandleTileClicked()
{
	if (const UNUGameMenuTileItem* Item = GetListItem<UNUGameMenuTileItem>())
	{
		if (UNUGameMenuWidget* Menu = Item->Menu.Get())
		{
			Menu->ActivateEntry(Item->EntryId);
		}
	}
}
