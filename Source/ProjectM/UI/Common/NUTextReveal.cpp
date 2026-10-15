#include "UI/Common/NUTextReveal.h"

#include "Components/TextBlock.h"
#include "Internationalization/BreakIterator.h"

void FNUTextReveal::SetText(const FText& Text)
{
	FullText = Text;
	FullString = Text.ToString();
	CharacterBoundaries.Reset();
	VisibleCharacterCount = INDEX_NONE;

	// 결합 문자와 서로게이트 쌍을 중간에서 자르지 않는다.
	const TSharedRef<IBreakIterator> Iterator = FBreakIterator::CreateCharacterBoundaryIterator();
	Iterator->SetString(FullString);
	Iterator->ResetToBeginning();
	for (int32 Boundary = Iterator->MoveToNext(); Boundary != INDEX_NONE; Boundary = Iterator->MoveToNext())
	{
		CharacterBoundaries.Add(Boundary);
	}
}

void FNUTextReveal::Reset()
{
	FullText = FText::GetEmpty();
	FullString.Reset();
	CharacterBoundaries.Reset();
	VisibleCharacterCount = INDEX_NONE;
}

void FNUTextReveal::ApplyTo(UTextBlock& TextBlock, int32 CharacterCount)
{
	const int32 Count = FMath::Clamp(CharacterCount, 0, CharacterBoundaries.Num());
	if (Count == VisibleCharacterCount)
	{
		return;
	}
	VisibleCharacterCount = Count;
	TextBlock.SetText(Count == CharacterBoundaries.Num() ? FullText
		: FText::FromString(Count > 0 ? FullString.Left(CharacterBoundaries[Count - 1]) : FString()));
}
