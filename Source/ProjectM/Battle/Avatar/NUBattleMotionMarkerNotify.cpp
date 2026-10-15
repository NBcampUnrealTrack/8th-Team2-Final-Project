// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleMotionMarkerNotify.h"

#include "AnimSequences/PaperZDAnimSequence.h"


FName UNUBattleMotionMarkerNotify::GetDisplayName_Implementation() const
{
	// 타임라인에서 종류가 바로 보이도록
	return Marker == ENUBattleMotionMarker::Cast ? TEXT("Cast") : TEXT("Hit");
}

bool UNUBattleMotionMarkerNotify::FindMarkerTime(const UPaperZDAnimSequence* Sequence,
	ENUBattleMotionMarker InMarker, float& OutTime)
{
	if (!IsValid(Sequence))
	{
		return false;
	}

	bool bFound = false;
	for (const UPaperZDAnimNotify_Base* Notify : Sequence->GetAnimNotifies())
	{
		const UNUBattleMotionMarkerNotify* MarkerNotify = Cast<UNUBattleMotionMarkerNotify>(Notify);
		if (!MarkerNotify || MarkerNotify->Marker != InMarker)
		{
			continue;
		}

		if (!bFound || MarkerNotify->Time < OutTime)
		{
			OutTime = MarkerNotify->Time;
			bFound = true;
		}
	}

	return bFound;
}
