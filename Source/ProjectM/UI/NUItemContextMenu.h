// 아이템 칸 메뉴 --- 패드 X(키보드·마우스는 Ctrl+우클릭)로 칸의 동작(사용·장착·나누기·버리기·창고로 ...)을 고르는 작은 팝업.
// 모양은 WBP_ItemContextMenu(프로젝트 세팅 NU UI의 ItemContextMenuClass)가 갖고, C++는 항목 버튼을 채우고 자리를 잡는다.
// 매니저 Popup 층 창이라 B/ESC는 매니저가 닫고(CloseTopWindow), 닫힐 때 매니저가 메뉴를 연 칸으로 포커스를 돌려준다.
// 메뉴는 하나만 뜬다 --- 다시 열면 앞의 것이 닫히고, 메뉴 밖을 누르면 닫힌다 (그 누름은 아래 위젯이 그대로 받는다).
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다.
//   MenuBG  (아무 위젯, 캔버스 자식)  메뉴 몸체 --- 캔버스 슬롯(AutoSize) 위치를 화면 좌표로 옮긴다
//   ItemBox (PanelWidget)            항목 버튼(ButtonClass, WBP 기본값)이 위에서 아래로 채워진다
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUItemContextMenu.generated.h"

class IInputProcessor;
class UNU_CommonButtonWidget;
class UPanelWidget;

// 메뉴 항목 하나 --- 고르면 메뉴를 닫고 Action을 부른다
struct FNUItemMenuEntry
{
	FText Label;
	FSimpleDelegate Action;
};

UCLASS(Abstract)
class PROJECTM_API UNUItemContextMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	// OwnerWindow = 이 메뉴를 연 창(칸이 있는 창). 그 창이 닫히면 메뉴도 같이 닫힌다.
	// ScreenPos = 메뉴 왼쪽 위(뷰포트 좌표, DPI 뺀 값). 항목이 없으면 열지 않는다.
	// 닫히면 매니저가 열기 전 포커스(연 칸)로 돌려준다
	static UNUItemContextMenu* Show(UUserWidget* OwnerWindow, TArray<FNUItemMenuEntry> InEntries, const FVector2D& ScreenPos);

	void Close();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// 버튼은 Tag에 항목 번호를 실어 하나의 핸들러로 받는다
	UFUNCTION()
	void HandleEntryClicked(int32 Tag);

	// 메뉴 몸체를 화면 안에 들어오게 옮긴다 (칸 오른쪽에 띄우되 화면 밖이면 안쪽으로)
	void PlaceAt(const FVector2D& ScreenPos);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> MenuBG;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ItemBox;

	// 항목 버튼 WBP (WBP_CommonButton) --- 메뉴 WBP 기본값에서 지정
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI")
	TSubclassOf<UNU_CommonButtonWidget> ButtonClass;

	TArray<FNUItemMenuEntry> Entries;
	FVector2D PendingScreenPos = FVector2D::ZeroVector;

	// 떠 있는 동안 메뉴 밖 마우스 누름을 지켜본다 (FNUItemMenuOutsideClick)
	TSharedPtr<IInputProcessor> OutsideClickWatcher;
};
