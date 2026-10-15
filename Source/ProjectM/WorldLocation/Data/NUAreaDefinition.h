#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NUAreaDefinition.generated.h"

UENUM(BlueprintType)
enum class ENUAreaType : uint8
{
	Unknown,
	Town,
	Encounter,
	Field,
	HuntingField,
	Dungeon
};

// 지역 정적 정의. 실제 맵, 서버 공간, 플레이어별 해금 상태는 별도로 관리한다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUAreaDefinition : public FTableRowBase
{
	GENERATED_BODY()

	// 숫자 RowName과 동일한 지역 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Identity")
	int32 AreaId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Identity")
	FName AreaKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Identity")
	FString EnglishName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Classification")
	ENUAreaType AreaType = ENUAreaType::Unknown;

	// 분류상 소속만 나타낸다. 상위 지역의 전투 규칙을 상속하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Reference")
	int32 ParentAreaId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Classification")
	int32 Chapter = 1;

	// 권장 레벨 범위. 입장 제한으로 사용하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Level")
	int32 MinLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Level")
	int32 MaxLevel = 1;

	// 0은 조건 없음. 완료 여부는 플레이어의 퀘스트 상태에서 판정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Reference")
	int32 UnlockQuestId = 0;

	// 이 지역에서 연결되는 지역 ID. 반대 방향 연결은 자동으로 추가하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Reference")
	TArray<int32> ConnectedAreas;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Rules")
	bool bSafeZone = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Rules")
	bool bCombatAllowed = false;

	// 다른 지역의 장소도 지정할 수 있다. 0은 기본 부활 장소 미지정.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Reference")
	int32 RespawnLocationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldLocation|Development")
	FString Description;

	bool ValidateRow(FName RowName, FString& OutError) const;
};
