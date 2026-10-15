// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterSelect/NUCharacterCreateTypes.h"

#include "CharacterSelect/Data/NUAppearanceDataSettings.h"

namespace
{
	// 한글 완성형 (가 ~ 힣). 자모가 조합된 글자만 여기 들어온다
	FORCEINLINE bool IsHangulSyllable(TCHAR C)
	{
		return C >= 0xAC00 && C <= 0xD7A3;
	}

	// 홀로 쓰인 자모. 호환 자모(ㄱ~ㅣ)가 대부분이고, 조합용 자모는 IME가 흘리는 경우가 있다.
	// 완성형과 갈라서 봐야 "완성되지 않은 글자" 안내가 나온다
	FORCEINLINE bool IsHangulJamo(TCHAR C)
	{
		return (C >= 0x1100 && C <= 0x11FF)  // 조합용 자모
			|| (C >= 0x3130 && C <= 0x318F)  // 호환 자모 (ㄱ ㄴ ㅏ ...)
			|| (C >= 0xA960 && C <= 0xA97F)  // 확장 A
			|| (C >= 0xD7B0 && C <= 0xD7FF); // 확장 B
	}

	FORCEINLINE bool IsAsciiLetter(TCHAR C)
	{
		return (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z');
	}

	FORCEINLINE bool IsAsciiDigit(TCHAR C)
	{
		return C >= '0' && C <= '9';
	}
}

namespace NURace
{
	const TArray<FNURaceFallback>& GetFallbacks()
	{
		// 문구는 기획서(메인 기획서 > 종족 기획)의 소제목 그대로다.
		// 세계관에는 데모니안·뱀파이어까지 여섯인데 특성이 적힌 건 이 넷뿐이라 확인 대기 중
		// 베이스 바디가 인간 것만 나와 있어 나머지는 잠가 둔다 (몸이 오면 true로)
		static const TArray<FNURaceFallback> Fallbacks =
		{
			{ FName("Human"),     1, TEXT("인간"),
				TEXT("결핍이 낳은, 가장 다양한 얼굴들.\n마법과 기계를 융합해 살아남은 종족입니다."),
				TEXT("/Game/Developers/dbals/UI/RaceEmblems/T_RaceEmblem_Human.T_RaceEmblem_Human"), true },
			{ FName("Elf"),       2, TEXT("엘프"),
				TEXT("고대의 자연 숭배자.\n스스로를 첫 뿌리의 후예라 부릅니다."),
				TEXT("/Game/Developers/dbals/UI/RaceEmblems/T_RaceEmblem_Elf.T_RaceEmblem_Elf"), false },
			{ FName("Dwarf"),     3, TEXT("드워프"),
				TEXT("어긋난 장인 정신을 품은 장인들."),
				TEXT("/Game/Developers/dbals/UI/RaceEmblems/T_RaceEmblem_Dwarf.T_RaceEmblem_Dwarf"), false },
			{ FName("Beastfolk"), 4, TEXT("수인"),
				TEXT("사냥터에 남은 마지막 야생."),
				TEXT("/Game/Developers/dbals/UI/RaceEmblems/T_RaceEmblem_Beastfolk.T_RaceEmblem_Beastfolk"), false },
		};
		return Fallbacks;
	}

	FText GetDisplayName(int32 RaceId)
	{
		// 0은 "안 고름"이다 --- 외형 필드가 생기기 전에 만든 캐릭터가 여기 해당한다
		if (RaceId <= 0)
		{
			return FText::GetEmpty();
		}

		const TArray<FNURaceFallback>& Fallbacks = GetFallbacks();
		const FNURaceFallback* Found = Fallbacks.FindByPredicate(
			[RaceId](const FNURaceFallback& Entry) { return Entry.RaceId == RaceId; });
		return Found != nullptr ? FText::FromString(Found->DisplayName) : FText::GetEmpty();
	}
}

namespace NUAppearance
{
	const FNUAppearancePartRow* FindPart(const TSoftObjectPtr<UDataTable>& Table, int32 PartId, int32 RaceId, int32 Gender, int32 JobId)
	{
		const UDataTable* Loaded = Table.LoadSynchronous();
		if (Loaded == nullptr)
		{
			return nullptr;
		}

		const FNUAppearancePartRow* Exact = nullptr;
		const FNUAppearancePartRow* First = nullptr; // 생성창 목록과 같은 순서의 맨 앞
		Loaded->ForeachRow<FNUAppearancePartRow>(TEXT("NUAppearance"),
			[&Exact, &First, PartId, RaceId, Gender, JobId](const FName& RowName, const FNUAppearancePartRow& Row)
			{
				if (!Row.FitsBody(RaceId, Gender, JobId))
				{
					return;
				}
				if (Exact == nullptr && Row.PartId == PartId)
				{
					Exact = &Row;
				}
				if (First == nullptr || Row.SortOrder < First->SortOrder
					|| (Row.SortOrder == First->SortOrder && Row.PartId < First->PartId))
				{
					First = &Row;
				}
			});
		return Exact != nullptr ? Exact : First;
	}

	int32 PackHairId(int32 PartId, int32 ColorIndex)
	{
		return FMath::Max(PartId, 0) + FMath::Max(ColorIndex, 0) * HairColorStride;
	}

	void SplitHairId(int32 HairId, int32& OutPartId, int32& OutColorIndex)
	{
		// 음수(INDEX_NONE = "아무 파츠나")는 번호가 아니라 그대로 둔다
		OutPartId = HairId < 0 ? HairId : HairId % HairColorStride;
		OutColorIndex = HairId < 0 ? 0 : HairId / HairColorStride;
	}

	bool GetHairTint(int32 ColorIndex, FLinearColor& OutTint)
	{
		const TArray<FLinearColor>& Colors = GetDefault<UNUAppearanceDataSettings>()->HairColors;
		if (ColorIndex <= 0 || !Colors.IsValidIndex(ColorIndex))
		{
			return false;
		}
		OutTint = Colors[ColorIndex];
		OutTint.A = 1.0f;
		return true;
	}

	const FNUBodyRow* FindBody(int32 RaceId, int32 Gender, int32 JobId)
	{
		// 맞는 항목 수가 점수 --- 직업 4점, 종족 2점, 성별 1점. 같은 점수면 먼저 적힌 줄 (전체 공용 줄은 0점이라 늘 마지막 순위).
		// 직업 줄은 그 직업일 때만 후보다 --- 직업이 없거나 다른 직업이면 기본 몸으로 간다
		const FNUBodyRow* Best = nullptr;
		int32 BestScore = -1;
		for (const FNUBodyRow& Row : GetDefault<UNUAppearanceDataSettings>()->Bodies)
		{
			const bool bRaceOk = Row.RaceId == 0 || Row.RaceId == RaceId;
			const bool bGenderOk = Row.Gender < 0 || Row.Gender == Gender;
			const bool bJobOk = Row.JobId == 0 || Row.JobId == JobId;
			if (!bRaceOk || !bGenderOk || !bJobOk)
			{
				continue;
			}
			const int32 Score = (Row.JobId != 0 && Row.JobId == JobId ? 4 : 0)
				+ (Row.RaceId == RaceId ? 2 : 0) + (Row.Gender == Gender ? 1 : 0);
			if (Score > BestScore)
			{
				Best = &Row;
				BestScore = Score;
			}
		}
		return Best;
	}

	bool IsGenderOpen(int32 RaceId, int32 Gender)
	{
		bool bAnyGenderRow = false;
		for (const FNUBodyRow& Row : GetDefault<UNUAppearanceDataSettings>()->Bodies)
		{
			if (Row.Gender < 0 || Row.JobId != 0 || Row.FieldSkin.IsNull())
			{
				continue; // 직업 몸은 기본 몸 위에 얹는 것이라 열림 여부는 기본 몸으로만 본다
			}
			bAnyGenderRow = true;
			if (Row.Gender == Gender && (Row.RaceId == 0 || Row.RaceId == RaceId))
			{
				return true;
			}
		}
		return !bAnyGenderRow; // 성별 몸이 하나도 없는 단계(공용 몸뿐)에서는 잠그지 않는다
	}
}

namespace NUCharacterName
{
	FString Trim(const FString& Raw)
	{
		FString Result = Raw;
		Result.TrimStartAndEndInline();
		return Result;
	}

	int32 GetWeight(const FString& TrimmedName)
	{
		int32 Weight = 0;
		for (const TCHAR C : TrimmedName)
		{
			Weight += IsHangulSyllable(C) ? HangulWeight : 1;
		}
		return Weight;
	}

	ENUNameCheck Check(const FString& TrimmedName)
	{
		// 길이보다 문자 종류를 먼저 본다 --- "ㄱ"은 짧기도 하지만 자모라는 것이 더 정확한 안내다.
		// 이모지는 서로게이트 페어라 어느 쪽 코드 유닛도 허용 범위 밖이어서 여기서 걸린다
		bool bHasJamo = false;
		bool bHasLetter = false; // 한글 완성형 또는 영문

		for (const TCHAR C : TrimmedName)
		{
			if (IsHangulSyllable(C) || IsAsciiLetter(C))
			{
				bHasLetter = true;
				continue;
			}
			if (IsAsciiDigit(C))
			{
				continue;
			}
			if (IsHangulJamo(C))
			{
				bHasJamo = true;
				continue;
			}
			// 공백·특수문자·이모지 --- 그 외 전부
			return ENUNameCheck::InvalidChar;
		}

		if (bHasJamo)
		{
			return ENUNameCheck::HasJamo;
		}

		const int32 Length = TrimmedName.Len();
		if (Length < MinLength)
		{
			return ENUNameCheck::TooShort;
		}
		if (Length > MaxLength)
		{
			return ENUNameCheck::TooLong;
		}

		// 여기까지 왔으면 허용 문자만 남아 Len()이 곧 글자 수다 (이모지는 위에서 걸렸다).
		// 한글을 2로 세는 합산 상한 --- 한글 8자, 한글 4자+영문 8자 따위가 여기서 갈린다
		if (GetWeight(TrimmedName) > MaxWeight)
		{
			return ENUNameCheck::TooLong;
		}

		// 숫자는 섞어 쓰는 것만 허용한다 (기획서 3.3: "숫자만 입력 시 불가")
		if (!bHasLetter)
		{
			return ENUNameCheck::DigitsOnly;
		}

		return ENUNameCheck::Ok;
	}

	FString Shorten(const FString& Name)
	{
		if (Name.Len() <= MaxLength && GetWeight(Name) <= MaxWeight)
		{
			return Name;
		}
		FString Out;
		int32 Weight = 0;
		for (const TCHAR Char : Name)
		{
			const int32 CharWeight = GetWeight(FString::Chr(Char));
			if (Out.Len() + 1 >= MaxLength || Weight + CharWeight + 1 > MaxWeight)
			{
				break;
			}
			Out.AppendChar(Char);
			Weight += CharWeight;
		}
		return Out + TEXT("…");   // ellipsis
	}

	FText GetMessage(ENUNameCheck Result)
	{
		switch (Result)
		{
		case ENUNameCheck::Ok:
			return FText::GetEmpty();

		case ENUNameCheck::TooShort:
			return NSLOCTEXT("NUCharacter", "NameShort", "이름은 한글 2자 또는 영문 2자 이상이어야 합니다.");

		case ENUNameCheck::TooLong:
			return NSLOCTEXT("NUCharacter", "NameLong", "이름이 너무 깁니다. (한글 8자 / 영문 14자 이내)");

		case ENUNameCheck::InvalidChar:
			return NSLOCTEXT("NUCharacter", "NameChar", "한글 또는 영문만 사용할 수 있습니다.");

		case ENUNameCheck::HasJamo:
			return NSLOCTEXT("NUCharacter", "NameJamo", "완성되지 않은 글자는 사용할 수 없습니다.");

		case ENUNameCheck::DigitsOnly:
			return NSLOCTEXT("NUCharacter", "NameDigits", "숫자만으로는 이름을 지을 수 없습니다.");

		case ENUNameCheck::Duplicate:
			return NSLOCTEXT("NUCharacter", "NameDup", "이미 사용 중인 이름입니다.");

		case ENUNameCheck::Banned:
			return NSLOCTEXT("NUCharacter", "NameBan", "사용할 수 없는 단어가 포함되어 있습니다.");

		case ENUNameCheck::Reserved:
			// 삭제 보호 기간이라는 내부 사정은 일부러 감춘다 (기획서 4장 --- 이름 선점 어뷰징 방지)
			return NSLOCTEXT("NUCharacter", "NameReserved", "현재 사용할 수 없는 이름입니다. 잠시 후 다시 시도해 주세요.");

		default:
			return NSLOCTEXT("NUCharacter", "NameUnknown", "사용할 수 없는 이름입니다.");
		}
	}

	ENUNameCheck FromResultCode(int32 ResultCode)
	{
		switch (ResultCode)
		{
		case 200:  return ENUNameCheck::Ok;
		case 1201: return ENUNameCheck::InvalidChar; // 서버는 길이·문자를 1201 하나로 뭉뚱그린다
		case 1202: return ENUNameCheck::Duplicate;
		case 1210: return ENUNameCheck::Banned;      // CHARACTER_NAME_FORBIDDEN
		case 1211: return ENUNameCheck::Reserved;    // CHARACTER_NAME_RESERVED (삭제 보호 3일)
		default:   return ENUNameCheck::Banned;      // 미지의 거절은 일단 "쓸 수 없는 이름"으로 안내
		}
	}
}
