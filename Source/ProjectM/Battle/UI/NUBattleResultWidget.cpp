// Fill out your copyright notice in the Description page of Project Settings.

#include "Battle/UI/NUBattleResultWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Battle/Avatar/NUBattlePlayerAvatar.h"
#include "Battle/UI/NUBattleResultEntryWidget.h"
#include "Battle/UI/NUBattleResultMemberWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "NUGAS/NUAbilitySystemComponent.h"

void UNUBattleResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 키를 직접 받으려면 포커스를 쥘 수 있어야 함
	SetIsFocusable(true);

	if (IsValid(ContinueButton))
	{
		ContinueButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleContinueClicked);
	}
}

void UNUBattleResultWidget::NativeDestruct()
{
	if (IsValid(ContinueButton))
	{
		ContinueButton->OnClicked.RemoveAll(this);
	}
	Motion.Finish();
	Super::NativeDestruct();
}

FReply UNUBattleResultWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 아무 키나 눌러 넘김, 확인 버튼이 있는 WBP 는 그 버튼이 Dismiss 를 부르면 됨
	// 등장 연출이 도는 중이면 첫 키는 연출을 끝 모습으로 보내기만 함
	if (!SkipReveal())
	{
		Dismiss();
	}

	return FReply::Handled();
}

void UNUBattleResultWidget::HandleContinueClicked()
{
	if (!SkipReveal())
	{
		Dismiss();
	}
}

void UNUBattleResultWidget::ShowResult(const FNUBattleResultView& InView)
{
	View = InView;
	bDismissed = false;

	SetRewardTexts(1.0);

	if (IsValid(ResultTitleText))
	{
		const bool bCaptured = !View.CapturedNaia.IsEmpty();
		ResultTitleText->SetText(View.Result == ENUBattleResult::Victory ? VictoryTitle : bCaptured ? CaptureTitle : EndTitle);
	}

	RebuildMembers();
	RebuildEntries(ItemBox, ItemPanel, View.Items, false);
	RebuildEntries(NaiaBox, NaiaPanel, View.CapturedNaia, true);

	StartReveal();

	// 로컬 · 넷 전투의 결과창이 모두 여기를 지남, 포획만 한 패배에는 승리음이 없음
	if (View.Result == ENUBattleResult::Victory)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Battle::Victory);
	}

	// 레벨이 오른 줄이 여럿이어도 한 번만
	if (View.Members.ContainsByPredicate([](const FNUBattleResultMember& Member) { return Member.bLeveledUp; }))
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Battle::LevelUp);
	}

	OnResultShown();
}

void UNUBattleResultWidget::Dismiss()
{
	if (bDismissed)
	{
		return;
	}

	bDismissed = true;

	OnDismissed.ExecuteIfBound();
}

void UNUBattleResultWidget::SetRewardTexts(double Alpha)
{
	const double T = FMath::Clamp(Alpha, 0.0, 1.0);
	if (IsValid(ExperienceText))
	{
		ExperienceText->SetText(FText::Format(ExperienceFormat, FText::AsNumber(FMath::RoundToInt64(View.Experience * T))));
	}

	if (IsValid(GoldText))
	{
		GoldText->SetText(FText::Format(GoldFormat, FText::AsNumber(FMath::RoundToInt64(View.Gold * T))));
	}
}

void UNUBattleResultWidget::RebuildMembers()
{
	MemberWidgets.Reset();

	if (!IsValid(MemberColumnBox))
	{
		return;
	}

	MemberColumnBox->ClearChildren();

	if (!MemberWidgetClass)
	{
		return;
	}

	const int32 PerColumn = FMath::Max(1, MembersPerColumn);
	UVerticalBox* Column = nullptr;

	for (int32 Index = 0; Index < View.Members.Num(); ++Index)
	{
		// 열이 없거나 이번 열이 다 찼으면 오른쪽에 열 추가
		if (Column == nullptr || Index % PerColumn == 0)
		{
			Column = NewObject<UVerticalBox>(this);
			if (!IsValid(Column))
			{
				return;
			}

			if (UHorizontalBoxSlot* ColumnSlot = Cast<UHorizontalBoxSlot>(MemberColumnBox->AddChild(Column)))
			{
				ColumnSlot->SetPadding(ColumnPadding);
				if (bFillSingleColumn && PerColumn >= View.Members.Num())
				{
					ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				}
			}
		}

		UNUBattleResultMemberWidget* MemberWidget = CreateWidget<UNUBattleResultMemberWidget>(this, MemberWidgetClass);
		if (!IsValid(MemberWidget))
		{
			continue;
		}

		MemberWidget->SetMember(View.Members[Index]);
		MemberWidgets.Add(MemberWidget);

		if (UVerticalBoxSlot* MemberSlot = Cast<UVerticalBoxSlot>(Column->AddChild(MemberWidget)))
		{
			MemberSlot->SetPadding(MemberPadding);
		}
	}
}

void UNUBattleResultWidget::RebuildEntries(UPanelWidget* Box, UWidget* Panel, const TArray<FNUBattleResultEntry>& Entries,
	bool bIsNaia)
{
	// 얻은 것이 없으면 목록만 비우는 게 아니라 칸을 통째로 숨긴다
	if (IsValid(Panel))
	{
		Panel->SetVisibility(Entries.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	if (!IsValid(Box))
	{
		return;
	}

	Box->ClearChildren();

	if (!EntryWidgetClass || Entries.IsEmpty())
	{
		return;
	}

	for (const FNUBattleResultEntry& Entry : Entries)
	{
		UNUBattleResultEntryWidget* EntryWidget = CreateWidget<UNUBattleResultEntryWidget>(this, EntryWidgetClass);
		if (!IsValid(EntryWidget))
		{
			continue;
		}

		EntryWidget->SetEntry(Entry, bIsNaia);
		if (UHorizontalBoxSlot* EntrySlot = Cast<UHorizontalBoxSlot>(Box->AddChild(EntryWidget)))
		{
			EntrySlot->SetPadding(EntryPadding);
		}
	}
}

void UNUBattleResultWidget::StartReveal()
{
	Motion.Finish();
	CountElapsed = -1.f;

	// 레벨 업 표시 --- 내 캐릭터 줄(첫 줄)이 올랐고 내 아바타를 하나로 가를 수 있을 때만
	CalloutAvatar.Reset();
	const bool bMeLeveled = View.Members.IsValidIndex(0) && View.Members[0].NaiaId == 0 && View.Members[0].bLeveledUp;
	if (IsValid(LevelUpCallout))
	{
		if (bMeLeveled)
		{
			const APlayerController* PC = GetOwningPlayer();
			const AActor* MyState = PC != nullptr ? PC->PlayerState : nullptr;
			TArray<ANUBattlePlayerAvatar*> Candidates;
			for (TActorIterator<ANUBattlePlayerAvatar> It(GetWorld()); It; ++It)
			{
				if (!It->IsAllyCombatant() || It->GetNaiaId() != INDEX_NONE)
				{
					continue;
				}
				// 로컬 전투는 ASC 소유자가 내 PlayerState --- 그러면 바로 정함
				const UNUAbilitySystemComponent* ASC = It->GetNUASC();
				if (ASC != nullptr && MyState != nullptr && ASC->GetOwnerActor() == MyState)
				{
					Candidates.Reset();
					Candidates.Add(*It);
					break;
				}
				Candidates.Add(*It);
			}
			if (Candidates.Num() == 1)
			{
				CalloutAvatar = Candidates[0];
			}
		}
		LevelUpCallout->SetVisibility(CalloutAvatar.IsValid() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		PlaceCallout();
	}

	if (RevealStagger <= 0.f)
	{
		for (UNUBattleResultMemberWidget* Member : MemberWidgets)
		{
			Member->PlayReveal(0.f);
		}
		if (CountUpSeconds > 0.f)
		{
			CountDelay = 0.f;
			CountElapsed = 0.f;
			SetRewardTexts(0.0);
		}
		return;
	}

	// 제목 → 시트 → 보상 숫자 → 명단 줄 → 획득 칸 → 레벨 업 표시
	float T = 0.f;
	if (IsValid(TitleRoot))
	{
		Motion.Add(TitleRoot, T, 0.5f, FVector2D(0.f, -18.f), 0.f, 1.06f);
	}
	if (IsValid(SheetRoot))
	{
		Motion.Add(SheetRoot, T + 0.08f, 0.45f, FVector2D(48.f, 0.f));
	}
	T += 0.32f;
	if (CountUpSeconds > 0.f)
	{
		CountDelay = T;
		CountElapsed = 0.f;
		SetRewardTexts(0.0);
	}
	for (UNUBattleResultMemberWidget* Member : MemberWidgets)
	{
		Motion.Add(Member, T, 0.3f, FVector2D(18.f, 0.f));
		Member->PlayReveal(T + 0.12f);
		T += RevealStagger;
	}
	for (UPanelWidget* Box : { ItemBox.Get(), NaiaBox.Get() })
	{
		if (!IsValid(Box))
		{
			continue;
		}
		for (int32 i = 0; i < Box->GetChildrenCount(); ++i)
		{
			Motion.Add(Box->GetChildAt(i), T, 0.28f, FVector2D(0.f, 10.f), 0.f, 0.85f);
			T += RevealStagger * 0.6f;
		}
	}
	if (CalloutAvatar.IsValid())
	{
		Motion.Add(LevelUpCallout, 0.6f, 0.4f, FVector2D(0.f, 14.f), 0.f, 1.3f);
	}
	Motion.Start();
}

bool UNUBattleResultWidget::SkipReveal()
{
	bool bWasRunning = Motion.IsRunning() || CountElapsed >= 0.f;
	for (UNUBattleResultMemberWidget* Member : MemberWidgets)
	{
		bWasRunning |= Member->IsRevealing();
		Member->FinishReveal();
	}
	Motion.Finish();
	if (CountElapsed >= 0.f)
	{
		CountElapsed = -1.f;
		SetRewardTexts(1.0);
	}
	return bWasRunning;
}

void UNUBattleResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	Motion.Advance(InDeltaTime);

	if (CountElapsed >= 0.f)
	{
		CountElapsed += InDeltaTime;
		const float T = FMath::Clamp((CountElapsed - CountDelay) / FMath::Max(CountUpSeconds, KINDA_SMALL_NUMBER), 0.f, 1.f);
		SetRewardTexts(1.0 - FMath::Pow(1.0 - T, 3.0));
		if (T >= 1.f)
		{
			CountElapsed = -1.f;
		}
	}

	PlaceCallout();
}

void UNUBattleResultWidget::PlaceCallout()
{
	if (!IsValid(LevelUpCallout) || !CalloutAvatar.IsValid())
	{
		return;
	}
	const ANUBattlePlayerAvatar* Avatar = Cast<ANUBattlePlayerAvatar>(CalloutAvatar.Get());
	const USceneComponent* Anchor = Avatar != nullptr ? Avatar->GetCommandUIAnchor() : nullptr;
	APlayerController* PC = GetOwningPlayer();
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LevelUpCallout->Slot);
	if (Anchor == nullptr || PC == nullptr || CanvasSlot == nullptr)
	{
		return;
	}
	FVector2D Screen;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Anchor->GetComponentLocation(), Screen, false))
	{
		return;
	}
	CanvasSlot->SetPosition(Screen + CalloutOffset);
}
