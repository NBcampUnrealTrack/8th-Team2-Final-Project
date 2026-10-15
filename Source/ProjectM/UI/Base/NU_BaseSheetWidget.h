// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/NU_BaseInGameWidget.h"
#include "NU_BaseSheetWidget.generated.h"

/*
인벤토리 / 캐릭터 정보처럼 "입력으로 열고 닫는 창(윈도우)" 계열의 베이스.

HUD 계열(UNU_BattleHUDWidget / UNU_FieldHUDWidget)과 수명이 다르다.
  - HUD  : 접속 직후 한 번 뷰포트에 올라가 계속 살아 있고, 상태에 따라 접혔다 펴진다.
  - Sheet: 열 때 뷰포트에 올라가고 닫을 때 내려간다. 그래서 구독도 열림/닫힘과 함께 붙고 떨어진다.

베이스가 책임지는 것 — 창마다 다시 쓰기 쉬운 것들만 모았다
  - 열기 / 닫기 / 토글, 그리고 그에 맞춘 마우스 커서 요청
  - 전투 진입 시 자동 닫기 (연출 위에 창이 남지 않게)
  - 같은 프레임의 다중 변경을 다음 틱 1회 갱신으로 합치기 (RequestRefresh)
  - 창 영역 좌클릭 드래그로 창 옮기기
  - 되돌릴 수 없는 동작 앞의 확인 창 (UNUConfirmDialog)

자식이 하는 것
  - RefreshSheet() 오버라이드에서 자기 데이터만 재구성
  - 자기 데이터 소유자(인벤토리 / 장비 / 퀘스트 컴포넌트 등) 구독은 자식이 정한다.
    무엇을 구독할지는 창마다 다르고, 베이스가 전부 알면 결합만 늘어난다.

주의: Inventory/UI/NUSheetWidgetBase 는 이 클래스와 무관한 프로토타입 계보다.
현재 인벤토리 / 캐릭터 정보 창은 아직 그쪽을 쓰고 있고, 정식 UI 로 교체할 때 이 클래스로 넘어온다.
*/

class ANUBattleEncounter;
class ANUPlayerController;
class UNUConfirmDialog;

UCLASS(Abstract)
class PROJECTM_API UNU_BaseSheetWidget : public UNU_BaseInGameWidget
{
	GENERATED_BODY()

public:
	/** 창을 뷰포트에 올린다. 이미 열려 있으면 아무것도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "NU|UI|Sheet")
	void OpenSheet();

	/** 창을 뷰포트에서 내린다. 구독도 함께 끊긴다(ReleaseWidget). */
	UFUNCTION(BlueprintCallable, Category = "NU|UI|Sheet")
	void CloseSheet();

	UFUNCTION(BlueprintCallable, Category = "NU|UI|Sheet")
	void ToggleSheet();

	UFUNCTION(BlueprintPure, Category = "NU|UI|Sheet")
	bool IsSheetOpen() const;

protected:
	virtual void InitializeWidget() override;
	virtual void ReleaseWidget() override;

	/** 전투에 들어가면 창을 닫는다 (bCloseOnBattleStart 가 true 일 때). */
	virtual void HandleEncounterChanged(ANUBattleEncounter* NewEncounter) override;

	// 창 영역 좌클릭 드래그로 창 전체를 옮긴다.
	// 여기까지 버블됐다 = 버튼 / 스크롤이 소비하지 않은 영역의 클릭이다.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/**
	 * 다음 틱에 RefreshSheet() 를 1회 부른다.
	 * 아이템 스왑처럼 한 프레임에 여러 델리게이트가 몰리는 경우를 한 번으로 합친다.
	 */
	void RequestRefresh();

	/** 자식 구현: 자기 데이터 재구성 + BIE 발화. 직접 부르지 말고 RequestRefresh() 를 쓴다. */
	virtual void RefreshSheet();

	/**
	 * 되돌릴 수 없는 동작 앞에 확인 창을 세운다.
	 * ConfirmDialogClass 미지정 시 경고만 남기고 즉시 실행한다 — 개발 중 동작이 죽지 않게.
	 */
	void RequestConfirm(const FText& InMessage, const FText& InConfirmLabel, FSimpleDelegate OnConfirmed);

	/** 커서 요청 / 해제를 한곳에서 처리한다. */
	void ApplyCursorRequest(bool bWant);

	ANUPlayerController* GetNUPlayerControllerChecked() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI|Sheet")
	void BP_OnSheetOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI|Sheet")
	void BP_OnSheetClosed();

protected:
	/**
	 * 커서를 요구하는 이유. ANUPlayerController::RequestCursor 의 키로 쓰이므로
	 * 창마다 서로 다른 값이어야 한다. 같은 값을 쓰면 한 창을 닫을 때 다른 창의 커서까지 풀린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|UI|Sheet")
	FName CursorReason = TEXT("Sheet");

	/** 창은 HUD(0) / 상호작용 인디케이터(10) 위, 전투 결과 연출(20) 아래에 둔다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|UI|Sheet")
	int32 SheetZOrder = 15;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|UI|Sheet")
	bool bCloseOnBattleStart = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|UI|Sheet")
	bool bDraggable = true;

	/** 확인 창 WBP. 창 WBP 의 클래스 기본값에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|UI|Sheet")
	TSubclassOf<UNUConfirmDialog> ConfirmDialogClass;

private:
	bool bRefreshQueued = false;

	// 창 드래그 상태.
	// RenderTranslation 이라 위젯을 닫았다 열어도(캐시 재사용) 위치가 유지된다.
	bool bDraggingWindow = false;
	FVector2D DragStartScreenPos = FVector2D::ZeroVector;
	FVector2D DragStartTranslation = FVector2D::ZeroVector;
	FVector2D WindowTranslation = FVector2D::ZeroVector;
};
