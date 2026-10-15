// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Battle/UI/NUBattleResultTypes.h"
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/NUScreenMotion.h"
#include "NUBattleResultWidget.generated.h"

class UButton;
class UHorizontalBox;
class UNUBattleResultEntryWidget;
class UNUBattleResultMemberWidget;
class UPanelWidget;
class UTextBlock;

// 전투가 끝나고 뜨는 결과창
// 전투 세트 종료를 이 창이 닫힐 때까지 붙잡으므로, 닫히는 곳이 반드시 하나는 있어야 함
UCLASS(Abstract)
class PROJECTM_API UNUBattleResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowResult(const FNUBattleResultView& InView);

	// WBP 의 확인 버튼과 PlayerController 의 확인 입력이 같이 부름, 두 번 불려도 한 번만 나감
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Result")
	void Dismiss();

public:
	// 창을 띄운 쪽이 무대 철거를 이어서 함
	FSimpleDelegate OnDismissed;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 전투가 끝나면 전투 입력 맵이 내려가서 PlayerController 로는 확인 키를 받을 수 없음
	// 창이 포커스를 쥐고 직접 받음
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 표시는 C++ 이 끝내므로 WBP 는 등장 연출만 얹으면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle|Result")
	void OnResultShown();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	FNUBattleResultView View;

	// --- 필수 ---

	// 명단 열이 가로로 붙는 자리, 열(세로 상자)은 C++ 이 만들어 넣음
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> MemberColumnBox;

	// 포획으로 끝나면 0 이 뜬다, 칸 자체는 항상 보임
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ExperienceText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> GoldText;

	// --- 선택, 배치는 UI 담당자가 정함 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	// 획득 아이템 칸 전체, 얻은 것이 없으면 숨김
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ItemPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemBox;

	// 획득 나이아 칸 전체, 잡은 것이 없으면 숨김
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> NaiaPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> NaiaBox;

	// --- 선택 (2026-10 결과창) --- 없으면 예전 창과 같음

	// 결과 제목 --- 승리면 VictoryTitle, 진 전투에서 잡기만 했으면 CaptureTitle, 그 밖은 EndTitle
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultTitleText;

	// 누르면 닫힘 (연출 중이면 먼저 연출을 끝 모습으로) --- 키 입력과 같은 규칙
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton;

	// 등장 연출에 쓰는 덩어리: 제목 묶음은 위에서 내려앉고, 시트는 오른쪽에서 들어온다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TitleRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SheetRoot;

	// 내 캐릭터 레벨이 올랐을 때 내 전투 아바타 머리 위에 붙는 "레벨 업!" (캔버스 칸 위젯, 아래 가운데가 머리 위).
	// 내 아바타를 하나로 가를 수 없으면(파티 넷 전투) 접힌다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LevelUpCallout;

	// --- 조절값 ---

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	TSubclassOf<UNUBattleResultMemberWidget> MemberWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	TSubclassOf<UNUBattleResultEntryWidget> EntryWidgetClass;

	// 한 열에 몇 줄까지 쌓을지, 넘으면 오른쪽에 새 열이 생김
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result", meta = (ClampMin = "1"))
	int32 MembersPerColumn = 3;

	// 열 사이 여백
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FMargin ColumnPadding = FMargin(0.f, 0.f, 12.f, 0.f);

	// 줄 사이 여백
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FMargin MemberPadding = FMargin(0.f, 0.f, 0.f, 8.f);

	// 명단이 한 열에 다 들어가면 그 열이 MemberColumnBox 폭을 다 쓴다 (한 줄짜리 명단 시트)
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	bool bFillSingleColumn = false;

	// 획득 칸 사이 여백 (ItemBox · NaiaBox 가 가로 상자일 때)
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FMargin EntryPadding = FMargin(0.f);

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FText VictoryTitle = NSLOCTEXT("NUBattleResult", "Victory", "승리");

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FText CaptureTitle = NSLOCTEXT("NUBattleResult", "Capture", "포획 성공");

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FText EndTitle = NSLOCTEXT("NUBattleResult", "End", "전투 종료");

	// 경험치 · 골드 글자 모양 ({0} = 숫자). 예: "+{0}"
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FText ExperienceFormat = FText::FromString(TEXT("{0}"));

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FText GoldFormat = FText::FromString(TEXT("{0}"));

	// 0 보다 크면 등장 연출을 켠다: 제목 → 시트 → 명단 줄 → 획득 칸을 이 간격으로 차례로. 0 = 연출 없이 바로 (예전 창)
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result", meta = (ClampMin = "0"))
	float RevealStagger = 0.f;

	// 0 보다 크면 경험치 · 골드 숫자가 이 시간 동안 0 에서 차오른다
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result", meta = (ClampMin = "0"))
	float CountUpSeconds = 0.f;

	// "레벨 업!" 의 아래 가운데를 아바타 머리 위 기준점에서 이만큼 띄운다 (화면 px, 위가 -)
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FVector2D CalloutOffset = FVector2D(0.f, -12.f);

private:
	// 명단을 세로로 쌓다가 MembersPerColumn 을 넘으면 오른쪽에 새 열을 추가함
	void RebuildMembers();

	// 목록을 채우고, 비어 있으면 그 칸(Panel)을 통째로 숨김
	void RebuildEntries(
		UPanelWidget* Box,
		UWidget* Panel,
		const TArray<FNUBattleResultEntry>& Entries,
		bool bIsNaia);

	// 등장 연출을 걸고, 레벨 업 표시를 붙일 내 아바타를 찾음
	void StartReveal();

	// 연출 중이면 끝 모습으로 보내고 true (첫 키는 건너뛰기, 다음 키가 닫기)
	bool SkipReveal();

	void SetRewardTexts(double Alpha);

	// 레벨 업 표시를 아바타 머리 위로 옮김
	void PlaceCallout();

	UFUNCTION()
	void HandleContinueClicked();

private:
	// 확인 입력이 연타로 들어와도 무대 철거를 두 번 이어가지 않도록 함
	bool bDismissed = false;

	FNUScreenMotion Motion;
	float CountElapsed = -1.f;      // 0 이상이면 숫자가 차오르는 중
	float CountDelay = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUBattleResultMemberWidget>> MemberWidgets;

	TWeakObjectPtr<AActor> CalloutAvatar;
};
