#include "Quest/UI/NUQuestJournalListItem.h"

void UNUQuestJournalListItem::Initialize(FName InQuestId, const FText& InTitle, FGameplayTag InCategoryTag, ENUQuestState InQuestState, int32 InSortOrder)
{
	QuestId = InQuestId;
	Title = InTitle;
	CategoryTag = InCategoryTag;
	QuestState = InQuestState;
	SortOrder = InSortOrder;

	StatusText = NUQuest::GetStateDisplayText(QuestState);
}
