#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Battle/Data/NUBattleDefinition.h"
#include "NUQuestBattleDefinition.generated.h"


class ANUQuestActionActor;
class ANUQuestTutorialEncounter;
class UUserWidget;
class UNUQuestAgitationWidget;
class UNUQuestDefinition;
class UNUDialogueDefinition;
struct FNUQuestProgressState;

UENUM(BlueprintType)
enum class ENUQuestBattleKind : uint8
{
	None = 0,
	BasicAttack = 1,
	Parry = 2,
	Dodge = 3,
	Calming = 4,
	Sparring = 5
};

namespace NUQuestBattle
{
	// 연습 횟수를 세는 종류(기본 공격·패링·회피)
	PROJECTM_API bool IsPracticeKind(ENUQuestBattleKind Kind);

	// 전용 실행기(ANUQuestTutorialEncounter)와 판정 규칙(FNUQuestTutorialBattleRules)을 쓰는 종류. 대련은 일반 전투 실행기를 쓴다.
	PROJECTM_API bool IsTutorialKind(ENUQuestBattleKind Kind);

	// 서버 battle_mode 번호. 서버와 약속한 번호라 enum 순서와 무관하게 여기서만 정한다. 0이면 서버 전투가 없는 종류다.
	PROJECTM_API int32 ToServerBattleMode(ENUQuestBattleKind Kind);
	PROJECTM_API ENUQuestBattleKind FromServerBattleMode(int32 BattleMode);
}

UENUM(BlueprintType)
enum class ENUQuestCalmingRule : uint8
{
	None,
	Grass,
	Earth,
	Water,
	Fire,
	Wind,
	Lightning
};

// 퀘스트 전투의 종류와 조정 수치를 공유하는 설정 에셋.
// 진행 중 동요/턴 수는 PlayerState의 QuestComponent에 보관한다.
// 아래 감소량은 기존 로컬 검증값이며 기획 확정 수치가 아니다.
UCLASS(BlueprintType)
class PROJECTM_API UNUQuestBattleDefinition : public UNUBattleDefinition
{
	GENERATED_BODY()

public:
	// 로컬 전투의 처리를 기존 퀘스트 브리지에 연결한다. 별도 컴포넌트는 생성하지 않는다.
	virtual TScriptInterface<INUBattleEventInterface> GetEventHandler(ANUBattleEncounter* Encounter) override;

	// 회수 진행을 필드 표시와 최종 전투에서 같은 값으로 해석한다.
	int32 GetCompletedGrassRecoveries(const UNUQuestDefinition& Quest, const FNUQuestProgressState& Progress) const;
	float GetGrassAgitation(int32 CompletedRecoveries) const;

	// 전투 종류에 맞는 목표 타입. 서버도 목표 타입과 battle_mode가 다르면 전투 시작을 거절한다.
	FGameplayTag GetObjectiveType() const;

	// 튜토리얼 종류와 진정 조우지 규칙
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	ENUQuestBattleKind Kind = ENUQuestBattleKind::None;

	// 튜토리얼 적 실행 액터
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	TSoftClassPtr<ANUQuestTutorialEncounter> TutorialEncounterClass;

	// 여섯 진정 조우의 전용 규칙
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	ENUQuestCalmingRule Rule = ENUQuestCalmingRule::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (EditCondition = "Rule == ENUQuestCalmingRule::Grass"))
	TArray<FName> GrassRecoveryObjectiveIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0", EditCondition = "Rule == ENUQuestCalmingRule::Grass"))
	float GrassRecoveryCalmAmount = 25.f;

	// 회수 전 나이아와 상호작용하면 전투 대신 보여줄 안내 대화.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (EditCondition = "Rule == ENUQuestCalmingRule::Grass"))
	TSoftObjectPtr<UNUDialogueDefinition> GrassRecoveryDialogue;

	// 같은 전투에서 돌을 옮겨야 최종 진정할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (EditCondition = "Rule == ENUQuestCalmingRule::Earth"))
	bool bRequireEarthStone = true;

	// 돌을 옮겼을 때 함께 진행할 퀘스트 목표. 비우면 전투 규칙에서만 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (EditCondition = "Rule == ENUQuestCalmingRule::Earth"))
	FName EarthStoneObjectiveId;

	// 기믹 구현용 배치 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	TSubclassOf<ANUQuestActionActor> WorldActionClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	FVector WorldActionOffset = FVector(0.f, -150.f, 0.f);

	// 물 기믹의 시야 제한 연출
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (EditCondition = "Rule == ENUQuestCalmingRule::Water"))
	TSubclassOf<UUserWidget> MistWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "Rule == ENUQuestCalmingRule::Water"))
	float MaximumMistOpacity = 0.75f;

	// 동요 표시로, 나이아 아바타에 전투 동안만 부착한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	TSubclassOf<UNUQuestAgitationWidget> AgitationWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	FVector AgitationWidgetOffset = FVector(0.f, 0.f, 110.f);

	// 조우지 장소와 이 전투가 진행할 퀘스트 목표. 결과는 대상 ID 대조 없이 ObjectiveId에만 반영한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	int32 LocationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle")
	FName ObjectiveId;

	// 진정 전투의 초기 동요와 행동별 감소량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "1"))
	float InitialAgitation = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "1"))
	float CalmAmount = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0"))
	float ReactionCalmAmount = 10.f;

	// 지상에 있을 때 N번째 턴마다 매몰을 시도한다. 돌 옮기기로 나오며 동요를 낮춘다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "1", EditCondition = "Rule == ENUQuestCalmingRule::Earth"))
	int32 EarthBurrowIntervalTurns = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0", EditCondition = "Rule == ENUQuestCalmingRule::Earth"))
	float EarthStoneCalmAmount = 50.f;

	// 매몰 중 달래기 실패 후 밖으로 나올 확률. 실패 자체는 동요를 낮추지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "Rule == ENUQuestCalmingRule::Earth"))
	float EarthEmergeChanceAfterFailedCalm = 0.5f;

	// 불 기믹: 턴 경과 자연 진정과 연속 턴 달래기 반동
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0"))
	float FireDecayPerTurn = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0"))
	float RepeatedCalmPenalty = 10.f;

	// 바람 기믹: 플레이어 행동 속도를 흩뜨리고 패링 성공의 동요 감소를 증폭한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0", EditCondition = "Rule == ENUQuestCalmingRule::Wind"))
	float WindPlayerSpeedMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "1", EditCondition = "Rule == ENUQuestCalmingRule::Wind"))
	float WindParryCalmMultiplier = 3.f;

	// 기존 에셋 호환용 속도 범위. 현재 전기 기믹은 턴마다 행동 큐를 섞으므로 사용하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0", EditCondition = "Rule == ENUQuestCalmingRule::Lightning"))
	float LightningMinimumSpeedMultiplier = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Battle", meta = (ClampMin = "0", EditCondition = "Rule == ENUQuestCalmingRule::Lightning"))
	float LightningMaximumSpeedMultiplier = 2.f;
};
