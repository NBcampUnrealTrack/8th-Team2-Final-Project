#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NUInteractionSettings.generated.h"

// 상호작용 선택지의 프로젝트 공용 표시값
// 개별 NPC 대사와 선택지 내용은 Dialogue/Quest 데이터가 소유하고, 공용 문구만 여기서 관리한다.
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "NU Interaction"))
class PROJECTM_API UNUInteractionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNUInteractionSettings();
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText DefaultDialogueOptionText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText DialogueCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText QuestCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText QuestInProgressCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText QuestTurnInCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText QuestTargetCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText JobSelectionOptionText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText JobCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText StarterNaiaContractOptionText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText NaiaContractCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText ShopOptionText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText ShopCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText StorageOptionText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText StorageCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText InstantRecoveryOptionText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText RestOptionText;


	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText PaidRecoveryOptionFormat;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText RecoveryCategoryText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText ExitOptionText;

	UPROPERTY(config, EditAnywhere, Category = "NU|Interaction|표시")
	FText ExitOptionCategoryText;

};
