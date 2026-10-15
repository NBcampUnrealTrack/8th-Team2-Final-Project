// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "UI/NUScreenMotion.h"
#include "NUCharacterSelectWidget.generated.h"

class UNUCharacterManager;
class UNUConfirmDialog;

// 캐릭터 선택창 (Abstract + BlueprintReadOnly + BIE). 칸은 SlotWidgetClass(WBP_CharacterSlot, BindWidget)로 만든다 --- 9/4 레브 님 지적:
// C++가 그리던 칸은 디자이너에서 못 찾는다. 창 자체의 버튼은 아직 이름 계약이다 (다음 정리 때 BindWidget으로).
//
// 위젯 계약(BindWidget --- WBP에 이 이름·타입이 없으면 WBP 컴파일이 실패한다):
//   CharacterListBox — 캐릭터 칸들이 들어갈 패널. UniformGridPanel이면 그리드(DNF식), 그 외 패널이면 세로 목록
//   CreateButton     — 캐릭터 생성 버튼 (생성 레벨로 이동한다. 슬롯 가득/요청 중엔 비활성)
//   EnterButton      — 입장 버튼. 칸을 골라 두고 이 버튼으로 입장한다 (선택 없으면 비활성)
//   TitleButton      — 타이틀(로그인) 화면으로 돌아간다. 확인 창을 거치고, 서버 연결을 끊는다
//   ErrorText        — 서버 실패 사유 표시용 TextBlock. 자리·색·글꼴은 WBP, 문구가 비면 접힌다
//
// 칸 클릭 = 선택만 (테두리 강조 + 미리보기가 대기=>걷기 모션으로). 입장은 EnterButton이 한다
//
// 선택 (10/14, 한 명 무대 WBP용 --- WBP에 그 이름이 없으면 조용히 넘어간다):
//   HeroPortrait (Overlay)        골라 둔 캐릭터를 크게 세우는 자리 (그림 1칸 = HeroPixelScale 픽셀, 발을 맞춘다)
//   HeroShadow   (아무 위젯)      캐릭터가 서 있을 때만 보이는 발밑 그림자
//   SelectedNameText / SelectedSubText (TextBlock)  골라 둔 캐릭터의 이름과 "Lv · 직업 · 종족". 없으면 안내 문구
//   SlotCountText (TextBlock)     "2 / 8"
//   EmptyNote    (아무 위젯)      캐릭터가 하나도 없을 때만 보인다
//   EnterShine   (아무 위젯)      시작하기가 눌릴 수 있을 때만 보이는 반짝임
//   등장 연출(bPlayIntro): IntroVeil · TitlePanel · FormPanel · SheetBody(자식이 차례로) · CharacterListBox(줄이 차례로).
//   고른 캐릭터가 바뀌면 HeroPortrait·HeroShadow가 살짝 올라오며 나타난다
UCLASS(Abstract)
class PROJECTM_API UNUCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 생성 레벨로 이동한다 (생성은 전용 레벨이다 --- 8/25 결정)
	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void OpenCreateWindow();

	// 선택 요청 — 서버 확정(OnCharacterSelected)을 받아야 실제로 이동한다
	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void SelectAndEnter(int64 CharacterIdx);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// BP가 목록을 직접 그리고 싶을 때 쓰는 통지 (이름 계약을 안 쓰는 경우)
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Character")
	void OnCharacterListUpdated();

	// 서버 실패 사유 통지 (BP가 팝업 등으로 표시)
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Character")
	void OnCharacterErrorReceived(int32 ResultCode, const FString& Message);

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	TArray<FNUCharacterSummary> Characters;

	// 칸 하나의 WBP (모양·크기는 WBP_CharacterSlot이 갖는다). 미지정이면 경고만 남기고 칸을 그리지 않는다
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	TSubclassOf<class UNUCharacterSlotWidget> SlotWidgetClass;

	// 그리드일 때 한 줄에 놓을 칸 수
	UPROPERTY(EditAnywhere, Category = "NU|Character", meta = (ClampMin = 1))
	int32 GridColumns = 4;

	// 서버가 슬롯 수를 알려주기 전까지 쓸 칸 수 (기획 확정값 8칸 = 4열 x 2줄)
	UPROPERTY(EditAnywhere, Category = "NU|Character", meta = (ClampMin = 1))
	int32 DefaultSlotCount = 8;

	// 삭제 확인 창 WBP (버리기와 같은 공용 확인창). 미지정 시 경고 후 확인 없이 실행 (개발 편의)
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	TSubclassOf<UNUConfirmDialog> ConfirmDialogClass;

	// 고른 캐릭터가 없으면 첫 캐릭터(삭제 예약이 아닌 것부터)를 골라 둔다 --- 켜면 열자마자 [시작하기]를 누를 수 있다.
	// 꺼 두면(기본) 지금처럼 칸을 눌러야 고른다
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	bool bAutoSelectFirst = false;

	// 큰 캐릭터(HeroPortrait)를 그릴 배율 --- 그림 1칸이 화면 몇 픽셀인가 (1080p 기준. 정수여야 도트가 고르다)
	UPROPERTY(EditAnywhere, Category = "NU|Character", meta = (ClampMin = 1))
	float HeroPixelScale = 5.f;

	// 열릴 때 등장 연출. 위젯 이름은 위 목록 --- 없는 것은 건너뛴다
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	bool bPlayIntro = false;


private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> CreateButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> EnterButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> TitleButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UPanelWidget> CharacterListBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> ErrorText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UOverlay> HeroPortrait;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> HeroShadow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> SelectedNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> SelectedSubText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> SlotCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> EmptyNote;

	UFUNCTION()
	void HandleListChanged();

	UFUNCTION()
	void HandleError(int32 ResultCode, const FString& Message);

	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleEnterClicked();

	// [타이틀로] --- 연결이 끊기는 동작이라 확인 창을 먼저 띄운다
	UFUNCTION()
	void HandleTitleClicked();

	void RefreshData();
	void PopulateCharacterList();

	// 골라 둔 캐릭터를 무대와 이름 줄에 다시 그린다 (그 위젯이 있는 WBP에서만)
	void RefreshSelectedDisplay();

	// 서버가 준 최대 슬롯 수, 없으면 DefaultSlotCount (캐릭터가 그보다 많으면 그 수)
	int32 GetSlotCount() const;

	// 칸 클릭: 캐릭터 칸은 선택만(입장은 EnterButton), 빈 칸(INDEX_NONE)은 [캐릭터 생성]과 같다
	void HandleSlotClicked(int64 CharacterIdx);
	void HandleSlotDeleteClicked(int64 CharacterIdx, bool bCancelPending);
	void HandleDeleteClicked(int64 CharacterIdx);
	void HandleDeleteCancelClicked(int64 CharacterIdx);

	// 슬롯 가득/생성 요청 중이면 CreateButton 비활성 (1초에 수십 번 눌러 서버 거절당하는 것 방지)
	void RefreshCreateButtonState();

	// 선택된 칸이 없거나 입장 요청 중이면 EnterButton 비활성
	void RefreshEnterButtonState();
	void SetErrorText(const FText& InText);

	UNUCharacterManager* GetManager() const;

	// 생성 요청을 보내고 응답(목록 갱신 or 오류)이 올 때까지 true
	bool bCreateInFlight = false;

	// 입장 요청을 보내고 응답(레벨 이동 or 오류)이 올 때까지 true — 연타 방지
	bool bEnterInFlight = false;

	// 칸 클릭으로 고른 캐릭터 (미선택 = INDEX_NONE)
	int64 SelectedCharacterIdx = INDEX_NONE;

	// 등장 연출, 목록이 처음 찼을 때의 줄 연출, 고른 캐릭터가 바뀔 때의 연출 (NativeTick이 돌린다)
	FNUScreenMotion IntroMotion;
	FNUScreenMotion HeroMotion;
	// 무대에 마지막으로 세운 캐릭터 --- 같으면 다시 올리지 않는다
	int64 ShownHeroIdx = INDEX_NONE;
	// 목록 줄을 한 번 차례로 띄웠다 (그 뒤로 다시 그릴 때는 그냥 그린다)
	bool bRowsIntroPlayed = false;

	// 줄이 처음 찼을 때 차례로 띄운다 (등장 연출을 켰을 때만)
	void PlayRowsIntro();
};
