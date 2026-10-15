#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NUNPCDefinition.generated.h"

class UNUDialogueDefinition;
class UPaperZDAnimInstance;
class UTexture2D;

UENUM(BlueprintType)
enum class ENUNPCSpecies : uint8
{
	Unknown,
	Human,
	Elf,
	Dwarf,
	Beastfolk
};

UENUM(BlueprintType)
enum class ENUNPCGender : uint8
{
	Unknown,
	Male,
	Female
};

// 숫자 RowName을 NpcId로 사용하는 NPC 정적 정의
// 퀘스트 진행 상태나 상점 재고처럼 플레이어마다 달라지는 값은 포함하지 않는다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUNPCDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Identity")
	ENUNPCSpecies Species = ENUNPCSpecies::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Identity")
	ENUNPCGender Gender = ENUNPCGender::Unknown;

	// 지역 테이블의 숫자 ID
	// 아직 지역 ID가 확정되지 않았으면 0을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Reference", meta = (ClampMin = "0"))
	int32 AreaId = 0;

	// NPCDefinition.csv의 기존 LocationId 열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Reference", meta = (ClampMin = "0"))
	int32 LocationId = 0;

	// 상점이 없는 NPC는 0을 사용한다. 
	// 실제 상품과 가격은 상점 테이블이 소유한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Reference", meta = (ClampMin = "0"))
	int32 ShopId = 0;

	// "대화하기"를 골랐을 때 나오는 대사
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Presentation")
	TSoftObjectPtr<UNUDialogueDefinition> DefaultDialogue;

	// 말을 걸자마자 선택지와 함께 나오는 첫 대사. 비우면 DefaultDialogue로 인사한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Presentation")
	TSoftObjectPtr<UNUDialogueDefinition> OpeningDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Presentation")
	TSoftClassPtr<UPaperZDAnimInstance> AnimationClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Presentation")
	TSoftObjectPtr<UTexture2D> Portrait;

	// 설명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|NPC|Development")
	FString DesignNote;

	static bool TryParseNpcId(FName RowName, int32& OutNpcId);
	bool ValidateRow(FName RowName, FString& OutError) const;
};
