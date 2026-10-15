// 알림 계열 창(보상 팝업 등)을 소유하는 UI 컨트롤러.
// 한 번에 하나만 띄우고 나머지는 큐에 쌓아 순서대로 보여준다.
#pragma once

#include "CoreMinimal.h"
#include "UI/Controller/NUUIController.h"
#include "UI/NURewardPopup.h"
#include "NUNotificationUIController.generated.h"

UCLASS()
class PROJECTM_API UNUNotificationUIController : public UNUUIController
{
	GENERATED_BODY()

public:
	// 보상 창 요청. 이미 떠 있으면 큐에 넣고 nullptr을 돌려준다
	UNURewardPopup* ShowReward(TSubclassOf<UNURewardPopup> PopupClass, const FText& InTitle, const TArray<FNURewardEntry>& InEntries);

	// 팝업이 스스로 닫힐 때 알려온다 --- 등록 해제 후 다음 요청을 띄운다
	void NotifyPopupClosed(UNURewardPopup* Popup);

	// 레벨이 바뀌면 이전 레벨의 큐는 버린다 (다음 레벨에서 튀어나오지 않게)
	virtual void OnPlayerControllerChanged(APlayerController* NewPC) override;

protected:
	virtual void OnDeinitialize() override;

private:
	UNURewardPopup* ShowImmediate(TSubclassOf<UNURewardPopup> PopupClass, const FText& InTitle, const TArray<FNURewardEntry>& InEntries);
	void ShowNextPending();

	UPROPERTY(Transient)
	TObjectPtr<UNURewardPopup> ActivePopup;

	TArray<FNURewardPopupRequest> PendingRequests;
};
