// 새 HUD(글래스 엣지)의 상호작용 안내 ("F 대화하기"). 플레이어 컨트롤러가 InteractionIndicatorWidgetClass로 만들어
// 대상 위에 띄우고 보이기/숨기기만 한다 (UUserWidget으로 다루므로 이 클래스를 몰라도 된다).
// 이 클래스는 세 가지만 더한다: 키 글자를 입력 설정에서 읽어 오고(KeyText), 나타날 때 살짝 튀어나오고(PromptRoot),
// 머무는 동안 키가 눌렸다 올라오기를 되풀이한다(KeyCap·KeyLip) --- 퀘스트 표시의 고리·빛과 갈라 "누르세요"로 읽히게.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/NUScreenMotion.h"
#include "NU_InteractPromptWidget.generated.h"

class UInputAction;
class UTextBlock;

UCLASS()
class PROJECTM_API UNU_InteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 상호작용 입력. 여기에 묶인 키보드 키 이름을 KeyText에 쓴다 (못 찾으면 WBP에 적힌 글자를 둔다)
	UPROPERTY(EditAnywhere, Category = "NU|HUD")
	TSoftObjectPtr<UInputAction> InteractAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Field/IA_Interact.IA_Interact")));

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|HUD")
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|HUD")
	TObjectPtr<UWidget> PromptRoot;

	// 선택: 눌릴 때 아래로 내려가는 키 윗면, 눌릴 때 사라지는 키 아래 두께(립)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|HUD")
	TObjectPtr<UWidget> KeyCap;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|HUD")
	TObjectPtr<UWidget> KeyLip;

	// 한 번 눌렸다 올라오는 주기(초)와 깊이(픽셀)
	UPROPERTY(EditAnywhere, Category = "NU|HUD")
	float PressPeriod = 1.6f;

	UPROPERTY(EditAnywhere, Category = "NU|HUD")
	float PressDepth = 3.f;

private:
	void RefreshKey();

	FNUScreenMotion PopMotion;
	uint64 LastTickFrame = 0;
	float PressTime = 0.f;
};
