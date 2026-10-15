// UI 공용층 — 창 열림/닫힘의 단일 진입점 (레이어 ZOrder·딤·커서/입력차단 일원화).
// 커서/입력 차단은 컨트롤러의 RequestCursor/RequestGameInputBlock을 대신 호출할 뿐 대체하지 않는다.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Net/EmberServerManager.h" // EEmberConnState (UFUNCTION 시그니처)
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UI/Controller/NUUIController.h"
#include "UI/HUD/NUFieldHUDMotion.h"
#include "UI/HUD/NUFieldHUDVisibility.h"
#include "UI/Common/NUPopupButtonHover.h"
#include "NUUIManager.generated.h"

// 뷰포트 ZOrder 밴드 — AddToViewport에 그대로 쓴다. 같은 밴드에서는 나중에 연 창이 위.
namespace NUUILayer
{
	constexpr int32 HUD = 0;        // 상시 표시 (전투 커맨드, 채팅, 파티 목록)
	constexpr int32 Indicator = 10; // 월드 부착 표시 (상호작용 인디케이터)
	constexpr int32 Tracker = 20;   // 상시 정보 (퀘스트 트래커)
	constexpr int32 Window = 100;   // 여닫는 창 (인벤/창고/캐릭터 정보/퀘스트 일지)
	constexpr int32 Dialogue = 200; // 대화창
	constexpr int32 Popup = 300;    // 알림/제안 팝업 (파티 초대, 보상)
	constexpr int32 Modal = 400;    // 응답을 강제하는 창 (확인 창)
	constexpr int32 Notice = 450;   // 알림 배너 — 확인 창 위에도 보이되 메뉴는 가리지 않는다
	constexpr int32 Transition = 1000; // HUD를 포함하는 전체 화면 전환
	constexpr int32 Menu = 500;     // 컨텍스트 메뉴/툴팁 — 항상 최상단
}

// 알림 종류별 표시 설정을 분리한다.
enum class ENUNoticeType : uint8
{
	General,
	LocationEntry
};

// 전체 메뉴(ESC)의 카테고리 — 열 순서 = 선언 순서. System은 하단 줄에 그려진다
UENUM(BlueprintType)
enum class ENUGameMenuCategory : uint8
{
	Character,
	Item,
	Naia,
	Adventure,
	System,
};

// 전체 메뉴 항목 — 각 기능이 자기 창 열기를 등록한다 (매니저는 목록만 알고 기능은 모른다)
struct FNUGameMenuEntry
{
	// TileView 항목과 고정 시스템 버튼이 공유하는 식별자. 배치와 무관하게 같은 동작을 찾는다.
	FName EntryId;

	ENUGameMenuCategory Category = ENUGameMenuCategory::Character;
	FText Label;

	// 단축키로 쓸 입력 동작 이름 (비우면 미표시). 실제 키는 그릴 때 사용자 설정에서 읽으므로
	// 키를 바꾸면 표기가 따라온다. 이름은 프로젝트 세팅 NU UI의 KeyBindingEntries와 같은 값
	FName ShortcutMapping;

	FSimpleDelegate OnActivated;

	// 파괴되면 항목이 자동으로 빠진다
	TWeakObjectPtr<const UObject> Owner;

	// 같은 카테고리 안 정렬 (작을수록 위)
	int32 SortOrder = 0;
};

// OpenWindow 부가 동작. 기본값 = 커서만 요청하는 일반 창.
USTRUCT(BlueprintType)
struct FNUUIWindowParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	int32 Layer = NUUILayer::Window;

	// 열려 있는 동안 배경을 어둡게. 딤은 매니저가 공용 1장으로 관리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bDimBackground = false;

	// ESC로 닫기 허용 — ESC를 직접 처리하는 창(확인 창)은 끈다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bCloseOnEscape = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bRequestCursor = true;

	// 이동 입력 차단 (화면을 크게 덮는 창용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bBlockGameInput = false;

	// Hide the persistent field panels while this window is open. Close/stale cleanup releases it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bHideFieldHUD = false;

	// 상주 창(뷰포트에 계속 두고 Visibility로 토글하는 창) 등록용.
	// 열림 판정을 IsInViewport 대신 Visibility로 하고, 닫기는 OnCloseRequested에 위임한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bVisibilityDriven = false;

	// 서버 세션이 끝나면 자동으로 닫는다 (채팅·파티·인벤처럼 서버 데이터를 그리는 창)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bCloseOnSessionEnd = false;

	// 창이 포커스와 키를 직접 관리한다 (대화창) --- 패드 모드에서 매니저가 첫 버튼에 포커스를 주지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bOwnsFocus = false;

	// 다른 창과 함께 뜨는 동반 창 (소지품을 열면 따라 뜨는 장비창, 상점·공방·창고가 주고받으려고 연 소지품).
	// 패드 첫 포커스는 같이 뜬 주 창이 받는다 --- 어느 창이 먼저 열렸는지와 상관없이 매니저 한 곳에서 고른다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bCompanion = false;

	// 열리고 닫힐 때 낼 효과음. 비우면 층에 맞는 공용 소리가 난다 --- 지도는 종이 펼치는 소리처럼 창마다 다르게 할 때 넣는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI", meta = (Categories = "NU.SFX"))
	FGameplayTag OpenSFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI", meta = (Categories = "NU.SFX"))
	FGameplayTag CloseSFX;

	// 이 액터를 따라다니는 창 (필드에서 캐릭터를 잡고 연 메뉴). 비우면 열린 화면 자리에 고정된다.
	// 열릴 때의 간격을 지키며 매니저가 자리를 맞추고, 액터가 사라지면 창을 닫는다
	UPROPERTY(Transient)
	TWeakObjectPtr<class AActor> AnchorActor;

	// 이 창을 띄운 주인 창 (칸에서 연 아이템 메뉴·수량 창). 주인 창이 닫히면 같이 닫힌다
	UPROPERTY(Transient)
	TWeakObjectPtr<UUserWidget> OwnerWindow;
};

DECLARE_MULTICAST_DELEGATE(FOnNUUIWindowsChanged);
DECLARE_MULTICAST_DELEGATE(FOnNUFieldHUDVisibilityChanged);
// 패드 <=> 키보드·마우스 전환 (true = 패드). 장치별 문구·단축키 표기를 가진 창이 구독한다
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUGamepadModeChanged, bool /*bGamepad*/);

// 공용 딤 — 코드로만 만드는 검정 60% 전체 화면 (클릭 통과)
UCLASS()
class PROJECTM_API UNUDimLayerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};

// 카메라 암전과 같은 알파로 HUD까지 덮는 전환 전용 레이어.
UCLASS()
class PROJECTM_API UNUTransitionLayerWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};

/// <summary>창 열림/닫힘의 단일 진입점. AddToViewport / RemoveFromParent를 직접 부르지 말고
/// OpenWindow / CloseWindow를 쓸 것. PC 기준 취득: UNUUIManager::Get(PC)</summary>
UCLASS()
class PROJECTM_API UNUUIManager : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UNUUIManager* Get(const APlayerController* PC);

	// Manual player preference only. Other features use RequestFieldHUDHidden instead.
	UFUNCTION(BlueprintCallable, Category = "NU|UI|HUD")
	void SetFieldHUDVisible(bool bVisible);

	// Effective target, including temporary requests; not the animation's current opacity.
	UFUNCTION(BlueprintPure, Category = "NU|UI|HUD")
	bool IsFieldHUDVisible() const { return bFieldHUDVisible; }

	UFUNCTION(BlueprintPure, Category = "NU|UI|HUD")
	bool IsFieldHUDUserVisible() const { return FieldHUDVisibility.IsUserVisible(); }

	UFUNCTION(BlueprintPure, Category = "NU|UI|HUD")
	bool HasFieldHUDHideRequests() const { return FieldHUDVisibility.HasRequests(); }

	// Same Owner+Reason is idempotent. Release with bHide=false; null owners/None reasons are ignored.
	// Requests end on PC/session change; destroyed owners are pruned within 0.25 seconds.
	UFUNCTION(BlueprintCallable, Category = "NU|UI|HUD", meta = (DefaultToSelf = "Owner"))
	void RequestFieldHUDHidden(FName Reason, bool bHide, const UObject* Owner, bool bAnimate = true);

	UFUNCTION(BlueprintCallable, Category = "NU|UI|HUD", meta = (DefaultToSelf = "Owner"))
	void ReleaseFieldHUDHideRequests(const UObject* Owner, bool bAnimate = true);

	FOnNUFieldHUDVisibilityChanged OnFieldHUDVisibilityChanged;

	// NormalVisibility describes the panel's own state, before the manual HUD preference.
	UFUNCTION(BlueprintCallable, Category = "NU|UI|HUD")
	void PresentFieldHUDWidget(UWidget* Widget, ESlateVisibility NormalVisibility);
	UFUNCTION(BlueprintCallable, Category = "NU|UI|HUD")
	void PresentFieldHUDGroup(class UPanelWidget* Container);
	// Restores and unregisters this widget and all registered descendants on teardown/reuse.
	UFUNCTION(BlueprintCallable, Category = "NU|UI|HUD")
	void ForgetFieldHUDWidget(UWidget* Widget);

	// 창을 레이어에 올린다 (AddToViewport 대행). 이미 등록된 창이면 무시.
	// OnCloseRequested: ESC 닫기 요청 시 부를 소유 코드의 닫기 함수 — 비우면 매니저가 직접 내린다
	void OpenWindow(UUserWidget* Widget, const FNUUIWindowParams& Params, FSimpleDelegate OnCloseRequested = FSimpleDelegate());

	// 창을 내린다 (RemoveFromParent 대행 + 커서/입력차단/딤 정리)
	void CloseWindow(UUserWidget* Widget);

	// Embedded popups (quest offers) use the same hover motion as registered popup windows.
	void SetPopupButtonHover(UWidget* Root, bool bEnabled);

	// ESC 한 번 = 최상단 닫기 가능 창 하나 (레이어 큰 것 => 최근 연 것). 키 바인딩은 2차
	bool CloseTopWindow();

	bool IsWindowOpen(const UUserWidget* Widget) const;

	bool IsAnyWindowOpen(int32 MinLayer = 0) const;

	// 그 층에 떠 있는(보이는) 창들 (등록 순). 패드 LB/RB로 창을 넘길 때 쓴다
	TArray<UUserWidget*> GetShownWindows(int32 Layer) const;

	// 패드 모드가 바뀌면 PlayerController가 알려준다. 패드면 맨 위 창의 첫 자리에 포커스를 주고,
	// 마우스로 돌아오면 버튼에 남은 포커스를 뷰포트로 돌려 Enter·Space가 그 버튼을 누르지 않게 한다.
	// 포커스 테두리 표시는 UNUPadFocusUIController가 맡는다 (여기서는 켜고 끄기만 알린다)
	void HandleGamepadModeChanged(bool bGamepad);

	// Slate 위젯이 어느 UMG 위젯인지 (열린 창들 안에서 찾는다, 목록 항목은 항목 위젯. 없으면 nullptr) --- 패드 포커스 보정용
	UWidget* FindWidgetForSlate(const TSharedPtr<SWidget>& SlateWidget) const;

	// Root와 그 아래 전부에 Fn --- 패널 자식·중첩 UserWidget 안·목록의 화면 항목까지 (UWidgetTree::ForEachWidgetAndDescendants는 중첩 UserWidget 안으로 안 들어간다)
	static void ForEachWidgetDeep(UWidget* Root, TFunctionRef<void(UWidget*)> Fn);

	// Root 아래(Root 자신은 빼고)에서 처음 만나는 포커스 가능한 조작부. 중첩 위젯 자체보다 그 안의 조작부, 목록 항목 안까지 본다
	static UWidget* FindFirstFocusableIn(UWidget* Root);
	static void FocusFirstFocusableIn(UWidget* Root);

	// Root 아래(패널 자식·중첩 위젯 안까지)에서 처음 만나는 누를 수 있는 버튼에 포커스. 이미 안에 포커스가 있으면 둔다.
	// 닫기 버튼(WBP_CloseButton)은 건너뛰고 그것뿐이면 그거라도 --- D패드가 설 자리가 있어야 한다.
	// 다음 틱에 준다: 같은 프레임에 커서 요청으로 SetInputMode가 걸리면 프레임 끝에 포커스가 뷰포트로 되돌아간다.
	// 매니저 창이 아닌 패널(퀘스트 제안)도 쓸 수 있게 공개 static
	static void FocusFirstButtonIn(UWidget* Root);

	// 같은 규칙으로 지금 바로 준다 (이미 다음 틱인 곳용). 안에 포커스가 있어도 옮긴다 --- 아이템 칸이 있으면 첫 칸이 먼저다.
	// 시트 창의 기본 첫 자리(UNUSheetWidgetBase::FocusForPad)가 이것이라 창을 열 때와 LB/RB로 건너올 때 자리가 같다
	static void FocusFirstButtonNow(UWidget* Root);

	// 화면 아래에 잠깐 떴다 사라지는 알림 배너. 어느 기능에서든 문구만 넘기면 된다.
	// 누를 것이 있으면 이게 아니라 확인 창(UNUConfirmDialog)을 쓸 것.
	// 배너 클래스는 프로젝트 세팅(NU UI)에서 읽고, 미지정이면 로그만 남긴다 --- 호출부는 분기하지 않는다
	void ShowNotice(const FText& Message, ENUNoticeType NoticeType = ENUNoticeType::General);

	// 서버 연결을 끊고 초기 화면(프로젝트 세팅 LoginLevel = 타이틀/로그인 레벨)으로 나간다.
	// 끊김 안내 창의 [초기화면으로]와 캐릭터 선택창의 [타이틀로]가 같은 길을 쓴다. 미지정이면 아무것도 안 한다
	void GoToLoginLevel();

	// --- 전체 메뉴 (ESC — 닫을 창이 없을 때) ---
	// 항목은 각 기능이 등록한다. 같은 Owner로 다시 등록하기 전에 UnregisterGameMenuEntries를 부르면 멱등

	void RegisterGameMenuEntry(
		FName EntryId,
		ENUGameMenuCategory Category,
		const FText& Label,
		FSimpleDelegate OnActivated,
		const UObject* Owner,
		int32 SortOrder = 0,
		FName ShortcutMapping = NAME_None);

	// 열려 있는 전체 메뉴를 다시 그린다 (키 설정을 적용했을 때처럼 표기만 바뀐 경우)
	void RefreshGameMenu();

	void UnregisterGameMenuEntries(const UObject* Owner);

	// 메뉴 위젯이 그릴 때 읽는다 — Owner가 죽은 항목은 걸러서 카테고리·정렬순서로 정렬해 준다
	TArray<FNUGameMenuEntry> GetGameMenuEntries() const;

	// 메뉴 클래스는 프로젝트 세팅(NU UI)에서 읽는다. 창 등록(Modal+딤+이동차단)은 매니저가 대신 한다
	void OpenGameMenu();
	void CloseGameMenu();
	bool IsGameMenuOpen() const;

	FOnNUUIWindowsChanged OnWindowsChanged;
	FOnNUGamepadModeChanged OnGamepadModeChanged;

	// --- UI 컨트롤러 레지스트리 ---
	// 매니저는 컨트롤러 종류를 모른다 (클래스 키만 안다). 생성 요청은 항상 기능 코드가 한다.

	// 없으면 만들어서 돌려준다. 상주형 컨트롤러는 이 호출로 처음 생긴다
	template<typename TController>
	TController* GetController()
	{
		return Cast<TController>(FindOrCreateController(TController::StaticClass()));
	}

	// 세션형 컨트롤러 정리 (거래 종료 등) — 없으면 무시
	void DestroyController(TSubclassOf<UNUUIController> ControllerClass);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 레벨 전환으로 PlayerController가 바뀌면 엔진이 호출한다 — 컨트롤러들에 전파
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	struct FNUUIWindowEntry
	{
		TWeakObjectPtr<UUserWidget> Widget;
		FNUUIWindowParams Params;
		FSimpleDelegate OnCloseRequested;
		FName Reason;         // 커서/입력차단 요청 사유 "UI:{위젯명}"
		int32 OpenSerial = 0; // 같은 레이어 내 최근 열림 판정
		TWeakObjectPtr<UWidget> PreviousFocus; // 열 때 포커스가 있던 조작부 --- 닫히면 패드 모드에서 여기로 되돌린다 (수량·확인 창 뒤 원래 칸으로)
		FVector2D AnchorOffset = FVector2D::ZeroVector; // 따라다니는 창: 열릴 때 잰 액터 화면 자리와 창 자리의 차이
	};

	TArray<FNUUIWindowEntry> Windows;
	FNUPopupButtonHover PopupButtonHover;
	FDelegateHandle PopupButtonHoverTickHandle;
	void TickPopupButtonHover(float DeltaTime);
	void StopPopupButtonHover();
	int32 OpenSerialCounter = 0;
	bool bFieldHUDVisible = true;
	bool bFieldHUDSuppressed = false;
	FNUFieldHUDVisibility FieldHUDVisibility;
	FTSTicker::FDelegateHandle FieldHUDRequestTickHandle;
	void RefreshFieldHUDVisibility(bool bAnimate, bool bPreferenceChanged = false);
	void RefreshFieldHUDRequestTick();
	bool TickFieldHUDRequests(float DeltaTime);
	FNUFieldHUDMotion FieldHUDMotion;
	FDelegateHandle FieldHUDMotionTickHandle;
	void TickFieldHUDMotion(float DeltaTime);
	void StopFieldHUDMotion();

	UPROPERTY(Transient)
	TObjectPtr<UNUDimLayerWidget> DimWidget;

	int32 DimZOrder = INDEX_NONE;

	// 알림 배너 — 창이 아니라 순간 표시라 Windows에 등록하지 않고 매니저가 직접 재사용한다
	UPROPERTY(Transient)
	TObjectPtr<class UNUNoticeBannerWidget> NoticeBanner;

	// 전체 메뉴 — 도메인 데이터가 없는 순수 셸이라 배너처럼 매니저가 직접 소유·재사용한다
	UPROPERTY(Transient)
	TObjectPtr<class UNUGameMenuWidget> GameMenu;

	// 등록된 전체 메뉴 항목들 (정렬은 GetGameMenuEntries에서)
	TArray<FNUGameMenuEntry> GameMenuEntries;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUUIController>> Controllers;

	UNUUIController* FindOrCreateController(TSubclassOf<UNUUIController> ControllerClass);

	// 서버 세션 상태 감시 (LoggedIn에서 벗어나면 세션 종속 창을 닫고 컨트롤러에 알린다).
	// 매니저는 LocalPlayer 수명이라 레벨 전환 중에 끊겨도 놓치지 않는다
	UFUNCTION()
	void HandleServerStateChanged(EEmberConnState NewState);

	void HandleSessionEnded();

	// 끊김 안내 창을 띄운다. [재접속]은 다시 붙기를 시도하고, [초기화면으로]는 로그인 레벨로 나간다.
	// 재접속이 실패하면 이 창을 다시 띄운다 --- 사용자가 포기할 때까지 두 선택지가 남아 있어야 한다
	void ShowSessionEndedDialog();

	class UEmberServerManager* GetServerManager() const;

	// 세션이 살아 있었는지 (LoggedIn -> 그 외로 바뀌는 순간만 종료로 본다)
	bool bSessionWasLive = false;

	// 재접속 시도 중인지. 이 동안의 상태 변화는 끊김이 아니라 재접속 결과로 읽는다
	bool bReconnecting = false;

	int32 FindEntry(const UUserWidget* Widget) const;

	// 창이 화면에 떠 있는지 — 등록 방식(bVisibilityDriven)에 따라 판정이 다르다
	static bool IsEntryShown(const FNUUIWindowEntry& Entry);

	// 매니저를 안 거치고 사라진 창 정리 + 요청 해제. 걷어낸 창에 포커스가 남아 있었으면 돌려주고 창 목록 변경을 알린다
	void SweepStaleWindows();

	// 상주 창(bVisibilityDriven)을 소유 코드가 가시성만 꺼서 닫은 순간 --- 매니저 닫기와 같이 정리한다.
	// 기다렸다 다음 창 여닫기 때 걷으면 그 사이 숨은 버튼에 포커스가 남고 패드 이동 막힘도 안 풀렸다 (퀘스트 일지, 9/28)
	UFUNCTION()
	void HandleResidentVisibilityChanged(ESlateVisibility InVisibility);

	// 닫힌 창의 포커스를 돌려준다 --- 패드 모드면 그 창을 열기 전 조작부로, 돌려줄 곳이 없고 그 창에 포커스가 있었으면 게임으로
	void ReturnFocusAfterClose(const FNUUIWindowEntry& Entry, bool bHadFocus);

	// 닫기를 소유 코드에 맡긴다 (닫기 요청이 없으면 매니저가 직접 내린다)
	void RequestCloseWindow(UUserWidget* Widget);

	// 액터를 따라다니는 창의 자리를 맞춘다 (대상이 사라졌으면 닫는다).
	// Slate가 그리기 직전에 부른다 --- 위젯 틱에서 옮기면 부모가 이미 자리를 잡은 뒤라 한 프레임 늦게 반영된다
	void HandleSlatePreTick(float DeltaTime);

	// 따라다니는 창이 있을 때만 위 갱신을 구독한다
	void RefreshAnchorTick();

	FDelegateHandle AnchorTickHandle;

	// 화면의 모든 버튼에 호버·누름 소리를 내는 입력 전처리기 (버튼마다 WBP 스타일에 소리를 넣지 않는다)
	TSharedPtr<class IInputProcessor> ButtonSounds;

	void ReleaseEntryRequests(const FNUUIWindowEntry& Entry);

	// 딤 필요 창 최상단 바로 아래에 공용 딤 유지 (없으면 내림)
	void UpdateDim();

	// Filter를 통과한 창 중 맨 위(레이어 큰 것 => 최근 연 것). 없으면 INDEX_NONE
	int32 FindTopWindowIndex(TFunctionRef<bool(const FNUUIWindowEntry&)> Filter) const;

	// 패드 모드에서 창이 열리거나 닫히면 다음 틱에 맨 위 창의 첫 자리에 포커스를 준다
	// (시트 창은 그 창의 FocusForPad, 창 안에 INUPadFocusTarget(NUPadFocusTarget.h)이 있으면 그것이 돌려준 위젯, 나머지는 첫 버튼).
	// 같은 프레임에 주면 SetInputMode(GameAndUI)가 프레임 끝에 포커스를 뷰포트로 되돌려 덮어쓴다 (채팅 입력창과 같은 함정)
	void RequestGamepadFocus();
	void FocusTopWindowDeferred();

	// 창 안 D패드·스틱 이동을 매니저가 직접 정한다 (창 루트의 Custom 규칙 --- Slate 기본 탐색은 쓰지 않는다).
	// 포커스가 들어 있는 창 안에서, 이번 프레임에 그려진 조작부 중 포커스 위젯 중심에서 그 방향으로 가장 가까운 것을 돌려준다.
	// 고른 것이 그쪽이 아니라 위아래 줄에 있는 것이면(폭이 넓은 탭 등) 같은 줄·열에 걸친 것 중 가장 가까운 것으로 바꾼다.
	// 없으면 nullptr = 제자리 (옆 창이나 HUD로는 넘어가지 않는다 --- 창 사이 이동은 LB·RB)
	UFUNCTION()
	UWidget* HandleWindowNavigation(EUINavigation Direction);


	class ANUPlayerController* GetNUPlayerController() const;
	APlayerController* GetOwningPlayerController() const;
};
