// 새 HUD(글래스 엣지)의 전투 명령 판. 플레이어 컨트롤러가 BattleCommandWidgetClass로 만들어 보이기/숨기기만 하고,
// 키 입력도 컨트롤러가 명령 컴포넌트로 보낸다. 이 위젯은 그 컴포넌트의 지금 상태를 그리고 마우스 클릭만 받는다.
//
// 판은 지금 행동하는 캐릭터 바로 옆에 뜬다: 몸 플립북 경계의 중심(없으면 CommandUIAnchor)을 화면에 비춰 오른쪽에 붙이고,
// 오른쪽이 모자라면 왼쪽으로 넘긴다(쪽은 열릴 때 한 번 정한다). 화면 안으로 밀어 넣고, 캐릭터와 판 사이를
// 금색 점·선(TetherDot / TetherLine)으로 잇는다. CommandRoot·Tether*는 루트 캔버스의 직속 자식이어야 한다(왼쪽 위 기준).
//
// 그리는 것 (모두 선택 --- 없는 위젯은 건너뛴다):
//   첫 메뉴(RootPanel)   Button_Attack/Skill/Item/NaiaSwap/Flee (공용 버튼). 고른 줄에 RootHighlight가 미끄러져 붙고, 못 쓰는 줄은 흐려진다
//                        (컨트롤러가 Button_Attack/Button_Skill의 글자를 바꾼다 --- 예전 WBP와 같은 이름)
//   세부(DetailPanel)    스킬·아이템·나이아 목록과 대상 고르기: Text_DetailCap / Text_DetailName / Text_DetailPos / Text_DetailBody,
//                        SP 투입(SPRow, Panel_SPPips --- 낼 수 있는 최대 타수만큼 칸, 지금 타수만큼 채움, Text_SPHits)
//   대기(WaitPanel)      제출 중 / 다른 사람 기다리는 중 (Text_Wait)
//   알림(Text_Notice)    못 쓰는 행동·거절·나이아 교체 실패를 잠깐 띄운다
//   Text_Source          "새벽의 행동"
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/NUBattleTypes.h"
#include "Styling/SlateBrush.h"
#include "UI/NUScreenMotion.h"
#include "NU_BattleCommandWidget.generated.h"

class UNUBattleCommandComponentBase;
class UNU_CommonButtonWidget;
class UTextBlock;
class UPanelWidget;

UCLASS()
class PROJECTM_API UNU_BattleCommandWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> CommandRoot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RootPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> DetailPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> WaitPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RootHighlight;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class USizeBox> CommandWidth;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RootRows;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TetherDot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TetherLine;
	// 대상 고르기: 대상 머리 위 표시 위의 커서(캔버스 정렬 0.5,1)와 발밑 고리(정렬 0.5,0.5). 루트 캔버스 직속
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TargetCursor;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TargetRing;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UNU_CommonButtonWidget> Button_Attack;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UNU_CommonButtonWidget> Button_Skill;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UNU_CommonButtonWidget> Button_Item;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UNU_CommonButtonWidget> Button_NaiaSwap;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UNU_CommonButtonWidget> Button_Flee;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Source;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_DetailCap;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_DetailName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_DetailPos;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_DetailBody;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SPRow;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Panel_SPPips;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SPHits;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Wait;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Notice;

	// 첫 메뉴 줄 간격 (RootHighlight가 이만큼씩 내려간다). 줄 순서: 공격, 스킬, 아이템, 나이아 교체, 도주
	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	float RowPitch = 54.f;

	// 판 폭: 첫 메뉴 / 세부(스킬 설명이 들어감)
	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	float RootWidth = 224.f;

	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	float DetailWidth = 300.f;

	// 캐릭터 기준점에서 판 가장자리까지 / 이음 점까지 (화면 단위)
	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	float AnchorGap = 70.f;

	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	float TetherDotOffset = 38.f;

	// 화면 가장자리 여백
	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	float ScreenMargin = 24.f;

	// 대상 커서와 머리 위 표시 사이 간격
	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	float CursorGap = 6.f;

	// 기준점이 움직일 때 판이 따라가는 빠르기 (FInterpTo 속도)
	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	float FollowSpeed = 14.f;

	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	FSlateBrush PipOnBrush;

	UPROPERTY(EditAnywhere, Category = "NU|Battle|HUD")
	FSlateBrush PipOffBrush;

private:
	UNUBattleCommandComponentBase* FindComponent() const;
	void Bind(UNUBattleCommandComponentBase* Component);
	void Refresh(UNUBattleCommandComponentBase* Component, bool bForce);
	void ShowNotice(const FText& Message);
	void UpdatePlacement(UNUBattleCommandComponentBase* Component, float DeltaTime, bool bSnap);
	void PlayOpen();
	static int32 RowOf(ENUBattleRootAction Action);

	UFUNCTION() void HandleActionUnavailable(ENUBattleRootAction Action);
	UFUNCTION() void HandleSubmissionFailed(ENUBattleCommandSubmitResult Result);
	UFUNCTION() void HandleNaiaSwapFailed(ENUNaiaSwapResult Result);
	UFUNCTION() void HandleAttackClicked();
	UFUNCTION() void HandleSkillClicked();
	UFUNCTION() void HandleItemClicked();
	UFUNCTION() void HandleNaiaSwapClicked();
	UFUNCTION() void HandleFleeClicked();
	void Select(ENUBattleRootAction Action);

	void MarkTarget(AActor* Target);
	void UpdateTargetCursor(UNUBattleCommandComponentBase* Component, float DeltaTime);
	static UUserWidget* FindTagWidget(AActor* Avatar, class UWidgetComponent** OutComponent = nullptr);
	void HandleOwnVisibility(ESlateVisibility InVisibility);
	TWeakObjectPtr<class UNU_CombatantStatusWidget> MarkedTag;
	TWeakObjectPtr<AActor> MarkedTarget;
	TArray<TWeakObjectPtr<class UNU_CombatantStatusWidget>> DimmedTags;
	TWeakObjectPtr<AActor> CursorTarget;
	FVector2D CursorPos = FVector2D::ZeroVector;
	FVector2D RingPos = FVector2D::ZeroVector;
	bool bCursorShown = false;
	float LockAge = 0.f;
	float CursorTime = 0.f;
	TWeakObjectPtr<UNUBattleCommandComponentBase> BoundComponent;
	FString LastStateKey;
	uint8 LastPhase = 255;
	float HighlightY = 0.f;
	float NoticeLeft = 0.f;
	FNUScreenMotion Motion;

	// 배치: 1 = 캐릭터 오른쪽, -1 = 왼쪽. 열릴 때 정한다
	float Side = 1.f;
	FVector2D PanelPos = FVector2D::ZeroVector;
	bool bPlaced = false;
	bool bPendingOpen = false;
	TWeakObjectPtr<AActor> PlacedFor;
};
