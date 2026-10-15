#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/Data/NUInteractionTypes.h"
#include "NUInteractionOptionEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnNUInteractionOptionEntrySelected,
	FNUInteractionOptionHandle,
	Handle);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnNUInteractionOptionEntryFocused,
	FNUInteractionOptionHandle,
	Handle);

// NPC 대화창 안에서 대화, 퀘스트, 상점 등의 선택지 한 건을 표시합니다.
UCLASS()
class PROJECTM_API UNUInteractionOptionEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NU|Interaction|Option")
	void SetOption(const FNUInteractionOption& InOption);

	UFUNCTION(BlueprintPure, Category = "NU|Interaction|Option")
	FNUInteractionOption GetOption() const { return Option; }

	bool IsOptionEnabled() const { return Option.bEnabled && Option.IsValid(); }
	bool FocusEntry();
	void SetFocused(bool bInFocused);

	UPROPERTY(BlueprintAssignable, Category = "NU|Interaction|Option")
	FOnNUInteractionOptionEntrySelected OnSelected;

	UPROPERTY(BlueprintAssignable, Category = "NU|Interaction|Option")
	FOnNUInteractionOptionEntryFocused OnFocused;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Interaction|Option", meta = (DisplayName = "On Option Changed"))
	void BP_OnOptionChanged(const FNUInteractionOption& InOption);

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Interaction|Option", meta = (DisplayName = "On Focus Changed"))
	void BP_OnFocusChanged(bool bInFocused);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Interaction|Option|Style")
	FLinearColor NormalTextColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Interaction|Option|Style")
	FLinearColor FocusedTextColor = FLinearColor(1.0f, 0.82f, 0.18f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Interaction|Option|Style")
	FLinearColor DisabledTextColor = FLinearColor(0.58f, 0.62f, 0.70f, 1.0f);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> OptionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> OptionText;

	// ---- 모양 선택 (기본값이면 기존 WBP 그대로) ----

	// false면 [진행 가능] 같은 머리말을 빼고, 줄 종류는 GroupIcon 그림으로 보인다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Interaction|Option|Style")
	bool bShowCategoryPrefix = true;

	// 줄 종류별 아이콘 (퀘스트 줄은 머리 위 표시·미니맵과 같은 배지). 표에 없으면 DefaultGroupIcon, 그것도 없으면 아이콘을 접는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Interaction|Option|Style")
	TMap<ENUInteractionOptionGroup, TObjectPtr<UTexture2D>> GroupIcons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Interaction|Option|Style")
	TObjectPtr<UTexture2D> DefaultGroupIcon;

	// 시스템 줄(나가기). System 그룹은 숨김 값이라 GroupIcons 표의 키로 고를 수 없어 따로 둔다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Interaction|Option|Style")
	TObjectPtr<UTexture2D> SystemGroupIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> GroupIcon;

	// 고른 줄(키·패드·마우스 올림)에만 보이는 표시
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> FocusHighlight;

private:
	FText ResolveCategoryText() const;
	FText BuildDisplayText() const;
	void RefreshEntry();

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	FNUInteractionOption Option;
	bool bFocused = false;
};
