// 새 HUD(글래스 엣지)의 아이콘 버튼. 공용 버튼(UNU_CommonButtonWidget)에 마우스를 올렸을 때의 모습만 더한다:
// 버튼이 살짝 떠오르고(HoverLift), 금색 표시(HoverMark)와 이름표(Tooltip)가 부드럽게 나타난다. 누르면 살짝 눌린다.
// 세 위젯 모두 선택 --- 없으면 그 효과만 빠진다. 클릭 처리는 공용 버튼 그대로다.
#pragma once

#include "CoreMinimal.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "NU_HudButtonWidget.generated.h"

UCLASS()
class PROJECTM_API UNU_HudButtonWidget : public UNU_CommonButtonWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 떠오르고 눌리는 부분 (보통 아이콘을 담은 패널)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|HUD")
	TObjectPtr<UWidget> HoverLift;

	// 마우스를 올렸을 때만 보이는 표시 (테두리·밑줄)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|HUD")
	TObjectPtr<UWidget> HoverMark;

	// 버튼 위에 뜨는 이름표
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|HUD")
	TObjectPtr<UWidget> Tooltip;

	UPROPERTY(EditAnywhere, Category = "NU|HUD")
	float LiftPx = 3.f;

	// 이름표에 붙는 단축키 칸 (선택). ShortcutAction 에 지금 묶인 키보드 키를 보여 준다 --- 키 설정에서 바꾸면 따라간다.
	// 묶인 키가 없거나 액션이 비면 칸을 접는다
	UPROPERTY(EditAnywhere, Category = "NU|HUD")
	TObjectPtr<class UInputAction> ShortcutAction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|HUD")
	TObjectPtr<UWidget> KeyCap;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|HUD")
	TObjectPtr<class UTextBlock> Txt_Key;

private:
	void RefreshShortcutKey();

	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();
	UFUNCTION() void HandlePressed();
	UFUNCTION() void HandleReleased();
	void ApplyHover();

	float Hover = 0.f;
	float HoverTarget = 0.f;
	float Press = 0.f;
	float PressTarget = 0.f;
};
