// Fill out your copyright notice in the Description page of Project Settings.

#include "Battle/UI/NUBattleResultEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "PaperFlipbook.h"
#include "UI/NUFlipbookImage.h"

void UNUBattleResultEntryWidget::SetEntry(const FNUBattleResultEntry& InEntry, bool bInIsNaia)
{
	Entry = InEntry;
	bIsNaia = bInIsNaia;

	if (IsValid(NameText))
	{
		NameText->SetText(Entry.DisplayName);
	}

	if (IsValid(CountText))
	{
		CountText->SetText(FText::AsNumber(Entry.Count));
	}

	if (IsValid(IconImage))
	{
		if (IsValid(Entry.Icon))
		{
			IconImage->SetBrushFromTexture(Entry.Icon, true);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 아이콘이 없는 나이아는 대기 그림을 세운다
	if (IsValid(NaiaFigure))
	{
		UPaperFlipbook* Book = nullptr;
		if (bIsNaia && !IsValid(Entry.Icon))
		{
			const UGameInstance* GameInstance = GetGameInstance();
			const UNUNaiaDataSubsystem* NaiaData = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
			const FNUNaiaDefinition* Definition = NaiaData != nullptr ? NaiaData->FindDefinitionById(Entry.NumericId) : nullptr;
			Book = Definition != nullptr ? Definition->PreviewFlipbook.LoadSynchronous() : nullptr;
		}
		NaiaFigure->SetVisibility(Book != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Book != nullptr)
		{
			NaiaFigure->SetFlipbookAtScale(Book, NaiaPixelScale);
		}
	}

	if (IsValid(NewNaiaRoot))
	{
		NewNaiaRoot->SetVisibility(bIsNaia ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IsValid(CountRoot))
	{
		CountRoot->SetVisibility(bIsNaia && Entry.Count <= 1 ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	OnEntrySet();
}
