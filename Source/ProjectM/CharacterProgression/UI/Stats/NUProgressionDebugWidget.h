#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "NUProgressionDebugWidget.generated.h"

class UNUProgressionComponent;
class UNUAbilitySystemComponent;
class UTextBlock;

// 디버깅용 UI
UCLASS()
class PROJECTM_API UNUProgressionDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeProgression(UNUProgressionComponent* InProgression);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleProgressionChanged();

	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void RequestRefresh();
	void RefreshText();
	void BindAttributeDelegates();
	void UnbindAttributeDelegates();
	static TArray<FGameplayAttribute> GetWatchedAttributes();

	TWeakObjectPtr<UNUProgressionComponent> Progression;
	TWeakObjectPtr<UNUAbilitySystemComponent> ASC;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatsText;

	bool bRefreshQueued = false;
};
