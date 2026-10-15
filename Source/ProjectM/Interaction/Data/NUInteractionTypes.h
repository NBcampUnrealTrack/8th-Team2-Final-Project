#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NUInteractionTypes.generated.h"

class AActor;
class UNUDialogueDefinition;

// Router가 상호작용 요청을 처리한 결과
UENUM(BlueprintType)
enum class ENUInteractionRouteResult : uint8
{
	Failed,
	Executed,
	SelectionRequired,
};

// 선택지가 하나일 때 Router가 처리할 방식
UENUM(BlueprintType)
enum class ENUInteractionRoutePolicy : uint8
{
	// 문, 줍기 오브젝트처럼 선택할 필요가 없는 단일 행동은 즉시 실행
	AutoExecuteSingle,

	// NPC처럼 선택지가 하나여도 대화창에 표시
	AlwaysShowSelection,
};

// NPC 기능 메뉴의 의미 기반 그룹
// 실제 스토리 대화 응답 순서에는 사용하지 않음
UENUM(BlueprintType)
enum class ENUInteractionOptionGroup : uint8
{
	Other			UMETA(DisplayName = "기타"),
	QuestTurnIn	UMETA(DisplayName = "퀘스트 완료 보고"),
	QuestOffer		UMETA(DisplayName = "퀘스트 제안"),
	QuestTarget		UMETA(DisplayName = "퀘스트 목표"),
	QuestInProgress	UMETA(DisplayName = "진행 중 퀘스트"),
	Dialogue		UMETA(DisplayName = "대화"),
	Service			UMETA(DisplayName = "상점·창고·제작"),
	System			UMETA(Hidden),
};

// 상호작용 요청에서 누가 누구에게 상호작용하는지
USTRUCT(BlueprintType)
struct PROJECTM_API FNUInteractionContext
{
	GENERATED_BODY()

	// 상호작용을 요청한 플레이어 
	UPROPERTY(BlueprintReadWrite, Category = "NU|Interaction")
	TObjectPtr<AActor> Interactor = nullptr;

	// NPC, 창고, 적 등 상호작용 대상
	UPROPERTY(BlueprintReadWrite, Category = "NU|Interaction")
	TObjectPtr<AActor> Target = nullptr;

	bool IsValid() const;
};

// 현재 선택지가 어떤 기능을 가리키는지 구분하는 식별자
USTRUCT(BlueprintType)
struct PROJECTM_API FNUInteractionOptionHandle
{
	GENERATED_BODY()

	// 대화, 퀘스트, 상점 등의 선택지 구분
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Interaction")
	FGameplayTag OptionType;

	// 해당 타입 안의 실제 대화 or 퀘스트 한 건 구분
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Interaction")
	FName OptionId = NAME_None;

	bool IsValid() const;
	bool operator==(const FNUInteractionOptionHandle& Other) const;

	friend uint32 GetTypeHash(const FNUInteractionOptionHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.OptionType), GetTypeHash(Handle.OptionId));
	}
};

// NPC 대화창 표시와 실행 요청에 필요한 상호작용 선택지 한 건
// ex)
// [완료 보고] 몬스터 처치
// 상점 열기
// 대화하기
// 접두어는 퀘스트 선택지에만 붙는다 (NUInteractionOptionEntryWidget). 닫기와 ESC는 대화창이 처리하므로 Provider 선택지에 포함하지 않음
USTRUCT(BlueprintType)
struct PROJECTM_API FNUInteractionOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Interaction")
	FNUInteractionOptionHandle Handle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Interaction")
	FText DisplayText;

	// 끝까지 읽은 경우에만 해당 선택지를 실행한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Interaction")
	TObjectPtr<UNUDialogueDefinition> ExecutionDialogue;

	// [진행 가능], [진행 중], [완료 보고]처럼 퀘스트 선택지의 상태를 설명하는 짧은 표기
	// 퀘스트 그룹이 아닌 선택지는 값이 있어도 UI가 접두어로 붙이지 않는다. 비어 있는 퀘스트 선택지는 기본 표기를 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Interaction")
	FText CategoryText;

	// NPC 기능의 의미 그룹. 그룹 간 순서는 Router 정책이 관리한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Interaction")
	ENUInteractionOptionGroup Group = ENUInteractionOptionGroup::Other;

	// 같은 Group 안에서 적용하는 기획 정렬 순서. 값이 작을수록 먼저 표시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NotReplicated, Category = "NU|Interaction")
	int32 SortOrder = 0;

	// 선택 가능 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Interaction")
	bool bEnabled = true;

	// 선택지 비활성화 이유
	// ex) 레벨 제한, 재화 부족 등
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Interaction")
	FText DisabledReason;

	bool IsValid() const;
};
