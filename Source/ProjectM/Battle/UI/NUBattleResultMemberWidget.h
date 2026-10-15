// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Battle/UI/NUBattleResultTypes.h"
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/NUScreenMotion.h"
#include "NUBattleResultMemberWidget.generated.h"

class UImage;
class UNUFlipbookImage;
class UOverlay;
class UProgressBar;
class UTextBlock;
class UTexture2D;

// 결과창 명단의 한 줄, 내 캐릭터와 파티 나이아가 같은 위젯을 씀
// 현재 나이아는 경험치가 없어 바가 비고 남은 경험치 글자도 숨는다
UCLASS(Abstract)
class PROJECTM_API UNUBattleResultMemberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMember(const FNUBattleResultMember& InMember);

	// 결과창의 등장 연출이 이 줄 차례에 부름: Delay 뒤에 바가 0 에서 차오르고, 레벨이 올랐으면 다 찬 뒤 레벨 업 덩어리가 튐.
	// BarFillSeconds 가 0 이면 아무것도 안 함 (SetMember 가 이미 끝 모습으로 채움)
	void PlayReveal(float Delay);

	// 연출 도중 확인 키를 누르면 끝 모습으로 바로 보냄
	void FinishReveal();

	// 차오르는 중인지 (결과창이 첫 키를 "건너뛰기"로 쓸지 가름)
	bool IsRevealing() const { return BarElapsed >= 0.f || Motion.IsRunning(); }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 이름·레벨·바는 C++ 이 직접 채우므로, WBP 는 여기서 연출만 얹으면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle|Result")
	void OnMemberSet();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	FNUBattleResultMember Member;

	// --- 필수 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ExperienceBar;

	// --- 선택, 배치는 UI 담당자가 정함 ---

	// 바 위에 작게 띄우는 "다음 레벨까지 남은 경험치"
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RemainingExperienceText;

	// 이번 전투로 레벨이 올랐을 때만 보이는 덩어리 (LEVEL UP 글자·아이콘 등)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LevelUpRoot;

	// --- 선택 (2026-10 결과창) --- 없으면 예전 줄과 같음

	// 내 캐릭터 얼굴 --- 선택창 줄과 같은 방법으로 몸·머리·얼굴 플립북을 겹친다 (위 정렬, PortraitPixelScale배). 나이아 줄은 접힘
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> PortraitBox;

	// 나이아 얼굴 --- 나이아 표의 PreviewFlipbook (NaiaPixelScale배). 캐릭터 줄은 접힘
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UNUFlipbookImage> NaiaFigure;

	// 나이아 속성 그림 (ElementIcons, 키 = 속성 태그 끝 이름 Fire · Water ...). 캐릭터 줄은 접힘
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ElementImage;

	// 바 + 남은 경험치 묶음 --- 경험치가 없는 줄(지금의 나이아)은 통째로 접는다 (빈 막대가 0 으로 읽히지 않게)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ExperienceRoot;

	// LevelUpRoot 안의 글자 --- LevelUpFormat ("레벨 업  {0} → {1}", 0 = 전 레벨, 1 = 지금 레벨)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelUpText;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	float PortraitPixelScale = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	float NaiaPixelScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	TMap<FName, TSoftObjectPtr<UTexture2D>> ElementIcons;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result")
	FText LevelUpFormat = NSLOCTEXT("NUBattleResult", "LevelUpFormat", "레벨 업  {0} → {1}");

	// 0 보다 크면 바가 이 시간 동안 0 에서 차오른다 (결과창이 PlayReveal 로 차례를 줌). 0 = 바로 끝 모습
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Result", meta = (ClampMin = "0"))
	float BarFillSeconds = 0.f;

private:
	float TargetRatio = 0.f;
	float BarDelay = 0.f;
	float BarElapsed = -1.f;        // 0 이상이면 차오르는 중
	FNUScreenMotion Motion;
};
