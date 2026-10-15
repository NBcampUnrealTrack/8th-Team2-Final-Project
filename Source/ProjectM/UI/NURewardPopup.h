#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NURewardPopup.generated.h"

class UTexture2D;

// 보상 결과
USTRUCT(BlueprintType)
struct PROJECTM_API FNURewardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

// 아직 표시하지 못한 보상 창 요청 (UNUNotificationUIController가 큐로 들고 있다)
struct FNURewardPopupRequest
{
	TWeakObjectPtr<UClass> PopupClass;
	FText Title;
	TArray<FNURewardEntry> Entries;
};

// 보상 획득을 알리는 공용 창. 퀘스트·전투·우편 등 획득처와 무관하게 쓴다.
// 표시 순서(큐)와 창 소유는 UNUNotificationUIController가, 화면 등록은 UNUUIManager가 맡는다.
//
// 팀 패턴: C++는 로직/계약만, 모양은 WBP가 담당한다.
//   - 데이터: Title / Entries (BlueprintReadOnly)
//   - 통지: OnRewardSet (BlueprintImplementableEvent)
//   - 이름 계약(그래프 배선 없이 쓰려면): TitleText / EntryText / CloseButton
UCLASS(Abstract)
class PROJECTM_API UNURewardPopup : public UUserWidget
{
	GENERATED_BODY()

public:
	// 보상 창을 띄운다. (내부적으로 알림 컨트롤러에 위임 — 호출부는 그대로)
	static UNURewardPopup* Show(
		APlayerController* OwningPlayer,
		TSubclassOf<UNURewardPopup> PopupClass,
		const FText& InTitle,
		const TArray<FNURewardEntry>& InEntries);

	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void Close();

	// 컨트롤러가 창을 띄우기 직전에 내용을 채운다
	void SetupReward(const FText& InTitle, const TArray<FNURewardEntry>& InEntries);

	// 소프트 참조 아이콘을 로드해 돌려준다. 지정되지 않았으면 nullptr
	UFUNCTION(BlueprintPure, Category = "NU|UI")
	static UTexture2D* ResolveRewardIcon(const FNURewardEntry& Entry);

protected:
	virtual void NativeConstruct() override;

	// ESC, Enter, Space로 닫는다
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// WBP가 목록을 직접 그릴 때 쓰는 통지
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnRewardSet();

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	TArray<FNURewardEntry> Entries;

	// 항목을 한 줄씩 이어 붙인 문자열. 아이콘 없이 간단히 표시할 때 쓴다
	UFUNCTION(BlueprintPure, Category = "NU|UI")
	FText BuildEntrySummary() const;

private:
	void BindCloseButton();

	// RewardRows가 있으면 항목 줄을 만들고 true (없으면 EntryText 요약만 쓴다)
	bool BuildEntryRows();

	UFUNCTION()
	void HandleCloseClicked();

	class UNUNotificationUIController* GetNotificationController() const;
};
