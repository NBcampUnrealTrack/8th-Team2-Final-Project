// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "NUBattleResultTypes.generated.h"

class UObject;
class UTexture2D;
struct FNUExperienceGrowth;

// 결과창 명단 한 줄, 캐릭터와 나이아가 같은 줄 위젯을 씀
USTRUCT(BlueprintType)
struct PROJECTM_API FNUBattleResultMember
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	int32 Level = 1;

	// 이번 전투로 레벨이 올랐는지, 줄 위에 LEVEL UP 문구를 띄우는 조건
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	bool bLeveledUp = false;

	// 전투 전 레벨, "5 → 6" 표시용. 오르지 않았으면 Level 과 같음
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	int32 PreviousLevel = 1;

	// 나이아 줄이면 나이아 표의 NaiaId, 내 캐릭터 줄이면 0 --- 줄 위젯이 얼굴 그림 · 속성을 찾는 열쇠
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	int32 NaiaId = 0;

	// 서버는 경험치를 캐릭터에게만 준다 - 나이아는 이 값이 거짓이라 바를 비워 그림
	// 나이아 경험치가 생기면 여기만 채우면 줄 위젯은 그대로 돔
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	bool bHasExperience = false;

	// 이번 레벨에서 쌓은 경험치
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	int64 ExperienceIntoLevel = 0;

	// 이 레벨을 채우는 데 필요한 총량, 남은 양이 아님
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	int64 RequiredExperience = 0;

	// 경험치 바 비율
	float GetExperienceRatio() const;

	// 다음 레벨까지 남은 양, 바 위에 작게 띄우는 숫자
	int64 GetRemainingExperience() const;
};

// 획득 목록 한 줄, 아이템과 나이아가 같은 줄 위젯을 씀
// 같은 것을 여러 개 얻으면 한 줄로 합쳐 개수만 올린다
USTRUCT(BlueprintType)
struct PROJECTM_API FNUBattleResultEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	int32 Count = 0;

	// 표의 숫자 id, 아이템이면 NumericId 나이아면 NaiaId
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	int32 NumericId = 0;

	// 못 찾으면 null, WBP 는 그때 이미지를 숨기고 이름만 띄우면 됨
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	TObjectPtr<UTexture2D> Icon = nullptr;
};

// 결과창이 한 번에 받는 값 전체
USTRUCT(BlueprintType)
struct PROJECTM_API FNUBattleResultView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	ENUBattleResult Result = ENUBattleResult::None;

	// 포획으로 끝나면 보상이 없어 둘 다 0 이지만 칸은 그대로 띄운다
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	int64 Experience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	int64 Gold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	TArray<FNUBattleResultMember> Members;

	// 비어 있으면 획득 아이템 칸 자체를 숨긴다
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	TArray<FNUBattleResultEntry> Items;

	// 비어 있으면 획득 나이아 칸 자체를 숨긴다
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Result")
	TArray<FNUBattleResultEntry> CapturedNaia;
};

namespace NUBattleResult
{
	// 내 캐릭터 + 나이아 로스터 파티로 명단을 만듦
	// 전투에 나갔던 전투원이 아니라 파티 편성을 기준으로 함, 교체로 빠진 나이아도 포함
	// Growth 는 서버가 준 캐릭터 성장 결과, 없으면(패배·도주·포획) 지금 성장 상태를 그대로 씀
	PROJECTM_API void BuildMembers(
		const UObject* WorldContextObject,
		const FNUExperienceGrowth* Growth,
		TArray<FNUBattleResultMember>& OutMembers);

	// 아이템 표의 숫자 id 목록을 획득 목록으로 만듦, 같은 id 는 한 줄로 합침
	PROJECTM_API void BuildItemEntries(
		const UObject* WorldContextObject,
		const TArray<int32>& ItemIds,
		const TArray<int32>& Counts,
		TArray<FNUBattleResultEntry>& OutEntries);

	// 포획한 나이아 id 목록을 획득 목록으로 만듦, 같은 종을 두 마리 잡으면 개수 2 로 합침
	PROJECTM_API void BuildNaiaEntries(
		const UObject* WorldContextObject,
		const TArray<int32>& NaiaIds,
		TArray<FNUBattleResultEntry>& OutEntries);
}
