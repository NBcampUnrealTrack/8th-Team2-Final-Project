#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUQuestAgitationWidget.generated.h"

class UProgressBar;
class UTextBlock;

// 나이아의 동요 게이즈를 표시하는 전용 UI 클래스로, 진정 전투의 동요만 표시한다.
// 배치와 색은 WBP Designer에서 수정하고 값은 QuestComponent가 전달한다.
UCLASS(Abstract, Blueprintable)
class PROJECTM_API UNUQuestAgitationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetAgitation(float Value, float Maximum);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> AgitationBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AgitationText;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest")
	float Agitation = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Quest")
	float MaxAgitation = 100.f;
};
