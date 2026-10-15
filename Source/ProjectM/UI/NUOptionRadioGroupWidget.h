// 옵션창의 한 줄짜리 라디오 버튼 묶음 --- 하/중/상/최상처럼 서로 배타적인 선택지를 네모 버튼(최대 5개)으로 보여준다.
// 모양은 WBP_OptionRadioGroup이 갖고(공용 버튼 5개), C++는 라벨·선택 표시·클릭 알림만 한다.
// 선택 표시는 WBP의 SelectedMark 위젯을 켜고 끄는 것이다. 공용 버튼 배경이 텍스처라 배경색 차이로는 안 보였다(9/10 PIE).
//
// 위젯 계약(BindWidget): Option0 ~ Option4 (UNU_CommonButtonWidget), SelectedMark0 ~ SelectedMark4 (아무 위젯 --- 고른 칸만 보인다).
//   칸마다 Overlay 하나에 버튼과 표시를 겹쳐 둔다. 선택지가 5개보다 적으면 남는 칸(Overlay)째로 접는다
// 인스턴스 값: LabelFontSize --- 세부 항목 줄처럼 작게 보여야 할 때 (0이면 WBP_CommonButton 기본)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUOptionRadioGroupWidget.generated.h"

class UNU_CommonButtonWidget;

DECLARE_DELEGATE_OneParam(FOnNUOptionSelected, int32 /*Index*/);

UCLASS(Abstract)
class PROJECTM_API UNUOptionRadioGroupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxOptions = 5;

	// 버튼 라벨. 5개를 넘는 것은 버리고, 모자라면 남는 버튼을 접는다
	void SetOptions(const TArray<FString>& Labels);

	// 선택 표시만 바꾼다 (OnSelected는 부르지 않는다). INDEX_NONE이면 아무것도 켜지지 않는다
	void SetSelectedIndex(int32 Index);
	int32 GetSelectedIndex() const { return SelectedIndex; }

	// 사용자가 버튼을 눌렀을 때만 온다 --- 프로그램이 SetSelectedIndex로 바꿀 때는 오지 않는다
	FOnNUOptionSelected OnSelected;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;

private:
	// 버튼마다 핸들러 하나 (OnCommonClicked에 페이로드가 없다 --- 옵션창 탭과 같은 처리)
	UFUNCTION() void HandleOption0Clicked() { HandleClicked(0); }
	UFUNCTION() void HandleOption1Clicked() { HandleClicked(1); }
	UFUNCTION() void HandleOption2Clicked() { HandleClicked(2); }
	UFUNCTION() void HandleOption3Clicked() { HandleClicked(3); }
	UFUNCTION() void HandleOption4Clicked() { HandleClicked(4); }
	void HandleClicked(int32 Index);

	TArray<UNU_CommonButtonWidget*, TInlineAllocator<MaxOptions>> GetButtons() const;
	TArray<UWidget*, TInlineAllocator<MaxOptions>> GetSelectedMarks() const;
	void RefreshVisual();

	UPROPERTY(EditAnywhere, Category = "NU|UI", meta = (ClampMin = 0))
	int32 LabelFontSize = 0;

	int32 SelectedIndex = INDEX_NONE;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> Option0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> Option1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> Option2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> Option3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> Option4;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SelectedMark0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SelectedMark1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SelectedMark2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SelectedMark3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SelectedMark4;
};
