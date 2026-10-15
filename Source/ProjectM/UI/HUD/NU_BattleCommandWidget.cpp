#include "UI/HUD/NU_BattleCommandWidget.h"

#include "Battle/Avatar/NUBattleAvatar.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "PaperFlipbookComponent.h"
#include "PlayerController/Component/NUBattleCommandComponentBase.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/Common/NU_CombatantStatusWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"

namespace
{
	void Show(UWidget* Widget, bool bShow, ESlateVisibility Visible = ESlateVisibility::SelfHitTestInvisible)
	{
		const ESlateVisibility Wanted = bShow ? Visible : ESlateVisibility::Collapsed;
		if (Widget != nullptr && Widget->GetVisibility() != Wanted)
		{
			Widget->SetVisibility(Wanted);
		}
	}

	void SetText(UTextBlock* Text, const FText& Value)
	{
		if (Text != nullptr)
		{
			Text->SetText(Value);
		}
	}
}

void UNU_BattleCommandWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Attack) { Button_Attack->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleAttackClicked); }
	if (Button_Skill) { Button_Skill->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleSkillClicked); }
	if (Button_Item) { Button_Item->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleItemClicked); }
	if (Button_NaiaSwap) { Button_NaiaSwap->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleNaiaSwapClicked); }
	if (Button_Flee) { Button_Flee->OnCommonClicked.AddUniqueDynamic(this, &ThisClass::HandleFleeClicked); }
	// 컨트롤러가 판을 접으면 틱이 멈추므로, 그때 대상 표시를 바로 거둔다
	OnNativeVisibilityChanged.RemoveAll(this);
	OnNativeVisibilityChanged.AddUObject(this, &ThisClass::HandleOwnVisibility);
	Show(Text_Notice, false);
	Show(TetherDot, false);
	Show(TetherLine, false);
	LastStateKey.Reset();
	LastPhase = 255;
	bPlaced = false;
	bPendingOpen = true;
	Refresh(FindComponent(), true);
}

void UNU_BattleCommandWidget::NativeDestruct()
{
	MarkTarget(nullptr);
	Bind(nullptr);
	Super::NativeDestruct();
}

UNUBattleCommandComponentBase* UNU_BattleCommandWidget::FindComponent() const
{
	const ANUPlayerController* PC = Cast<ANUPlayerController>(GetOwningPlayer());
	return PC != nullptr ? PC->GetBattleCommandComponent() : nullptr;
}

void UNU_BattleCommandWidget::Bind(UNUBattleCommandComponentBase* Component)
{
	if (UNUBattleCommandComponentBase* Old = BoundComponent.Get())
	{
		Old->OnActionUnavailable.RemoveDynamic(this, &ThisClass::HandleActionUnavailable);
		Old->OnSubmissionFailed.RemoveDynamic(this, &ThisClass::HandleSubmissionFailed);
		Old->OnNaiaSwapFailed.RemoveDynamic(this, &ThisClass::HandleNaiaSwapFailed);
	}
	BoundComponent = Component;
	if (Component != nullptr)
	{
		Component->OnActionUnavailable.AddUniqueDynamic(this, &ThisClass::HandleActionUnavailable);
		Component->OnSubmissionFailed.AddUniqueDynamic(this, &ThisClass::HandleSubmissionFailed);
		Component->OnNaiaSwapFailed.AddUniqueDynamic(this, &ThisClass::HandleNaiaSwapFailed);
	}
}

int32 UNU_BattleCommandWidget::RowOf(ENUBattleRootAction Action)
{
	switch (Action)
	{
	case ENUBattleRootAction::Attack: return 0;
	case ENUBattleRootAction::Skill: return 1;
	case ENUBattleRootAction::Item: return 2;
	case ENUBattleRootAction::NaiaSwap: return 3;
	case ENUBattleRootAction::Flee: return 4;
	default: return 0;
	}
}

void UNU_BattleCommandWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 지역/서버 전투에 따라 컨트롤러가 쓰는 컴포넌트가 바뀐다 --- 매 틱 지금 것을 보고, 바뀐 상태만 다시 그린다
	UNUBattleCommandComponentBase* Component = FindComponent();
	if (Component != BoundComponent.Get())
	{
		Bind(Component);
	}
	Refresh(Component, false);

	// 행동하는 캐릭터가 바뀌면(플레이어 다음 나이아 등) 그 옆으로 다시 연다
	AActor* Source = Component != nullptr ? Component->GetCurrentSourceAvatar() : nullptr;
	if (bPlaced && Source != PlacedFor.Get())
	{
		bPendingOpen = true;
	}
	if (bPendingOpen)
	{
		// 판 크기를 알아야 자리를 정할 수 있다 --- 이번 프레임 배치를 먼저 잰다
		ForceLayoutPrepass();
		UpdatePlacement(Component, InDeltaTime, true);
		PlayOpen();
		bPendingOpen = false;
	}
	else
	{
		UpdatePlacement(Component, InDeltaTime, false);
	}

	UpdateTargetCursor(Component, InDeltaTime);
	Motion.Advance(InDeltaTime);
	if (RootHighlight != nullptr)
	{
		const float Target = Component != nullptr ? RowOf(Component->GetSelectedRootAction()) * RowPitch : 0.f;
		HighlightY = FMath::FInterpTo(HighlightY, Target, InDeltaTime, 22.f);
		FWidgetTransform Transform = RootHighlight->GetRenderTransform();
		Transform.Translation = FVector2D(0.f, HighlightY);
		RootHighlight->SetRenderTransform(Transform);
	}
	if (Text_Notice != nullptr && NoticeLeft > 0.f)
	{
		NoticeLeft -= InDeltaTime;
		Text_Notice->SetRenderOpacity(FMath::Clamp(NoticeLeft / 0.4f, 0.f, 1.f));
		if (NoticeLeft <= 0.f)
		{
			Show(Text_Notice, false);
		}
	}
}

void UNU_BattleCommandWidget::Refresh(UNUBattleCommandComponentBase* Component, bool bForce)
{
	if (Component == nullptr)
	{
		return;
	}
	const ENUCommandSelectionPhase Phase = Component->GetSelectionPhase();
	const ENUBattleRootAction Selected = Component->GetSelectedRootAction();

	// 바뀐 것이 있을 때만 다시 그린다 (텍스트 몇 개를 매 틱 다시 넣지 않게)
	FString Key = FString::Printf(TEXT("%d|%d|%d|%d|%d|"), (int32)Phase, (int32)Selected, Component->GetPendingSPSpend(),
		Component->GetPendingHitCount(), Component->GetMaxHitCount());
	switch (Phase)
	{
	case ENUCommandSelectionPhase::SkillList: Key += Component->GetCurrentSkillDisplayText().ToString() + Component->GetSkillPositionText().ToString(); break;
	case ENUCommandSelectionPhase::ItemList: Key += Component->GetCurrentItemDisplayText().ToString() + Component->GetItemPositionText().ToString(); break;
	case ENUCommandSelectionPhase::NaiaList: Key += Component->GetCurrentNaiaDisplayText().ToString() + Component->GetNaiaPositionText().ToString(); break;
	case ENUCommandSelectionPhase::TargetSelection: Key += Component->GetCurrentTargetName().ToString(); break;
	default: break;
	}
	Key += Component->GetCurrentSourceName().ToString() + FString::Printf(TEXT("|%d"), Component->GetCurrentTargetCombatantId());
	if (!bForce && Key == LastStateKey)
	{
		return;
	}
	LastStateKey = Key;

	const bool bRoot = Phase == ENUCommandSelectionPhase::RootMenu;
	const bool bWait = Phase == ENUCommandSelectionPhase::Submitting || Phase == ENUCommandSelectionPhase::WaitingForOthers;
	const bool bDetail = !bRoot && !bWait && Phase != ENUCommandSelectionPhase::Inactive;
	Show(RootPanel, bRoot);
	Show(WaitPanel, bWait, ESlateVisibility::HitTestInvisible);
	MarkTarget(Phase == ENUCommandSelectionPhase::TargetSelection ? Component->GetCurrentTargetAvatar() : nullptr);
	Show(DetailPanel, bDetail, ESlateVisibility::HitTestInvisible);

	if (CommandWidth != nullptr)
	{
		CommandWidth->SetWidthOverride(bDetail ? DetailWidth : RootWidth);
	}

	// 화면이 바뀌면: 대기·비활성에서 첫 메뉴로 오면 캐릭터 옆에서 새로 열리고, 메뉴 안에서 오가면 그 판만 살짝 미끄러진다
	if ((uint8)Phase != LastPhase)
	{
		const bool bFromOutside = LastPhase == 255
			|| LastPhase == (uint8)ENUCommandSelectionPhase::Inactive
			|| LastPhase == (uint8)ENUCommandSelectionPhase::Submitting
			|| LastPhase == (uint8)ENUCommandSelectionPhase::WaitingForOthers;
		if (bRoot && bFromOutside)
		{
			bPendingOpen = true;
		}
		else
		{
			Motion.Finish();
			Motion.Add(bRoot ? RootPanel : bWait ? WaitPanel : DetailPanel, 0.f, 0.2f, FVector2D(-Side * 8.f, 0.f));
			Motion.Start();
		}
		if (bRoot)
		{
			HighlightY = RowOf(Selected) * RowPitch;    // 새로 열린 메뉴는 고른 줄에서 바로 시작
		}
		LastPhase = (uint8)Phase;
	}

	SetText(Text_Source, FText::Format(NSLOCTEXT("NUBattleHUD", "CommandSource", "{0}의 행동"), Component->GetCurrentSourceName()));

	// 첫 메뉴: 못 쓰는 줄은 흐리게
	const TPair<UNU_CommonButtonWidget*, ENUBattleRootAction> Rows[] = {
		{ Button_Attack, ENUBattleRootAction::Attack }, { Button_Skill, ENUBattleRootAction::Skill }, { Button_Item, ENUBattleRootAction::Item },
		{ Button_NaiaSwap, ENUBattleRootAction::NaiaSwap }, { Button_Flee, ENUBattleRootAction::Flee } };
	for (const TPair<UNU_CommonButtonWidget*, ENUBattleRootAction>& Row : Rows)
	{
		if (Row.Key != nullptr)
		{
			Row.Key->SetRenderOpacity(Component->IsRootActionAvailable(Row.Value) ? 1.f : 0.38f);
		}
	}

	// 세부
	FText Cap, Name, Pos, Body;
	bool bSP = false;
	switch (Phase)
	{
	case ENUCommandSelectionPhase::SkillList:
		Cap = NSLOCTEXT("NUBattleHUD", "CapSkill", "스킬");
		Name = Component->GetCurrentSkillDisplayText();
		Pos = Component->GetSkillPositionText();
		Body = Component->GetCurrentSkillDescription();
		bSP = true;
		break;
	case ENUCommandSelectionPhase::ItemList:
		Cap = NSLOCTEXT("NUBattleHUD", "CapItem", "아이템");
		Name = Component->GetCurrentItemDisplayText();
		Pos = Component->GetItemPositionText();
		break;
	case ENUCommandSelectionPhase::NaiaList:
		Cap = NSLOCTEXT("NUBattleHUD", "CapNaia", "나이아 교체");
		Name = Component->GetCurrentNaiaDisplayText();
		Pos = Component->GetNaiaPositionText();
		break;
	case ENUCommandSelectionPhase::TargetSelection:
		Cap = NSLOCTEXT("NUBattleHUD", "CapTarget", "대상 고르기");
		Name = Component->GetCurrentTargetName();
		bSP = Selected == ENUBattleRootAction::Attack || Selected == ENUBattleRootAction::Skill;
		break;
	default:
		break;
	}
	SetText(Text_DetailCap, Cap);
	SetText(Text_DetailName, Name);
	SetText(Text_DetailPos, Pos);
	SetText(Text_DetailBody, Body);
	Show(Text_DetailBody, !Body.IsEmpty(), ESlateVisibility::HitTestInvisible);

	const int32 MaxHits = Component->GetMaxHitCount();
	bSP = bSP && MaxHits > 1;
	Show(SPRow, bSP, ESlateVisibility::HitTestInvisible);
	if (bSP && Panel_SPPips != nullptr)
	{
		Panel_SPPips->ClearChildren();
		const int32 Hits = Component->GetPendingHitCount();
		for (int32 Index = 0; Index < MaxHits; ++Index)
		{
			UImage* Pip = NewObject<UImage>(this);
			Pip->SetBrush(Index < Hits ? PipOnBrush : PipOffBrush);
			Panel_SPPips->AddChild(Pip);
			if (UHorizontalBoxSlot* PipSlot = Cast<UHorizontalBoxSlot>(Pip->Slot))
			{
				PipSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				PipSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		SetText(Text_SPHits, FText::Format(NSLOCTEXT("NUBattleHUD", "SPHits", "{0}타 · SP {1}"),
			FText::AsNumber(Hits), FText::AsNumber(Component->GetPendingSPSpend())));
	}

	SetText(Text_Wait, Phase == ENUCommandSelectionPhase::Submitting
		? NSLOCTEXT("NUBattleHUD", "Submitting", "명령을 보내는 중…")
		: NSLOCTEXT("NUBattleHUD", "WaitingOthers", "다른 파티원을 기다리는 중…"));
}

void UNU_BattleCommandWidget::UpdatePlacement(UNUBattleCommandComponentBase* Component, float DeltaTime, bool bSnap)
{
	UCanvasPanelSlot* PanelSlot = CommandRoot != nullptr ? Cast<UCanvasPanelSlot>(CommandRoot->Slot) : nullptr;
	if (PanelSlot == nullptr)
	{
		return;
	}
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(this) / Scale;
	const FVector2D Size = CommandRoot->GetDesiredSize();

	// 행동하는 캐릭터 몸 그림의 한가운데를 화면으로. CommandUIAnchor(루트 +80)는 액터 배율을 따라가 머리 위로 뜨고
	// 나이아·보스처럼 키가 다른 몸에 안 맞아서, 몸 플립북의 경계 중심을 먼저 쓴다
	AActor* Source = Component != nullptr ? Component->GetCurrentSourceAvatar() : nullptr;
	FVector2D Anchor = FVector2D::ZeroVector;
	bool bAnchored = false;
	if (Source != nullptr)
	{
		FVector World = Source->GetActorLocation() + FVector(0.f, 0.f, 45.f);
		if (const UPaperFlipbookComponent* Body = Source->FindComponentByClass<UPaperFlipbookComponent>();
			Body != nullptr && Body->Bounds.SphereRadius > 1.f)
		{
			World = Body->Bounds.Origin;
		}
		else if (const ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Source))
		{
			if (const USceneComponent* AnchorComponent = Avatar->GetCommandUIAnchor())
			{
				World = AnchorComponent->GetComponentLocation();
			}
		}
		bAnchored = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), World, Anchor, false);
	}

	if (bSnap)
	{
		// 오른쪽(가운데 빈 전장 쪽)을 먼저, 모자라면 왼쪽. 열려 있는 동안은 쪽을 바꾸지 않는다 (판이 튀지 않게)
		Side = (bAnchored && Anchor.X + AnchorGap + Size.X > View.X - ScreenMargin) ? -1.f : 1.f;
		PlacedFor = Source;
	}

	FVector2D Target;
	if (bAnchored)
	{
		Target.X = Side > 0.f ? Anchor.X + AnchorGap : Anchor.X - AnchorGap - Size.X;
		Target.Y = Anchor.Y - Size.Y * 0.5f;
	}
	else
	{
		// 기준 캐릭터를 못 찾으면 오른쪽 아래
		Target = FVector2D(View.X - ScreenMargin - Size.X - 16.f, View.Y - ScreenMargin - Size.Y - 24.f);
	}
	Target.X = FMath::Clamp(Target.X, ScreenMargin, FMath::Max(ScreenMargin, View.X - ScreenMargin - Size.X));
	Target.Y = FMath::Clamp(Target.Y, ScreenMargin, FMath::Max(ScreenMargin, View.Y - ScreenMargin - Size.Y));
	PanelPos = (bSnap || !bPlaced) ? Target : FMath::Vector2DInterpTo(PanelPos, Target, DeltaTime, FollowSpeed);
	bPlaced = true;
	PanelSlot->SetPosition(PanelPos);
	CommandRoot->SetRenderTransformPivot(FVector2D(Side > 0.f ? 0.f : 1.f, 0.5f));   // 캐릭터 쪽에서 자라 나온다

	// 캐릭터와 판을 잇는 점·선
	Show(TetherDot, bAnchored, ESlateVisibility::HitTestInvisible);
	Show(TetherLine, bAnchored, ESlateVisibility::HitTestInvisible);
	if (!bAnchored)
	{
		return;
	}
	const float LineY = FMath::Clamp(Anchor.Y, PanelPos.Y + 18.f, PanelPos.Y + FMath::Max(Size.Y - 18.f, 18.f));
	const FVector2D Dot(Anchor.X + Side * TetherDotOffset, LineY);
	if (UCanvasPanelSlot* DotSlot = TetherDot != nullptr ? Cast<UCanvasPanelSlot>(TetherDot->Slot) : nullptr)
	{
		DotSlot->SetPosition(Dot);
	}
	if (UCanvasPanelSlot* LineSlot = TetherLine != nullptr ? Cast<UCanvasPanelSlot>(TetherLine->Slot) : nullptr)
	{
		const float From = Dot.X + Side * 6.f;
		const float Edge = Side > 0.f ? PanelPos.X : PanelPos.X + Size.X;
		LineSlot->SetPosition(FVector2D(FMath::Min(From, Edge), LineY - 0.5f));
		LineSlot->SetSize(FVector2D(FMath::Max(FMath::Abs(Edge - From), 0.f), 1.f));
	}
}

void UNU_BattleCommandWidget::PlayOpen()
{
	// 캐릭터 쪽에서 판이 커지며 나오고, 줄이 위에서부터 차례로 들어온다
	Motion.Finish();
	Motion.Add(CommandRoot, 0.f, 0.24f, FVector2D(-Side * 18.f, 0.f), 0.f, 0.92f);
	Motion.AddChildren(RootRows, 0.06f, 0.03f, 0.18f, FVector2D(-Side * 10.f, 0.f));
	Motion.Add(TetherDot, 0.f, 0.22f, FVector2D::ZeroVector, 0.f, 0.2f);
	Motion.Add(TetherLine, 0.05f, 0.18f, FVector2D(-Side * 8.f, 0.f), 0.f);
	Motion.Start();
}

void UNU_BattleCommandWidget::HandleOwnVisibility(ESlateVisibility InVisibility)
{
	if (InVisibility == ESlateVisibility::Collapsed || InVisibility == ESlateVisibility::Hidden)
	{
		MarkTarget(nullptr);
		LastStateKey.Reset();     // 다시 보일 때 처음부터 다시 그리고, 캐릭터 옆에서 새로 열린다
		LastPhase = 255;
	}
}

void UNU_BattleCommandWidget::MarkTarget(AActor* Target)
{
	if (Target == MarkedTarget.Get() && (Target != nullptr || DimmedTags.IsEmpty()))
	{
		return;
	}
	// 지난 대상과 흐려 둔 표시를 되돌린다
	if (UNU_CombatantStatusWidget* Old = MarkedTag.Get())
	{
		Old->SetTargeted(false);
	}
	for (const TWeakObjectPtr<UNU_CombatantStatusWidget>& Dimmed : DimmedTags)
	{
		if (UNU_CombatantStatusWidget* Tag = Dimmed.Get())
		{
			Tag->SetDimmed(false);
		}
	}
	DimmedTags.Reset();
	MarkedTag.Reset();
	MarkedTarget = Target;
	if (Target == nullptr)
	{
		return;
	}

	// 고른 대상의 표시는 금색 테두리로, 같은 편의 다른 후보들은 흐리게 --- 지금 누구를 노리는지 한눈에
	const ANUBattleAvatar* TargetAvatar = Cast<ANUBattleAvatar>(Target);
	if (UNU_CombatantStatusWidget* Tag = Cast<UNU_CombatantStatusWidget>(FindTagWidget(Target)))
	{
		Tag->SetTargeted(true);
		MarkedTag = Tag;
	}
	if (TargetAvatar == nullptr)
	{
		return;
	}
	for (TActorIterator<ANUBattleAvatar> It(GetWorld()); It; ++It)
	{
		if (*It == TargetAvatar || It->IsAllyCombatant() != TargetAvatar->IsAllyCombatant())
		{
			continue;
		}
		if (UNU_CombatantStatusWidget* Tag = Cast<UNU_CombatantStatusWidget>(FindTagWidget(*It)))
		{
			Tag->SetDimmed(true);
			DimmedTags.Add(Tag);
		}
	}
}

UUserWidget* UNU_BattleCommandWidget::FindTagWidget(AActor* Avatar, UWidgetComponent** OutComponent)
{
	// 아바타의 머리 위 상태 표시(화면 공간 위젯 컴포넌트)
	if (Avatar == nullptr)
	{
		return nullptr;
	}
	TInlineComponentArray<UWidgetComponent*> WidgetComponents(Avatar);
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (UNU_CombatantStatusWidget* Tag = Cast<UNU_CombatantStatusWidget>(WidgetComponent->GetUserWidgetObject()))
		{
			if (OutComponent != nullptr)
			{
				*OutComponent = WidgetComponent;
			}
			return Tag;
		}
	}
	return nullptr;
}

void UNU_BattleCommandWidget::UpdateTargetCursor(UNUBattleCommandComponentBase* Component, float DeltaTime)
{
	AActor* Target = Component != nullptr && Component->GetSelectionPhase() == ENUCommandSelectionPhase::TargetSelection
		? Component->GetCurrentTargetAvatar() : nullptr;

	// 커서는 대상의 머리 위 표시 바로 위, 고리는 발밑
	FVector2D TagPoint = FVector2D::ZeroVector;
	FVector2D FeetPoint = FVector2D::ZeroVector;
	bool bOnScreen = false;
	if (Target != nullptr && !Target->IsHidden())
	{
		APlayerController* PC = GetOwningPlayer();
		UWidgetComponent* TagComponent = nullptr;
		const UUserWidget* Tag = FindTagWidget(Target, &TagComponent);
		const FVector TagWorld = TagComponent != nullptr ? TagComponent->GetComponentLocation() : Target->GetActorLocation() + FVector(0.f, 0.f, 120.f);
		const float TagHalf = Tag != nullptr ? Tag->GetDesiredSize().Y * 0.5f : 17.f;
		bOnScreen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, TagWorld, TagPoint, false)
			&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Target->GetActorLocation(), FeetPoint, false);
		TagPoint.Y -= TagHalf + CursorGap;
	}
	Show(TargetCursor, bOnScreen, ESlateVisibility::HitTestInvisible);
	Show(TargetRing, bOnScreen, ESlateVisibility::HitTestInvisible);
	if (!bOnScreen)
	{
		bCursorShown = false;
		CursorTarget.Reset();
		return;
	}

	// 처음 잡을 땐 그 자리에서 튀어나오고, 대상을 바꾸면 미끄러져 가며 다시 한 번 튄다
	if (!bCursorShown)
	{
		CursorPos = TagPoint;
		RingPos = FeetPoint;
	}
	else
	{
		CursorPos = FMath::Vector2DInterpTo(CursorPos, TagPoint, DeltaTime, 20.f);
		RingPos = FMath::Vector2DInterpTo(RingPos, FeetPoint, DeltaTime, 20.f);
	}
	if (!bCursorShown || Target != CursorTarget.Get())
	{
		LockAge = 0.f;
	}
	bCursorShown = true;
	CursorTarget = Target;
	LockAge += DeltaTime;
	CursorTime += DeltaTime;

	const float Pop = FMath::Square(FMath::Clamp(1.f - LockAge / 0.24f, 0.f, 1.f));
	if (UCanvasPanelSlot* CursorSlot = TargetCursor != nullptr ? Cast<UCanvasPanelSlot>(TargetCursor->Slot) : nullptr)
	{
		CursorSlot->SetPosition(CursorPos + FVector2D(0.f, -4.f + 4.f * FMath::Sin(CursorTime * 6.f) - 14.f * Pop));
		TargetCursor->SetRenderScale(FVector2D(1.f + 0.35f * Pop));
	}
	if (UCanvasPanelSlot* RingSlot = TargetRing != nullptr ? Cast<UCanvasPanelSlot>(TargetRing->Slot) : nullptr)
	{
		RingSlot->SetPosition(RingPos);
		TargetRing->SetRenderScale(FVector2D(1.f + 0.6f * Pop + 0.05f * FMath::Sin(CursorTime * 5.f)));
		TargetRing->SetRenderOpacity(0.88f + 0.12f * FMath::Sin(CursorTime * 5.f) - 0.5f * Pop);
	}
}

void UNU_BattleCommandWidget::ShowNotice(const FText& Message)
{
	if (Text_Notice == nullptr)
	{
		return;
	}
	Text_Notice->SetText(Message);
	Text_Notice->SetRenderOpacity(1.f);
	Show(Text_Notice, true, ESlateVisibility::HitTestInvisible);
	NoticeLeft = 2.2f;
}

void UNU_BattleCommandWidget::HandleActionUnavailable(ENUBattleRootAction Action)
{
	ShowNotice(NSLOCTEXT("NUBattleHUD", "ActionUnavailable", "지금은 쓸 수 없는 행동입니다"));
}

void UNU_BattleCommandWidget::HandleSubmissionFailed(ENUBattleCommandSubmitResult Result)
{
	ShowNotice(NSLOCTEXT("NUBattleHUD", "SubmitFailed", "명령이 받아들여지지 않았습니다"));
}

void UNU_BattleCommandWidget::HandleNaiaSwapFailed(ENUNaiaSwapResult Result)
{
	ShowNotice(NSLOCTEXT("NUBattleHUD", "NaiaSwapFailed", "나이아를 바꿀 수 없습니다"));
}

void UNU_BattleCommandWidget::Select(ENUBattleRootAction Action)
{
	if (UNUBattleCommandComponentBase* Component = FindComponent())
	{
		Component->SelectRootAction(Action);
	}
}

void UNU_BattleCommandWidget::HandleAttackClicked() { Select(ENUBattleRootAction::Attack); }
void UNU_BattleCommandWidget::HandleSkillClicked() { Select(ENUBattleRootAction::Skill); }
void UNU_BattleCommandWidget::HandleItemClicked() { Select(ENUBattleRootAction::Item); }
void UNU_BattleCommandWidget::HandleNaiaSwapClicked() { Select(ENUBattleRootAction::NaiaSwap); }
void UNU_BattleCommandWidget::HandleFleeClicked() { Select(ENUBattleRootAction::Flee); }
