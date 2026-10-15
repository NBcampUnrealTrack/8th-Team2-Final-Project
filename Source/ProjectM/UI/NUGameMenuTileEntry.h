#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "NUGameMenuTileEntry.generated.h"

class UNUGameMenuWidget;

// TileView가 보관하는 표시 데이터. 실제 동작은 메뉴의 레지스트리에서 실행한다.
UCLASS(BlueprintType)
class PROJECTM_API UNUGameMenuTileItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "NU|GameMenu")
	FName EntryId;

	UPROPERTY(BlueprintReadOnly, Category = "NU|GameMenu")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "NU|GameMenu")
	FText Shortcut;

	UPROPERTY(BlueprintReadOnly, Category = "NU|GameMenu")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(Transient)
	TWeakObjectPtr<UNUGameMenuWidget> Menu;
};

// 모양은 전용 WBP가 담당하고, 재사용되는 타일의 데이터와 클릭만 연결한다.
UCLASS(Abstract)
class PROJECTM_API UNUGameMenuTileEntry : public UNU_CommonButtonWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

private:
	UFUNCTION()
	void HandleTileClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HubStatBar1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HubStatBar2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HubStatBar3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HubStatBaseline;
};
