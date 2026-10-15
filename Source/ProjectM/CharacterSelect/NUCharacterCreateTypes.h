// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NUCharacterCreateTypes.generated.h"

class UPaperFlipbook;
class UPaperZDAnimationSkin;
class UPaperZDAnimSequence;
class UTexture2D;

// 생성 시 고르는 성별. 서버는 CharacterCreate의 gender를 저장·에코만 하므로 값의 뜻은 클라가 정한다.
// 남성이 0인 이유: 성별을 보내기 전(8/26 이전)에 만든 캐릭터는 서버에 0으로 남아 있는데, 그것이 여성으로 읽히면
// 여자 몸이 붙어 버린다. 0 = 남성(예전 몸)으로 두면 "여성을 골라 만든 캐릭터"만 1이 된다
UENUM(BlueprintType)
enum class ENUCharacterGender : uint8
{
	Male,
	Female
};

// 종족 한 줄. RowName이 종족 id (아이템 테이블과 같은 규칙).
// 기획서에 특성이 적힌 건 4종인데 세계관에는 6종이 나온다 --- 행만 추가하면 늘어나게 두었다
USTRUCT(BlueprintType)
struct FNURaceRow : public FTableRowBase
{
	GENERATED_BODY()

	// 서버 race_id로 나가는 숫자. 서버는 해석하지 않으므로 클라가 정한 값이 곧 정답
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Race")
	int32 RaceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Race")
	FText DisplayName;

	// 생성창 종족 칸에 붙는 설명문 (레퍼런스의 국적·신수 칸과 같은 자리)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Race", meta = (MultiLine = true))
	FText Description;

	// 종족 문양 (생성창 100px 칸). 비워 두면 빈 네모로 나온다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Race")
	TSoftObjectPtr<UTexture2D> Emblem;

	// 목록에 그릴 순서. 같으면 RowName 순
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Race")
	int32 SortOrder = 0;

	// 고를 수 있는 종족인지. 베이스 바디가 없는 종족은 끄고 "(준비 중)"으로 보여만 준다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Race")
	bool bPlayable = true;
};

// 외형 파츠 한 줄. 머리·외모·피부가 같은 형식을 쓰고 표만 따로 준다.
//
// **순서가 아니라 PartId로 서버와 주고받는다.** 목록 몇 번째냐로 저장하면
// 표 중간에 한 줄만 끼워 넣어도 이미 만든 캐릭터들의 외형이 전부 밀린다.
// 종족이 RaceId를 쓰는 것과 같은 이유다
USTRUCT(BlueprintType)
struct FNUAppearancePartRow : public FTableRowBase
{
	GENERATED_BODY()

	// 서버로 나가는 번호 (hair_id / face_id / skin_id). 한 번 정하면 바꾸지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	int32 PartId = 0;

	// 고르는 칸에 그릴 아이콘. 없으면 빈 네모로 나온다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	TSoftObjectPtr<UTexture2D> Icon;

	// 생성창 스탠드·선택창 칸에 겹쳐 그릴 대기 그림
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	TSoftObjectPtr<UPaperFlipbook> Flipbook;

	// 선택창에서 골라 둔 칸의 걷기 (몸 줄의 WalkFlipbook과 짝). 비우면 Flipbook 그대로.
	// 새 남캐는 파츠가 프레임마다 움직이고 걷기 캔버스(46x95)가 대기(38x96)와 달라, 걷는 몸에 대기 그림을 얹으면 어긋난다 (9/11)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	TSoftObjectPtr<UPaperFlipbook> WalkFlipbook;

	// 필드 캐릭터에 입힐 스킨. 필드는 4방향으로 움직이므로 플립북 한 장으로는 안 되고
	// 장비와 같은 PaperZD 스킨이어야 한다 (NUSpriteVisualComponent가 레이어에 입힌다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	TSoftObjectPtr<UPaperZDAnimationSkin> FieldSkin;

	// 그림 대신 색으로 표현하는 파츠(피부색)용. 흰색이면 색을 건드리지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	FLinearColor Tint = FLinearColor::White;

	// 이 파츠를 쓸 수 있는 종족. 0이면 모든 종족 공용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	int32 RaceId = 0;

	// 이 파츠를 쓸 수 있는 성별 (ENUCharacterGender 값, 남성 0 / 여성 1). -1이면 공용.
	// 파츠 그림은 몸에 맞춰 그리므로 몸이 갈리면 파츠도 갈린다 --- 남캐용 머리를 여캐 몸에 얹지 않게 한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	int32 Gender = -1;

	// 이 파츠를 쓸 수 있는 직업 (직업 표의 JobId). 0이면 모든 직업 공용.
	// 여캐 직업 그림은 머리·얼굴이 직업마다 따로 그려져 있다 (몸에 가려진 자리와 대기 때 까딱이는 박자가 직업마다 다르다) --- 그 직업일 때만 얹는다.
	// 직업이 없는 몸(생성창, 머리가 몸에 그려진 여자 기본 몸)에는 얹히지 않는다 (10/10)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	int32 JobId = 0;

	// 이 종족·성별·직업 몸에 얹어도 되는 파츠인가
	bool FitsBody(int32 InRaceId, int32 InGender, int32 InJobId) const
	{
		return (RaceId == 0 || RaceId == InRaceId) && (Gender < 0 || Gender == InGender) && (JobId == 0 || JobId == InJobId);
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	int32 SortOrder = 0;
};

// 생성창에서 고른 것 전부. 지금 서버로 가는 건 Name뿐이고 나머지는 필드가 생기면 얹는다
USTRUCT(BlueprintType)
struct FNUCharacterCreateParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	FName RaceRowName;

	// 서버 race_id로 나가는 숫자 (종족 표의 RaceId). 서버는 저장·에코만 한다
	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 RaceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	ENUCharacterGender Gender = ENUCharacterGender::Female;

	// 외형 파츠 번호 --- 파츠 표의 PartId다. 목록 순서가 아니다
	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 HairId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 FaceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 SkinId = 0;
};

// 이름 검증 결과. 기획서 4장의 에러 코드와 1:1로 맞춘다.
// Duplicate/Banned/Reserved는 클라가 판정하지 못하고 서버 응답으로만 온다
UENUM(BlueprintType)
enum class ENUNameCheck : uint8
{
	Ok,
	TooShort,	// E_NAME_SHORT
	TooLong,	// E_NAME_LONG
	InvalidChar,	// E_NAME_CHAR --- 공백·특수문자·이모지
	HasJamo,	// E_NAME_JAMO --- 완성되지 않은 글자가 섞여 있음 (ㄱㄱ, ㅏㅓ)
	DigitsOnly,	// 숫자만 (서버는 1201로 뭉뚱그리지만 문구는 갈라 준다)
	Duplicate,	// E_NAME_DUP --- 서버 1202
	Banned,		// E_NAME_BAN --- 서버가 금칙어 코드를 주면 여기로
	Reserved	// E_NAME_RESERVED --- 삭제 보호 기간(3일). 서버가 별도 코드를 주면 여기로
};

// 종족 표가 붙기 전까지 쓰는 임시 목록의 한 줄.
// 생성창과 선택창이 같은 종족 이름을 써야 해서 한 곳에 둔다
struct FNURaceFallback
{
	FName RowName;
	int32 RaceId = 0;
	const TCHAR* DisplayName = nullptr;
	const TCHAR* Description = nullptr;
	const TCHAR* EmblemPath = nullptr;
	bool bPlayable = true; // 베이스 바디가 없는 종족은 끄고 "(준비 중)"으로 보여만 준다
};

namespace NURace
{
	// 임시 목록 (인간1 엘프2 드워프3 수인4). 종족 표(DT)가 생기면 생성창은 표를 쓰고
	// 이 목록은 표가 없을 때의 폴백으로만 남는다
	PROJECTM_API const TArray<FNURaceFallback>& GetFallbacks();

	// race_id => 표시 이름. 모르는 값이면 빈 문자열 (슬롯에 종족 줄을 아예 안 그리게)
	PROJECTM_API FText GetDisplayName(int32 RaceId);
}

// 몸 한 벌 --- 종족·성별 조합마다 한 줄 (프로젝트 세팅 NU Appearance Data의 Bodies).
// 파츠(머리·외모)와 달리 서버 번호가 아니라 조합 키로 찾는다. 0/-1은 "전체"라 공용 줄 하나로 시작할 수 있다
USTRUCT(BlueprintType)
struct FNUBodyRow
{
	GENERATED_BODY()

	// 종족 표의 RaceId. 0이면 모든 종족
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	int32 RaceId = 0;

	// ENUCharacterGender 값 (남성 0, 여성 1). -1이면 모든 성별 --- 남성이 0이라 0을 전체로 쓸 수 없다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	int32 Gender = -1;

	// 직업 표의 JobId. 0이면 모든 직업. 직업을 고르면 몸통이 그 직업 몸으로 바뀐다 (9/12).
	// 얼굴·머리는 파츠 표가 정한다 --- 공용 파츠면 그대로, 직업 전용 파츠(FNUAppearancePartRow::JobId)가 있으면 같이 바뀐다.
	// 직업 전용 파츠 줄을 두는 직업은 몸 줄도 여기 있어야 한다 --- 없으면 기본 몸 위에 그 파츠가 얹힌다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	int32 JobId = 0;

	// 필드·미리보기 몸 (레이어 0). 애님 시퀀스(Idle/Walk x 4방향)를 전부 덮어야 원래 몸이 새지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	TSoftObjectPtr<UPaperZDAnimationSkin> FieldSkin;

	// 선택창 칸·생성창 스탠드의 정면 대기. 비우면 각 화면에 지정된 공용 외형을 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	TSoftObjectPtr<UPaperFlipbook> IdleFlipbook;

	// 선택창에서 골라 둔 칸의 걷기. 비우면 IdleFlipbook 그대로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	TSoftObjectPtr<UPaperFlipbook> WalkFlipbook;

	// 뒷모습에서 머리·외모 파츠(레이어 6·7)를 위아래로 미는 값 (언리얼 단위 = 도트, 음수가 아래).
	// 파츠는 몸과 같은 캔버스라 원점만 맞으면 되는데, 이 몸의 뒷모습 그림만 목 라인이 공용 파츠
	// 기준(노비스 back 목 y33)보다 낮을 때 쓰는 임시 보정 --- 시트가 기준에 맞으면 0으로 되돌린다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	float BackViewPartTuneZ = 0.0f;

	// 선택창·HUD 초상(UI 그림)에서만 머리·얼굴을 이만큼 올린다 (그림 칸, 음수면 내림) --- 필드는 그대로.
	// 직업 몸은 공용 머리가 몸 맨 윗줄에 딱 붙어 크게 그리면 떠 보인다 (나이아 나이트는 깃이 높아 머리가 이미 묻혀 0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance")
	float PortraitPartTuneZ = 0.0f;

	// 전투 아바타의 몸·머리·얼굴 (레이어 0·6·7). 전투 그림은 머리·얼굴이 직업마다 프레임별로 그려져 있어
	// 파츠 표가 아니라 몸 줄에 둔다. 고른 머리 모양은 CombatHairSkinsByPart 로 따라가고, 거기 없는 머리는 CombatHairSkin 으로 선다.
	// 몸 칸이 비면 전투에서도 필드 모습(뒷모습)으로 선다 (전투 그림이 없는 몸 · 여캐)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance|Combat")
	TSoftObjectPtr<UPaperZDAnimationSkin> CombatBodySkin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance|Combat")
	TSoftObjectPtr<UPaperZDAnimationSkin> CombatHairSkin;

	// 머리 파츠 번호(FNUAppearancePartRow::PartId, 색 번호를 뗀 값) => 그 머리의 전투 머리 스킨.
	// 전투 그림이 없는 머리 번호는 넣지 않는다 --- CombatHairSkin 으로 선다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance|Combat")
	TMap<int32, TSoftObjectPtr<UPaperZDAnimationSkin>> CombatHairSkinsByPart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance|Combat")
	TSoftObjectPtr<UPaperZDAnimationSkin> CombatFaceSkin;

	// 전투 공격 모션. 직업마다 길이와 타격 프레임(Hit·Cast 표시)이 달라 직업별 시퀀스를 둔다.
	// 비면 공용 PZA_Player_Combat_Attack --- 노비스 길이(0.8초)에서 끝나서 더 긴 직업 그림은 끝이 잘린다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Appearance|Combat")
	TSoftObjectPtr<UPaperZDAnimSequence> CombatAttackSequence;

	// 이 머리 파츠 번호의 전투 머리 스킨, 표에 없으면 CombatHairSkin
	const TSoftObjectPtr<UPaperZDAnimationSkin>& FindCombatHairSkin(int32 HairPartId) const
	{
		const TSoftObjectPtr<UPaperZDAnimationSkin>* Skin = CombatHairSkinsByPart.Find(HairPartId);
		return Skin != nullptr && !Skin->IsNull() ? *Skin : CombatHairSkin;
	}
};

namespace NUAppearance
{
	// PartId로 파츠 한 줄을 찾는다. 그 종족·성별·직업 몸에 맞는 파츠만 본다 (다른 몸용 번호가 와도 그 파츠는 그리지 않는다).
	// 번호가 맞는 파츠가 없으면 그 몸의 첫 파츠(SortOrder => PartId 순)를 돌려준다 --- 새 남캐 몸은 머리가 파츠에 있어
	// 번호가 없는 캐릭터(0)나 표에서 빠진 번호를 비워 두면 머리 없이 그려진다 (9/11). 표가 없거나 맞는 파츠가 하나도 없으면 nullptr
	// 생성창은 목록을 통째로 들고 있지만 필드는 번호 하나로 바로 찾아야 해서 여기 둔다
	PROJECTM_API const FNUAppearancePartRow* FindPart(const TSoftObjectPtr<UDataTable>& Table, int32 PartId, int32 RaceId, int32 Gender, int32 JobId);

	// 종족·성별·직업에 맞는 몸 줄. 직업 줄(JobId != 0)은 그 직업일 때만 후보이고 맞으면 맨 앞 --- 직업이 없으면(0) 기본 몸.
	// 그다음 딱 맞는 줄 > 종족만 맞는 줄 > 성별만 맞는 줄 > 전체 공용 줄. 없으면 nullptr(원래 몸)
	PROJECTM_API const FNUBodyRow* FindBody(int32 RaceId, int32 Gender, int32 JobId);

	// 생성창에서 이 성별을 고를 수 있는가 --- 그 성별 몸(Gender가 정확히 맞고 FieldSkin이 있는 줄)이 표에 있어야 열린다.
	// 종족의 bPlayable과 같은 뜻이다 (몸이 없는 성별은 "(준비 중)"). 성별을 따로 적은 줄이 하나도 없으면 전부 연다
	PROJECTM_API bool IsGenderOpen(int32 RaceId, int32 Gender);

	// 머리 색 (10/13). 서버에 칸이 따로 없어(hair_id는 서버가 저장·에코만 한다) 한 숫자에 같이 싣는다:
	//   hair_id = 머리 파츠 번호(PartId) + 색 번호 * HairColorStride
	// 색 번호 0은 "그림 원래 색"이라 색을 고르기 전에 만든 캐릭터(hair_id = PartId)가 그대로 읽힌다.
	// 그래서 머리 파츠의 PartId는 HairColorStride보다 작아야 한다. hair_id를 받아 파츠를 찾는 곳은 먼저 SplitHairId로 갈라야 한다.
	// 서버에 hair_color 칸이 생기면 이 두 함수만 걷어내면 된다
	constexpr int32 HairColorStride = 1000;
	PROJECTM_API int32 PackHairId(int32 PartId, int32 ColorIndex);
	PROJECTM_API void SplitHairId(int32 HairId, int32& OutPartId, int32& OutColorIndex);

	// 색 번호 => 물들일 색 (프로젝트 세팅 NU Appearance Data의 HairColors). 0번이거나 견본에 없는 번호면 false --- 물들이지 않는다
	PROJECTM_API bool GetHairTint(int32 ColorIndex, FLinearColor& OutTint);
}

namespace NUCharacterName
{
	// 8/26 회의: "한글 8자, 영어 14자 / 혼용 허용, 합산 시 한글 2바이트 카운팅".
	// 상한이 두 개라 한 가지 잣대로는 둘 다 못 맞춘다 --- 글자 수와 가중치를 따로 본다.
	//   글자 수 <= 14 이면서 가중치 합 <= 16
	// 이러면 한글 8자(8글자 16몫)와 영문 14자(14글자 14몫)가 각각 경계가 되고,
	// 한글 9자는 가중치 18로, 영문 15자는 글자 수 15로 걸린다
	constexpr int32 MinLength = 2;
	constexpr int32 MaxLength = 14;
	constexpr int32 MaxWeight = 16;

	// 한글 한 글자가 차지하는 몫 (회의: "한글 2바이트 카운팅"). 영문·숫자는 1
	constexpr int32 HangulWeight = 2;

	// 앞뒤 공백을 떼어낸다 (기획서: "앞뒤 공백은 입력 시 자동 trim")
	PROJECTM_API FString Trim(const FString& Raw);

	// 혼용 이름의 가중치 합 (한글 2, 그 밖에 1). Check가 MaxWeight와 견주는 값이다
	PROJECTM_API int32 GetWeight(const FString& TrimmedName);

	// 클라가 판정할 수 있는 것만 본다 --- 중복·금칙어·보호 기간은 서버 몫이라 여기서 Ok가 나와도
	// 생성 요청은 거절당할 수 있다. Trim은 호출부에서 먼저 해 둘 것
	PROJECTM_API ENUNameCheck Check(const FString& TrimmedName);

	// 화면에 띄울 문구 (기획서 4장 노출 문구 그대로)
	PROJECTM_API FText GetMessage(ENUNameCheck Result);

	// 허용 길이(글자 수 MaxLength, 가중치 MaxWeight)를 넘는 이름을 그 안으로 자르고 끝에 "…"를 붙인다 ("…"도 한 칸).
	// 허용 안이면 그대로. 서버 없이 시험할 때의 계정 ID처럼 규칙을 거치지 않은 이름을 HUD에 띄울 때 쓴다
	PROJECTM_API FString Shorten(const FString& Name);

	// 서버 결과 코드를 검증 결과로 옮긴다 (1201 규칙 위반 / 1202 중복).
	// 금칙어·보호 기간 코드는 서버에 요청해 둔 상태라 아직 매핑할 값이 없다
	PROJECTM_API ENUNameCheck FromResultCode(int32 ResultCode);
}
