// 화면 가운데 아래에 잠깐 떴다 사라지는 공용 알림 배너.
// 거래 결과·가방 가득 참처럼 "알리기만 하면 되는 것"에 쓴다 (누를 것이 있으면 확인 창을 쓸 것).
//
// 어느 기능에서든 UNUUIManager::ShowNotice(문구) 한 줄로 띄운다.
// 전투 행동 배너(UNUActionBannerWidget)와 같은 방식 --- 내용을 갈아치우고 수명을 다시 센다.
// 머리 위 말풍선(UNUSpeechBubbleUIController, WBP_SpeechBubble)도 이 클래스를 쓴다 --- 문구를 띄우고 Lifetime 뒤 숨는 동작이 같다.
//
// 팀 패턴: C++는 로직/계약만, 모양은 WBP가 담당한다.
//   - 데이터: Message (BlueprintReadOnly)
//   - 통지: OnNoticeShown (BlueprintImplementableEvent) --- 등장 애니메이션
//   - 위젯 계약(BindWidget): MessageText --- WBP에 이 이름이 없으면 WBP 컴파일이 실패한다
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/NUScreenMotion.h"
#include "NUNoticeBannerWidget.generated.h"

UCLASS(Abstract)
class PROJECTM_API UNUNoticeBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 문구를 보여준다. 이미 떠 있으면 내용을 갈아치우고 수명을 다시 센다
	void ShowMessage(const FText& InMessage);

protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ShowMessage 직후 호출 --- WBP가 등장 애니메이션을 재생한다
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnNoticeShown();

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FText Message;

	// 이 시간이 지나면 스스로 숨는다.
	// WBP 애니메이션이 없거나 중간에 끊겨도 사라지도록 애니메이션 길이보다 길게 둘 것
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI", meta = (ClampMin = "0.5"))
	float Lifetime = 2.5f;

	// --- 말풍선용 (기본값이면 예전 배너와 같음) ---

	// 0 보다 크면 C++ 가 줄을 직접 나눈다: 이 폭(Slate 단위)에 맞춰 줄 수를 정하고, 두 줄이면 문장 끝(. ! ? ~ …)에서 먼저 끊고,
	// 아니면 줄 길이를 고르게 맞춘다 ("잡으실 / 분"처럼 한두 글자만 넘어가는 줄을 막는다). 0 = WBP 의 자동 줄바꿈 그대로
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI", meta = (ClampMin = "0"))
	float BalancedWrapWidth = 0.f;

	// 위 줄 나눔으로 이 줄 수를 넘으면 마지막 줄 끝을 "…"로 자른다. 0 = 제한 없음
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI", meta = (ClampMin = "0"))
	int32 MaxLines = 0;

	// NoticeRoot 등장 연출의 시작 자리(제자리에서 떨어진 만큼)와 크기 --- 배너는 위에서 내려앉고, 말풍선은 아래에서 살짝 튀어 오른다
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI")
	FVector2D EnterOffset = FVector2D(0.f, -16.f);

	UPROPERTY(EditDefaultsOnly, Category = "NU|UI")
	float EnterFromScale = 0.97f;

	UPROPERTY(EditDefaultsOnly, Category = "NU|UI", meta = (ClampMin = "0.05"))
	float EnterSeconds = 0.38f;

private:
	// BalancedWrapWidth 가 있으면 줄을 나눈 문구, 없으면 그대로
	FText BalanceLines(const FText& InMessage) const;

	// 문구 자리. BindWidget이라 WBP에서 이름이나 타입이 바뀌면 컴파일 때 드러난다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> MessageText;

	// 선택: 있으면 C++가 등장(위에서 내려앉기)과 퇴장(흐려지며 살짝 올라가기)을 직접 움직인다 --- WBP 애니메이션이 없어도 된다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> NoticeRoot;

	FNUScreenMotion NoticeMotion;
	bool bLeaving = false;

	UFUNCTION()
	void HideBanner();

	FTimerHandle HideTimer;
};
