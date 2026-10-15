#include "UI/NUUIManager.h"
#include "UI/NUGameMenuMotion.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/PanelWidget.h"
#include "Inventory/UI/NUItemCellWidget.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "Components/ScrollBox.h"
#include "Components/SpinBox.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Border.h"
#include "Core/NUGameUserSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Layout/WidgetPath.h"
#include "GameFramework/Actor.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/EmberServerManager.h"
#include "PlayerController/NUPlayerController.h"
#include "TimerManager.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Controller/NUPadFocusUIController.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUGameMenuWidget.h"
#include "UI/NUNoticeBannerWidget.h"
#include "UI/NUPadFocusTarget.h"
#include "UI/NUUISettings.h"

/////////////////////////////////////////////////////////////////
// UNUDimLayerWidget
/////////////////////////////////////////////////////////////////

TSharedRef<SWidget> UNUDimLayerWidget::RebuildWidget()
{
	// WBP 없이 쓰는 위젯이라 루트를 코드로 만든다
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
		Root->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

/////////////////////////////////////////////////////////////////
// UNUUIManager
/////////////////////////////////////////////////////////////////

TSharedRef<SWidget> UNUTransitionLayerWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
		Root->SetBrushColor(FLinearColor::Black);
		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UNUTransitionLayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const APlayerController* PC = GetOwningPlayer();
	const APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager : nullptr;
	SetRenderOpacity(CameraManager && CameraManager->bEnableFading ? CameraManager->FadeAmount : 0.0f);
}

UNUUIManager* UNUUIManager::Get(const APlayerController* PC)
{
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	return LP ? LP->GetSubsystem<UNUUIManager>() : nullptr;
}

void UNUUIManager::SetFieldHUDVisible(bool bVisible)
{
	const bool bPreferenceChanged = FieldHUDVisibility.IsUserVisible() != bVisible;
	FieldHUDVisibility.SetUserVisible(bVisible);
	RefreshFieldHUDVisibility(true, bPreferenceChanged);
}

void UNUUIManager::RequestFieldHUDHidden(FName Reason, bool bHide, const UObject* Owner, bool bAnimate)
{
	FieldHUDVisibility.RequestHidden(Owner, Reason, bHide);
	RefreshFieldHUDVisibility(bAnimate);
}

void UNUUIManager::ReleaseFieldHUDHideRequests(const UObject* Owner, bool bAnimate)
{
	FieldHUDVisibility.ReleaseOwner(Owner);
	RefreshFieldHUDVisibility(bAnimate);
}

void UNUUIManager::RefreshFieldHUDVisibility(bool bAnimate, bool bPreferenceChanged)
{
	FieldHUDVisibility.PruneInvalidOwners();
	const bool bVisible = FieldHUDVisibility.IsVisible();
	const bool bSuppressed = FieldHUDVisibility.HasRequests();
	const bool bVisibilityChanged = bFieldHUDVisible != bVisible;
	const bool bSuppressionChanged = bFieldHUDSuppressed != bSuppressed;
	bFieldHUDVisible = bVisible;
	bFieldHUDSuppressed = bSuppressed;
	if (bVisibilityChanged)
	{
		APlayerController* PC = GetOwningPlayerController();
		FieldHUDMotion.SetVisible(bVisible, PC ? UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PC) : FGeometry());
	}
	if (!bAnimate || !FSlateApplication::IsInitialized())
	{
		StopFieldHUDMotion();
	}
	else if (bVisibilityChanged && !FieldHUDMotionTickHandle.IsValid())
	{
		FieldHUDMotionTickHandle = FSlateApplication::Get().OnPreTick().AddUObject(this, &ThisClass::TickFieldHUDMotion);
	}
	RefreshFieldHUDRequestTick();
	// Preference/suppression changes also refresh the toggle, even if the effective target is unchanged.
	if (bVisibilityChanged || bSuppressionChanged || bPreferenceChanged)
	{
		OnFieldHUDVisibilityChanged.Broadcast();
	}
}

void UNUUIManager::RefreshFieldHUDRequestTick()
{
	if (FieldHUDVisibility.HasRequests())
	{
		if (!FieldHUDRequestTickHandle.IsValid())
		{
			FieldHUDRequestTickHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &ThisClass::TickFieldHUDRequests), 0.25f);
		}
	}
	else if (FieldHUDRequestTickHandle.IsValid())
	{
		FTSTicker::RemoveTicker(FieldHUDRequestTickHandle);
		FieldHUDRequestTickHandle.Reset();
	}
}

bool UNUUIManager::TickFieldHUDRequests(float DeltaTime)
{
	// Direct RemoveFromParent/visibility-driven closes also release the window's request.
	// Do not broadcast while iterating Windows: a listener can open or close another window.
	for (const FNUUIWindowEntry& Entry : Windows)
	{
		if (Entry.Params.bHideFieldHUD && !IsEntryShown(Entry))
		{
			FieldHUDVisibility.RequestHidden(Entry.Widget.Get(), Entry.Reason, false);
		}
	}
	RefreshFieldHUDVisibility(true);
	return FieldHUDVisibility.HasRequests();
}

void UNUUIManager::PresentFieldHUDWidget(UWidget* Widget, ESlateVisibility NormalVisibility)
{
	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		// Focus/hover presentation can still change the UserWidget's opacity independently.
		FieldHUDMotion.Present(Widget, NormalVisibility, ENUFieldHUDMotionRole::Container);
		FieldHUDMotion.Present(UserWidget->GetRootWidget(), ESlateVisibility::SelfHitTestInvisible, ENUFieldHUDMotionRole::Content);
	}
	else
	{
		FieldHUDMotion.Present(Widget, NormalVisibility);
	}
}

void UNUUIManager::PresentFieldHUDGroup(UPanelWidget* Container)
{
	if (!Container)
	{
		return;
	}
	FieldHUDMotion.Present(Container, ESlateVisibility::SelfHitTestInvisible, ENUFieldHUDMotionRole::Container);
	for (UWidget* Child : Container->GetAllChildren())
	{
		// E.g. the minimap still owns its map-availability and battle visibility.
		FieldHUDMotion.Present(Child, Child->GetVisibility(), ENUFieldHUDMotionRole::Content);
	}
}

void UNUUIManager::ForgetFieldHUDWidget(UWidget* Widget)
{
	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		ForgetFieldHUDWidget(UserWidget->GetRootWidget());
	}
	else if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (UWidget* Child : Panel->GetAllChildren())
		{
			ForgetFieldHUDWidget(Child);
		}
	}
	FieldHUDMotion.Forget(Widget);
}

void UNUUIManager::TickFieldHUDMotion(float DeltaTime)
{
	if (!FieldHUDMotion.Advance(DeltaTime))
	{
		StopFieldHUDMotion();
	}
}

void UNUUIManager::StopFieldHUDMotion()
{
	if (FieldHUDMotionTickHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().Remove(FieldHUDMotionTickHandle);
	}
	FieldHUDMotionTickHandle.Reset();
	FieldHUDMotion.Finish();
}

void UNUUIManager::ForEachWidgetDeep(UWidget* Widget, TFunctionRef<void(UWidget*)> Fn)
{
	if (Widget == nullptr)
	{
		return;
	}
	Fn(Widget);
	if (const UUserWidget* User = Cast<UUserWidget>(Widget))
	{
		if (User->WidgetTree != nullptr)
		{
			ForEachWidgetDeep(User->WidgetTree->RootWidget, Fn);
		}
	}
	else if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			ForEachWidgetDeep(Panel->GetChildAt(Index), Fn);
		}
	}
	else if (const UListViewBase* List = Cast<UListViewBase>(Widget))
	{
		for (UUserWidget* Entry : List->GetDisplayedEntryWidgets())
		{
			ForEachWidgetDeep(Entry, Fn);
		}
	}
}

namespace
{
	// 포커스가 설 수 있는 조작부인지. 중첩 위젯(공용 버튼 WBP 등) 자체가 포커스를 받아도 안에 조작부가 있으면 그쪽을 조작부로 본다
	bool IsFocusableControl(UWidget* Widget)
	{
		const TSharedPtr<SWidget> Slate = Widget != nullptr ? Widget->GetCachedWidget() : nullptr;
		if (!Slate.IsValid() || !Slate->SupportsKeyboardFocus())
		{
			return false;
		}
		// 개수 칸(SpinBox)은 패드가 설 곳이 아니다 --- 포커스를 받으면 글자 입력 상태가 되는데 패드 키는 안 받고,
		// 입력 중인 칸은 엔진이 표시를 안 바꿔서 창이 D패드로 값을 올려도 숫자가 그대로였다 (공방 상세, 닫기 버튼에서 오른쪽)
		if (Widget->IsA<USpinBox>())
		{
			return false;
		}
		// 조상까지 본다 --- UMG IsVisible·GetIsEnabled는 자기 것만 봐서, 접히거나 꺼진 공용 버튼 안쪽 버튼도 후보가 됐다
		// (상점 판매 줄의 접힌 구매 버튼 안쪽 버튼에 포커스를 주다 실패해 포커스가 사라졌다)
		for (TSharedPtr<SWidget> Cursor = Slate; Cursor.IsValid(); Cursor = Cursor->GetParentWidget())
		{
			if (!Cursor->GetVisibility().IsVisible() || !Cursor->IsEnabled())
			{
				return false;
			}
		}
		if (Widget->IsA<UUserWidget>())
		{
			bool bInnerControl = false;
			UNUUIManager::ForEachWidgetDeep(Widget, [Widget, &bInnerControl](UWidget* Child) { bInnerControl |= Child != Widget && IsFocusableControl(Child); });
			return !bInnerControl;
		}
		return true;
	}

	// 액터에 붙는 창을 그 지점의 어느 쪽에 놓을지 (0 = 지점의 오른쪽·아래, 1 = 왼쪽·위).
	// 지금 쪽이 화면 안에 들어가면 그대로 둬 가장자리에서 덜덜거리지 않게 하고, 안 들어가면 반대쪽으로 넘긴다
	double PickAnchorSide(double Base, double Size, double ViewportSize, double CurrentAlignment)
	{
		auto Fits = [Base, Size, ViewportSize](double Alignment)
		{
			const double Left = Base - Alignment * Size;
			return Left >= 0.0 && Left + Size <= ViewportSize;
		};
		if (Fits(CurrentAlignment))
		{
			return CurrentAlignment;
		}
		const double Flipped = 1.0 - CurrentAlignment;
		return Fits(Flipped) ? Flipped : CurrentAlignment;
	}
}

void UNUUIManager::OpenWindow(UUserWidget* Widget, const FNUUIWindowParams& Params, FSimpleDelegate OnCloseRequested)
{
	SweepStaleWindows();

	if (Widget == nullptr || FindEntry(Widget) != INDEX_NONE)
	{
		return;
	}

	FNUUIWindowEntry Entry;
	Entry.Widget = Widget;
	Entry.Params = Params;
	Entry.OnCloseRequested = MoveTemp(OnCloseRequested);
	Entry.Reason = FName(*FString::Printf(TEXT("UI:%s"), *Widget->GetName()));
	Entry.OpenSerial = ++OpenSerialCounter;
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWidget> Focused;
		FSlateApplication::Get().ForEachUser([&Focused](FSlateUser& User) { if (!Focused.IsValid()) { Focused = User.GetFocusedWidget(); } });
		Entry.PreviousFocus = FindWidgetForSlate(Focused);
	}
	Windows.Add(Entry);

	// AddToViewport(NativeConstruct)가 재진입해 Windows가 재할당될 수 있어 배열 참조 대신 로컬 복사본을 쓴다
	// 상주 창(bVisibilityDriven)은 이미 뷰포트에 있으므로 아래 조건에서 자연히 건너뛴다
	if (!Widget->IsInViewport())
	{
		Widget->AddToViewport(Params.Layer);
	}
	if (Params.Layer >= NUUILayer::Popup && Params.Layer <= NUUILayer::Modal)
	{
		SetPopupButtonHover(Widget, true);
	}
	// 상주 창은 소유 코드가 가시성만 꺼서 닫는다 --- 그 순간 등록을 풀어야 포커스·패드 이동 막힘이 바로 돌아온다
	if (Params.bVisibilityDriven)
	{
		Widget->OnVisibilityChanged.AddUniqueDynamic(this, &UNUUIManager::HandleResidentVisibilityChanged);
	}
	// 여닫는 창은 D패드·방향키 포커스 이동을 그 창 안에 가둔다 (옆 창이나 HUD로 새지 않게 --- 창 사이 이동은 LB·RB).
	// Slate 기본 탐색은 창 사각형 안에 있으면 남의 창 조작부도 후보로 잡는다 (창 루트가 화면을 덮는 캔버스라 사각형이 화면이다).
	// 그래서 기본 탐색을 쓰지 않고 매니저가 그 방향으로 가장 가까운 조작부를 직접 고른다
	if (Params.Layer >= NUUILayer::Window)
	{
		FCustomWidgetNavigationDelegate Nav;
		Nav.BindDynamic(this, &UNUUIManager::HandleWindowNavigation);
		for (const EUINavigation Direction : { EUINavigation::Up, EUINavigation::Down, EUINavigation::Left, EUINavigation::Right })
		{
			Widget->SetNavigationRuleCustom(Direction, Nav);
		}
		Widget->SetNavigationRuleBase(EUINavigation::Next, EUINavigationRule::Stop);
		Widget->SetNavigationRuleBase(EUINavigation::Previous, EUINavigationRule::Stop);

		// 스크롤 상자 안에서 포커스가 움직이면 보이게 따라 스크롤한다 (키 설정 목록·아이템 격자처럼 긴 창). WBP 기본값(NoScroll)만 바꾼다
		ForEachWidgetDeep(Widget, [](UWidget* Child)
		{
			UScrollBox* ScrollBox = Cast<UScrollBox>(Child);
			if (ScrollBox == nullptr || ScrollBox->GetScrollWhenFocusChanges() != EScrollWhenFocusChanges::NoScroll)
			{
				return;
			}
			// 즉시 스크롤 --- 부드럽게 굴리면 그동안 포커스 테두리가 먼저 도착해 아직 잘려 있는 칸 위에 그려진다.
			// 포커스 칸이 가장자리에 딱 붙지 않게 하는 여백은 WBP 스크롤 상자의 NavigationScrollPadding (C++에서는 못 넣는다)
			ScrollBox->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
		});
	}

	if (ANUPlayerController* PC = GetNUPlayerController())
	{
		if (Params.bRequestCursor)
		{
			PC->RequestCursor(Entry.Reason, true, Widget);
		}
		if (Params.bBlockGameInput)
		{
			PC->RequestGameInputBlock(Entry.Reason, true, Widget);
		}
	}

	// 액터를 따라다니는 창은 열린 자리와 액터의 화면 자리 차이를 재 둔다
	// (마우스로 열면 커서가 캐릭터에서 떨어진 만큼 그 간격을 지키며 따라간다)
	if (Params.AnchorActor.IsValid())
	{
		const int32 AnchorIndex = FindEntry(Widget);
		UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get();
		FVector2D AnchorPos = FVector2D::ZeroVector;
		if (AnchorIndex != INDEX_NONE && Viewport != nullptr
			&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
				GetNUPlayerController(), Params.AnchorActor->GetActorLocation(), AnchorPos, true))
		{
			const FMargin Offsets = Viewport->GetWidgetSlot(Widget).Offsets;
			Windows[AnchorIndex].AnchorOffset = FVector2D(Offsets.Left, Offsets.Top) - AnchorPos;
		}
		RefreshAnchorTick();
	}

	if (Params.bHideFieldHUD && FindEntry(Widget) != INDEX_NONE)
	{
		RequestFieldHUDHidden(Entry.Reason, true, Widget);
	}

	// 창이 뜨는 소리 --- 같이 따라 뜨는 동반 창은 뺀다 (한 번에 두세 장이 열린다).
	// 대화창·컨텍스트 메뉴·툴팁 층은 여기서 내지 않는다 (저마다 맡은 곳이 낸다)
	if (Params.OpenSFX.IsValid())
	{
		UNUSFXSubsystem::PlayCommon(Widget, Params.OpenSFX);
	}
	else if (!Params.bCompanion)
	{
		if (Params.Layer >= NUUILayer::Window && Params.Layer < NUUILayer::Dialogue)
		{
			UNUSFXSubsystem::PlayCommon(Widget, NU::SFX::UI::WindowOpen);
		}
		else if (Params.Layer >= NUUILayer::Popup && Params.Layer < NUUILayer::Modal)
		{
			UNUSFXSubsystem::PlayCommon(Widget, NU::SFX::UI::PopupOpen);
		}
		else if (Params.Layer >= NUUILayer::Modal && Params.Layer < NUUILayer::Notice)
		{
			UNUSFXSubsystem::PlayCommon(Widget, NU::SFX::UI::ModalOpen);
		}
	}
	UpdateDim();
	OnWindowsChanged.Broadcast();
	RequestGamepadFocus();
}

void UNUUIManager::SetPopupButtonHover(UWidget* Root, bool bEnabled)
{
	if (bEnabled) { PopupButtonHover.RegisterRoot(Root); }
	else { PopupButtonHover.UnregisterRoot(Root); }
	if (PopupButtonHover.HasTargets() && FSlateApplication::IsInitialized())
	{
		if (!PopupButtonHoverTickHandle.IsValid())
		{
			PopupButtonHoverTickHandle = FSlateApplication::Get().OnPreTick().AddUObject(this, &ThisClass::TickPopupButtonHover);
		}
	}
	else { StopPopupButtonHover(); }
}

void UNUUIManager::TickPopupButtonHover(float DeltaTime)
{
	PopupButtonHover.Advance(DeltaTime);
	if (!PopupButtonHover.HasTargets()) { StopPopupButtonHover(); }
}

void UNUUIManager::StopPopupButtonHover()
{
	if (PopupButtonHoverTickHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().Remove(PopupButtonHoverTickHandle);
	}
	PopupButtonHoverTickHandle.Reset();
	PopupButtonHover.Reset();
}

void UNUUIManager::CloseWindow(UUserWidget* Widget)
{
	if (Widget && Widget == GameMenu)
	{
		// 닫히는 동안(DeferClose 가 메뉴를 꺼 둔다) ESC 로 다시 불리면 닫힘 소리를 또 내지 않는다
		const bool bAlreadyClosing = !Widget->GetIsEnabled();
		const TWeakObjectPtr<UNUUIManager> WeakThis(this);
		const TWeakObjectPtr<UUserWidget> WeakWidget(Widget);
		if (NUGameMenuMotion::DeferClose(Widget, FSimpleDelegate::CreateLambda([WeakThis, WeakWidget]()
		{
			if (WeakThis.IsValid() && WeakWidget.IsValid()) { WeakThis->CloseWindow(WeakWidget.Get()); }
		})))
		{
			if (!bAlreadyClosing)
			{
				UNUSFXSubsystem::PlayCommon(Widget, NU::SFX::UI::WindowClose);
			}
			return;
		}
	}
	SweepStaleWindows();

	if (FindEntry(Widget) == INDEX_NONE)
	{
		return;
	}

	// 이 창이 띄운 딸림 창(칸에서 연 아이템 메뉴·수량 창)을 먼저 닫는다 --- 주인 창만 닫으면 화면에 남는다.
	// 닫는 동안 목록이 바뀌므로 대상을 먼저 모으고, 아래에서 자리를 다시 찾는다
	TArray<UUserWidget*, TInlineAllocator<2>> Owned;
	for (const FNUUIWindowEntry& Other : Windows)
	{
		if (Other.Params.OwnerWindow.Get() == Widget && Other.Widget.IsValid())
		{
			Owned.Add(Other.Widget.Get());
		}
	}
	for (UUserWidget* Child : Owned)
	{
		RequestCloseWindow(Child);
	}

	const int32 Index = FindEntry(Widget);
	if (Index == INDEX_NONE)
	{
		return;
	}

	// 목록에서 먼저 제거 — RemoveFromParent 재진입에 안전
	const FNUUIWindowEntry Entry = Windows[Index];
	const bool bHadFocus = Entry.Widget.IsValid() && Entry.Widget->HasFocusedDescendants();
	Windows.RemoveAt(Index);
	SetPopupButtonHover(Widget, false);
	if (Entry.Params.CloseSFX.IsValid())
	{
		UNUSFXSubsystem::PlayCommon(Widget, Entry.Params.CloseSFX);
	}
	else if (!Entry.Params.bCompanion && Entry.Params.Layer >= NUUILayer::Window && Entry.Params.Layer < NUUILayer::Dialogue)
	{
		UNUSFXSubsystem::PlayCommon(Widget, NU::SFX::UI::WindowClose);
	}

	// 상주 창은 뷰포트에서 내리지 않는다 — 숨기는 것은 소유 코드의 몫이고 매니저는 등록만 해제한다
	if (!Entry.Params.bVisibilityDriven && Entry.Widget.IsValid() && Entry.Widget->IsInViewport())
	{
		Entry.Widget->RemoveFromParent();
	}
	ReleaseEntryRequests(Entry);
	ReturnFocusAfterClose(Entry, bHadFocus);

	RefreshAnchorTick();
	UpdateDim();
	OnWindowsChanged.Broadcast();
	RequestGamepadFocus();
}

bool UNUUIManager::CloseTopWindow()
{
	SweepStaleWindows();

	const int32 BestIndex = FindTopWindowIndex([](const FNUUIWindowEntry& Entry) { return Entry.Params.bCloseOnEscape; });

	if (BestIndex == INDEX_NONE)
	{
		return false;
	}

	FNUUIWindowEntry& Best = Windows[BestIndex];
	if (Best.OnCloseRequested.IsBound())
	{
		FSimpleDelegate Pending = Best.OnCloseRequested;
		Pending.ExecuteIfBound(); // 실행 중 CloseWindow 재진입으로 Windows가 바뀔 수 있어 복사본 호출
	}
	else
	{
		CloseWindow(Best.Widget.Get());
	}
	return true;
}

int32 UNUUIManager::FindTopWindowIndex(TFunctionRef<bool(const FNUUIWindowEntry&)> Filter) const
{
	int32 BestIndex = INDEX_NONE;
	for (int32 i = 0; i < Windows.Num(); ++i)
	{
		if (!Filter(Windows[i]))
		{
			continue;
		}
		if (BestIndex == INDEX_NONE
			|| Windows[i].Params.Layer > Windows[BestIndex].Params.Layer
			|| (Windows[i].Params.Layer == Windows[BestIndex].Params.Layer && Windows[i].OpenSerial > Windows[BestIndex].OpenSerial))
		{
			BestIndex = i;
		}
	}
	return BestIndex;
}

void UNUUIManager::HandleGamepadModeChanged(bool bGamepad)
{
	if (UNUPadFocusUIController* FocusFrame = GetController<UNUPadFocusUIController>())
	{
		FocusFrame->SetActive(bGamepad);
	}
	// 메뉴 단축키 표기(키보드 키 / 패드 버튼)와 창 안내 문구가 장치를 따라간다
	RefreshGameMenu();
	OnGamepadModeChanged.Broadcast(bGamepad);
	if (bGamepad)
	{
		RequestGamepadFocus();
		return;
	}

	// 마우스·키보드로 돌아옴 --- 버튼에 포커스가 남아 있으면 Enter(채팅 열기)·Space가 그 버튼을 눌러 버리므로 뷰포트로 돌린다.
	// 글자 입력칸(채팅·수량)에 있는 포커스는 그대로 둔다
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	bool bButtonFocused = false;
	FSlateApplication::Get().ForEachUser([this, &bButtonFocused](FSlateUser& User)
	{
		const TSharedPtr<SWidget> Focused = User.GetFocusedWidget();
		// 아이템 칸도 버튼처럼 --- 남겨 두면 포커스 툴팁이 마우스 모드에서도 떠 있다
		bButtonFocused |= Focused.IsValid() && (Focused->GetType() == TEXT("SButton") || Cast<UNUItemCellWidget>(FindWidgetForSlate(Focused)) != nullptr);
	});
	if (bButtonFocused)
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void UNUUIManager::RequestGamepadFocus()
{
	ANUPlayerController* PC = GetNUPlayerController();
	if (PC == nullptr || !PC->IsGamepadMode() || PC->GetWorld() == nullptr)
	{
		return;
	}
	PC->GetWorldTimerManager().SetTimerForNextTick(this, &UNUUIManager::FocusTopWindowDeferred);
}

void UNUUIManager::FocusTopWindowDeferred()
{
	SweepStaleWindows();
	const ANUPlayerController* PC = GetNUPlayerController();
	if (PC == nullptr || !PC->IsGamepadMode())
	{
		return;
	}
	// 여닫는 창(Window 층 이상)만 --- HUD 층 상주 창의 버튼에 포커스가 가면 안 된다.
	// 동반 창(소지품에 따라 뜨는 장비창, 상점·공방·창고가 연 소지품)은 나중에 열려 맨 위에 있어도 건너뛴다 --- 첫 포커스는 주 창의 것.
	// 주 창 없이 동반 창만 떠 있으면 그중 맨 위에 준다
	const auto IsShownWindow = [](const FNUUIWindowEntry& Entry) { return Entry.Params.Layer >= NUUILayer::Window && IsEntryShown(Entry); };
	int32 TopIndex = FindTopWindowIndex([&IsShownWindow](const FNUUIWindowEntry& Entry) { return !Entry.Params.bCompanion && IsShownWindow(Entry); });
	if (TopIndex == INDEX_NONE)
	{
		TopIndex = FindTopWindowIndex(IsShownWindow);
	}
	if (TopIndex == INDEX_NONE || Windows[TopIndex].Params.bOwnsFocus)
	{
		return;
	}
	// 같은 층(이상)의 다른 창에 이미 포커스가 있으면 둔다 (마우스로 누른 칸, 닫힌 창이 돌려준 자리).
	// 더 높은 층(팝업·모달)이 새로 열린 경우는 아래 창의 포커스를 두고 볼 수 없으니 준다
	for (const FNUUIWindowEntry& Entry : Windows)
	{
		if (Entry.Params.Layer >= Windows[TopIndex].Params.Layer && IsEntryShown(Entry) && Entry.Widget.IsValid() && Entry.Widget->HasFocusedDescendants())
		{
			return;
		}
	}
	// 시트 창은 첫 자리를 스스로 정한다 (공방·상점은 목록 첫 줄, 거래는 내 제안 칸) ---
	// 키보드로 연 창에서 패드를 잡아도 패드로 열었을 때와 같은 자리에 선다
	UUserWidget* Top = Windows[TopIndex].Widget.Get();
	if (UNUSheetWidgetBase* Sheet = Cast<UNUSheetWidgetBase>(Top))
	{
		Sheet->FocusForPad();
		return;
	}
	// 창 안의 페이지도 첫 자리를 정할 수 있다 --- 통합 메뉴를 인벤토리로 열면 왼쪽 메뉴 첫 버튼이 아니라 첫 칸
	UWidget* PageTarget = nullptr;
	ForEachWidgetDeep(Top, [&PageTarget](UWidget* Child)
	{
		const INUPadFocusTarget* Page = Cast<INUPadFocusTarget>(Child);
		if (PageTarget == nullptr && Page != nullptr && Child->IsVisible())
		{
			PageTarget = Page->GetPadFocusTarget();
		}
	});
	if (PageTarget != nullptr)
	{
		PageTarget->SetFocus();
		return;
	}
	FocusFirstButtonNow(Top);
}

TArray<UUserWidget*> UNUUIManager::GetShownWindows(int32 Layer) const
{
	TArray<UUserWidget*> Result;
	for (const FNUUIWindowEntry& Entry : Windows)
	{
		if (Entry.Params.Layer == Layer && IsEntryShown(Entry) && Entry.Widget.IsValid())
		{
			Result.Add(Entry.Widget.Get());
		}
	}
	return Result;
}

UWidget* UNUUIManager::FindWidgetForSlate(const TSharedPtr<SWidget>& SlateWidget) const
{
	UWidget* Found = nullptr;
	for (const FNUUIWindowEntry& Entry : Windows)
	{
		if (Found == nullptr && SlateWidget.IsValid() && Entry.Widget.IsValid())
		{
			ForEachWidgetDeep(Entry.Widget.Get(), [&Found, &SlateWidget](UWidget* Child)
			{
				if (Found == nullptr && Child->GetCachedWidget() == SlateWidget)
				{
					Found = Child;
				}
			});
		}
	}
	return Found;
}

UWidget* UNUUIManager::FindFirstFocusableIn(UWidget* Root)
{
	UWidget* Target = nullptr;
	ForEachWidgetDeep(Root, [Root, &Target](UWidget* Child)
	{
		if (Target == nullptr && Child != Root && IsFocusableControl(Child))
		{
			Target = Child;
		}
	});
	return Target;
}

void UNUUIManager::FocusFirstFocusableIn(UWidget* Root)
{
	if (UWidget* Target = FindFirstFocusableIn(Root))
	{
		Target->SetFocus();
	}
}

UWidget* UNUUIManager::HandleWindowNavigation(EUINavigation Direction)
{
	if (!FSlateApplication::IsInitialized())
	{
		return nullptr;
	}
	TSharedPtr<SWidget> Focused;
	FSlateApplication::Get().ForEachUser([&Focused](FSlateUser& User)
	{
		if (!Focused.IsValid())
		{
			Focused = User.GetFocusedWidget();
		}
	});
	if (!Focused.IsValid())
	{
		return nullptr;
	}
	// 포커스가 들어 있는 창을 Slate 부모를 타고 올라가 찾는다 --- 맨 위 창이 아니라 지금 보고 있는 창 안에서만 고른다
	UUserWidget* Owner = nullptr;
	for (TSharedPtr<SWidget> Cursor = Focused; Cursor.IsValid() && Owner == nullptr; Cursor = Cursor->GetParentWidget())
	{
		for (const FNUUIWindowEntry& Entry : Windows)
		{
			if (Entry.Widget.IsValid() && Entry.Widget->GetCachedWidget() == Cursor)
			{
				Owner = Entry.Widget.Get();
				break;
			}
		}
	}
	const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Focused.ToSharedRef());
	if (!Window.IsValid() || Owner == nullptr)
	{
		return nullptr;
	}

	// 히트 테스트 격자에 있는 것 = 이번 프레임에 실제로 그려진 것. 스크롤 상자 밖으로 잘려 안 그려진 줄은 좌표가 옛것이라 뺀다
	const FHittestGrid& Grid = Window->GetHittestGrid();
	const FVector2f FromMin(Focused->GetCachedGeometry().GetAbsolutePosition());
	const FVector2f FromMax = FromMin + FVector2f(Focused->GetCachedGeometry().GetAbsoluteSize());
	// 좌우로 갈 때는 가로로 나아가고 세로 범위로 같은 줄을 가린다 (상하는 반대)
	const int32 MoveAxis = Direction == EUINavigation::Left || Direction == EUINavigation::Right ? 0 : 1;
	const int32 BandAxis = 1 - MoveAxis;
	// 포커스 위젯을 담은 그릇(목록·타일 뷰 자체도 포커스를 받는다)은 같은 줄로 치지 않는다 --- 늘 걸쳐 있어 안의 항목을 이긴다
	TArray<const SWidget*, TInlineAllocator<32>> Holders;
	for (TSharedPtr<SWidget> Parent = Focused->GetParentWidget(); Parent.IsValid(); Parent = Parent->GetParentWidget())
	{
		Holders.Add(Parent.Get());
	}
	UWidget* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	bool bBestBeside = true;
	UWidget* BandBest = nullptr;
	float BandPrimary = TNumericLimits<float>::Max();
	float BandSecondary = TNumericLimits<float>::Max();
	ForEachWidgetDeep(Owner, [&](UWidget* Child)
	{
		const TSharedPtr<SWidget> Slate = Child->GetCachedWidget();
		if (Slate == Focused || !IsFocusableControl(Child) || !Grid.ContainsWidget(Slate.Get()))
		{
			return;
		}
		const FVector2f Min(Slate->GetCachedGeometry().GetAbsolutePosition());
		const FVector2f Max = Min + FVector2f(Slate->GetCachedGeometry().GetAbsoluteSize());
		const FVector2f Delta = (Min + Max - FromMin - FromMax) * 0.5f;
		// 그 방향으로 나아간 거리(Primary)가 있어야 하고, 옆으로 벗어난 거리(Secondary)는 두 배로 센다
		float Primary = 0.0f, Secondary = 0.0f;
		switch (Direction)
		{
		case EUINavigation::Up:    Primary = -Delta.Y; Secondary = FMath::Abs(Delta.X); break;
		case EUINavigation::Down:  Primary = Delta.Y;  Secondary = FMath::Abs(Delta.X); break;
		case EUINavigation::Left:  Primary = -Delta.X; Secondary = FMath::Abs(Delta.Y); break;
		case EUINavigation::Right: Primary = Delta.X;  Secondary = FMath::Abs(Delta.Y); break;
		default: return;
		}
		if (Primary <= 1.0f)
		{
			return;
		}
		const float Score = Primary + Secondary * 2.0f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Child;
			// 중심이 포커스 위젯의 그 방향 가장자리를 넘어야 정말 그쪽에 있는 것이다
			bBestBeside = Primary > (FromMax[MoveAxis] - FromMin[MoveAxis]) * 0.5f;
		}
		// 같은 줄·열에 걸친 것 중 가장 가까운 것도 같이 찾아 둔다
		if (FMath::Min(FromMax[BandAxis], Max[BandAxis]) - FMath::Max(FromMin[BandAxis], Min[BandAxis]) > 1.0f && !Holders.Contains(Slate.Get())
			&& (Primary < BandPrimary || (Primary == BandPrimary && Secondary < BandSecondary)))
		{
			BandPrimary = Primary;
			BandSecondary = Secondary;
			BandBest = Child;
		}
	});
	// 점수로 고른 것이 그쪽에 있지 않으면(폭이 넓은 조작부에서는 위아래 줄의 것이 중심만 조금 나아가 있어도 점수가 좋다)
	// 같은 줄·열에 걸친 것으로 바꾼다 --- 통합 메뉴 인벤토리의 분류 탭 "장비"에서 오른쪽이 "소모품"이 아니라 아래 줄 필터 "무기"로 갔다.
	// 걸친 것이 없으면 점수로 고른 것 그대로 (갈 곳이 없어지지는 않는다)
	if (!bBestBeside && BandBest != nullptr)
	{
		Best = BandBest;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[패드] %s 창 안 %s 최근접 이동: %s"), *Owner->GetName(), *UEnum::GetValueAsString(Direction), *GetPathNameSafe(Best));
	return Best;
}

void UNUUIManager::FocusFirstButtonIn(UWidget* Root)
{
	UWorld* World = Root != nullptr ? Root->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<UWidget> WeakRoot(Root);
	World->GetTimerManager().SetTimerForNextTick([WeakRoot]()
	{
		// 이미 안에 포커스가 있으면 둔다 (창 첫 포커스용)
		UWidget* Target = WeakRoot.Get();
		if (Target != nullptr && !Target->HasFocusedDescendants())
		{
			FocusFirstButtonNow(Target);
		}
	});
}

void UNUUIManager::FocusFirstButtonNow(UWidget* Root)
{
	if (Root == nullptr)
	{
		return;
	}

	// 아이템 칸이 있는 창(소지품·창고·장비)은 첫 칸이 본체다 --- 정렬·닫기 버튼보다 먼저
	UNUItemCellWidget* FirstCell = nullptr;
	ForEachWidgetDeep(Root, [&FirstCell](UWidget* Child)
	{
		if (FirstCell == nullptr && IsFocusableControl(Child))
		{
			FirstCell = Cast<UNUItemCellWidget>(Child);
		}
	});
	if (FirstCell != nullptr)
	{
		FirstCell->SetFocus();
		return;
	}

	UButton* Target = nullptr;
	UButton* CloseFallback = nullptr;
	ForEachWidgetDeep(Root, [&Target, &CloseFallback](UWidget* Child)
	{
		if (Target != nullptr)
		{
			return;
		}
		// 접힌 패널 안 버튼은 뺀다 (조상까지 본다) --- 공방 창의 접힌 상세 패널 [만들기]가 잡혀 포커스가 어디에도 안 섰다
		UButton* Button = Cast<UButton>(Child);
		if (Button == nullptr || !IsFocusableControl(Button))
		{
			return;
		}
		if (Button->GetTypedOuter<UNU_CloseButtonWidget>() != nullptr)
		{
			CloseFallback = CloseFallback != nullptr ? CloseFallback : Button;
			return;
		}
		Target = Button;
	});

	if (Target == nullptr)
	{
		Target = CloseFallback;
	}
	if (Target != nullptr)
	{
		Target->SetFocus();
		UE_LOG(LogTemp, Verbose, TEXT("[패드] 첫 버튼 포커스: %s (%s 아래)"), *Target->GetPathName(), *GetNameSafe(Root));
		return;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[패드] %s 아래에 포커스할 버튼 없음"), *GetNameSafe(Root));
	// 버튼이 없으면 창 자체가 포커스 가능할 때 창에 준다 (ESC·B를 직접 받는 팝업)
	if (UUserWidget* User = Cast<UUserWidget>(Root); User != nullptr && User->IsFocusable())
	{
		User->SetFocus();
	}
}

bool UNUUIManager::IsWindowOpen(const UUserWidget* Widget) const
{
	const int32 Index = FindEntry(Widget);
	return Index != INDEX_NONE && IsEntryShown(Windows[Index]);
}

bool UNUUIManager::IsEntryShown(const FNUUIWindowEntry& Entry)
{
	const UUserWidget* Widget = Entry.Widget.Get();
	if (Widget == nullptr || !Widget->IsInViewport())
	{
		return false;
	}
	if (!Entry.Params.bVisibilityDriven)
	{
		return true;
	}
	// 상주 창은 뷰포트에 계속 있으므로 가시성으로 판정한다
	const ESlateVisibility Vis = Widget->GetVisibility();
	return Vis != ESlateVisibility::Collapsed && Vis != ESlateVisibility::Hidden;
}

bool UNUUIManager::IsAnyWindowOpen(int32 MinLayer) const
{
	for (const FNUUIWindowEntry& Entry : Windows)
	{
		if (Entry.Params.Layer >= MinLayer && IsEntryShown(Entry))
		{
			return true;
		}
	}
	return false;
}

void UNUUIManager::ShowNotice(const FText& Message, ENUNoticeType NoticeType)
{
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	if (Settings == nullptr)
	{
		return;
	}

	const bool bEnabled = NoticeType == ENUNoticeType::LocationEntry ? Settings->bEnableLocationEntryNotice : Settings->bEnableNoticeBanner;
	if (!bEnabled || Message.IsEmptyOrWhitespace())
	{
		// 장소 진입 알림은 공용 알림과 독립적으로 켜고 끈다.
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	TSubclassOf<UNUNoticeBannerWidget> BannerClass = Settings->NoticeBannerClass.LoadSynchronous();

	if (PC == nullptr || BannerClass == nullptr)
	{
		// 배너가 없어도 동작이 멈추지 않도록 --- 호출부가 분기하지 않게 한다
		UE_LOG(LogTemp, Warning, TEXT("[알림] %s (NoticeBannerClass 미지정 --- 프로젝트 세팅 > 게임 > NU UI)"), *Message.ToString());
		return;
	}

	// 레벨이 바뀌었으면 이전 월드의 배너는 버린다 (딤과 같은 처리)
	if (NoticeBanner != nullptr && NoticeBanner->GetWorld() != PC->GetWorld())
	{
		NoticeBanner->RemoveFromParent();
		NoticeBanner = nullptr;
	}
	if (NoticeBanner == nullptr || NoticeBanner->GetClass() != BannerClass)
	{
		NoticeBanner = CreateWidget<UNUNoticeBannerWidget>(PC, BannerClass);
		if (NoticeBanner == nullptr)
		{
			return;
		}
	}

	if (!NoticeBanner->IsInViewport())
	{
		NoticeBanner->AddToViewport(NUUILayer::Notice);
	}
	NoticeBanner->ShowMessage(Message);
	// 장소 진입 소리는 장소 배너(UNUWorldLocationWidget::ShowBanner)가 낸다 --- 둘 다 켜져 있어도 한 번만
	if (NoticeType != ENUNoticeType::LocationEntry)
	{
		UNUSFXSubsystem::PlayCommon(PC, NU::SFX::UI::Notice);
	}
}

void UNUUIManager::RegisterGameMenuEntry(
	FName EntryId,
	ENUGameMenuCategory Category,
	const FText& Label,
	FSimpleDelegate OnActivated,
	const UObject* Owner,
	int32 SortOrder,
	FName ShortcutMapping)
{
	if (Owner == nullptr || !OnActivated.IsBound())
	{
		return;
	}

	FNUGameMenuEntry Entry;
	Entry.EntryId = EntryId;
	Entry.Category = Category;
	Entry.Label = Label;
	Entry.ShortcutMapping = ShortcutMapping;
	Entry.OnActivated = MoveTemp(OnActivated);
	Entry.Owner = Owner;
	Entry.SortOrder = SortOrder;
	GameMenuEntries.Add(MoveTemp(Entry));
}

void UNUUIManager::RefreshGameMenu()
{
	if (GameMenu != nullptr)
	{
		GameMenu->RefreshEntries();
	}
}

void UNUUIManager::UnregisterGameMenuEntries(const UObject* Owner)
{
	GameMenuEntries.RemoveAll([Owner](const FNUGameMenuEntry& Entry)
	{
		return !Entry.Owner.IsValid() || Entry.Owner.Get() == Owner;
	});
}

TArray<FNUGameMenuEntry> UNUUIManager::GetGameMenuEntries() const
{
	TArray<FNUGameMenuEntry> Result;
	Result.Reserve(GameMenuEntries.Num());
	for (const FNUGameMenuEntry& Entry : GameMenuEntries)
	{
		if (Entry.Owner.IsValid() && Entry.OnActivated.IsBound())
		{
			Result.Add(Entry);
		}
	}

	// 카테고리(선언 순서) => SortOrder, 같으면 등록 순서 유지
	Result.StableSort([](const FNUGameMenuEntry& A, const FNUGameMenuEntry& B)
	{
		if (A.Category != B.Category)
		{
			return static_cast<uint8>(A.Category) < static_cast<uint8>(B.Category);
		}
		return A.SortOrder < B.SortOrder;
	});
	return Result;
}

void UNUUIManager::OpenGameMenu()
{
	APlayerController* PC = GetOwningPlayerController();
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	TSubclassOf<UNUGameMenuWidget> MenuClass = Settings ? Settings->GameMenuClass.LoadSynchronous() : nullptr;

	if (PC == nullptr || MenuClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[메뉴] GameMenuClass 미지정 --- 프로젝트 세팅 > 게임 > NU UI"));
		return;
	}

	// 레벨이 바뀌었으면 이전 월드의 메뉴는 버린다 (배너와 같은 처리)
	if (GameMenu != nullptr && GameMenu->GetWorld() != PC->GetWorld())
	{
		GameMenu->RemoveFromParent();
		GameMenu = nullptr;
	}
	if (GameMenu == nullptr || GameMenu->GetClass() != MenuClass)
	{
		GameMenu = CreateWidget<UNUGameMenuWidget>(PC, MenuClass);
		if (GameMenu == nullptr)
		{
			return;
		}
	}

	if (IsWindowOpen(GameMenu))
	{
		return;
	}

	// 열릴 때마다 레지스트리를 다시 읽는다 --- 항목 증감이 자동 반영
	GameMenu->RefreshEntries();

	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Modal;
	// The hub owns localized edge fades; keep the central game view unobscured.
	Params.bDimBackground = false;
	// The top-anchored profile occupies the field HUD's upper-right icon area.
	Params.bHideFieldHUD = true;
	Params.bCloseOnEscape = true;
	Params.bRequestCursor = true;
	Params.bBlockGameInput = true;
	Params.bCloseOnSessionEnd = true;
	Params.OpenSFX = NU::SFX::UI::WindowOpen;
	OpenWindow(GameMenu, Params);
}

void UNUUIManager::CloseGameMenu()
{
	if (GameMenu != nullptr)
	{
		CloseWindow(GameMenu);
	}
}

bool UNUUIManager::IsGameMenuOpen() const
{
	return GameMenu != nullptr && IsWindowOpen(GameMenu);
}

int32 UNUUIManager::FindEntry(const UUserWidget* Widget) const
{
	if (Widget == nullptr)
	{
		return INDEX_NONE;
	}
	return Windows.IndexOfByPredicate([Widget](const FNUUIWindowEntry& Entry)
	{
		return Entry.Widget.Get() == Widget;
	});
}

void UNUUIManager::SweepStaleWindows()
{
	// 매니저를 안 거치고 닫힌 창(파괴·직접 제거·상주 창의 가시성 끄기)을 등록에서 걷어낸다
	TArray<FNUUIWindowEntry> RemovedEntries;
	for (int32 i = Windows.Num() - 1; i >= 0; --i)
	{
		if (!IsEntryShown(Windows[i]))
		{
			RemovedEntries.Add(Windows[i]);
			Windows.RemoveAt(i);
		}
	}
	// Remove first: releasing HUD suppression / returning focus can synchronously open/close windows through listeners.
	for (const FNUUIWindowEntry& Entry : RemovedEntries)
	{
		SetPopupButtonHover(Entry.Widget.Get(), false);
		// 가시성을 꺼서 닫은 상주 창(퀘스트 수첩 등)은 CloseWindow 를 지나지 않아 닫는 소리를 여기서 낸다
		if (Entry.Params.bVisibilityDriven && Entry.Widget.IsValid() && !Entry.Params.bCompanion
			&& Entry.Params.Layer >= NUUILayer::Window && Entry.Params.Layer < NUUILayer::Dialogue)
		{
			UNUSFXSubsystem::PlayCommon(Entry.Widget.Get(), Entry.Params.CloseSFX.IsValid() ? Entry.Params.CloseSFX : FGameplayTag(NU::SFX::UI::WindowClose));
		}
		const bool bHadFocus = Entry.Widget.IsValid() && Entry.Widget->HasFocusedDescendants();
		// 커서/입력차단 해제는 사유(FName) 기준이라 위젯이 이미 죽었어도 동작한다
		ReleaseEntryRequests(Entry);
		// 가시성만 꺼진 상주 창은 뷰포트에 남아 있어 숨은 버튼에 포커스가 남는다 --- 그 창에 있던 포커스만 돌려준다
		if (bHadFocus)
		{
			ReturnFocusAfterClose(Entry, true);
		}
	}
	if (RemovedEntries.Num() > 0)
	{
		// 창 목록을 보는 쪽(패드 이동 막힘 등)이 닫힘을 알게 한다
		RefreshAnchorTick();
		UpdateDim();
		OnWindowsChanged.Broadcast();
	}
}

void UNUUIManager::HandleResidentVisibilityChanged(ESlateVisibility InVisibility)
{
	if (InVisibility == ESlateVisibility::Collapsed || InVisibility == ESlateVisibility::Hidden)
	{
		SweepStaleWindows();
	}
}

void UNUUIManager::ReturnFocusAfterClose(const FNUUIWindowEntry& Entry, bool bHadFocus)
{
	// 패드 모드면 이 창을 열기 전에 포커스가 있던 조작부로 돌린다 (수량·확인 창을 닫으면 원래 칸으로). 그 창이 사라졌으면 다음 틱 매니저가 맨 위 창 첫 버튼에 준다
	const ANUPlayerController* PC = GetNUPlayerController();
	UWidget* Previous = Entry.PreviousFocus.Get();
	if (PC != nullptr && PC->IsGamepadMode() && Previous != nullptr && Previous->GetCachedWidget().IsValid() && Previous->GetIsEnabled() && Previous->IsVisible())
	{
		Previous->SetFocus();
	}
	else if (bHadFocus && FSlateApplication::IsInitialized())
	{
		// 돌려줄 곳이 없으면 게임으로 --- 상주 창은 뷰포트에 남아 있어서, 숨긴 버튼에 포커스가 남으면
		// 포커스 테두리가 그 빈 자리에 남고 Enter·Space가 안 보이는 버튼을 누른다
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void UNUUIManager::HandleSlatePreTick(float DeltaTime)
{
	const APlayerController* PC = GetNUPlayerController();
	UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get();
	if (PC == nullptr || Viewport == nullptr)
	{
		return;
	}
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(PC), KINDA_SMALL_NUMBER);
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(PC) / Scale;

	// 대상이 사라졌거나 화면 밖으로 나간 창은 그 자리에서 닫으면 목록이 바뀌므로 모아 두고 아래에서 처리한다
	TArray<UUserWidget*, TInlineAllocator<2>> Lost;
	for (FNUUIWindowEntry& Entry : Windows)
	{
		if (Entry.Params.AnchorActor.IsExplicitlyNull() || !IsEntryShown(Entry))
		{
			continue;
		}
		const AActor* Anchor = Entry.Params.AnchorActor.Get();
		if (Anchor == nullptr)
		{
			Lost.Add(Entry.Widget.Get());
			continue;
		}
		// 화면 밖으로 나간 대상은 붙어 있을 자리가 없으니 창을 닫는다 (카메라 뒤도 투영이 실패해 같이 걸린다)
		FVector2D AnchorPos = FVector2D::ZeroVector;
		const bool bOnScreen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Anchor->GetActorLocation(), AnchorPos, true)
			&& AnchorPos.X >= 0.0 && AnchorPos.X <= ViewportSize.X
			&& AnchorPos.Y >= 0.0 && AnchorPos.Y <= ViewportSize.Y;
		if (!bOnScreen)
		{
			Lost.Add(Entry.Widget.Get());
			continue;
		}
		// 창이 화면 밖으로 밀려나면 자리를 화면 안으로 당기지 않고 붙는 쪽을 반대로 넘긴다.
		// 당기면 대상은 계속 가장자리로 가는데 창만 멈춰 서로 떨어진다 --- 넘기면 대상이 화면을 벗어날 때까지 붙어 있다
		const FGameViewportWidgetSlot Slot = Viewport->GetWidgetSlot(Entry.Widget.Get());
		const FVector2D Size = Entry.Widget->GetDesiredSize();
		const FVector2D Base = AnchorPos + Entry.AnchorOffset; // 창이 붙는 지점 (Alignment가 가리키는 자리)
		const FVector2D Alignment(
			PickAnchorSide(Base.X, Size.X, ViewportSize.X, Slot.Alignment.X),
			PickAnchorSide(Base.Y, Size.Y, ViewportSize.Y, Slot.Alignment.Y));
		// 어느 쪽으로도 안 들어가는 좁은 화면에서만 화면 안으로 당긴다
		const FVector2D Wanted = Base - Alignment * Size;
		const FVector2D TopLeft(
			FMath::Clamp(Wanted.X, 0.0, FMath::Max(0.0, ViewportSize.X - Size.X)),
			FMath::Clamp(Wanted.Y, 0.0, FMath::Max(0.0, ViewportSize.Y - Size.Y)));
		const FVector2D Position = TopLeft + Alignment * Size;
		if (!Slot.Alignment.Equals(Alignment))
		{
			Entry.Widget->SetAlignmentInViewport(Alignment);
		}
		// 자리가 그대로면 레이아웃을 건드리지 않는다 (가만히 서 있을 때)
		if (!FVector2D(Slot.Offsets.Left, Slot.Offsets.Top).Equals(Position, 0.5))
		{
			Entry.Widget->SetPositionInViewport(Position, false); // DPI를 이미 뺀 위젯 좌표
		}
	}

	for (UUserWidget* Widget : Lost)
	{
		RequestCloseWindow(Widget);
	}
	RefreshAnchorTick();
}

void UNUUIManager::RequestCloseWindow(UUserWidget* Widget)
{
	const int32 Index = FindEntry(Widget);
	if (Index == INDEX_NONE)
	{
		return;
	}
	FSimpleDelegate Pending = Windows[Index].OnCloseRequested; // 실행 중 목록이 바뀔 수 있어 복사본 호출
	if (Pending.IsBound())
	{
		Pending.Execute();
	}
	else
	{
		CloseWindow(Widget);
	}
}

void UNUUIManager::RefreshAnchorTick()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	const bool bWant = Windows.ContainsByPredicate([](const FNUUIWindowEntry& Entry)
	{
		return !Entry.Params.AnchorActor.IsExplicitlyNull() && IsEntryShown(Entry);
	});
	if (bWant == AnchorTickHandle.IsValid())
	{
		return;
	}
	if (bWant)
	{
		AnchorTickHandle = FSlateApplication::Get().OnPreTick().AddUObject(this, &UNUUIManager::HandleSlatePreTick);
		return;
	}
	FSlateApplication::Get().OnPreTick().Remove(AnchorTickHandle);
	AnchorTickHandle.Reset();
}

void UNUUIManager::ReleaseEntryRequests(const FNUUIWindowEntry& Entry)
{
	if (ANUPlayerController* PC = GetNUPlayerController())
	{
		if (Entry.Params.bRequestCursor)
		{
			PC->RequestCursor(Entry.Reason, false, Entry.Widget.Get());
		}
		if (Entry.Params.bBlockGameInput)
		{
			PC->RequestGameInputBlock(Entry.Reason, false, Entry.Widget.Get());
		}
	}
	// HUD listeners may open another window; finish the old input requests before notifying them.
	if (Entry.Params.bHideFieldHUD)
	{
		RequestFieldHUDHidden(Entry.Reason, false, Entry.Widget.Get());
	}
}

void UNUUIManager::UpdateDim()
{
	int32 TopDimLayer = INDEX_NONE;
	for (const FNUUIWindowEntry& Entry : Windows)
	{
		if (Entry.Params.bDimBackground && IsEntryShown(Entry))
		{
			TopDimLayer = FMath::Max(TopDimLayer, Entry.Params.Layer);
		}
	}

	APlayerController* PC = GetOwningPlayerController();

	// 레벨이 바뀌었으면 이전 월드의 딤은 버린다
	if (DimWidget != nullptr && (PC == nullptr || DimWidget->GetWorld() != PC->GetWorld()))
	{
		DimWidget->RemoveFromParent();
		DimWidget = nullptr;
		DimZOrder = INDEX_NONE;
	}

	if (TopDimLayer == INDEX_NONE)
	{
		if (DimWidget != nullptr && DimWidget->IsInViewport())
		{
			DimWidget->RemoveFromParent();
		}
		return;
	}

	if (PC == nullptr)
	{
		return;
	}

	if (DimWidget == nullptr)
	{
		DimWidget = CreateWidget<UNUDimLayerWidget>(PC, UNUDimLayerWidget::StaticClass());
		if (DimWidget == nullptr)
		{
			return;
		}
		DimWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// 딤 창 바로 아래 — 그 뒤(HUD 등)만 어둡게
	const int32 DesiredZ = TopDimLayer - 1;
	if (!DimWidget->IsInViewport() || DimZOrder != DesiredZ)
	{
		DimWidget->RemoveFromParent(); // ZOrder는 추가 시점에만 정해져 다시 올린다
		DimWidget->AddToViewport(DesiredZ);
		DimZOrder = DesiredZ;
	}
}

/////////////////////////////////////////////////////////////////
// UI 컨트롤러 레지스트리
/////////////////////////////////////////////////////////////////

UNUUIController* UNUUIManager::FindOrCreateController(TSubclassOf<UNUUIController> ControllerClass)
{
	if (ControllerClass == nullptr)
	{
		return nullptr;
	}

	const int32 Index = Controllers.IndexOfByPredicate([ControllerClass](const TObjectPtr<UNUUIController>& C)
	{
		return C != nullptr && C->GetClass() == ControllerClass;
	});
	if (Index != INDEX_NONE)
	{
		return Controllers[Index];
	}

	UNUUIController* Controller = NewObject<UNUUIController>(this, ControllerClass);
	if (Controller == nullptr)
	{
		return nullptr;
	}
	Controllers.Add(Controller);
	Controller->InitializeController(this); // 등록 후 초기화 — 초기화 중 GetController 재진입에 안전
	return Controller;
}

void UNUUIManager::DestroyController(TSubclassOf<UNUUIController> ControllerClass)
{
	if (ControllerClass == nullptr)
	{
		return;
	}

	for (int32 i = Controllers.Num() - 1; i >= 0; --i)
	{
		if (Controllers[i] == nullptr || Controllers[i]->GetClass() != ControllerClass)
		{
			continue;
		}
		// 목록에서 먼저 빼고 정리 — 정리 중 CloseWindow 재진입에 안전
		UNUUIController* Controller = Controllers[i];
		Controllers.RemoveAt(i);
		Controller->DeinitializeController();
	}
}

namespace
{
	// 게임 화면의 버튼 위에 마우스가 올라가거나 버튼을 누르면 소리를 낸다.
	// 버튼 스타일(WBP)에 소리를 넣는 길은 쓰지 않는다: 창이 백여 개로 흩어져 있고, 공용 버튼은 뜰 때 스타일을 새로 만들어 넣은 소리가 지워진다.
	// 입력은 삼키지 않고 지켜보기만 한다
	class FNUButtonSounds : public IInputProcessor
	{
	public:
		explicit FNUButtonSounds(ULocalPlayer* InPlayer) : Player(InPlayer) {}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
		{
			// 패드 모드에서는 커서를 숨길 뿐 Slate 가 그 자리를 계속 짚으므로, 보이지 않는 커서의 호버는 소리를 내지 않는다
			const ANUPlayerController* PC = Player.IsValid() ? Cast<ANUPlayerController>(Player->PlayerController) : nullptr;
			const TSharedPtr<SWidget> Button = PC != nullptr && PC->IsGamepadMode() ? nullptr : FindButtonUnderCursor(SlateApp);
			if (Button != Hovered.Pin())
			{
				Hovered = Button;
				if (Button.IsValid())
				{
					Play(NU::SFX::UI::Hover);
				}
			}
		}

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				Pressed = FindButtonAt(SlateApp, MouseEvent);
			}
			return false;
		}

		// 빠르게 두 번 누르면 두 번째 누름은 이 이벤트로 온다 (버튼은 보통 누름으로 받는다)
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			return HandleMouseButtonDownEvent(SlateApp, MouseEvent);
		}

		virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				// 누른 버튼 위에서 뗐을 때만 눌린 것이다 (밖으로 끌고 나가 떼면 취소)
				const TSharedPtr<SWidget> Button = Pressed.Pin();
				Pressed.Reset();
				if (Button.IsValid() && Button == FindButtonAt(SlateApp, MouseEvent))
				{
					PlayClick();
				}
			}
			return false;
		}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
		{
			KeyPressed = FindFocusedButton(SlateApp, KeyEvent);
			return false;
		}

		virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
		{
			// 패드 A·Enter·Space 로 포커스가 선 버튼을 누를 때 --- 버튼은 누른 키를 그 버튼 위에서 뗄 때 눌린다
			const TSharedPtr<SWidget> Button = KeyPressed.Pin();
			KeyPressed.Reset();
			if (Button.IsValid() && Button == FindFocusedButton(SlateApp, KeyEvent))
			{
				PlayClick();
			}
			return false;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("NUButtonSounds"); }

	private:
		static TSharedPtr<SWidget> GetViewport()
		{
			return GEngine != nullptr && GEngine->GameViewport != nullptr ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
		}

		// 누르는 키(패드 A·Enter·Space)일 때 게임 화면 안에서 포커스가 선 버튼
		static TSharedPtr<SWidget> FindFocusedButton(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent)
		{
			const FKey Key = KeyEvent.GetKey();
			if (Key != EKeys::Virtual_Gamepad_Accept.GetVirtualKey() && Key != EKeys::Enter && Key != EKeys::SpaceBar)
			{
				return nullptr;
			}

			const TSharedPtr<SWidget> Focused = SlateApp.GetUserFocusedWidget(KeyEvent.GetUserIndex());
			const TSharedPtr<SWidget> Viewport = GetViewport();
			return Focused.IsValid() && Viewport.IsValid() && Focused->GetType() == ButtonType && Focused->IsEnabled()
				&& SlateApp.HasFocusedDescendants(Viewport.ToSharedRef()) ? Focused : nullptr;
		}

		// 커서 아래 위젯들 가운데 게임 화면 안의 가장 안쪽 버튼 (PIE 에서 에디터 버튼은 세지 않는다, 꺼진 버튼도 뺀다)
		static TSharedPtr<SWidget> FindButtonUnderCursor(FSlateApplication& SlateApp)
		{
			const TSharedPtr<FSlateUser> User = SlateApp.GetCursorUser();
			const TSharedPtr<SWidget> Viewport = GetViewport();
			if (!User.IsValid() || !Viewport.IsValid())
			{
				return nullptr;
			}

			TSharedPtr<SWidget> Button;
			bool bInViewport = false;
			for (const TWeakPtr<SWidget>& Weak : User->GetLastWidgetsUnderCursor().Widgets)
			{
				const TSharedPtr<SWidget> Widget = Weak.Pin();
				if (!Widget.IsValid())
				{
					return nullptr;
				}

				PickButton(Widget, Viewport, bInViewport, Button);
			}
			return Button;
		}

		// 누른 자리의 버튼 --- 커서 기록이 아니라 그 순간 화면을 다시 본다 (화면이 바뀐 뒤 커서를 안 움직이고 누를 수 있다)
		static TSharedPtr<SWidget> FindButtonAt(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
		{
			const TSharedPtr<SWidget> Viewport = GetViewport();
			TSharedPtr<SWidget> Button;
			if (Viewport.IsValid())
			{
				bool bInViewport = false;
				const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(MouseEvent.GetScreenSpacePosition(), SlateApp.GetInteractiveTopLevelWindows());
				for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
				{
					PickButton(Path.Widgets[Index].Widget, Viewport, bInViewport, Button);
				}
			}
			return Button;
		}

		// 바깥 => 안쪽 순서로 위젯을 넘기면 게임 화면 안의 가장 안쪽 버튼이 남는다 (꺼진 버튼이면 비운다)
		static void PickButton(const TSharedPtr<SWidget>& Widget, const TSharedPtr<SWidget>& Viewport, bool& bInViewport, TSharedPtr<SWidget>& Button)
		{
			if (Widget == Viewport)
			{
				bInViewport = true;
			}
			else if (bInViewport && Widget->GetType() == ButtonType)
			{
				Button = Widget->IsEnabled() ? Widget : nullptr;
			}
		}

		void Play(const FGameplayTag& Event) const
		{
			if (Player.IsValid())
			{
				UNUSFXSubsystem::Play(Player->PlayerController, Event);
			}
		}

		// 버튼이 제 소리(탭·확인·창 열림)를 내면 클릭음은 나지 않는다
		void PlayClick() const
		{
			if (Player.IsValid())
			{
				UNUSFXSubsystem::PlayCommon(Player->PlayerController, NU::SFX::UI::Click);
			}
		}

		inline static const FName ButtonType = TEXT("SButton");

		TWeakObjectPtr<ULocalPlayer> Player;
		TWeakPtr<SWidget> Hovered;
		TWeakPtr<SWidget> Pressed;
		TWeakPtr<SWidget> KeyPressed;
	};
}

void UNUUIManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (FSlateApplication::IsInitialized())
	{
		ButtonSounds = MakeShared<FNUButtonSounds>(GetLocalPlayer<ULocalPlayer>());
		FSlateApplication::Get().RegisterInputPreProcessor(ButtonSounds);
	}

	// 볼륨은 오디오 디바이스마다 걸어야 한다 --- 엔진은 시작 때 한 번 적용하지만 PIE는 디바이스가 그 뒤에 생기므로
	// 로컬 플레이어가 서는 여기서 저장값을 한 번 더 건다 (옵션창을 열지 않아도 저장한 볼륨으로 들린다)
	UNUGameUserSettings::Get()->ApplyAudioVolumes();

	// 서버 세션은 GameInstance 수명이라 매니저(LocalPlayer 수명)보다 오래 산다 — 구독은 짧은 쪽이 한다
	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->OnStateChanged.AddDynamic(this, &UNUUIManager::HandleServerStateChanged);
		bSessionWasLive = Server->IsLoggedIn();
	}
}

void UNUUIManager::Deinitialize()
{
	if (ButtonSounds.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(ButtonSounds);
	}
	ButtonSounds.Reset();

	StopPopupButtonHover();
	StopFieldHUDMotion();
	if (AnchorTickHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().Remove(AnchorTickHandle);
		AnchorTickHandle.Reset();
	}

	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->OnStateChanged.RemoveDynamic(this, &UNUUIManager::HandleServerStateChanged);
	}

	for (int32 i = Controllers.Num() - 1; i >= 0; --i)
	{
		if (Controllers[i] != nullptr)
		{
			Controllers[i]->DeinitializeController();
		}
	}
	Controllers.Reset();
	FTSTicker::RemoveTicker(FieldHUDRequestTickHandle);
	FieldHUDRequestTickHandle.Reset();
	FieldHUDVisibility.ClearRequests();
	StopFieldHUDMotion();
	FieldHUDMotion.Reset();

	Super::Deinitialize();
}

UEmberServerManager* UNUUIManager::GetServerManager() const
{
	const ULocalPlayer* LP = GetLocalPlayer<ULocalPlayer>();
	UGameInstance* GI = LP ? LP->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}

void UNUUIManager::HandleServerStateChanged(EEmberConnState NewState)
{
	if (NewState == EEmberConnState::LoggedIn)
	{
		bSessionWasLive = true;
		if (bReconnecting)
		{
			// 재접속 성공. 새 세션은 캐릭터가 안 골라진 상태라 있던 캐릭터로 다시 들어간다 ---
			// 인벤·퀘스트도 캐릭터가 정해진 뒤라야 요청이 통하므로 그쪽은 레벨을 다시 열며 초기화한다
			bReconnecting = false;
			const ULocalPlayer* LP = GetLocalPlayer<ULocalPlayer>();
			UGameInstance* GI = LP != nullptr ? LP->GetGameInstance() : nullptr;
			if (UNUCharacterManager* Characters = GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr)
			{
				Characters->RestoreSessionAfterReconnect();
			}
		}
		return;
	}

	// 재접속 시도 중이면 중간 상태(Connecting 등)는 끊김이 아니다 --- 실패했을 때만 다시 물어본다
	if (bReconnecting)
	{
		if (NewState == EEmberConnState::Failed)
		{
			bReconnecting = false;
			ShowSessionEndedDialog(); // 포기는 사용자가 정한다
		}
		return;
	}

	// 살아 있던 세션이 끊긴 순간만 처리한다 (로그인 전의 상태 변화는 무시)
	if (bSessionWasLive)
	{
		bSessionWasLive = false;
		HandleSessionEnded();
	}
}

void UNUUIManager::HandleSessionEnded()
{
	FieldHUDVisibility.ClearRequests();
	RefreshFieldHUDVisibility(false);
	// 서버 데이터를 그리던 창을 전부 닫는다. 소유 코드의 닫기 연쇄를 타야 하므로 델리게이트 우선
	SweepStaleWindows();
	for (int32 i = Windows.Num() - 1; i >= 0; --i)
	{
		if (!Windows.IsValidIndex(i) || !Windows[i].Params.bCloseOnSessionEnd)
		{
			continue;
		}

		FSimpleDelegate Pending = Windows[i].OnCloseRequested;
		UUserWidget* Widget = Windows[i].Widget.Get();
		if (Pending.IsBound())
		{
			Pending.ExecuteIfBound(); // 실행 중 Windows가 바뀔 수 있어 복사본 호출
		}
		else
		{
			CloseWindow(Widget);
		}
	}

	for (UNUUIController* Controller : Controllers)
	{
		if (Controller != nullptr)
		{
			Controller->OnServerSessionEnded();
		}
	}

	ShowSessionEndedDialog();
}

void UNUUIManager::ShowSessionEndedDialog()
{
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	TSubclassOf<UNUConfirmDialog> DialogClass = Settings ? Settings->SessionEndedDialogClass.LoadSynchronous() : nullptr;
	if (DialogClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("서버 연결이 끊어졌습니다 (안내 창 미지정 — 프로젝트 세팅 > 게임 > NU UI)"));
		return;
	}

	// 갈 곳이 정해져 있을 때만 [초기화면으로]를 단다 --- 눌러도 아무 일 없는 버튼은 만들지 않는다
	const bool bHasLoginLevel = Settings != nullptr && !Settings->LoginLevel.IsNull();

	UNUConfirmDialog::Show(
		GetOwningPlayerController(),
		DialogClass,
		NSLOCTEXT("NUUI", "SessionEnded", "서버와의 연결이 끊어졌습니다."),
		NSLOCTEXT("NUUI", "SessionEndedRetry", "재접속"),
		FSimpleDelegate::CreateWeakLambda(this, [this]()
			{
				// 플래그를 먼저 세운다 --- Connect가 그 자리에서 실패하면 상태 변화가
				// TryReconnect가 돌아오기 전에 날아온다
				bReconnecting = true;
				UEmberServerManager* Server = GetServerManager();
				if (Server == nullptr || !Server->TryReconnect())
				{
					bReconnecting = false;
					ShowSessionEndedDialog(); // 시도조차 못 했다 --- 창을 도로 세운다
				}
			}),
		bHasLoginLevel
			? FSimpleDelegate::CreateWeakLambda(this, [this]() { GoToLoginLevel(); })
			: FSimpleDelegate(),
		bHasLoginLevel ? NSLOCTEXT("NUUI", "SessionEndedToTitle", "초기화면으로") : FText(),
		ENUConfirmIntent::Positive); // 재접속은 되돌리는 동작이지 파괴가 아니다
}

void UNUUIManager::GoToLoginLevel()
{
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	if (Settings == nullptr || Settings->LoginLevel.IsNull())
	{
		return;
	}

	// 남은 연결을 끊고 나간다 --- 안 끊으면 로그인 화면에서 이전 세션이 살아 있는 것처럼 보인다
	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->Disconnect();
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Settings->LoginLevel);
}

void UNUUIManager::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	StopPopupButtonHover();
	Super::PlayerControllerChanged(NewPlayerController);
	FieldHUDVisibility.ClearRequests();
	RefreshFieldHUDVisibility(false);
	// A new field starts in the chosen state, without replaying the previous field's last frames.
	StopFieldHUDMotion();

	// 레벨 전환 경계 — 이전 레벨의 창 등록을 걷어내고 컨트롤러에 재바인딩 기회를 준다
	SweepStaleWindows();

	for (UNUUIController* Controller : Controllers)
	{
		if (Controller != nullptr)
		{
			Controller->OnPlayerControllerChanged(NewPlayerController);
		}
	}
}

ANUPlayerController* UNUUIManager::GetNUPlayerController() const
{
	return Cast<ANUPlayerController>(GetOwningPlayerController());
}

APlayerController* UNUUIManager::GetOwningPlayerController() const
{
	const ULocalPlayer* LP = GetLocalPlayer<ULocalPlayer>();
	return LP ? LP->GetPlayerController(LP->GetWorld()) : nullptr;
}
