// 캐릭터 선택창의 칸 하나 --- 모양은 WBP_CharacterSlot이 갖고, C++는 자료와 상태만 넣는다.
// (레브 님 9/4: C++가 런타임에 그리던 칸은 디자이너·위젯 리플렉터 어디서도 못 찾아 디자인을 손댈 수 없었다)
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다 --- 아트팀이 이름을 바꾸면 그 자리에서 드러난다.
// 이름 조회(GetWidgetFromName)처럼 조용히 죽지 않는다. 선택(Optional)은 WBP가 빼도 된다.
//   SlotButton      (Button)            칸 전체 클릭 --- 캐릭터 칸은 선택, 빈 칸은 생성으로
//   CharacterBox    (아무 위젯)         캐릭터가 있을 때 보이는 묶음
//   EmptyBox        (아무 위젯)         빈 칸일 때 보이는 묶음
//   PortraitOverlay (Overlay)           몸·머리·눈 플립북을 C++가 얹는 자리. 높이는 PortraitHeight
//   NameText        (TextBlock)
//   SubText         (TextBlock, 선택)   "Lv.1 · 인간" 줄바꿈 "노비스" (직업 이름이 길어 두 줄. 직업 없음 = 노비스)
//   PendingText     (TextBlock, 선택)   삭제 예약 배지 --- 예약이 아니면 Hidden (자리는 남긴다)
//   DeleteButton    (Button, 선택)      삭제 / 삭제 취소. DeleteText(TextBlock, 선택)에 글자
// 줄 모양 칸(한 명 무대 선택창, 10/14)을 위한 값은 아래 "NU|Character|줄" --- 기본값은 전부 기존 카드 칸과 같다
//   SelectionFrame  (아무 위젯, 선택)   골라 둔 칸에만 보이는 테두리 --- 색·두께는 WBP, C++는 켜고 끄기만
//   JobIconImage    (Image, 선택)       있으면 칸에 얼굴 대신 직업 문장을 띄운다 (JobIcons, 키 = 직업 표 JobKey, 노비스 = Novice).
//                                       칸마다 플립북 세 겹이 계속 도는 것보다 가볍다 (10/14 선택창 프레임 저하)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "NUCharacterSlotWidget.generated.h"

class UButton;
class UImage;
class UOverlay;
class UPaperFlipbook;
class UTexture2D;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnNUCharacterSlotClicked, int64 /*CharacterIdx --- 빈 칸은 INDEX_NONE*/);
DECLARE_DELEGATE_TwoParams(FOnNUCharacterSlotDelete, int64 /*CharacterIdx*/, bool /*bCancelPending*/);

UCLASS(Abstract)
class PROJECTM_API UNUCharacterSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 캐릭터 칸으로 채운다. 골라 둔 칸은 테두리가 켜지고 대기 대신 걷기 모션이 돈다
	void SetCharacter(const FNUCharacterSummary& Character, bool bSelected);

	// 빈 칸 --- 누르면 생성으로 (선택창이 [캐릭터 생성]과 같이 처리한다)
	void SetEmpty();

	FOnNUCharacterSlotClicked OnSlotClicked;
	FOnNUCharacterSlotDelete OnDeleteClicked;

	// 외형(몸 => 외모 => 머리)을 Into에 겹쳐 그린다 --- 칸과 선택창의 큰 캐릭터가 같은 그림을 쓴다 (머리 색 포함).
	// PixelScale이 0보다 크면 Height 대신 그림 1칸을 그 배율로 그린다. bAlignTop이면 발 대신 머리 쪽을 맞춘다 (얼굴만 보이는 칸)
	static void BuildPortrait(UUserWidget* Owner, UOverlay* Into, const FNUCharacterSummary& Character, bool bWalk,
		float Height, float PixelScale, bool bAlignTop, UPaperFlipbook* DefaultIdle, UPaperFlipbook* DefaultWalk);

	// "Lv.12 · 나이아 나이트 · 인간" (한 줄) 또는 카드 칸의 두 줄 ("Lv.12 · 인간" 줄바꿈 "나이아 나이트")
	static FText DescribeCharacter(const UObject* WorldContext, const FNUCharacterSummary& Character, bool bSingleLine);

	// 몸 표에 없는 조합의 공용 외형 --- 선택창의 큰 캐릭터가 칸과 같은 것을 쓰려고 읽는다
	UPaperFlipbook* LoadDefaultIdleFlipbook() const;
	UPaperFlipbook* LoadDefaultWalkFlipbook() const;

protected:
	virtual void NativeOnInitialized() override;

	// 외형 플립북을 이 높이에 맞춰 원본 비율로 그린다 (몸과 파츠가 같은 캔버스라 같은 높이면 어긋나지 않는다)
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	float PortraitHeight = 140.f;

	// 몸 표에 없는 종족·성별 조합의 공용 외형 (대기 / 골랐을 때 걷기)
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	TSoftObjectPtr<UPaperFlipbook> DefaultIdleFlipbook;

	UPROPERTY(EditAnywhere, Category = "NU|Character")
	TSoftObjectPtr<UPaperFlipbook> DefaultWalkFlipbook;

	// --- 줄 모양 칸 (기본값은 기존 카드 칸의 동작) ---

	// 0보다 크면 PortraitHeight 대신 그림 1칸을 이 배율로 그린다 (얼굴 칸은 2 --- 정수 배율이라 도트가 고르다)
	UPROPERTY(EditAnywhere, Category = "NU|Character|줄", meta = (ClampMin = 0))
	float PortraitPixelScale = 0.f;

	// 발 대신 머리 쪽을 맞춘다 --- 얼굴만 보이는 작은 칸용. 어디를 보일지는 WBP가 PortraitOverlay의 자리로 정한다
	UPROPERTY(EditAnywhere, Category = "NU|Character|줄")
	bool bPortraitAlignTop = false;

	// 골라 둔 칸이 걷기 그림으로 바뀐다. 얼굴 칸은 끈다
	UPROPERTY(EditAnywhere, Category = "NU|Character|줄")
	bool bWalkWhenSelected = true;

	// SubText를 "Lv.12 · 직업 · 종족" 한 줄로 쓴다 (칸이 가로로 넉넉한 줄 모양용)
	UPROPERTY(EditAnywhere, Category = "NU|Character|줄")
	bool bSingleLineSub = false;

	// 삭제 버튼을 골라 둔 칸과 삭제 예약 중인 칸에만 보인다 (줄마다 버튼이 있으면 목록이 어수선하다)
	UPROPERTY(EditAnywhere, Category = "NU|Character|줄")
	bool bDeleteOnlyWhenSelected = false;

	// 직업 문장 (JobIconImage 가 있을 때). 키 = 직업 표의 JobKey, 전직 전(노비스)과 표에 없는 직업은 "Novice"
	UPROPERTY(EditAnywhere, Category = "NU|Character|줄")
	TMap<FName, TSoftObjectPtr<UTexture2D>> JobIcons;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CharacterBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> EmptyBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> PortraitOverlay;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PendingText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DeleteButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DeleteText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectionFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> JobIconImage;

	// 직업 문장을 띄웠으면 true (얼굴 플립북은 만들지 않는다)
	bool ApplyJobIcon(int32 JobId);

	UFUNCTION()
	void HandleSlotClicked();

	UFUNCTION()
	void HandleDeleteClicked();

	int64 CharacterIdx = INDEX_NONE;
	bool bDeletePending = false;
};
