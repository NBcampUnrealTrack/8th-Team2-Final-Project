// Fill out your copyright notice in the Description page of Project Settings.


#include "NUChatWidget.h"

#include "Chat/NUChatSubsystem.h"
#include "Chat/UI/NUChatChannelButtonWidget.h"
#include "Chat/UI/NUChatLineWidget.h"
#include "Chat/UI/NUChatTabButtonWidget.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/Common/NU_CloseButtonWidget.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"

namespace
{
	const FVector2D ChatChannelIconSize(24.f, 24.f);

	UTexture2D* GetSingleChannelIconFromMask(int32 ChannelMask)
	{
		if (ChannelMask == NUChat::AllChannels)
		{
			return NUChat::GetGeneralIcon();
		}

		for (ENUChatChannel Channel : NUChat::GetSendableChannels())
		{
			if (ChannelMask == NUChat::ChannelBit(Channel))
			{
				return NUChat::GetChannelIcon(Channel);
			}
		}

		if (ChannelMask == NUChat::ChannelBit(ENUChatChannel::System))
		{
			return NUChat::GetChannelIcon(ENUChatChannel::System);
		}

		return nullptr;
	}
}

// TabPrevButton/TabNextButton/SendChannelButton 전용 --- 순정 Button(레거시), UNU_CloseButtonWidget,
// UNU_CommonButtonWidget(WBP_CommonButton), UNUChatTabButtonWidget(WBP_ChatTabButton, 탭 버튼을 이 자리에
// 재사용할 때) 넷 다 지원한다. AddDynamic/RemoveDynamic은 매크로라 컴파일타임 함수 리터럴이 필요해서
// 함수로 감쌀 수 없으므로 여기서만 매크로로 반복한다.
// UNUChatTabButtonWidget::OnTabClicked은 UFUNCTION 기반 동적 델리게이트가 아니라 TDelegate(싱글캐스트)라
// AddDynamic이 안 통한다 --- Bind/Unbind로 대신하고, 탭 인덱스 인자는 이 자리에서 의미가 없어 람다로 버린다.
#define NU_BIND_NAMED_BUTTON(Widget, Handler) \
	do \
	{ \
		if (UButton* NativeButton_##Handler = Cast<UButton>(Widget)) \
		{ \
			NativeButton_##Handler->OnClicked.RemoveDynamic(this, &ThisClass::Handler); \
			NativeButton_##Handler->OnClicked.AddDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNU_CloseButtonWidget* CloseStyleButton_##Handler = Cast<UNU_CloseButtonWidget>(Widget)) \
		{ \
			CloseStyleButton_##Handler->OnCloseClicked.RemoveDynamic(this, &ThisClass::Handler); \
			CloseStyleButton_##Handler->OnCloseClicked.AddDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNU_CommonButtonWidget* CommonButton_##Handler = Cast<UNU_CommonButtonWidget>(Widget)) \
		{ \
			CommonButton_##Handler->OnCommonClicked.RemoveDynamic(this, &ThisClass::Handler); \
			CommonButton_##Handler->OnCommonClicked.AddDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNUChatTabButtonWidget* TabStyleButton_##Handler = Cast<UNUChatTabButtonWidget>(Widget)) \
		{ \
			TabStyleButton_##Handler->OnTabClicked.BindLambda([this](int32) { Handler(); }); \
		} \
	} while (0)

#define NU_UNBIND_NAMED_BUTTON(Widget, Handler) \
	do \
	{ \
		if (UButton* NativeButton_##Handler = Cast<UButton>(Widget)) \
		{ \
			NativeButton_##Handler->OnClicked.RemoveDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNU_CloseButtonWidget* CloseStyleButton_##Handler = Cast<UNU_CloseButtonWidget>(Widget)) \
		{ \
			CloseStyleButton_##Handler->OnCloseClicked.RemoveDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNU_CommonButtonWidget* CommonButton_##Handler = Cast<UNU_CommonButtonWidget>(Widget)) \
		{ \
			CommonButton_##Handler->OnCommonClicked.RemoveDynamic(this, &ThisClass::Handler); \
		} \
		else if (UNUChatTabButtonWidget* TabStyleButton_##Handler = Cast<UNUChatTabButtonWidget>(Widget)) \
		{ \
			TabStyleButton_##Handler->OnTabClicked.Unbind(); \
		} \
	} while (0)

namespace
{
	constexpr int32 MAX_LINES = 200;

	// 맨 아래에 있는지 볼 때 두는 여유 픽셀
	// 줄마다 높이가 달라지면 끝 위치가 딱 떨어지지 않아 정확히 같은지 보면 안 됨
	constexpr float SCROLL_BOTTOM_TOLERANCE = 8.f;
}

void UNUChatWidget::AddMessage(ENUChatChannel Channel, const FText& SenderName, const FText& Text, bool bIsMine)
{
	if (!IsValid(ChatScrollBox)) return;
	
	// 채팅창 스크롤이 아래 있을때만 자동스크롤링 되도록 미리 아래 있는지 판정
	bool bWasAtBottom = IsScrolledToBottom();
	
	FNUChatLine& Line = Lines.AddDefaulted_GetRef();
	Line.Channel = Channel;
	Line.SenderName = SenderName;
	Line.Text = Text;
	Line.bIsMine = bIsMine;
	
	// 현재 탭에 맞지 않는 메세지면 쌓기만하고 위젯에 추가하진 않음
	if (CheckFilter(Channel))
	{
		AppendLineWidget(Line);
		if (bWasAtBottom)
		{
			ScrollChatToEnd();
		}
	}
	
	TrimChatLog();
}

void UNUChatWidget::AddSystemMessage(const FText& Text)
{
	AddMessage(ENUChatChannel::System, FText::GetEmpty(), Text, false);
}

void UNUChatWidget::BeginFocus()
{
	if (bFocusing || !IsValid(InputBox)) return;

	// Explicit chat input must never focus an invisible editable text box.
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		// Chat may restore the user's preference, but cannot override another feature's suppression.
		if (UI->HasFieldHUDHideRequests())
		{
			return;
		}
		UI->SetFieldHUDVisible(true);
		if (!UI->IsFieldHUDVisible())
		{
			return;
		}
	}
	
	bFocusing = true;
	ApplyFocusLook(true);        // 접혀 있던 입력창을 먼저 펼친다 (보이지 않는 입력창에는 포커스를 줄 수 없다)
	InputBox->SetText(FText::GetEmpty());
	InputBox->SetKeyboardFocus();

	// 귓속말 채널로 열렸으면 대상을 다시 채워줌
	RefreshWhisperCommand();

	OnFocusChanged.ExecuteIfBound(true);   // PlayerController가 IMC_Field 제거

	// 바로 위에서 PlayerController 가 커서 모드(FInputModeGameAndUI)로 바꾼다.
	// 그 모드가 SetWidgetToFocus 없이 걸려서 엔진이 포커스를 게임 뷰포트로 가져가는데,
	// 그 이동은 이 프레임 끝에 처리되는 예약이라 위의 SetKeyboardFocus 를 나중에 덮어쓴다.
	// 그래서 한 틱 뒤에 입력창으로 다시 끌어온다 - 안 하면 창은 열렸는데 포커스가 뷰포트에 있어서
	// 창 안의 버튼을 누를 때마다 포커스가 다시 기어 올라오느라 클릭이 씹힌다
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FocusInputBoxDeferred);
	}
	OnFocusStateChanged(true);   // WBP가 불투명도를 올리면 됨
}

void UNUChatWidget::EndFocus()
{
	if (!bFocusing || !IsValid(InputBox)) return;
	
	bFocusing = false;
	InputBox->SetText(FText::GetEmpty());
	SetSendChannelMenuOpen(false);   // 채팅 닫으면 채널 목록도 접어야 함

	SetCustomTabPanelOpen(false);
	bModalOpen = false;

	OnFocusChanged.ExecuteIfBound(false);
	ApplyFocusLook(false);
	OnFocusStateChanged(false);

	// 입력창이 쥔 키보드 포커스는 PlayerController 가 커서를 내리면서 가져가는데,
	// 전투처럼 커서를 원하는 것이 더 있으면 입력 모드를 다시 걸지 않아 포커스가 그대로 남음
	// 그러면 창은 닫혔는데 키가 전부 입력창으로 들어가 전투 입력이 돌아오지 않게 됨
	// 따라서 다음 틱에 포커스를 해제함
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::ReleaseInputFocusDeferred);
	}
}

bool UNUChatWidget::IsFocusing() const
{
	return bFocusing;
}

void UNUChatWidget::SelectTab(int32 TabIndex)
{
	if (!Tabs.IsValidIndex(TabIndex) || CurrentTab == TabIndex) return;

	CurrentTab = TabIndex;
	RebuildChat();
	RefreshTabHighlight();
	OnTabChanged(TabIndex);
}

void UNUChatWidget::AddCustomTab(const FText& Label, int32 ChannelMask)
{
	FNUChatTab& NewTab = Tabs.AddDefaulted_GetRef();
	NewTab.Label = Label;
	NewTab.ChannelMask = ChannelMask;

	// 새 탭은 맨 뒤에 붙으므로 창을 끝으로 보냄
	// 방금 만든 탭이 화면 밖이면 만들어졌는지 알 수 없으므로 맨 오른쪽으로 이동
	TabWindowStart = Tabs.Num() - MaxVisibleTabs;

	// 커스텀 탭이 추가되었으니 다시 상단 탭을 조정해야 함
	RebuildTabButtons();
}

void UNUChatWidget::RebuildTabButtons()
{
	if (!IsValid(ChatTabBox)) return;

	ChatTabBox->ClearChildren();
	TabButtons.Reset();

	if (!TabButtonClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] TabButtonClass 미지정 --- 채팅 상단 탭을 그릴 수 없음 (WBP_Chat 클래스 기본값에 WBP_ChatTabButton 을 넣으세요)"));
		return;
	}

	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		UNUChatTabButtonWidget* Button = CreateWidget<UNUChatTabButtonWidget>(this, TabButtonClass);
		if (!IsValid(Button))
		{
			continue;
		}

		Button->SetTab(Index, Tabs[Index].Label);
		Button->SetIconSize(ChatChannelIconSize);
		Button->SetIconTexture(GetSingleChannelIconFromMask(Tabs[Index].ChannelMask));
		Button->OnTabClicked.BindUObject(this, &ThisClass::HandleTabButtonPicked);

		if (UHorizontalBoxSlot* TabSlot = Cast<UHorizontalBoxSlot>(ChatTabBox->AddChild(Button)))
		{
			// Fill 이면 탭이 늘어도 가로로 균등하게 나뉘고, Automatic 이면 각 탭이 글자 폭만큼만 잡음
			TabSlot->SetSize(FSlateChildSize(bStretchTabsToFill
				? ESlateSizeRule::Fill
				: ESlateSizeRule::Automatic));
			TabSlot->SetPadding(TabSlotPadding);
		}

		TabButtons.Add(Button);
	}

	RefreshTabWindow();
	RefreshTabHighlight();
}

void UNUChatWidget::RefreshTabWindow()
{
	// 탭이 줄어 현재 시작점이 범위를 벗어나 있을 수 있음
	int32 MaxStart = FMath::Max(0, Tabs.Num() - MaxVisibleTabs);
	TabWindowStart = FMath::Clamp(TabWindowStart, 0, MaxStart);

	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		if (!IsValid(TabButtons[Index])) continue;

		// TabWindowStart ~ TabWindowStart + MaxVisibleTabs 구간
		bool bVisible = Index >= TabWindowStart
			&& Index < TabWindowStart + MaxVisibleTabs;

		// Collapsed는 HorizontalBox에서 자리 차지 X
		TabButtons[Index]->SetVisibility(bVisible
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	// 양 끝이면 그쪽 방향 버튼을 끔
	if (IsValid(TabPrevButton))
	{
		TabPrevButton->SetIsEnabled(TabWindowStart > 0);
	}
	if (IsValid(TabNextButton))
	{
		TabNextButton->SetIsEnabled(TabWindowStart < MaxStart);
	}
}

void UNUChatWidget::HandleTabPrevClicked()
{
	// 하한은 RefreshTabWindow에서 처리
	--TabWindowStart;
	RefreshTabWindow();
}

void UNUChatWidget::HandleTabNextClicked()
{
	// 상한은 RefreshTabWindow에서 처리
	++TabWindowStart;
	RefreshTabWindow();
}

void UNUChatWidget::HandleTabButtonPicked(int32 TabIndex)
{
	SelectTab(TabIndex);
}

void UNUChatWidget::HandleChannelButtonPicked(int32 ChannelValue)
{
	SetSendChannel(static_cast<ENUChatChannel>(ChannelValue));
}

void UNUChatWidget::RefreshTabHighlight()
{
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		UNUChatTabButtonWidget* Button = TabButtons[Index];
		if (!IsValid(Button)) continue;

		Button->SetSelected(Index == CurrentTab);
	}
}

void UNUChatWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	CacheTabSideButtonWidgets();
}

void UNUChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Tabs = NUChat::MakeDefaultTabs();

	ChatScrollBox = Cast<UScrollBox>(GetWidgetFromName(TEXT("ChatScrollBox")));
	InputBox = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("ChatInputBox")));

	if (!IsValid(ChatScrollBox) || !IsValid(InputBox))
	{
		UE_LOG(LogTemp, Error, TEXT("[NU] 채팅 WBP 수신, 입력창 위젯 누락! - ChatScrollBox / ChatInputBox"));
		return;
	}

	// 여기서 클래스 지정을 확인함
	if (!LineWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] LineWidgetClass 미지정 --- 채팅 로그를 그릴 수 없음 (WBP_Chat 클래스 기본값에 WBP_ChatLine 을 넣으세요)"));
	}

	InputBox->OnTextCommitted.AddDynamic(this, &ThisClass::HandleTextCommited);
	InputBox->OnTextChanged.AddDynamic(this, &ThisClass::HandleTextChanged);

	// 상단 탭 박스는 없어도 채팅이 동작함 return하지 않고 경고만
	// 없을 경우엔 일반탭만 사용함
	ChatTabBox = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("ChatTabBox")));
	if (IsValid(ChatTabBox))
	{
		RebuildTabButtons();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[NU] 채팅 WBP 탭 박스 누락! - ChatTabBox"));
	}

	// 탭 이동 화살표도 없어도 채팅은 동작함, 이 경우 앞의 MAX_VISIBLE_TABS개만 사용 가능함
	CacheTabSideButtonWidgets();

	if (IsValid(TabPrevButton) && IsValid(TabNextButton))
	{
		NU_BIND_NAMED_BUTTON(TabPrevButton, HandleTabPrevClicked);
		NU_BIND_NAMED_BUTTON(TabNextButton, HandleTabNextClicked);

		RefreshTabWindow();
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] 채팅 WBP 탭 이동 버튼 누락! - ChatTabPrevButton / ChatTabNextButton"));
	}

	SendChannelButton = GetWidgetFromName(TEXT("ChatSendChannelButton"));
	SendChannelText = Cast<UTextBlock>(GetWidgetFromName(TEXT("ChatSendChannelText")));
	SendChannelMenu = Cast<UVerticalBox>(GetWidgetFromName(TEXT("ChatSendChannelMenu")));

	// 전송 채널 쪽은 없어도 채팅 자체는 동작하므로 경고만
	// 없을 경우엔 Local 로만 대화할 수 있음
	if (!IsValid(SendChannelButton) || !IsValid(SendChannelText) || !IsValid(SendChannelMenu))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] 채팅 WBP 전송 채널 위젯 누락! - ChatSendChannelButton / ChatSendChannelText / ChatSendChannelMenu"));
	}

	if (IsValid(SendChannelButton))
	{
		NU_BIND_NAMED_BUTTON(SendChannelButton, HandleSendChannelButtonClicked);
	}

	SetSendChannelMenuOpen(false);
	RefreshSendChannelVisual();

	// 커스텀 탭 창이 WBP에 있으면 C++가 연다 (없으면 예전처럼 WBP 그래프가 + 버튼을 처리한다)
	if (GetWidgetFromName(TEXT("ChatCustomTabPanel")) != nullptr)
	{
		if (IsValid(TabAddButton))
		{
			NU_BIND_NAMED_BUTTON(TabAddButton, HandleCustomTabOpen);
		}
		UWidget* ApplyButton = GetWidgetFromName(TEXT("ChatCustomTabApply"));
		UWidget* CancelButton = GetWidgetFromName(TEXT("ChatCustomTabCancel"));
		NU_BIND_NAMED_BUTTON(ApplyButton, HandleCustomTabApply);
		NU_BIND_NAMED_BUTTON(CancelButton, HandleCustomTabCancel);
		SetCustomTabPanelOpen(false);
	}
	ApplyFocusLook(bFocusing);

	RebuildChat();
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnFieldHUDVisibilityChanged.RemoveAll(this);
		UI->OnFieldHUDVisibilityChanged.AddUObject(this, &ThisClass::ApplyHUDVisibility);
	}
	ApplyHUDVisibility();
}

void UNUChatWidget::ApplyHUDVisibility()
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	const bool bVisible = !UI || UI->IsFieldHUDVisible();
	if (!bVisible)
	{
		EndFocus();
		SetSendChannelMenuOpen(false);
	}
	if (UI)
	{
		UI->PresentFieldHUDWidget(this, ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UNUChatWidget::CacheTabSideButtonWidgets()
{
	TabPrevButton = GetWidgetFromName(TEXT("ChatTabPrevButton"));
	TabNextButton = GetWidgetFromName(TEXT("ChatTabNextButton"));
	TabAddButton = GetWidgetFromName(TEXT("ChatAddTabButton"));
}

void UNUChatWidget::NativeDestruct()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->OnFieldHUDVisibilityChanged.RemoveAll(this);
		UI->ForgetFieldHUDWidget(this);
	}
	if (IsValid(InputBox))
	{
		InputBox->OnTextCommitted.RemoveAll(this);
		InputBox->OnTextChanged.RemoveAll(this);
	}

	if (IsValid(SendChannelButton))
	{
		NU_UNBIND_NAMED_BUTTON(SendChannelButton, HandleSendChannelButtonClicked);
	}

	if (IsValid(TabPrevButton))
	{
		NU_UNBIND_NAMED_BUTTON(TabPrevButton, HandleTabPrevClicked);
	}
	if (IsValid(TabNextButton))
	{
		NU_UNBIND_NAMED_BUTTON(TabNextButton, HandleTabNextClicked);
	}

	Super::NativeDestruct();
}

// 마우스 클릭이 끝난 뒤에 입력창으로 포커스를 되돌림
// 그 사이에 창이 닫혔거나 입력창이 이미 포커스를 갖고 있으면 아무 것도 하지 않음
void UNUChatWidget::FocusInputBoxDeferred()
{
	if (!bFocusing || !IsValid(InputBox) || InputBox->HasKeyboardFocus())
	{
		return;
	}

	InputBox->SetKeyboardFocus();
}

void UNUChatWidget::ReleaseInputFocusDeferred()
{
	if (bFocusing || !FSlateApplication::IsInitialized())
	{
		return;
	}

	// 입력창의 포커스는 안쪽 SEditableText 가 쥐므로 InputBox 만 물으면 갈리지 않음
	// 채팅창 아래 어딘가에 남아 있는지로 봐야 함
	if (!HasFocusedDescendants())
	{
		return;
	}

	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

bool UNUChatWidget::NativeSupportsKeyboardFocus() const
{
	// 탭, 채널 버튼은 InitIsFocusable(false)라 포커스 대상 X
	// Slate가 채팅창을 지나쳐 게임 뷰포트에 포커스를 주는 문제가 있음
	
	// 따라서 창이 열려있는 것 자체가 포커스 대상이 되도록 함
	// 닫혀있을땐 예전처럼 뷰포트가 대상이 됨
	return bFocusing;
}

FReply UNUChatWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	// 위에서 멈춘 포커스를 입력창으로 넘김
	// 넘기지 않으면 창은 열려있는데 글자가 넘어가지 않음
	if (bFocusing && IsValid(InputBox))
	{
		// 마우스로 들어온 포커스는 이 자리에서 넘기면 안 된다
		// 창 안의 버튼은 InitIsFocusable(false) 라서, 누르면 Slate 가 포커스 가능한 조상을 찾아
		// 여기로 온다. 그 마우스다운을 처리하는 도중에 여기서 포커스를 또 옮기면 중첩 포커스
		// 변경이 되어 눌림이 씹힌다.
		if (InFocusEvent.GetCause() == EFocusCause::Mouse)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FocusInputBoxDeferred);
			}
			return FReply::Unhandled();
		}

		if (TSharedPtr<SWidget> Target = InputBox->GetCachedWidget())
		{
			return FReply::Handled().SetUserFocus(
				Target.ToSharedRef(), 
				InFocusEvent.GetCause()
				);
		}
	}
	
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

FReply UNUChatWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 입력 중에만 채팅이 키를 가로채도록
	if (bFocusing)
	{
		FKey Key = InKeyEvent.GetKey();

		// Tab키 - 전송채널 변경
		if (Key == EKeys::Tab)
		{
			// Handled를 반환해야 Slate의 포커스 이동이 일어나지 않음
			CycleSendChannel();
			return FReply::Handled();
		}

		// ESC키 - 채팅 종료
		if (Key == EKeys::Escape)
		{
			EndFocus();
			return FReply::Handled();
		}

	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UNUChatWidget::NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath,
	const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);

	if (!bFocusing) return;

	// 이 시점엔 Slate가 아직 포커스 기록을 갱신하지 않았으므로(SetFocusPath가 나중)
	// HasUserFocusedDescendants가 아니라 넘어온 새 경로를 직접 봐야 한다
	TSharedPtr<SWidget> Self = GetCachedWidget();
	if (!Self.IsValid()) return;

	// 닫기는 확인/취소 버튼과 Esc가 처리하도록
	// 창이 열려있으면 Focus가 튀어서 닫히는일이 없어야 함
	if (bModalOpen) return;

	// 채팅창 안쪽으로 옮겨간 것이면 계속 열어둔다 (탭 편집창의 이름 입력 등)
	if (NewWidgetPath.ContainsWidget(Self.Get()))
	{
		return;
	}

	EndFocus();
}

void UNUChatWidget::ApplyFocusLook(bool bFocus)
{
	if (WidgetTree == nullptr)
	{
		return;
	}
	WidgetTree->ForEachWidget([bFocus](UWidget* Widget)
	{
		const FString Name = Widget->GetName();
		if (Name.StartsWith(TEXT("FocusOnly")))
		{
			Widget->SetVisibility(bFocus ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		else if (Name.StartsWith(TEXT("IdleOnly")))
		{
			Widget->SetVisibility(bFocus ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	});
	if (USizeBox* LogSize = Cast<USizeBox>(GetWidgetFromName(TEXT("ChatLogSize"))))
	{
		LogSize->SetHeightOverride(bFocus ? FocusLogHeight : IdleLogHeight);
		if (IsValid(ChatScrollBox))
		{
			ChatScrollBox->ScrollToEnd();
		}
	}
}

void UNUChatWidget::HandleCustomTabOpen()
{
	SetCustomTabPanelOpen(true);
}

void UNUChatWidget::HandleCustomTabCancel()
{
	SetCustomTabPanelOpen(false);
}

void UNUChatWidget::HandleCustomTabApply()
{
	static const TPair<const TCHAR*, ENUChatChannel> Checks[] = {
		{ TEXT("ChatCustomCheck_Local"), ENUChatChannel::Local },
		{ TEXT("ChatCustomCheck_World"), ENUChatChannel::World },
		{ TEXT("ChatCustomCheck_Party"), ENUChatChannel::Party },
		{ TEXT("ChatCustomCheck_Whisper"), ENUChatChannel::Whisper },
		{ TEXT("ChatCustomCheck_System"), ENUChatChannel::System },
	};
	int32 Mask = 0;
	for (const TPair<const TCHAR*, ENUChatChannel>& Check : Checks)
	{
		if (const UCheckBox* Box = Cast<UCheckBox>(GetWidgetFromName(Check.Key)))
		{
			Mask |= Box->IsChecked() ? NUChat::ChannelBit(Check.Value) : 0;
		}
	}
	if (Mask == 0)
	{
		return;     // 아무 채널도 고르지 않은 탭은 만들지 않는다 (창은 열어 둔다)
	}
	FText Label = NSLOCTEXT("NUChat", "CustomTabDefault", "새 탭");
	if (const UEditableTextBox* Name = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("ChatCustomTabName"))))
	{
		const FString Trimmed = Name->GetText().ToString().TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			Label = FText::FromString(Trimmed.Left(8));
		}
	}
	AddCustomTab(Label, Mask);
	SetCustomTabPanelOpen(false);
}

void UNUChatWidget::SetCustomTabPanelOpen(bool bOpen)
{
	UWidget* Panel = GetWidgetFromName(TEXT("ChatCustomTabPanel"));
	if (Panel == nullptr)
	{
		return;
	}
	if (bOpen)
	{
		if (UEditableTextBox* Name = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("ChatCustomTabName"))))
		{
			Name->SetText(FText::GetEmpty());
		}
	}
	SetModalOpen(bOpen);
	Panel->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bOpen)
	{
		if (UWidget* Name = GetWidgetFromName(TEXT("ChatCustomTabName")))
		{
			Name->SetKeyboardFocus();
		}
	}
	else if (bFocusing && IsValid(InputBox))
	{
		InputBox->SetKeyboardFocus();
	}
}

void UNUChatWidget::SetModalOpen(bool bOpen)
{
	bModalOpen = bOpen;
}

bool UNUChatWidget::IsModalOpen() const
{
	return bModalOpen;
}

void UNUChatWidget::HandleTextCommited(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Esc는 NativeOnPreviewKeyDown이, 포커스 이탈은 NativeOnFocusChanging이 처리함
	// 여기서 닫으면 채팅창 안의 다른 위젯으로 포커스가 옮겨갈 때도 닫혀서 창이 떠있는데 필드 입력이 됨
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}

	FString Line = Text.ToString().TrimStartAndEnd();

	// 비우기 전에 봐야 함, 닫는 길로 가든 보내는 길로 가든 입력창은 비워야 하기 때문
	const bool bUntouched = IsInputUntouched(Line);

	{
		TGuardValue<bool> Guard(bRewritingInput, true);
		InputBox->SetText(FText::GetEmpty());	// 보냈으면 채팅창 비우기
	}

	// 아무것도 안 치고 엔터를 누른 것은 채팅창을 닫겠다는 뜻, Esc 와 똑같이 처리함
	if (bUntouched)
	{
		EndFocus();
		return;
	}

	// "/w [이름] [본문]" 은 사용자가 친 그대로 넘기기
	// SubmitChat이 명령어를 파싱
	OnChatSubmitted.ExecuteIfBound(SendChannel, Line);

	// 델리게이트가 동기라 여기선 대상이 이미 갱신돼 있음
	// 방금 보낸 상대가 그대로 다시 채워져서 본문만 이어 칠 수 있음
	RefreshWhisperCommand();

	InputBox->SetKeyboardFocus();
}

// 엔터를 눌렀을 때 보낼 것이 있는지 확인함
bool UNUChatWidget::IsInputUntouched(const FString& Line) const
{
	if (Line.IsEmpty())
	{
		return true;
	}

	if (SendChannel != ENUChatChannel::Whisper)
	{
		return false;
	}

	UNUChatSubsystem* Chat = GetChatSubsystem();
	const FString Target = IsValid(Chat) ? Chat->GetWhisperTargetName() : FString();
	if (Target.IsEmpty())
	{
		return false;
	}

	// 손으로 친 "/w 이름" 은 본문을 채우라는 안내가 맞으므로 자동으로 채운 것만 가려냄
	// RefreshWhisperCommand 가 채널을 옮길 때 쓰는 것과 같은 판정
	// MakeWhisperCommand 는 뒤에 공백을 붙이는데 Line 은 이미 잘려 있어 같이 잘라 맞춤
	return Line.Equals(NUChat::MakeWhisperCommand(Target).TrimStartAndEnd());
}

void UNUChatWidget::SetSendChannel(ENUChatChannel Channel)
{
	SendChannel = Channel;

	RefreshSendChannelVisual();
	RefreshWhisperCommand();   // 귓속말로 들어오면 대상을 채우고, 나가면 남은 명령어를 치움
	SetSendChannelMenuOpen(false);   // 골랐으면 목록 닫기
	OnSendChannelChanged(SendChannel);
}

ENUChatChannel UNUChatWidget::GetSendChannel() const
{
	return SendChannel;
}

void UNUChatWidget::CycleSendChannel()
{
	const TArray<ENUChatChannel>& Sendable = NUChat::GetSendableChannels();
	if (Sendable.IsEmpty()) return;

	// 목록에 없는 채널이면 IndexOfByKey가 -1이라 0번 채널으로 이동함
	int32 Current = Sendable.IndexOfByKey(SendChannel);
	int32 Next = (Current + 1) % Sendable.Num();

	SetSendChannel(Sendable[Next]);
}

void UNUChatWidget::ToggleSendChannelMenu()
{
	SetSendChannelMenuOpen(!bSendChannelMenuOpen);
}

void UNUChatWidget::HandleSendChannelButtonClicked()
{
	ToggleSendChannelMenu();
}

void UNUChatWidget::SetSendChannelMenuOpen(bool bOpen)
{
	if (!IsValid(SendChannelMenu)) return;

	bSendChannelMenuOpen = bOpen;

	// 나중에 파티/길드가 없으면 제외 같은 상황이 있을 수 있으므로 열때마다 만들어두기
	if (bOpen)
	{
		RebuildSendChannelMenu();
	}

	SendChannelMenu->SetVisibility(bOpen
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed);
}

void UNUChatWidget::RebuildSendChannelMenu()
{
	if (!IsValid(SendChannelMenu)) return;

	SendChannelMenu->ClearChildren();

	if (!ChannelButtonClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] ChannelButtonClass 미지정 --- 채널 목록을 그릴 수 없음 (WBP_Chat 클래스 기본값에 WBP_ChatChannelButton 을 넣으세요)"));
		return;
	}
	
	// 덮어쓰기가 꺼져 있는 SizeBox 는 Get~Override 가 0 을 돌려준다.
	// 그걸 그대로 크기로 쓰면 목록만 엉뚱한 크기가 되므로, 양수인 축만 가져온다
	FVector2D ChannelButtonSize = FVector2D::ZeroVector;
	const USizeBox* SendChannelButtonSizeBox = bMatchChannelMenuToSendButton
		? Cast<USizeBox>(GetWidgetFromName(TEXT("ChatSendChannelButtonSize")))
		: nullptr;
	if (SendChannelButtonSizeBox != nullptr)
	{
		ChannelButtonSize = FVector2D(SendChannelButtonSizeBox->GetWidthOverride(), SendChannelButtonSizeBox->GetHeightOverride());
	}

	// 전송채널 버튼 목록 만들기, 탭과 달리 채널마다 글자색이 다르므로 색을 같이 넘긴다
	// 크기는 SetPreferredSize 가 WBP 안의 RootSizeBox 에 물려줌
	for (ENUChatChannel Channel : NUChat::GetSendableChannels())
	{
		UNUChatChannelButtonWidget* Button = CreateWidget<UNUChatChannelButtonWidget>(this, ChannelButtonClass);
		if (!IsValid(Button))
		{
			continue;
		}

		Button->SetChannel(Channel, NUChat::GetChannelLabel(Channel), NUChat::GetChannelColor(Channel));
		Button->SetPreferredSize(ChannelButtonSize);
		Button->OnChannelClicked.BindUObject(this, &ThisClass::HandleChannelButtonPicked);

		if (UVerticalBoxSlot* MenuSlot = Cast<UVerticalBoxSlot>(SendChannelMenu->AddChild(Button)))
		{
			MenuSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UNUChatWidget::RefreshSendChannelVisual()
{
	// 버튼은 어느 채널로 보내는지만 말함
	// 누구에게 보내는지는 입력창의 "/w 이름 " 이 보여줌
	const FText ChannelLabel = NUChat::GetChannelLabel(SendChannel);
	const FLinearColor ChannelColor = NUChat::GetChannelColor(SendChannel);

	// SendChannelText(레거시 레이아웃, 버튼 밖 별도 텍스트)가 있으면 거기에 쓰고,
	// 없으면 SendChannelButton 자신의 타입에 맞춰 라벨에 반영한다
	// (WBP_CommonButton으로 교체된 경우, 또는 WBP_ChatTabButton으로 교체된 경우).
	if (IsValid(SendChannelText))
	{
		SendChannelText->SetText(ChannelLabel);
		SendChannelText->SetColorAndOpacity(FSlateColor(ChannelColor));
	}
	else if (UNU_CommonButtonWidget* CommonButtonWidget = Cast<UNU_CommonButtonWidget>(SendChannelButton))
	{
		CommonButtonWidget->SetLabelText(ChannelLabel);
		CommonButtonWidget->SetLabelColor(ChannelColor);
		CommonButtonWidget->SetIconSize(ChatChannelIconSize);
		CommonButtonWidget->SetIconTexture(NUChat::GetChannelIcon(SendChannel));
	}
	else if (UNUChatTabButtonWidget* TabButtonWidget = Cast<UNUChatTabButtonWidget>(SendChannelButton))
	{
		// 탭 버튼 계약엔 라벨 색을 밖에서 받는 API가 없음(선택/비선택 색은 자기 자신이 관리)
		// --- 채널별 색상은 반영 못 하지만, 최소한 문구는 보이게 함
		TabButtonWidget->SetTab(TabButtonWidget->GetTabIndex(), ChannelLabel);
		TabButtonWidget->SetIconSize(ChatChannelIconSize);
		TabButtonWidget->SetIconTexture(NUChat::GetChannelIcon(SendChannel));
	}

	InputBox->SetForegroundColor(ChannelColor);
}

void UNUChatWidget::HandleTextChanged(const FText& Text)
{
	// SetText가 이 함수를 다시 호출하므로 막는것이 필요함
	if (bRewritingInput || !IsValid(InputBox))
	{
		return;
	}

	// "/r " - 마지막으로 나에게 귓속말한 사람을 대상으로한 "/w "로 변경
	if (!NUChat::IsReplyCommandPrefix(Text.ToString()))
	{
		return;
	}

	UNUChatSubsystem* Chat = GetChatSubsystem();
	const FString ReplyTo = IsValid(Chat) ? Chat->GetLastIncomingWhisperName() : FString();

	if (ReplyTo.IsEmpty())
	{
		AddSystemMessage(NSLOCTEXT("NUChat", "NoReplyTarget", "답장할 상대가 없습니다."));
		return;
	}

	BeginWhisperCommand(ReplyTo);
}

void UNUChatWidget::BeginWhisperCommand(const FString& TargetName)
{
	if (TargetName.IsEmpty() || !IsValid(InputBox))
	{
		return;
	}

	// 여기는 대상을 새로 지정하는 자리라 지금 내용을 덮어쓰게 됨
	// SetSendChannel이 RefreshWhisperCommand를 태우므로 가드 안에 같이 넣어
	// 이전 대상이 잠깐 스쳤다 사라지는 일이 없게 함
	TGuardValue<bool> Guard(bRewritingInput, true);

	// 명령어를 지우고 그냥 치는 경우를 위해 채널도 맞춰둠
	SetSendChannel(ENUChatChannel::Whisper);

	// MakeWhisperCommand가 뒤에 공백을 붙여줌, 캐럿이 그 뒤에 놓여 바로 본문을 침
	InputBox->SetText(FText::FromString(NUChat::MakeWhisperCommand(TargetName)));
	InputBox->SetHintText(FText::GetEmpty());
}

void UNUChatWidget::RefreshWhisperCommand()
{
	// 다시 쓰는 중이면 손대지 않음
	if (bRewritingInput || !IsValid(InputBox))
	{
		return;
	}

	UNUChatSubsystem* Chat = GetChatSubsystem();
	FString Target = IsValid(Chat) ? Chat->GetWhisperTargetName() : FString();
	FString Current = InputBox->GetText().ToString();

	if (SendChannel != ENUChatChannel::Whisper)
	{
		InputBox->SetHintText(FText::GetEmpty());

		// 자동으로 채워둔 "/w 이름 " 만 남아 있으면 지움
		// SubmitChat은 채널과 무관하게 /w를 먼저 파싱하므로,
		// 이게 남아 있으면 라벨은 지역인데 귓속말이 나감
		if (!Target.IsEmpty() && Current.Equals(NUChat::MakeWhisperCommand(Target)))
		{
			TGuardValue<bool> Guard(bRewritingInput, true);
			InputBox->SetText(FText::GetEmpty());
		}
		return;
	}

	// 이미 명령어로 시작하면 사용자가 대상을 정하는 중이라 손대지 않음
	// "/w 홍" 처럼 이름을 치는 중인 경우도 여기 걸림
	FString UnusedName;
	FString UnusedBody;
	if (NUChat::ParseWhisperCommand(Current, UnusedName, UnusedBody))
	{
		return;
	}

	// 정해둔 대상이 없음, 명령어를 보여줌
	if (Target.IsEmpty())
	{
		InputBox->SetHintText(NSLOCTEXT("NUChat", "WhisperHint", "/w 이름 내용"));
		return;
	}

	InputBox->SetHintText(FText::GetEmpty());

	// 치던 글자는 살리고 앞에만 붙임
	TGuardValue<bool> Guard(bRewritingInput, true);
	InputBox->SetText(FText::FromString(NUChat::MakeWhisperCommand(Target) + Current));
}

UNUChatSubsystem* UNUChatWidget::GetChatSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUChatSubsystem>() : nullptr;
}

void UNUChatWidget::TrimChatLog()
{
	while (Lines.Num() > MAX_LINES)
	{
		// 지워지는 줄이 화면에 있었다면 위젯도 맨앞에서 하나 뺌
		bool bWasVisible = CheckFilter(Lines[0].Channel);
		
		Lines.RemoveAt(0);
		
		if (!bWasVisible || DisplayedLineCount <= 0)
		{
			continue;
		}

		// 자식 순서가 곧 화면 순서라 맨 앞을 떼고, 버리는 대신 맨 뒤에 여분으로 붙여 둠
		// 옮기는 것은 한 개뿐이라 만들고 버리던 것보다 좋음
		UNUChatLineWidget* Oldest = Cast<UNUChatLineWidget>(ChatScrollBox->GetChildAt(0));
		ChatScrollBox->RemoveChildAt(0);
		--DisplayedLineCount;

		if (IsValid(Oldest))
		{
			Oldest->SetDisplayed(false);
			ChatScrollBox->AddChild(Oldest);
		}
	}
}

bool UNUChatWidget::IsScrolledToBottom() const
{
	return ChatScrollBox->GetScrollOffset()
		>= ChatScrollBox->GetScrollOffsetOfEnd() - SCROLL_BOTTOM_TOLERANCE;
}

void UNUChatWidget::ScrollChatToEnd()
{
	if (!IsValid(ChatScrollBox))
	{
		return;
	}

	// 현재 틱에 한 번, 한 줄짜리 메세지는 이걸로 이미 맞고 화면도 곧바로 따라감
	ChatScrollBox->ScrollToEnd();

	// 다음 틱에 한 번 더 내림
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::ScrollChatToEndDeferred);
	}
}

void UNUChatWidget::ScrollChatToEndDeferred()
{
	if (!IsValid(ChatScrollBox))
	{
		return;
	}

	ChatScrollBox->ScrollToEnd();
}

bool UNUChatWidget::CheckFilter(ENUChatChannel Channel) const
{
	return Tabs.IsValidIndex(CurrentTab) 
		&& (Tabs[CurrentTab].ChannelMask & NUChat::ChannelBit(Channel)) != 0;
}

// 접어 둔 여분이 있으면 그것을 쓰고, 없을 때만 새로 만든다
// 여분은 항상 DisplayedLineCount 자리에 있음
UNUChatLineWidget* UNUChatWidget::AcquireLineWidget()
{
	if (DisplayedLineCount < ChatScrollBox->GetChildrenCount())
	{
		if (UNUChatLineWidget* Spare = Cast<UNUChatLineWidget>(ChatScrollBox->GetChildAt(DisplayedLineCount)))
		{
			return Spare;
		}
	}

	if (!LineWidgetClass)
	{
		return nullptr;
	}

	UNUChatLineWidget* Created = CreateWidget<UNUChatLineWidget>(this, LineWidgetClass);
	if (!IsValid(Created))
	{
		return nullptr;
	}

	ChatScrollBox->AddChild(Created);
	return Created;
}

void UNUChatWidget::CollapseSpareLineWidgets()
{
	for (int32 Index = DisplayedLineCount; Index < ChatScrollBox->GetChildrenCount(); ++Index)
	{
		if (UNUChatLineWidget* Spare = Cast<UNUChatLineWidget>(ChatScrollBox->GetChildAt(Index)))
		{
			Spare->SetDisplayed(false);
		}
	}
}

void UNUChatWidget::AppendLineWidget(const FNUChatLine& Line)
{
	if (!IsValid(ChatScrollBox)) return;
	
	UNUChatLineWidget* LineWidget = AcquireLineWidget();
	if (!IsValid(LineWidget))
	{
		return;
	}

	LineWidget->SetLine(Line);
	LineWidget->SetDisplayed(true);
	++DisplayedLineCount;
}

void UNUChatWidget::RebuildChat()
{
	if (!IsValid(ChatScrollBox)) return;

	// 자식을 지우지 않음, 앞에서부터 다시 채우고 남는 것만 접음
	DisplayedLineCount = 0;
	
	for (const FNUChatLine& Line : Lines)
	{
		if (CheckFilter(Line.Channel))
		{
			AppendLineWidget(Line);
		}
	}
	
	CollapseSpareLineWidgets();
	
	ScrollChatToEnd();	// 탭 변경시 맨 아래로 이동
}
