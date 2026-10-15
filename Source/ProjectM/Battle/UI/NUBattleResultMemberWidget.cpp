// Fill out your copyright notice in the Description page of Project Settings.

#include "Battle/UI/NUBattleResultMemberWidget.h"

#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/UI/NUCharacterSlotWidget.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "PaperFlipbook.h"
#include "UI/NUFlipbookImage.h"

void UNUBattleResultMemberWidget::SetMember(const FNUBattleResultMember& InMember)
{
	Member = InMember;
	TargetRatio = Member.GetExperienceRatio();
	BarElapsed = -1.f;

	if (IsValid(NameText))
	{
		NameText->SetText(Member.DisplayName);
	}

	if (IsValid(LevelText))
	{
		LevelText->SetText(FText::AsNumber(Member.Level));
	}

	if (IsValid(ExperienceBar))
	{
		// 차오르는 연출이 있으면 결과창이 차례를 줄 때까지 비워 둔다
		ExperienceBar->SetPercent(BarFillSeconds > 0.f ? 0.f : TargetRatio);
	}

	if (IsValid(RemainingExperienceText))
	{
		// 경험치가 없는 줄(나이아)은 숫자를 띄우지 않는다, 0 을 띄우면 다 채운 것처럼 읽힘
		RemainingExperienceText->SetVisibility(Member.bHasExperience
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);

		if (Member.bHasExperience)
		{
			RemainingExperienceText->SetText(FText::AsNumber(Member.GetRemainingExperience()));
		}
	}

	if (IsValid(ExperienceRoot))
	{
		ExperienceRoot->SetVisibility(Member.bHasExperience ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IsValid(LevelUpRoot))
	{
		LevelUpRoot->SetVisibility(Member.bLeveledUp
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (IsValid(LevelUpText))
	{
		LevelUpText->SetText(FText::Format(LevelUpFormat, FText::AsNumber(Member.PreviousLevel), FText::AsNumber(Member.Level)));
	}

	// 얼굴 --- 캐릭터는 선택창 줄과 같은 겹친 플립북, 나이아는 표의 PreviewFlipbook
	const bool bIsNaia = Member.NaiaId > 0;
	const UGameInstance* GameInstance = GetGameInstance();
	if (IsValid(PortraitBox))
	{
		PortraitBox->SetVisibility(bIsNaia ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		const UNUCharacterManager* Characters = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;
		FNUCharacterSummary Summary;
		if (!bIsNaia && Characters != nullptr && Characters->GetSelectedOrTestCharacter(Summary))
		{
			UNUCharacterSlotWidget::BuildPortrait(this, PortraitBox, Summary, false, 0.f, PortraitPixelScale, true, nullptr, nullptr);
		}
	}

	const UNUNaiaDataSubsystem* NaiaData = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	const FNUNaiaDefinition* Definition = bIsNaia && NaiaData != nullptr ? NaiaData->FindDefinitionById(Member.NaiaId) : nullptr;
	if (IsValid(NaiaFigure))
	{
		UPaperFlipbook* Book = Definition != nullptr ? Definition->PreviewFlipbook.LoadSynchronous() : nullptr;
		NaiaFigure->SetVisibility(Book != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Book != nullptr)
		{
			NaiaFigure->SetFlipbookAtScale(Book, NaiaPixelScale);
		}
	}
	if (IsValid(ElementImage))
	{
		UTexture2D* Icon = nullptr;
		if (Definition != nullptr && Definition->ElementTag.IsValid())
		{
			FString Tail = Definition->ElementTag.GetTagName().ToString();
			int32 Dot = INDEX_NONE;
			if (Tail.FindLastChar(TEXT('.'), Dot))
			{
				Tail.RightChopInline(Dot + 1);
			}
			if (const TSoftObjectPtr<UTexture2D>* Found = ElementIcons.Find(FName(*Tail)))
			{
				Icon = Found->LoadSynchronous();
			}
		}
		ElementImage->SetVisibility(Icon != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Icon != nullptr)
		{
			ElementImage->SetBrushFromTexture(Icon, false);
		}
	}

	OnMemberSet();
}

void UNUBattleResultMemberWidget::PlayReveal(float Delay)
{
	if (BarFillSeconds <= 0.f)
	{
		return;
	}
	BarDelay = Delay;
	BarElapsed = 0.f;
	if (IsValid(ExperienceBar))
	{
		ExperienceBar->SetPercent(0.f);
	}
	// 레벨 업 덩어리는 바가 다 찬 순간 크게 튀어나온다
	if (Member.bLeveledUp && IsValid(LevelUpRoot))
	{
		Motion.Add(LevelUpRoot, Delay + BarFillSeconds, 0.32f, FVector2D(0.f, 6.f), 0.f, 1.35f);
		Motion.Start();
	}
}

void UNUBattleResultMemberWidget::FinishReveal()
{
	BarElapsed = -1.f;
	if (IsValid(ExperienceBar))
	{
		ExperienceBar->SetPercent(TargetRatio);
	}
	Motion.Finish();
}

void UNUBattleResultMemberWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	Motion.Advance(InDeltaTime);
	if (BarElapsed < 0.f)
	{
		return;
	}
	BarElapsed += InDeltaTime;
	const float T = FMath::Clamp((BarElapsed - BarDelay) / FMath::Max(BarFillSeconds, KINDA_SMALL_NUMBER), 0.f, 1.f);
	if (IsValid(ExperienceBar))
	{
		// 레벨이 올랐으면 한 번 끝까지 찼다가 새 레벨의 값으로 다시 차오른다
		const float Eased = 1.f - FMath::Pow(1.f - T, 3.f);
		const float Ratio = Member.bLeveledUp
			? (Eased < 0.6f ? Eased / 0.6f : (Eased - 0.6f) / 0.4f * TargetRatio)
			: Eased * TargetRatio;
		ExperienceBar->SetPercent(Ratio);
	}
	if (T >= 1.f)
	{
		BarElapsed = -1.f;
	}
}
