#pragma once

#include "CoreMinimal.h"

// 이름 뒤 조사를 받침에 맞춰 고른다 --- "아쿠퐁과 함께하기", "불코이와 함께하기", "트래커로 정하기".
// 한글이 아닌 글자로 끝나면 받침 없는 쪽(와 / 로)을 쓴다
namespace NUKoreanText
{
	// 마지막 글자의 받침 번호 (0 = 없음, 8 = ㄹ). 한글 음절이 아니면 0
	inline int32 FinalConsonant(const FString& Word)
	{
		const FString Trimmed = Word.TrimEnd();
		if (Trimmed.IsEmpty())
		{
			return 0;
		}
		const TCHAR Last = Trimmed[Trimmed.Len() - 1];
		return Last >= 0xAC00 && Last <= 0xD7A3 ? (Last - 0xAC00) % 28 : 0;
	}

	// 와 / 과
	inline FString WaGwa(const FString& Word)
	{
		return Word + (FinalConsonant(Word) != 0 ? TEXT("과") : TEXT("와"));
	}

	// 로 / 으로 (ㄹ 받침은 "로")
	inline FString Ro(const FString& Word)
	{
		const int32 Final = FinalConsonant(Word);
		return Word + (Final != 0 && Final != 8 ? TEXT("으로") : TEXT("로"));
	}
}
