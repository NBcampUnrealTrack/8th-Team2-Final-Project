#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/UI/NUProgressionWindowBase.h"
#include "NUPlayerSkillWindowWidget.generated.h"

class UNUPlayerSkillComponent;
class UTextBlock;
class UVerticalBox;
class UWidget;

// 스킬 보유 목록 표시
USTRUCT(BlueprintType)
struct PROJECTM_API FNUPlayerSkillViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill|UI")
	int32 SkillId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill|UI")
	FName SkillKey;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill|UI")
	FText DisplayName;

	// 액티브/패시브처럼 목록에서 보여 줄 스킬 종류
	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill|UI")
	FText SkillTypeText;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill|UI")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill|UI")
	int32 SPCost = 0;
};

//실제 배치는 WBP에서 구현
UCLASS(Abstract)
class PROJECTM_API UNUPlayerSkillWindowWidget : public UNUProgressionWindowBase
{
	GENERATED_BODY()

public:
	void InitializeSkillWindow(UNUPlayerSkillComponent* InSkillComponent, FSimpleDelegate InCloseRequested);

	UFUNCTION(BlueprintCallable, Category = "NU|Skill|UI")
	void RefreshSkillEntries();

	UFUNCTION(BlueprintPure, Category = "NU|Skill|UI")
	const TArray<FNUPlayerSkillViewData>& GetSkillEntries() const { return SkillEntries; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 기본 구현은 SkillListPanel에 텍스트 목록을 만든다.
	// WBP에서 오버라이드하면 ListView, 카드 등 원하는 표현으로 완전히 교체 가능
	UFUNCTION(BlueprintNativeEvent, Category = "NU|Skill|UI", meta = (DisplayName = "보유 스킬 목록 갱신"))
	void BP_OnSkillEntriesChanged(const TArray<FNUPlayerSkillViewData>& InSkillEntries);
	virtual void BP_OnSkillEntriesChanged_Implementation(const TArray<FNUPlayerSkillViewData>& InSkillEntries);

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill|UI")
	TArray<FNUPlayerSkillViewData> SkillEntries;

	// 기본 목록이 들어갈 자리
	// WBP에서 같은 이름의 VerticalBox를 배치한다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> SkillListPanel;

	// 보유 스킬이 없을 때만 표시하는 안내 문구
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyStateText;

private:
	UWidget* BuildSkillTooltip(const FNUPlayerSkillViewData& Entry) const;

	UFUNCTION()
	void HandleSkillsChanged();

	TWeakObjectPtr<UNUPlayerSkillComponent> SkillComponent;
};
