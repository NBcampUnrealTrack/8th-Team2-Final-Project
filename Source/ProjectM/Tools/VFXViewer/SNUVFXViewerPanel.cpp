// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/VFXViewer/SNUVFXViewerPanel.h"

#include "Battle/Data/NUBattleDataSettings.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Tools/VFXViewer/NUVFXViewerDirector.h"
#include "Tools/VFXViewer/SNUVFXViewerTimeline.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

namespace
{
	const FLinearColor SelectedColor(1.f, 0.72f, 0.22f);
	const FLinearColor SelectedTextColor(0.07f, 0.05f, 0.02f);
	const FLinearColor ButtonColor(0.22f, 0.24f, 0.30f);
	const FLinearColor PlayButtonColor(0.20f, 0.42f, 0.30f);
	const FLinearColor WarningButtonColor(0.60f, 0.30f, 0.20f);
	const FLinearColor PanelColor(0.02f, 0.025f, 0.04f, 0.94f);
	const FLinearColor CardColor(0.075f, 0.085f, 0.115f, 1.f);
	const FLinearColor FieldColor(0.035f, 0.04f, 0.055f, 1.f);
	const FLinearColor TitleColor(1.f, 0.85f, 0.45f);
	const FLinearColor TextColor(0.93f, 0.94f, 0.97f);
	const FLinearColor DimTextColor(0.62f, 0.65f, 0.72f);

	// 간격 (Slate 단위), 버튼이 붙어 보이지 않게 줄 · 버튼 · 묶음 사이를 띄움
	constexpr float PanelPadding = 12.f;
	constexpr float CardGap = 10.f;
	constexpr float CardPadding = 10.f;
	constexpr float RowGap = 8.f;
	constexpr float ButtonGap = 5.f;
	constexpr float LabelWidth = 76.f;
	constexpr float ButtonMinHeight = 26.f;

	// 후보 버튼 수, 넘는 후보는 [ ] 로 넘김
	constexpr int32 MaxCandidateButtons = 12;

	const FLinearColor CandidateBadgeColor(0.55f, 0.85f, 1.f);

	// 목록 높이, 패널 전체는 스크롤됨 (무대는 다섯 줄쯤)
	constexpr float StageListHeight = 124.f;
	constexpr float SkillListHeight = 280.f;
	constexpr float AssetListHeight = 200.f;

	// 무대 레벨 이름 앞머리는 다 같아서 목록에는 뺌
	FString ShortStageLabel(const FString& Label)
	{
		FString Short = Label;
		Short.RemoveFromStart(TEXT("L_Battle_"));
		return Short;
	}

	FSlateFontInfo PanelFont(int32 Size, const TCHAR* Weight = TEXT("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Weight, Size);
	}

	// 버튼 바탕에 쓸 색, 밝은 HDR 속성색을 글자가 읽히는 밝기로 줄임
	FLinearColor ToButtonColor(const FLinearColor& Color)
	{
		const float MaxChannel = FMath::Max(FMath::Max3(Color.R, Color.G, Color.B), 1.f);
		constexpr float Dim = 0.55f;
		return FLinearColor(Color.R / MaxChannel * Dim, Color.G / MaxChannel * Dim, Color.B / MaxChannel * Dim, 1.f);
	}

	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	// 묶음 · 입력 칸 바탕, 색은 BorderBackgroundColor 로 입힘
	const FSlateBrush* RoundedBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 6.f);
		return &Brush;
	}

	// 평평한 둥근 버튼, 색은 ButtonColorAndOpacity 로 입힘
	const FButtonStyle& FlatButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White, 4.f))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.3f, 1.3f, 1.3f, 1.f), 4.f))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.75f, 0.75f, 0.75f, 1.f), 4.f))
			.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.5f, 0.5f, 0.5f, 1.f), 4.f))
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		return Style;
	}

	// 줄 사이를 RowGap 만큼 띄워 쌓음
	TSharedRef<SVerticalBox> StackRows(std::initializer_list<TSharedRef<SWidget>> Rows)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		bool bFirst = true;
		for (const TSharedRef<SWidget>& Row : Rows)
		{
			Box->AddSlot()
			.AutoHeight()
			.Padding(0.f, bFirst ? 0.f : RowGap, 0.f, 0.f)
			[
				Row
			];
			bFirst = false;
		}
		return Box;
	}
}

void SNUVFXViewerPanel::Construct(const FArguments& InArgs)
{
	Director = InArgs._Director;

	if (const ANUVFXViewerDirector* Owner = Director.Get())
	{
		for (const FNUVFXViewerChoice& Choice : Owner->GetStageChoices())
		{
			StageItems.Add(MakeShared<FNUVFXViewerChoice>(Choice));
		}
	}

	RebuildStageFilter();
	RebuildSkillFilter();
	RebuildAssetFilter();

	// 게임 뷰포트가 이미 UI 배율(DPI)을 곱해 주므로 따로 키우지 않음, 감독이 3D 칸을 잡을 때도 같은 배율을 씀
	ChildSlot
	[
		SNew(SHorizontalBox)

		// 왼쪽 패널 (Tab 으로 숨김)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ANUVFXViewerDirector::PanelWidth)
			.Visibility(this, &SNUVFXViewerPanel::GetPanelVisibility)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(PanelColor)
				.Padding(FMargin(PanelPadding, 10.f, PanelPadding - 4.f, 8.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 4.f, CardGap) [ MakeHeader() ]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						SAssignNew(PanelScroll, SScrollBox)
						+ SScrollBox::Slot().Padding(0.f, 0.f, 4.f, 0.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight() [ MakeSceneCard() ]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, CardGap, 0.f, 0.f) [ MakeSourceCard() ]
							+ SVerticalBox::Slot().AutoHeight() [ MakePendingCard() ]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, CardGap, 0.f, 0.f) [ MakeOptionsCard() ]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, CardGap, 0.f, 0.f) [ MakeReportCard() ]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 4.f, 0.f) [ MakeShortcutHint() ]
				]
			]
		]

		// 3D 화면 칸과 그 아래 진행바
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			[
				// 멀리 감는 동안만 직전 화면으로 덮음 (그동안 월드를 안 그림)
				SNew(SBox)
				.Visibility(EVisibility::HitTestInvisible)
				.WidthOverride_Lambda([this]()
				{
					return FOptionalSize(GetDirector() ? GetDirector()->GetViewAreaSlateSize().X : 0.f);
				})
				.HeightOverride_Lambda([this]()
				{
					return FOptionalSize(GetDirector() ? GetDirector()->GetViewAreaSlateSize().Y : 0.f);
				})
				[
					SNew(SImage)
					.Image_Lambda([this]()
					{
						return GetDirector() ? GetDirector()->GetFreezeFrameBrush() : nullptr;
					})
					.Visibility_Lambda([this]()
					{
						return GetDirector() && GetDirector()->IsFreezeFrameVisible() ? EVisibility::HitTestInvisible : EVisibility::Hidden;
					})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			[
				MakeTransportBar()
			]
		]
	];

	// 빈 곳은 3D 화면 클릭을 막지 않게
	SetVisibility(EVisibility::SelfHitTestInvisible);
}

void SNUVFXViewerPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const ANUVFXViewerDirector* Owner = GetDirector();
	if (Owner && Owner->GetPendingVersion() != LastPendingVersion)
	{
		RebuildPendingRows();
	}

	if (!Owner || !SkillList.IsValid() || Owner->GetSetup().Mode != ENUVFXViewerMode::Skill
		|| Owner->GetSetup().SkillId == LastSyncedSkillId)
	{
		return;
	}

	// 목록에서 누른 것이면 이미 선택돼 있음, 단축키로 옮긴 것이면 선택하고 보이게 굴림 (Direct 라 다시 틀지 않음)
	LastSyncedSkillId = Owner->GetSetup().SkillId;
	const TSharedPtr<FNUVFXViewerSkillItem>* Found = FilteredSkills.FindByPredicate(
		[this](const TSharedPtr<FNUVFXViewerSkillItem>& Item)
		{
			return Item.IsValid() && Item->SkillId == LastSyncedSkillId;
		});

	if (Found)
	{
		SkillList->SetSelection(*Found, ESelectInfo::Direct);
		SkillList->RequestScrollIntoView(*Found);
	}
}

void SNUVFXViewerPanel::ScrollPanelToEnd()
{
	if (PanelScroll.IsValid())
	{
		PanelScroll->ScrollToEnd();
	}
}

void SNUVFXViewerPanel::RefreshLists()
{
	RebuildStageFilter();
	RebuildSkillFilter();
	RebuildAssetFilter();
}

EVisibility SNUVFXViewerPanel::GetPanelVisibility() const
{
	return GetDirector() && GetDirector()->IsPanelVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

/////////////////////////////////////////////////////////////////
// 공용 조각
/////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SNUVFXViewerPanel::MakeCard(const FText& Title, const TSharedRef<SWidget>& Content) const
{
	return SNew(SBorder)
		.BorderImage(RoundedBrush())
		.BorderBackgroundColor(CardColor)
		.Padding(FMargin(CardPadding, 8.f, CardPadding, CardPadding))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock)
				.Font(PanelFont(11, TEXT("Bold")))
				.ColorAndOpacity(TitleColor)
				.Text(Title)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				Content
			]
		];
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeButton(const TAttribute<FText>& Label, TFunction<bool()> IsSelected,
	TFunction<void()> OnClick) const
{
	return MakeButton(Label, MoveTemp(IsSelected), MoveTemp(OnClick), ButtonColor);
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeButton(const TAttribute<FText>& Label, TFunction<bool()> IsSelected,
	TFunction<void()> OnClick, const FLinearColor& BaseColor, const FText& ToolTip) const
{
	// 포커스를 안 받게 해야 Space(일시정지)가 마지막에 누른 버튼을 다시 누르지 않음
	return SNew(SBox)
		.MinDesiredHeight(ButtonMinHeight)
		[
			SNew(SButton)
			.ButtonStyle(&FlatButtonStyle())
			.IsFocusable(false)
			.ContentPadding(FMargin(10.f, 4.f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ToolTipText(ToolTip)
			.ButtonColorAndOpacity_Lambda([IsSelected, BaseColor]()
			{
				return FSlateColor(IsSelected && IsSelected() ? SelectedColor : BaseColor);
			})
			.OnClicked_Lambda([OnClick]()
			{
				if (OnClick)
				{
					OnClick();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(PanelFont(10))
				.Text(Label)
				.ColorAndOpacity_Lambda([IsSelected]()
				{
					return FSlateColor(IsSelected && IsSelected() ? SelectedTextColor : TextColor);
				})
			]
		];
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeLabeledRow(const FText& Label, const TSharedRef<SWidget>& Content) const
{
	// 이름은 첫 줄 버튼 글자 높이에 맞춤 (버튼이 여러 줄로 접혀도 위에 붙음)
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(0.f, 5.f, 6.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(LabelWidth)
			[
				SNew(STextBlock)
				.Font(PanelFont(10))
				.ColorAndOpacity(DimTextColor)
				.Text(Label)
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			Content
		];
}

TSharedRef<SWrapBox> SNUVFXViewerPanel::MakeWrap() const
{
	return SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(ButtonGap, ButtonGap));
}

/////////////////////////////////////////////////////////////////
// 머리 · 무대 · 전투원
/////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SNUVFXViewerPanel::MakeHeader()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font(PanelFont(15, TEXT("Bold")))
			.ColorAndOpacity(TitleColor)
			.Text(FText::FromString(TEXT("전투 VFX 뷰어")))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Font(PanelFont(10))
			.ColorAndOpacity(DimTextColor)
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				const ANUVFXViewerDirector* Owner = GetDirector();
				return Owner ? Owner->GetStatusText() : FText::GetEmpty();
			})
		];
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeSceneCard()
{
	return MakeCard(FText::FromString(TEXT("무대 · 전투원")),
		StackRows({
			MakeStageRow(),
			MakeCasterRow(),
			MakeGenderRow(),
			MakeJobRow(),
			MakeNaiaRow(false),
			MakeNaiaRow(true),
			MakeEnemyCountRow(),
			MakeTargetRow()
		}));
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeStageRow()
{
	return MakeLabeledRow(FText::FromString(TEXT("무대")),
		StackRows({
			SNew(SEditableTextBox)
			.Font(PanelFont(10))
			.HintText(FText::FromString(TEXT("무대 검색")))
			.OnTextChanged_Lambda([this](const FText& Text)
			{
				StageFilter = Text.ToString();
				RebuildStageFilter();
			}),

			SNew(SBorder)
			.BorderImage(RoundedBrush())
			.BorderBackgroundColor(FieldColor)
			.Padding(FMargin(2.f, 4.f))
			[
				SNew(SBox)
				.HeightOverride(StageListHeight)
				[
					SAssignNew(StageList, SListView<TSharedPtr<FNUVFXViewerChoice>>)
					.ListItemsSource(&FilteredStages)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SNUVFXViewerPanel::GenerateStageRow)
					.OnMouseButtonClick(this, &SNUVFXViewerPanel::HandleStageClicked)
				]
			]
		}));
}

void SNUVFXViewerPanel::RebuildStageFilter()
{
	FilteredStages.Reset();
	for (const TSharedPtr<FNUVFXViewerChoice>& Item : StageItems)
	{
		if (Item.IsValid() && (StageFilter.IsEmpty() || Item->Label.Contains(StageFilter)))
		{
			FilteredStages.Add(Item);
		}
	}

	if (StageList.IsValid())
	{
		StageList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SNUVFXViewerPanel::GenerateStageRow(TSharedPtr<FNUVFXViewerChoice> Item,
	const TSharedRef<STableViewBase>& Owner)
{
	const int32 StageIndex = Item.IsValid() ? Item->Id : INDEX_NONE;

	return SNew(STableRow<TSharedPtr<FNUVFXViewerChoice>>, Owner)
		.Padding(FMargin(6.f, 3.f))
		.ToolTipText(FText::FromString(Item.IsValid() ? Item->Path.ToString() : FString()))
		[
			SNew(STextBlock)
			.Font(PanelFont(10))
			.ColorAndOpacity_Lambda([this, StageIndex]()
			{
				const ANUVFXViewerDirector* Viewer = GetDirector();
				return FSlateColor(Viewer && Viewer->GetSetup().StageIndex == StageIndex ? SelectedColor : TextColor);
			})
			.Text(FText::FromString(Item.IsValid() ? ShortStageLabel(Item->Label) : FString()))
		];
}

void SNUVFXViewerPanel::HandleStageClicked(TSharedPtr<FNUVFXViewerChoice> Item)
{
	// 무대를 바꾸면 레벨을 다시 올리므로 방향키 이동으로는 안 바꾸고 클릭으로만 받음
	if (Item.IsValid() && GetDirector())
	{
		GetDirector()->SetStage(Item->Id);
	}
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeCasterRow()
{
	TSharedRef<SWrapBox> Wrap = MakeWrap();

	const TPair<ENUVFXViewerCaster, const TCHAR*> Casters[] =
	{
		{ ENUVFXViewerCaster::Player, TEXT("캐릭터") },
		{ ENUVFXViewerCaster::AllyNaia, TEXT("아군 나이아") },
		{ ENUVFXViewerCaster::Enemy, TEXT("적") }
	};

	for (const TPair<ENUVFXViewerCaster, const TCHAR*>& Caster : Casters)
	{
		const ENUVFXViewerCaster Kind = Caster.Key;
		Wrap->AddSlot()
		[
			MakeButton(FText::FromString(Caster.Value),
				[this, Kind]() { return GetDirector() && GetDirector()->GetSetup().Caster == Kind; },
				[this, Kind]() { if (GetDirector()) { GetDirector()->SetCaster(Kind); } })
		];
	}

	return MakeLabeledRow(FText::FromString(TEXT("시전자")), Wrap);
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeGenderRow()
{
	TSharedRef<SWrapBox> Wrap = MakeWrap();

	for (int32 Gender = 0; Gender < 2; ++Gender)
	{
		Wrap->AddSlot()
		[
			MakeButton(FText::FromString(Gender == 0 ? TEXT("남캐") : TEXT("여캐")),
				[this, Gender]() { return GetDirector() && GetDirector()->GetSetup().Gender == Gender; },
				[this, Gender]() { if (GetDirector()) { GetDirector()->SetGender(Gender); } },
				ButtonColor,
				FText::FromString(TEXT("그 성별에 지금 직업 그림이 없으면 그 성별의 첫 직업으로 바뀜")))
		];
	}

	return MakeLabeledRow(FText::FromString(TEXT("성별")), Wrap);
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeJobRow()
{
	TSharedRef<SWrapBox> Wrap = MakeWrap();

	// 지금 성별에 전투 그림이 있는 직업만 보임 (같은 직업이 남 · 여 두 줄일 수 있어 성별로 거름)
	const ANUVFXViewerDirector* Owner = GetDirector();
	if (Owner)
	{
		for (const FNUVFXViewerChoice& Choice : Owner->GetJobChoices())
		{
			const int32 JobId = Choice.Id;
			const int32 Gender = Choice.Gender;
			Wrap->AddSlot()
			[
				SNew(SBox)
				.Visibility_Lambda([this, Gender]()
				{
					return GetDirector() && GetDirector()->GetSetup().Gender == Gender ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					MakeButton(FText::FromString(Choice.Label),
						[this, JobId, Gender]()
						{
							return GetDirector() && GetDirector()->GetSetup().JobId == JobId && GetDirector()->GetSetup().Gender == Gender;
						},
						[this, JobId]() { if (GetDirector()) { GetDirector()->SetJob(JobId); } })
				]
			];
		}
	}

	return MakeLabeledRow(FText::FromString(TEXT("직업")), Wrap);
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeNaiaRow(bool bEnemy)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeButton(FText::FromString(TEXT("<")), nullptr, [this, bEnemy]() { StepNaia(bEnemy, -1); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(ButtonGap, 0.f)
		[
			SNew(SBorder)
			.BorderImage(RoundedBrush())
			.BorderBackgroundColor(FieldColor)
			.Padding(FMargin(8.f, 0.f))
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredHeight(ButtonMinHeight)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(PanelFont(10))
					.ColorAndOpacity(TextColor)
					.Text_Lambda([this, bEnemy]()
					{
						const ANUVFXViewerDirector* Owner = GetDirector();
						if (!Owner)
						{
							return FText::GetEmpty();
						}

						const int32 Id = bEnemy ? Owner->GetSetup().EnemyNaiaId : Owner->GetSetup().AllyNaiaId;
						return FText::FromString(Owner->GetChoiceLabel(Owner->GetNaiaChoices(), Id));
					})
				]
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeButton(FText::FromString(TEXT(">")), nullptr, [this, bEnemy]() { StepNaia(bEnemy, 1); })
		];

	return MakeLabeledRow(FText::FromString(bEnemy ? TEXT("적 나이아") : TEXT("아군 나이아")), Row);
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeEnemyCountRow()
{
	TSharedRef<SWrapBox> Wrap = MakeWrap();

	for (int32 Count = 1; Count <= 3; ++Count)
	{
		Wrap->AddSlot()
		[
			MakeButton(FText::FromString(FString::Printf(TEXT("%d마리"), Count)),
				[this, Count]() { return GetDirector() && GetDirector()->GetSetup().EnemyCount == Count; },
				[this, Count]() { if (GetDirector()) { GetDirector()->SetEnemyCount(Count); } })
		];
	}

	return MakeLabeledRow(FText::FromString(TEXT("적 수")), Wrap);
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeTargetRow()
{
	TSharedRef<SWrapBox> Wrap = MakeWrap();

	// 아군이 시전하면 적 번호, 적이 시전하면 캐릭터 · 나이아
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Wrap->AddSlot()
		[
			SNew(SBox)
			.Visibility_Lambda([this, Index]()
			{
				const ANUVFXViewerDirector* Owner = GetDirector();
				if (!Owner)
				{
					return EVisibility::Collapsed;
				}

				const FNUVFXViewerSetup& Setup = Owner->GetSetup();
				const int32 Count = Setup.Caster == ENUVFXViewerCaster::Enemy ? 2 : Setup.EnemyCount;
				return Index < Count ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				MakeButton(
					TAttribute<FText>::CreateLambda([this, Index]()
					{
						const ANUVFXViewerDirector* Owner = GetDirector();
						if (Owner && Owner->GetSetup().Caster == ENUVFXViewerCaster::Enemy)
						{
							return FText::FromString(Index == 0 ? TEXT("캐릭터") : TEXT("아군 나이아"));
						}
						return FText::FromString(FString::Printf(TEXT("적 %d"), Index + 1));
					}),
					[this, Index]() { return GetDirector() && GetDirector()->GetSetup().TargetIndex == Index; },
					[this, Index]() { if (GetDirector()) { GetDirector()->SetTargetIndex(Index); } })
			]
		];
	}

	return MakeLabeledRow(FText::FromString(TEXT("대상")), Wrap);
}

void SNUVFXViewerPanel::StepNaia(bool bEnemy, int32 Delta)
{
	ANUVFXViewerDirector* Owner = GetDirector();
	if (!Owner || Owner->GetNaiaChoices().IsEmpty())
	{
		return;
	}

	const TArray<FNUVFXViewerChoice>& Choices = Owner->GetNaiaChoices();
	const int32 CurrentId = bEnemy ? Owner->GetSetup().EnemyNaiaId : Owner->GetSetup().AllyNaiaId;
	const int32 CurrentIndex = FMath::Max(0, Choices.IndexOfByPredicate([CurrentId](const FNUVFXViewerChoice& Choice)
	{
		return Choice.Id == CurrentId;
	}));
	const int32 NextIndex = (CurrentIndex + Delta + Choices.Num()) % Choices.Num();

	if (bEnemy)
	{
		Owner->SetEnemyNaia(Choices[NextIndex].Id);
	}
	else
	{
		Owner->SetAllyNaia(Choices[NextIndex].Id);
	}
}

/////////////////////////////////////////////////////////////////
// 재생할 연출 (스킬 표 · 직접 지정)
/////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SNUVFXViewerPanel::MakeSourceCard()
{
	const auto IsMode = [this](ENUVFXViewerMode Mode)
	{
		return GetDirector() && GetDirector()->GetSetup().Mode == Mode;
	};

	TSharedRef<SWidget> Tabs = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0.f, 0.f, ButtonGap * 0.5f, 0.f)
		[
			MakeButton(FText::FromString(TEXT("스킬 표")),
				[IsMode]() { return IsMode(ENUVFXViewerMode::Skill); },
				[this]() { if (GetDirector()) { GetDirector()->SetMode(ENUVFXViewerMode::Skill); } },
				ButtonColor,
				FText::FromString(TEXT("DT_SkillVFX 의 행을 골라 게임과 같은 규칙으로 재생")))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(ButtonGap * 0.5f, 0.f, 0.f, 0.f)
		[
			MakeButton(FText::FromString(TEXT("직접 지정")),
				[IsMode]() { return IsMode(ENUVFXViewerMode::Custom); },
				[this]() { if (GetDirector()) { GetDirector()->SetMode(ENUVFXViewerMode::Custom); } },
				ButtonColor,
				FText::FromString(TEXT("Cast · Proj · Impact 세 칸에 /Game/VFX 의 나이아가라를 직접 골라 재생")))
		];

	return MakeCard(FText::FromString(TEXT("재생할 연출")),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			Tabs
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, RowGap, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([IsMode]()
			{
				return IsMode(ENUVFXViewerMode::Skill) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				MakeSkillSection()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, RowGap, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([IsMode]()
			{
				return IsMode(ENUVFXViewerMode::Custom) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				MakeCustomSection()
			]
		]);
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeSkillSection()
{
	return StackRows({
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(SEditableTextBox)
			.Font(PanelFont(10))
			.HintText(FText::FromString(TEXT("검색 (번호 · 이름 · 직업 이름 · 나이아 · 보스)")))
			.OnTextChanged_Lambda([this](const FText& Text)
			{
				SkillFilter = Text.ToString();
				RebuildSkillFilter();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ButtonGap, 0.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]()
			{
				return GetDirector() && GetDirector()->HasCandidates() ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				MakeButton(FText::FromString(TEXT("후보 있는 것만")),
					[this]() { return bCandidatesOnly; },
					[this]()
					{
						bCandidatesOnly = !bCandidatesOnly;
						RebuildSkillFilter();
					},
					ButtonColor,
					FText::FromString(TEXT("후보 파일에 후보가 있는 스킬만 목록에 둠")))
			]
		],

		SNew(SBorder)
		.BorderImage(RoundedBrush())
		.BorderBackgroundColor(FieldColor)
		.Padding(FMargin(2.f, 4.f))
		[
			SNew(SBox)
			.HeightOverride(SkillListHeight)
			[
				SAssignNew(SkillList, SListView<TSharedPtr<FNUVFXViewerSkillItem>>)
				.ListItemsSource(&FilteredSkills)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SNUVFXViewerPanel::GenerateSkillRow)
				.OnMouseButtonClick(this, &SNUVFXViewerPanel::HandleSkillClicked)
				.OnSelectionChanged(this, &SNUVFXViewerPanel::HandleSkillSelectionChanged)
			]
		],

		MakeCandidateSection(),

		MakeButton(FText::FromString(TEXT("이 스킬을 직접 지정 칸으로 복사")), nullptr,
			[this]() { if (GetDirector()) { GetDirector()->CopySkillToCustom(); } },
			ButtonColor,
			FText::FromString(TEXT("마지막 재생의 세 칸 · 색 · 이동 · 타격 수를 그대로 옮김, 한 칸만 바꿔 비교할 때")))
	});
}

void SNUVFXViewerPanel::RebuildSkillFilter()
{
	FilteredSkills.Reset();

	const ANUVFXViewerDirector* Owner = GetDirector();
	if (Owner)
	{
		for (const TSharedPtr<FNUVFXViewerSkillItem>& Item : Owner->GetSkillItems())
		{
			if (Item.IsValid() && (SkillFilter.IsEmpty() || Item->SearchKey.Contains(SkillFilter))
				&& (!bCandidatesOnly || Owner->GetCandidateCount(Item->SkillId) > 0))
			{
				FilteredSkills.Add(Item);
			}
		}
	}

	if (SkillList.IsValid())
	{
		SkillList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SNUVFXViewerPanel::GenerateSkillRow(TSharedPtr<FNUVFXViewerSkillItem> Item,
	const TSharedRef<STableViewBase>& Owner)
{
	const int32 SkillId = Item.IsValid() ? Item->SkillId : 0;

	return SNew(STableRow<TSharedPtr<FNUVFXViewerSkillItem>>, Owner)
		.Padding(FMargin(6.f, 3.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.Font(PanelFont(10))
				.ColorAndOpacity_Lambda([this, SkillId]()
				{
					const ANUVFXViewerDirector* Viewer = GetDirector();
					const bool bCurrent = Viewer && Viewer->GetSetup().Mode == ENUVFXViewerMode::Skill
						&& Viewer->GetSetup().SkillId == SkillId;
					return FSlateColor(bCurrent ? SelectedColor : TextColor);
				})
				.Text(FText::FromString(Item.IsValid() ? Item->Label : FString()))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Font(PanelFont(9, TEXT("Bold")))
				.ColorAndOpacity(CandidateBadgeColor)
				.Text_Lambda([this, SkillId]()
				{
					const ANUVFXViewerDirector* Viewer = GetDirector();
					const int32 Count = Viewer ? Viewer->GetCandidateCount(SkillId) : 0;
					return Count > 1 ? FText::FromString(FString::Printf(TEXT("후보 %d"), Count - 1)) : FText::GetEmpty();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Font(PanelFont(9))
				.ColorAndOpacity(DimTextColor)
				.Text(FText::FromString(Item.IsValid() ? Item->Category : FString()))
			]
		];
}

void SNUVFXViewerPanel::HandleSkillClicked(TSharedPtr<FNUVFXViewerSkillItem> Item)
{
	// 같은 줄을 다시 눌러도 다시 틀 수 있게 클릭으로 받음
	if (Item.IsValid() && GetDirector())
	{
		GetDirector()->SelectSkill(Item->SkillId);
	}
}

void SNUVFXViewerPanel::HandleSkillSelectionChanged(TSharedPtr<FNUVFXViewerSkillItem> Item, ESelectInfo::Type SelectInfo)
{
	// 마우스는 HandleSkillClicked 가 받음, 여기서는 방향키로 옮길 때만
	if (SelectInfo == ESelectInfo::OnNavigation && Item.IsValid() && GetDirector())
	{
		GetDirector()->SelectSkill(Item->SkillId);
	}
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeCustomSection()
{
	TSharedRef<SWrapBox> MotionWrap = MakeWrap();
	const TPair<ENUBattleActionMotion, const TCHAR*> Motions[] =
	{
		{ ENUBattleActionMotion::InPlace, TEXT("제자리") },
		{ ENUBattleActionMotion::MoveToCenter, TEXT("중앙으로") },
		{ ENUBattleActionMotion::MoveToTarget, TEXT("대상에게") }
	};

	for (const TPair<ENUBattleActionMotion, const TCHAR*>& Motion : Motions)
	{
		const ENUBattleActionMotion Value = Motion.Key;
		MotionWrap->AddSlot()
		[
			MakeButton(FText::FromString(Motion.Value),
				[this, Value]() { return GetDirector() && GetDirector()->GetSetup().CustomMotion == Value; },
				[this, Value]() { if (GetDirector()) { GetDirector()->SetCustomMotion(Value); } })
		];
	}

	TSharedRef<SHorizontalBox> HitRow = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeButton(FText::FromString(TEXT("-")), nullptr, [this]()
			{
				if (GetDirector())
				{
					GetDirector()->SetCustomHitCount(GetDirector()->GetSetup().CustomHitCount - 1);
				}
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(12.f, 0.f)
		[
			SNew(STextBlock)
			.Font(PanelFont(11, TEXT("Bold")))
			.ColorAndOpacity(TextColor)
			.Text_Lambda([this]()
			{
				return FText::AsNumber(GetDirector() ? GetDirector()->GetSetup().CustomHitCount : 1);
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeButton(FText::FromString(TEXT("+")), nullptr, [this]()
			{
				if (GetDirector())
				{
					GetDirector()->SetCustomHitCount(GetDirector()->GetSetup().CustomHitCount + 1);
				}
			})
		];

	return StackRows({
		MakeCustomSlotRow(0, FText::FromString(TEXT("Cast"))),
		MakeCustomSlotRow(1, FText::FromString(TEXT("Proj"))),
		MakeCustomSlotRow(2, FText::FromString(TEXT("Impact"))),

		SNew(SBox)
		.Visibility_Lambda([this]()
		{
			const ANUVFXViewerDirector* Owner = GetDirector();
			const bool bImpactOnly = Owner && !Owner->GetSetup().CustomImpact.IsNull()
				&& Owner->GetSetup().CustomProjectile.IsNull();
			return bImpactOnly ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			MakeButton(FText::FromString(TEXT("임팩트만 있음 - 게임처럼 운반체 투사체 채우기")), nullptr,
				[this]() { if (GetDirector()) { GetDirector()->FillCarrierProjectile(); } },
				WarningButtonColor,
				FText::FromString(TEXT("임팩트는 투사체가 도착할 때만 뜸, 게임이 쓰는 보이지 않는 투사체(NS_Job_Proj_Carrier)를 넣음")))
		],

		SNew(SEditableTextBox)
		.Font(PanelFont(10))
		.HintText(FText::FromString(TEXT("나이아가라 검색 (/Game/VFX)")))
		.OnTextChanged_Lambda([this](const FText& Text)
		{
			AssetFilter = Text.ToString();
			RebuildAssetFilter();
		}),

		SNew(SBorder)
		.BorderImage(RoundedBrush())
		.BorderBackgroundColor(FieldColor)
		.Padding(FMargin(2.f, 4.f))
		[
			SNew(SBox)
			.HeightOverride(AssetListHeight)
			[
				SAssignNew(AssetList, SListView<TSharedPtr<FNUVFXViewerAssetItem>>)
				.ListItemsSource(&FilteredAssets)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SNUVFXViewerPanel::GenerateAssetRow)
			]
		],

		MakeColorRows(),
		MakeLabeledRow(FText::FromString(TEXT("이동")), MotionWrap),
		MakeLabeledRow(FText::FromString(TEXT("타격 수")), HitRow)
	});
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeCustomSlotRow(int32 SlotIndex, const FText& SlotName)
{
	return MakeLabeledRow(SlotName,
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(PanelFont(10))
			.ColorAndOpacity(TextColor)
			.Text_Lambda([this, SlotIndex]() { return GetCustomSlotText(SlotIndex); })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ButtonGap, 0.f, 0.f, 0.f)
		[
			MakeButton(FText::FromString(TEXT("목록에서")), nullptr, [this, SlotIndex]()
			{
				ANUVFXViewerDirector* Owner = GetDirector();
				const TArray<TSharedPtr<FNUVFXViewerAssetItem>> Selected =
					AssetList.IsValid() ? AssetList->GetSelectedItems() : TArray<TSharedPtr<FNUVFXViewerAssetItem>>();
				if (Owner && !Selected.IsEmpty() && Selected[0].IsValid())
				{
					Owner->SetCustomSlot(SlotIndex, Selected[0]->Path);
				}
			},
			ButtonColor,
			FText::FromString(TEXT("아래 나이아가라 목록에서 고른 것을 이 칸에 넣음")))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ButtonGap, 0.f, 0.f, 0.f)
		[
			MakeButton(FText::FromString(TEXT("비우기")), nullptr, [this, SlotIndex]()
			{
				if (GetDirector())
				{
					GetDirector()->SetCustomSlot(SlotIndex, FSoftObjectPath());
				}
			})
		]);
}

FText SNUVFXViewerPanel::GetCustomSlotText(int32 SlotIndex) const
{
	const ANUVFXViewerDirector* Owner = GetDirector();
	if (!Owner)
	{
		return FText::GetEmpty();
	}

	const FNUVFXViewerSetup& Setup = Owner->GetSetup();
	const FSoftObjectPath& Path = SlotIndex == 0 ? Setup.CustomCast : (SlotIndex == 1 ? Setup.CustomProjectile : Setup.CustomImpact);
	return FText::FromString(Path.IsNull() ? FString(TEXT("-")) : Path.GetAssetName());
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeColorRows()
{
	TSharedRef<SWrapBox> Wrap = MakeWrap();

	// 게임이 쓰는 속성색 그대로 (DefaultGame.ini), 흰색은 고정색 행의 기본
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	const TPair<const TCHAR*, FLinearColor> Presets[] =
	{
		{ TEXT("흰색"), FLinearColor::White },
		{ TEXT("불"), Settings->FireElementColor },
		{ TEXT("물"), Settings->WaterElementColor },
		{ TEXT("풀"), Settings->GrassElementColor },
		{ TEXT("번개"), Settings->LightningElementColor },
		{ TEXT("땅"), Settings->EarthElementColor },
		{ TEXT("바람"), Settings->WindElementColor }
	};

	for (const TPair<const TCHAR*, FLinearColor>& Preset : Presets)
	{
		const FLinearColor Color = Preset.Value;
		Wrap->AddSlot()
		[
			MakeButton(FText::FromString(Preset.Key),
				[this, Color]() { return GetDirector() && GetDirector()->GetSetup().CustomColor.Equals(Color); },
				[this, Color]() { if (GetDirector()) { GetDirector()->SetCustomColor(Color); } },
				ToButtonColor(Color))
		];
	}

	// 직접 입력, HDR 이라 1 을 넘을 수 있음
	TSharedRef<SHorizontalBox> Channels = SNew(SHorizontalBox);
	for (int32 Channel = 0; Channel < 3; ++Channel)
	{
		Channels->AddSlot()
		.FillWidth(1.f)
		.Padding(Channel == 0 ? 0.f : ButtonGap, 0.f, 0.f, 0.f)
		[
			SNew(SSpinBox<float>)
			.Font(PanelFont(10))
			.MinValue(0.f)
			.MaxValue(10.f)
			.Delta(0.05f)
			.Value_Lambda([this, Channel]()
			{
				const ANUVFXViewerDirector* Owner = GetDirector();
				return Owner ? Owner->GetSetup().CustomColor.Component(Channel) : 0.f;
			})
			.OnValueCommitted_Lambda([this, Channel](float Value, ETextCommit::Type)
			{
				ANUVFXViewerDirector* Owner = GetDirector();
				if (Owner)
				{
					FLinearColor Color = Owner->GetSetup().CustomColor;
					Color.Component(Channel) = Value;
					Owner->SetCustomColor(Color);
				}
			})
		];
	}

	return StackRows({
		MakeLabeledRow(FText::FromString(TEXT("색")), Wrap),
		MakeLabeledRow(FText::FromString(TEXT("R G B")), Channels)
	});
}

void SNUVFXViewerPanel::RebuildAssetFilter()
{
	FilteredAssets.Reset();

	const ANUVFXViewerDirector* Owner = GetDirector();
	if (Owner)
	{
		for (const TSharedPtr<FNUVFXViewerAssetItem>& Item : Owner->GetAssetItems())
		{
			if (Item.IsValid() && (AssetFilter.IsEmpty() || Item->Label.Contains(AssetFilter)))
			{
				FilteredAssets.Add(Item);
			}
		}
	}

	if (AssetList.IsValid())
	{
		AssetList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SNUVFXViewerPanel::GenerateAssetRow(TSharedPtr<FNUVFXViewerAssetItem> Item,
	const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<TSharedPtr<FNUVFXViewerAssetItem>>, Owner)
		.Padding(FMargin(6.f, 3.f))
		.ToolTipText(FText::FromString(Item.IsValid() ? Item->Path.ToString() : FString()))
		[
			SNew(STextBlock)
			.Font(PanelFont(10))
			.ColorAndOpacity(TextColor)
			.Text(FText::FromString(Item.IsValid() ? Item->Label : FString()))
		];
}

/////////////////////////////////////////////////////////////////
// 옵션 · 결과 · 단축키
/////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SNUVFXViewerPanel::MakeCandidateSection()
{
	// 지금 스킬과 그 후보 수 (스킬 표 모드가 아니면 0)
	auto CurrentSkill = [this]() -> int32
	{
		const ANUVFXViewerDirector* Owner = GetDirector();
		return Owner && Owner->GetSetup().Mode == ENUVFXViewerMode::Skill ? Owner->GetSetup().SkillId : INDEX_NONE;
	};

	auto CandidateCount = [this, CurrentSkill]() -> int32
	{
		const ANUVFXViewerDirector* Owner = GetDirector();
		const int32 SkillId = CurrentSkill();
		return Owner && SkillId != INDEX_NONE ? Owner->GetCandidateCount(SkillId) : 0;
	};

	// 후보 버튼, 첫 칸은 표 그대로 · 고른 후보에는 ✓
	TSharedRef<SWrapBox> CandidateRow = MakeWrap();
	for (int32 Index = 0; Index < MaxCandidateButtons; ++Index)
	{
		CandidateRow->AddSlot()
		[
			SNew(SBox)
			.Visibility_Lambda([CandidateCount, Index]()
			{
				return Index < CandidateCount() ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				MakeButton(
					TAttribute<FText>::CreateLambda([this, CurrentSkill, Index]()
					{
						const ANUVFXViewerDirector* Owner = GetDirector();
						const int32 SkillId = CurrentSkill();
						if (!Owner || SkillId == INDEX_NONE)
						{
							return FText::GetEmpty();
						}

						return FText::FromString(FString::Printf(TEXT("%s%d %s"),
							Owner->GetChosenCandidate(SkillId) == Index ? TEXT("✓ ") : TEXT(""), Index + 1,
							*Owner->GetCandidateLabel(SkillId, Index)));
					}),
					[this, Index]()
					{
						const ANUVFXViewerDirector* Owner = GetDirector();
						return Owner && Owner->GetCandidateIndex() == Index;
					},
					[this, Index]()
					{
						if (ANUVFXViewerDirector* Owner = GetDirector())
						{
							Owner->SelectCandidate(Index);
						}
					},
					ButtonColor,
					FText::FromString(TEXT("이 후보를 틂 ([ ])")))
			]
		];
	}

	TSharedRef<SWrapBox> ActionRow = MakeWrap();
	ActionRow->AddSlot()
	[
		MakeButton(
			TAttribute<FText>::CreateLambda([this, CurrentSkill]()
			{
				// "지금"(표 그대로)을 틀고 있으면 그 스킬을 대기에서 빼는 버튼
				const ANUVFXViewerDirector* Owner = GetDirector();
				const bool bTable = Owner && Owner->GetCandidateIndex() == 0;
				const bool bPending = Owner && Owner->GetChosenCandidate(CurrentSkill()) != INDEX_NONE;
				return FText::FromString(bTable
					? (bPending ? TEXT("적용 대기에서 빼기 (Enter)") : TEXT("지금 그대로 (Enter)"))
					: TEXT("적용 대기에 담기 (Enter)"));
			}),
			nullptr,
			[this]() { if (GetDirector()) { GetDirector()->ChooseCandidate(); } },
			PlayButtonColor,
			FText::FromString(TEXT("지금 후보를 적용 대기에 담음 (스킬마다 하나, Saved/VFXViewer/Pending.json), 지금(표 그대로)이면 대기에서 뺌")))
	];
	ActionRow->AddSlot()
	[
		MakeButton(FText::FromString(TEXT("번갈아 보기")),
			[this]() { return GetDirector() && GetDirector()->IsCompareCycle(); },
			[this]() { if (GetDirector()) { GetDirector()->SetCompareCycle(!GetDirector()->IsCompareCycle()); } },
			ButtonColor,
			FText::FromString(TEXT("재생이 끝날 때마다 다음 후보로 넘어가며 계속 틂")))
	];

	return SNew(SBorder)
		.BorderImage(RoundedBrush())
		.BorderBackgroundColor(FieldColor)
		.Padding(FMargin(8.f, 6.f, 8.f, 8.f))
		.Visibility_Lambda([CandidateCount]()
		{
			return CandidateCount() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			StackRows({
				SNew(STextBlock)
				.Font(PanelFont(10, TEXT("Bold")))
				.ColorAndOpacity(CandidateBadgeColor)
				.Text_Lambda([CandidateCount]()
				{
					return FText::FromString(FString::Printf(TEXT("후보 %d개 + 지금"), FMath::Max(CandidateCount() - 1, 0)));
				}),
				CandidateRow,
				SNew(STextBlock)
				.Font(PanelFont(10))
				.ColorAndOpacity(DimTextColor)
				.AutoWrapText(true)
				.Text_Lambda([this, CurrentSkill]()
				{
					const ANUVFXViewerDirector* Owner = GetDirector();
					const int32 SkillId = CurrentSkill();
					return Owner && SkillId != INDEX_NONE
						? FText::FromString(Owner->GetCandidateNote(SkillId, Owner->GetCandidateIndex()))
						: FText::GetEmpty();
				}),
				ActionRow,
				SNew(STextBlock)
				.Font(PanelFont(9))
				.ColorAndOpacity(DimTextColor)
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					const ANUVFXViewerDirector* Owner = GetDirector();
					if (!Owner)
					{
						return FText::GetEmpty();
					}

					// 마지막 소식 (읽은 후보 파일 수 · 고름)
					return FText::FromString(Owner->GetCompareMessage());
				})
			})
		];
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakePendingCard()
{
	SAssignNew(PendingRows, SVerticalBox);
	RebuildPendingRows();

	// 버튼은 그 일을 할 수 있을 때만 보임 (다시 채우는 동안은 적용 · 되돌리기를 숨김)
	const auto ShowWhen = [this](TFunction<bool(const ANUVFXViewerDirector&)> Condition)
	{
		return TAttribute<EVisibility>::CreateLambda([this, Condition]()
		{
			const ANUVFXViewerDirector* Owner = GetDirector();
			return Owner && Condition(*Owner) ? EVisibility::Visible : EVisibility::Collapsed;
		});
	};

	TSharedRef<SWrapBox> Footer = MakeWrap();
	Footer->AddSlot()
	[
		SNew(SBox)
		.Visibility(ShowWhen([](const ANUVFXViewerDirector& Owner)
		{
			return !Owner.GetPendingChanges().IsEmpty() && !Owner.IsReimportingTable();
		}))
		[
			MakeButton(
				TAttribute<FText>::CreateLambda([this]()
				{
					const ANUVFXViewerDirector* Owner = GetDirector();
					return FText::FromString(FString::Printf(TEXT("적용 (%d건)"), Owner ? Owner->GetPendingChanges().Num() : 0));
				}),
				nullptr,
				[this]() { if (GetDirector()) { GetDirector()->ApplyPendingChanges(); } },
				PlayButtonColor,
				FText::FromString(TEXT("적용 대기를 원본 CSV(SkillVFXTable.csv)에 쓰고 DT_SkillVFX 를 다시 채움 - 쓰기 전에 확인 창 · 백업")))
		]
	];
	Footer->AddSlot()
	[
		SNew(SBox)
		.Visibility(ShowWhen([](const ANUVFXViewerDirector& Owner)
		{
			return Owner.CanRevertLastApply() && !Owner.IsReimportingTable();
		}))
		[
			MakeButton(FText::FromString(TEXT("마지막 적용 되돌리기")), nullptr,
				[this]() { if (GetDirector()) { GetDirector()->RevertLastApply(); } },
				ButtonColor,
				FText::FromString(TEXT("마지막 적용(되돌리지 않은 것)을 그 전 값으로 - 확인 창에 바뀌는 칸이 나옴")))
		]
	];
	Footer->AddSlot()
	[
		SNew(SBox)
		.Visibility(ShowWhen([](const ANUVFXViewerDirector& Owner)
		{
			return Owner.NeedsTableReimport() && !Owner.IsReimportingTable();
		}))
		[
			MakeButton(FText::FromString(TEXT("DT 다시 채우기")), nullptr,
				[this]() { if (GetDirector()) { GetDirector()->ReimportSkillVFXTable(); } },
				ButtonColor,
				FText::FromString(TEXT("원본 CSV 로 DT_SkillVFX 를 다시 채움 (화면 없는 에디터, 1~2분, 에디터가 꺼져 있어야 함)")))
		]
	];
	Footer->AddSlot()
	[
		SNew(SBox)
		.Visibility(ShowWhen([](const ANUVFXViewerDirector& Owner)
		{
			return !Owner.GetPendingChanges().IsEmpty();
		}))
		[
			MakeButton(FText::FromString(TEXT("모두 비우기")), nullptr,
				[this]() { if (GetDirector()) { GetDirector()->ClearPendingChanges(); } },
				WarningButtonColor,
				FText::FromString(TEXT("적용 대기를 전부 뺌 (표는 그대로)")))
		]
	];

	// 후보 · 담은 것 · 되돌릴 적용 · 다시 채울 DT 가 있을 때만
	return SNew(SBox)
		.Visibility_Lambda([this]()
		{
			const ANUVFXViewerDirector* Owner = GetDirector();
			return Owner && (Owner->HasCandidates() || !Owner->GetPendingChanges().IsEmpty() || Owner->CanRevertLastApply()
				|| Owner->NeedsTableReimport() || Owner->IsReimportingTable())
				? EVisibility::Visible
				: EVisibility::Collapsed;
		})
		.Padding(FMargin(0.f, CardGap, 0.f, 0.f))
		[
			SNew(SBorder)
			.BorderImage(RoundedBrush())
			.BorderBackgroundColor(CardColor)
			.Padding(FMargin(CardPadding, 8.f, CardPadding, CardPadding))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(STextBlock)
					.Font(PanelFont(11, TEXT("Bold")))
					.ColorAndOpacity(TitleColor)
					.Text_Lambda([this]()
					{
						const ANUVFXViewerDirector* Owner = GetDirector();
						return FText::FromString(FString::Printf(TEXT("적용 대기 %d건"), Owner ? Owner->GetPendingChanges().Num() : 0));
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					PendingRows.ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, RowGap, 0.f, 0.f)
				[
					Footer
				]

				// 적용 · 다시 채우기 상태, DT 가 CSV 와 다르면 알림, 되돌릴 적용
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(PanelFont(9))
					.ColorAndOpacity(DimTextColor)
					.AutoWrapText(true)
					.Text_Lambda([this]()
					{
						const ANUVFXViewerDirector* Owner = GetDirector();
						if (!Owner)
						{
							return FText::GetEmpty();
						}

						TArray<FString> Lines;
						if (!Owner->GetTableStatus().IsEmpty())
						{
							Lines.Add(Owner->GetTableStatus());
						}

						if (Owner->NeedsTableReimport() && !Owner->IsReimportingTable())
						{
							Lines.Add(TEXT("DT_SkillVFX 가 원본 CSV 와 다름 - [DT 다시 채우기] (에디터가 켜져 있으면 에디터에서 다시 가져오기)"));
						}

						if (Owner->CanRevertLastApply())
						{
							Lines.Add(TEXT("마지막 적용: ") + Owner->GetLastApplySummary());
						}

						return FText::FromString(FString::Join(Lines, TEXT("\n")));
					})
				]
			]
		];
}

void SNUVFXViewerPanel::RebuildPendingRows()
{
	const ANUVFXViewerDirector* Owner = GetDirector();
	if (!PendingRows.IsValid() || !Owner)
	{
		return;
	}

	LastPendingVersion = Owner->GetPendingVersion();
	PendingRows->ClearChildren();

	if (Owner->GetPendingChanges().IsEmpty())
	{
		PendingRows->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font(PanelFont(10))
			.ColorAndOpacity(DimTextColor)
			.AutoWrapText(true)
			.Text(FText::FromString(TEXT("담은 후보가 없음 - 후보를 틀고 Enter (적용 대기에 담기)")))
		];
		return;
	}

	// 줄마다: 스킬 → 후보 이름, [보기] [빼기], 아래에 칸별 "지금 -> 바꿀 값"
	bool bFirst = true;
	for (const FNUVFXViewerPendingChange& Change : Owner->GetPendingChanges())
	{
		const int32 SkillId = Change.SkillId;
		const TArray<FString> Lines = Owner->DescribePendingChange(Change);

		PendingRows->AddSlot()
		.AutoHeight()
		.Padding(0.f, bFirst ? 0.f : RowGap, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(RoundedBrush())
			.BorderBackgroundColor(FieldColor)
			.Padding(FMargin(8.f, 6.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(PanelFont(10, TEXT("Bold")))
						.ColorAndOpacity(TextColor)
						.AutoWrapText(true)
						.Text(FText::FromString(FString::Printf(TEXT("%s  ->  %s"), *Owner->GetSkillLabel(SkillId), *Change.Label)))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(ButtonGap, 0.f, 0.f, 0.f)
					[
						MakeButton(FText::FromString(TEXT("보기")), nullptr,
							[this, SkillId]() { if (GetDirector()) { GetDirector()->ViewPendingChange(SkillId); } },
							ButtonColor,
							FText::FromString(TEXT("그 스킬로 가서 담은 후보를 틂")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(ButtonGap, 0.f, 0.f, 0.f)
					[
						MakeButton(FText::FromString(TEXT("빼기")), nullptr,
							[this, SkillId]() { if (GetDirector()) { GetDirector()->RemovePendingChange(SkillId); } },
							ButtonColor,
							FText::FromString(TEXT("적용 대기에서 뺌")))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(PanelFont(9))
					.ColorAndOpacity(DimTextColor)
					.AutoWrapText(true)
					.Text(FText::FromString(FString::Join(Lines, TEXT("\n"))))
				]
			]
		];

		bFirst = false;
	}
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeOptionsCard()
{
	TSharedRef<SWrapBox> Wrap = MakeWrap();

	Wrap->AddSlot()
	[
		MakeButton(FText::FromString(TEXT("피격 이펙트")),
			[this]() { return GetDirector() && GetDirector()->GetSetup().bPlayHitCue; },
			[this]() { if (GetDirector()) { GetDirector()->SetPlayHitCue(!GetDirector()->GetSetup().bPlayHitCue); } },
			ButtonColor,
			FText::FromString(TEXT("타격마다 GameplayCue.Battle.Hit (NS_HitEffect · 밀려남 · 점등 · 흔들림)를 같이 냄")))
	];

	Wrap->AddSlot()
	[
		MakeButton(FText::FromString(TEXT("새로고침 (F5)")), nullptr,
			[this]() { if (GetDirector()) { GetDirector()->RequestReload(); } },
			ButtonColor,
			FText::FromString(TEXT("에디터에서 저장한 이펙트를 디스크에서 다시 읽음")))
	];

	Wrap->AddSlot()
	[
		MakeButton(FText::FromString(TEXT("패널 숨김 (Tab)")), nullptr,
			[this]() { if (GetDirector()) { GetDirector()->TogglePanel(); } },
			ButtonColor,
			FText::FromString(TEXT("패널과 진행바를 숨기고 3D 화면을 창 전체로")))
	];

	return MakeCard(FText::FromString(TEXT("옵션")), Wrap);
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeReportCard()
{
	return MakeCard(FText::FromString(TEXT("재생 결과")),
		SNew(STextBlock)
		.Font(PanelFont(10))
		.ColorAndOpacity(TextColor)
		.AutoWrapText(true)
		.LineHeightPercentage(1.1f)
		.Text_Lambda([this]()
		{
			const ANUVFXViewerDirector* Owner = GetDirector();
			return Owner ? Owner->GetReportText() : FText::GetEmpty();
		}));
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeShortcutHint() const
{
	return SNew(STextBlock)
		.Font(PanelFont(9))
		.ColorAndOpacity(DimTextColor)
		.AutoWrapText(true)
		.Text(FText::FromString(
			TEXT("R 처음부터 · Space 재생/정지 · ← → 한 프레임 · Shift+← → 0.1초 · Home 처음으로 · 1~4 속도 · F5 새로고침 · Tab 패널"
				" · 후보: [ ] 바꾸기 · Shift+[ ] 후보 있는 스킬 · Enter 적용 대기에 담기")));
}

/////////////////////////////////////////////////////////////////
// 3D 화면 아래 진행바
/////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SNUVFXViewerPanel::MakeTransportButton(TAttribute<ENUVFXViewerTransportIcon> Icon, const FText& ToolTip,
	TFunction<void()> OnClick) const
{
	return SNew(SButton)
		.ButtonStyle(&FlatButtonStyle())
		.IsFocusable(false)
		.ContentPadding(FMargin(8.f, 6.f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ToolTipText(ToolTip)
		.ButtonColorAndOpacity(ButtonColor)
		.OnClicked_Lambda([OnClick]()
		{
			if (OnClick)
			{
				OnClick();
			}
			return FReply::Handled();
		})
		[
			SNew(SBox)
			.WidthOverride(14.f)
			.HeightOverride(14.f)
			[
				SNew(SNUVFXViewerTransportIcon)
				.Icon(Icon)
			]
		];
}

TSharedRef<SWidget> SNUVFXViewerPanel::MakeTransportBar()
{
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	const auto AddButton = [this, &Buttons](TAttribute<ENUVFXViewerTransportIcon> Icon, const TCHAR* ToolTip, TFunction<void()> OnClick)
	{
		Buttons->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 4.f, 0.f)
		[
			MakeTransportButton(MoveTemp(Icon), FText::FromString(ToolTip), MoveTemp(OnClick))
		];
	};

	AddButton(ENUVFXViewerTransportIcon::ToStart, TEXT("처음으로 (Home)"),
		[this]() { if (GetDirector()) { GetDirector()->SeekTo(0.f); } });
	AddButton(ENUVFXViewerTransportIcon::BackSecond, TEXT("0.1초 뒤로 (Shift + ←)"),
		[this]() { if (GetDirector()) { GetDirector()->StepSeconds(-0.1f); } });
	AddButton(ENUVFXViewerTransportIcon::BackFrame, TEXT("한 프레임 뒤로 (←), 멈춘 채로 옮김"),
		[this]() { if (GetDirector()) { GetDirector()->StepFrames(-1); } });
	AddButton(TAttribute<ENUVFXViewerTransportIcon>::CreateLambda([this]()
		{
			return GetDirector() && !GetDirector()->IsPaused() && GetDirector()->GetPlaybackTime() < GetDirector()->GetTimelineLength()
				? ENUVFXViewerTransportIcon::Pause : ENUVFXViewerTransportIcon::Play;
		}),
		TEXT("재생 · 일시정지 (Space), 끝난 뒤면 처음부터"),
		[this]() { if (GetDirector()) { GetDirector()->TogglePause(); } });
	AddButton(ENUVFXViewerTransportIcon::ForwardFrame, TEXT("한 프레임 앞으로 (→), 멈춘 채로 옮김"),
		[this]() { if (GetDirector()) { GetDirector()->StepFrames(1); } });
	AddButton(ENUVFXViewerTransportIcon::ForwardSecond, TEXT("0.1초 앞으로 (Shift + →)"),
		[this]() { if (GetDirector()) { GetDirector()->StepSeconds(0.1f); } });

	// 속도 · 반복
	TSharedRef<SHorizontalBox> Options = SNew(SHorizontalBox);
	const float SpeedValues[] = { 0.1f, 0.25f, 0.5f, 1.f };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SpeedValues); ++Index)
	{
		const float Speed = SpeedValues[Index];
		Options->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 4.f, 0.f)
		[
			MakeButton(FText::FromString(FString::Printf(TEXT("x%g"), Speed)),
				[this, Speed]() { return GetDirector() && FMath::IsNearlyEqual(GetDirector()->GetSetup().Speed, Speed); },
				[this, Speed]() { if (GetDirector()) { GetDirector()->SetSpeed(Speed); } },
				ButtonColor,
				FText::FromString(FString::Printf(TEXT("재생 속도 x%g (%d)"), Speed, Index + 1)))
		];
	}

	Options->AddSlot()
	.AutoWidth()
	.Padding(6.f, 0.f, 0.f, 0.f)
	[
		MakeButton(FText::FromString(TEXT("반복")),
			[this]() { return GetDirector() && GetDirector()->GetSetup().bLoop; },
			[this]() { if (GetDirector()) { GetDirector()->SetLoop(!GetDirector()->GetSetup().bLoop); } },
			ButtonColor,
			FText::FromString(TEXT("끝나면 이펙트가 사라진 뒤 처음부터 다시")))
	];

	return SNew(SBox)
		.Visibility(this, &SNUVFXViewerPanel::GetPanelVisibility)
		.WidthOverride_Lambda([this]()
		{
			return FOptionalSize(GetDirector() ? GetDirector()->GetViewAreaSlateSize().X : 0.f);
		})
		.HeightOverride(ANUVFXViewerDirector::TransportHeight)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(PanelColor)
			.Padding(FMargin(10.f, 4.f))
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					Buttons
				]

				// 지금 시각 / 전체, 그 아래 프레임 번호 (고정 60fps 기준)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 10.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride(96.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 10))
							.ColorAndOpacity(TextColor)
							.Text_Lambda([this]()
							{
								const ANUVFXViewerDirector* Owner = GetDirector();
								return Owner
									? FText::FromString(FString::Printf(TEXT("%.2f / %.2f"), Owner->GetPlaybackTime(), Owner->GetTimelineLength()))
									: FText::GetEmpty();
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(PanelFont(8))
							.ColorAndOpacity(DimTextColor)
							.Text_Lambda([this]()
							{
								const ANUVFXViewerDirector* Owner = GetDirector();
								const int32 Frame = Owner
									? FMath::RoundToInt32(Owner->GetPlaybackTime() / ANUVFXViewerDirector::FrameStep)
									: 0;
								return FText::FromString(FString::Printf(TEXT("초 · %d 프레임"), Frame));
							})
						]
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(SNUVFXViewerTimeline)
					.Director(Director)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10.f, 0.f, 0.f, 0.f)
				[
					Options
				]
			]
		];
}
