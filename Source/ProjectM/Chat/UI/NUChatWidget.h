// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Chat/NUChatTypes.h"
#include "NUChatWidget.generated.h"

DECLARE_DELEGATE_TwoParams(FNUOnChatSubmitted, ENUChatChannel, const FString&);

// 입력 포커스가 열리고 닫히는 것을 PlayerController에 알림, IMC_Field 제거 타이밍 전달 용도
DECLARE_DELEGATE_OneParam(FNUOnChatFocusChanged, bool);

UCLASS(Abstract)
class PROJECTM_API UNUChatWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 메세지 추가, 발신자 이름은 호출자가 넘기는 방식
	void AddMessage(ENUChatChannel Channel, const FText& SenderName, const FText& Text, bool bIsMine);
	
	// 연결끊김, 초대 실패 등 시스템 메세지 추가
	void AddSystemMessage(const FText& Text);
	
	UFUNCTION(BlueprintCallable, Category = "NU|Chat")
	void BeginFocus();

	// ESC, 포커스 이탈 처리
	UFUNCTION(BlueprintCallable, Category = "NU|Chat")
	void EndFocus();

	UFUNCTION(BlueprintPure, Category = "NU|Chat")
	bool IsFocusing() const;
	
	// 채팅창 상단 탭 선택 시 필터링 변경
	UFUNCTION(BlueprintCallable, Category = "NU|Chat")
	void SelectTab(int32 TabIndex);
	
	// 커스텀 탭 추가, 탭 이름과 필터링할 채널 비트를 받아서 생성함
	UFUNCTION(BlueprintCallable, Category = "NU|Chat")
	void AddCustomTab(const FText& Label, int32 ChannelMask);

	// 커스텀 탭 창 열기 설정
	UFUNCTION(BlueprintCallable, Category = "NU|Chat")
	void SetModalOpen(bool bOpen);

	// 커스텀 탭 창 열려있는지 확인
	UFUNCTION(BlueprintPure, Category = "NU|Chat")
	bool IsModalOpen() const;

	
	// --- 전송 채널 ---

	// 전송 채널 설정
	UFUNCTION(BlueprintCallable, Category = "NU|Chat")
	void SetSendChannel(ENUChatChannel Channel);

	UFUNCTION(BlueprintPure, Category = "NU|Chat")
	ENUChatChannel GetSendChannel() const;

	// Tab 키, 다음 전송 채널로 변경
	UFUNCTION(BlueprintCallable, Category = "NU|Chat")
	void CycleSendChannel();

	// 채널 버튼 클릭 시 선택 목록을 펼치거나 접음
	UFUNCTION(BlueprintCallable, Category = "NU|Chat")
	void ToggleSendChannelMenu();
	
	// "/w 이름 " 을 채우고 이어서 본문을 칠 수 있게 함
	// PlayerController에서 컨텍스트 메뉴로 귓속말 하기를 선택했을 때 입력창을 채우는 것 
	void BeginWhisperCommand(const FString& TargetName);

public:
	FNUOnChatSubmitted OnChatSubmitted;
	FNUOnChatFocusChanged OnFocusChanged;
	
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 창 안쪽을 클릭했을 때 포커스가 게임 뷰포트로 빠지는것을 막음
	virtual bool NativeSupportsKeyboardFocus() const override;
	
	// 창이 받은 포커스를 입력창으로 넘겨줌
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

	// 입력창이 포커스를 갖고 있어도 Tab/Esc를 먼저 잡기 위해 Preview를 사용해야 함
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	
	
	// 포커스가 채팅창 밖으로 나가면 입력을 닫음
	// Slate는 이전·새 포커스 경로의 모든 위젯에 이걸 보내므로 조상인 채팅창도 받음
	virtual void NativeOnFocusChanging(
		const FWeakWidgetPath& PreviousFocusPath,
		const FWidgetPath& NewWidgetPath,
		const FFocusEvent& InFocusEvent) override;
	
	// 포커스 상태 바뀌면 채팅창 변화 처리
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Chat")
	void OnFocusStateChanged(bool bInFocusing);

	// 채팅 탭 변경
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Chat")
	void OnTabChanged(int32 TabIndex);

	// 전송 채널 변경
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Chat")
	void OnSendChannelChanged(ENUChatChannel NewChannel);
	
private:
	void ApplyHUDVisibility();

	// 새 HUD(글래스 엣지): 이름이 "FocusOnly"로 시작하는 위젯은 입력 중에만, "IdleOnly"로 시작하는 위젯은 입력하지 않을 때만 보인다
	// (평소엔 최근 줄만 떠 있고 Enter를 누르면 탭·입력창·배경이 펼쳐진다). 그런 이름이 없으면 아무것도 하지 않는다
	void ApplyFocusLook(bool bFocus);

	// 같은 규칙으로, 이름이 ChatLogSize인 SizeBox가 있으면 대화 줄 영역 높이를 바꾼다 (평소엔 최근 몇 줄만)
	UPROPERTY(EditAnywhere, Category = "NU|Chat")
	float IdleLogHeight = 92.f;

	UPROPERTY(EditAnywhere, Category = "NU|Chat")
	float FocusLogHeight = 236.f;

	// 커스텀 탭 만들기 창 (C++ 판). WBP에 ChatCustomTabPanel이 있을 때만 ChatAddTabButton이 이 창을 연다 ---
	// 이름 입력 ChatCustomTabName, 채널 체크 ChatCustomCheck_Local/World/Party/Whisper/System, 버튼 ChatCustomTabApply/Cancel
	UFUNCTION() void HandleCustomTabOpen();
	UFUNCTION() void HandleCustomTabApply();
	UFUNCTION() void HandleCustomTabCancel();
	void SetCustomTabPanelOpen(bool bOpen);

	UFUNCTION()
	void HandleTextCommited(const FText& Text, ETextCommit::Type CommitMethod);

	// 입력 중에 "/r " 을 "/w {이름} " 으로 갈아끼우기 위해 확인해야 함
	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	// 사용자가 실제로 친 것이 있는지, 귓속말 채널이 자동으로 채워 둔 "/w 이름" 은 친 것으로 안 침
	bool IsInputUntouched(const FString& Line) const;

	class UNUChatSubsystem* GetChatSubsystem() const;

	// 마우스로 들어온 포커스를 다음 틱에 입력창으로 넘김, 클릭이 씹히지 않게 하기 위함
	void FocusInputBoxDeferred();

	// 닫을 때 입력창에 남은 키보드 포커스를 게임으로 돌려줌, 다음 틱에 확인해야 함
	void ReleaseInputFocusDeferred();

	UFUNCTION()
	void HandleSendChannelButtonClicked();

	// 탭이 보이는 범위를 왼쪽으로 이동
	UFUNCTION()
	void HandleTabPrevClicked();

	// 탭이 보이는 범위를 오른쪽으로 이동
	UFUNCTION()
	void HandleTabNextClicked();

	void RebuildTabButtons();
	void RefreshTabHighlight();

	// 상단 탭·채널 목록·로그 줄 모두 디자이너가 고른 WBP로 만들고,
	// 눌린 것이 무엇인지는 각 위젯이 델리게이트에 실어 돌려줌
	void HandleTabButtonPicked(int32 TabIndex);

	void HandleChannelButtonPicked(int32 ChannelValue);

	// 보이는 탭 범위와 화살표 버튼 활성 상태를 현재 창 위치에 맞게 설정
	void RefreshTabWindow();

	void TrimChatLog();
	bool IsScrolledToBottom() const;

	// 채팅 위치를 맨 아래로 내림, 이번 프레임에 한 번 다음 틱에 한 번 더 내림
	void ScrollChatToEnd();

	// 위 함수가 예약해 둔 두 번째 호출
	void ScrollChatToEndDeferred();

	bool CheckFilter(ENUChatChannel Channel) const;

	void AppendLineWidget(const FNUChatLine& Line);
	void RebuildChat();

	// 로그 줄 위젯은 만들었다 버리지 않고 스크롤박스 안에 남겨 다시 씀
	// 앞에서부터 DisplayedLineCount 수가 지금 보이는 줄 개수이고, 그 뒤는 접어 둔 여분
	class UNUChatLineWidget* AcquireLineWidget();

	// DisplayedLineCount 뒤에 남은 여분을 전부 접음
	void CollapseSpareLineWidgets();

	void SetSendChannelMenuOpen(bool bOpen);
	void RebuildSendChannelMenu();
	void RefreshSendChannelVisual();

	// 귓속말 채널일 때 입력창을 "/w [대상] " 으로 시작하게 유지함
	// 누구에게 보내는 중인지 계속 보이게 하는 역할
	void RefreshWhisperCommand();

	void CacheTabSideButtonWidgets();

private:
	UPROPERTY(Transient)
	TObjectPtr<class UScrollBox> ChatScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<class UEditableTextBox> InputBox;

	// 탭 버튼이 들어갈 상단 박스
	UPROPERTY(Transient)
	TObjectPtr<class UHorizontalBox> ChatTabBox;

	// ChatTabBox 에 들어가는 탭 버튼들 (TabButtonClass 인스턴스, 탭 순서와 같은 순서)
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UNUChatTabButtonWidget>> TabButtons;

	// 탭 보이는 범위를 옮기는 화살표, 양 끝에서는 해당 방향이 꺼짐
	// 순정 Button과 UNU_CloseButtonWidget/UNU_CommonButtonWidget(공용 부품) 둘 다 지원하려면
	// 타입을 하나로 고정할 수 없어 UWidget으로 들고, SetIsEnabled(공통 API)만 여기서 직접 쓴다.
	// 클릭 바인딩은 이름 계약(BindNamedButtonClicked)이 실제 타입을 캐스팅해 처리한다.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> TabPrevButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> TabNextButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> TabAddButton;

	// 전송 채널 버튼, 입력창 왼쪽
	UPROPERTY(Transient)
	TObjectPtr<UWidget> SendChannelButton;

	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> SendChannelText;

	// 버튼을 누르면 펼쳐지는 채널 목록, 평소엔 Collapsed
	UPROPERTY(Transient)
	TObjectPtr<class UVerticalBox> SendChannelMenu;

	TArray<FNUChatLine> Lines;	// 채팅 라인
	TArray<FNUChatTab> Tabs;	// 실제 채팅탭 정보 Array

	int32 CurrentTab = 0;

	// 스크롤박스 자식 중 지금 줄을 보여주고 있는 개수 (앞에서부터)
	int32 DisplayedLineCount = 0;

	// 지금 보이는 탭 범위의 왼쪽 끝 인덱스
	int32 TabWindowStart = 0;

	ENUChatChannel SendChannel = ENUChatChannel::Local;

	bool bFocusing = false;
	bool bSendChannelMenuOpen = false;

	// SetText가 OnTextChanged를 호출하므로 다시 호출되는 일 방지하는 용도
	bool bRewritingInput = false;

	// 커스텀 탭 창이 열려 있는지, 포커스 이탈로 닫히지 않게 하는 역할
	bool bModalOpen = false;
	
	
	// --- 조절값  ---

	// 탭 하나의 클래스
	// 미지정이면 경고만 남기고 상단 탭을 그리지 않음
	UPROPERTY(EditDefaultsOnly, Category = "NU|Chat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UNUChatTabButtonWidget> TabButtonClass;

	// 탭을 ChatTabBox 폭에 균등하게 늘릴지, 각 탭이 보고하는 크기 그대로 둘지.
	// 끄면 탭 폭이 글자 길이만큼만 잡힘
	UPROPERTY(EditDefaultsOnly, Category = "NU|Chat", meta = (AllowPrivateAccess = "true"))
	bool bStretchTabsToFill = true;

	// 탭 사이 여백. 인접한 두 탭 사이는 좌우가 합쳐지므로 실제 간격은 이 값의 두 배
	UPROPERTY(EditDefaultsOnly, Category = "NU|Chat", meta = (AllowPrivateAccess = "true"))
	FMargin TabSlotPadding = FMargin(0.5f, 0.f);

	// 한 번에 보이는 탭 수, 나머지는 화살표로 밀어서 봄
	UPROPERTY(EditDefaultsOnly, Category = "NU|Chat", meta = (AllowPrivateAccess = "true"))
	int32 MaxVisibleTabs = 4;

	// 전송 채널 목록 항목 하나의 클래스
	// 미지정이면 경고만 남기고 채널 목록을 그리지 않음
	UPROPERTY(EditDefaultsOnly, Category = "NU|Chat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UNUChatChannelButtonWidget> ChannelButtonClass;

	// 채널 목록 항목의 크기를 ChatSendChannelButtonSize 에 맞출지.
	// 끄면 WBP_ChatChannelButton 이 정한 크기가 그대로 쓰임 - 채널 목록만 다른 크기로 가고 싶을 때
	UPROPERTY(EditDefaultsOnly, Category = "NU|Chat", meta = (AllowPrivateAccess = "true"))
	bool bMatchChannelMenuToSendButton = true;

	// 로그 한 줄의 클래스. 폰트·글자 크기·줄 간격·줄바꿈이 전부 이 WBP로 넘어감
	// 미지정이면 경고만 남기고 로그를 그리지 않음, 메세지는 쌓이지만 화면에 안 나옴
	UPROPERTY(EditDefaultsOnly, Category = "NU|Chat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UNUChatLineWidget> LineWidgetClass;
};
