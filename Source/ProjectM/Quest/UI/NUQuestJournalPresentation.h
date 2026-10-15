#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "String/LexFromString.h"
#include "NUGAS/NUGameplayTags.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"

namespace NUQuestJournalPresentation
{

// A quest's authored location is distinct from the player's current location.
// Missing/non-numeric legacy IDs have no display label; never expose a raw ID.
inline FText GetLocationText(const UWorld* World, const UNUQuestDefinition* Definition)
{
	int32 LocationId = 0;
	if (!World || !World->GetGameInstance() || !Definition
		|| !LexTryParseString(LocationId, *Definition->LocationId.ToString()) || LocationId <= 0)
	{
		return FText::GetEmpty();
	}
	const UNUWorldDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UNUWorldDataSubsystem>();
	const FNULocationDefinition* Location = Data ? Data->FindLocationDefinition(LocationId) : nullptr;
	return Location ? Location->DisplayName : FText::GetEmpty();
}

// Short category label for list cards ("메인", "서브" ...); sub-tags use their parent's label.
inline FText GetCategoryShortText(FGameplayTag CategoryTag)
{
	static const TMap<FGameplayTag, FText> Labels = {
		{ NU::Quest::Category::Main, NSLOCTEXT("NUQuestJournal", "CategoryShortMain", "메인") },
		{ NU::Quest::Category::Side, NSLOCTEXT("NUQuestJournal", "CategoryShortSide", "서브") },
		{ NU::Quest::Category::Daily, NSLOCTEXT("NUQuestJournal", "CategoryShortDaily", "일일") },
		{ NU::Quest::Category::Tutorial, NSLOCTEXT("NUQuestJournal", "CategoryShortTutorial", "튜토리얼") },
		{ NU::Quest::Category::Job, NSLOCTEXT("NUQuestJournal", "CategoryShortJob", "직업") },
		{ NU::Quest::Category::Craft, NSLOCTEXT("NUQuestJournal", "CategoryShortCraft", "생활") },
		{ NU::Quest::Category::Hidden, NSLOCTEXT("NUQuestJournal", "CategoryShortHidden", "히든") },
	};
	for (FGameplayTag Current = CategoryTag; Current.IsValid(); Current = Current.RequestDirectParent())
	{
		if (const FText* Found = Labels.Find(Current))
		{
			return *Found;
		}
	}
	return FText::GetEmpty();
}

}
