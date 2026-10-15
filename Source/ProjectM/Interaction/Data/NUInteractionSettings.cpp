#include "Interaction/Data/NUInteractionSettings.h"

#include "Quest/Data/NUQuestTypes.h"

#define LOCTEXT_NAMESPACE "NUInteractionSettings"

UNUInteractionSettings::UNUInteractionSettings()
{
	DefaultDialogueOptionText = LOCTEXT("DefaultDialogueOption", "대화하기");
	DialogueCategoryText = LOCTEXT("DialogueCategory", "대화");
	QuestCategoryText = NUQuest::GetStateDisplayText(ENUQuestState::Available);
	QuestInProgressCategoryText = NUQuest::GetStateDisplayText(ENUQuestState::Active);
	QuestTurnInCategoryText = NUQuest::GetStateDisplayText(ENUQuestState::ReadyToTurnIn);
	QuestTargetCategoryText = NUQuest::GetStateDisplayText(ENUQuestState::Active);
	JobSelectionOptionText = LOCTEXT("JobSelectionOption", "직업 선택");
	JobCategoryText = LOCTEXT("JobCategory", "전직");
	StarterNaiaContractOptionText = LOCTEXT("StarterNaiaContractOption", "나이아 계약");
	NaiaContractCategoryText = LOCTEXT("NaiaContractCategory", "계약");
	ShopOptionText = LOCTEXT("ShopOption", "상품 보기");
	ShopCategoryText = LOCTEXT("ShopCategory", "상점");
	StorageOptionText = LOCTEXT("StorageOption", "창고 이용");
	StorageCategoryText = LOCTEXT("StorageCategory", "창고");
	InstantRecoveryOptionText = LOCTEXT("InstantRecoveryOption", "즉시 회복");
	PaidRecoveryOptionFormat = LOCTEXT("PaidRecoveryOptionFormat", "즉시 회복 {0}골드");
	RestOptionText = LOCTEXT("RestOption", "무료 휴식");
	RecoveryCategoryText = LOCTEXT("RecoveryCategory", "회복");
	ExitOptionText = LOCTEXT("ExitOption", "나가기");
	ExitOptionCategoryText = LOCTEXT("ExitOptionCategory", "종료");
}

#undef LOCTEXT_NAMESPACE
