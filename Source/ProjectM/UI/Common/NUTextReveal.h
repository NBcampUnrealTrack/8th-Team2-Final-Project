#pragma once

#include "CoreMinimal.h"

class UTextBlock;

// 표시할 글자 수만 받아 문장을 갱신한다. 재생 시간은 호출자가 관리한다.
class PROJECTM_API FNUTextReveal
{
public:
	void SetText(const FText& Text);
	void Reset();
	void ApplyTo(UTextBlock& TextBlock, int32 CharacterCount);
	int32 GetCharacterCount() const { return CharacterBoundaries.Num(); }
	bool IsRevealing() const { return !CharacterBoundaries.IsEmpty() && VisibleCharacterCount < CharacterBoundaries.Num(); }

private:
	FText FullText;
	FString FullString;
	TArray<int32> CharacterBoundaries;
	int32 VisibleCharacterCount = INDEX_NONE;
};
