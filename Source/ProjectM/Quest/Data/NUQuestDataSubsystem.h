#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUQuestDataSubsystem.generated.h"

class UNUQuestDefinition;
class UDataTable;
struct FNUActorPresenceRow;
struct FNUQuestSceneRow;

/**
 * QuestId로 퀘스트 정의 조회 및 캐싱
 * 
 * 모든 플레이어가 공유하는 정적 정의만 관리함
 * 플레이어마다 달라지는 정보는 Quest Component에서 소유함
 */
UCLASS()
class PROJECTM_API UNUQuestDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// QuestId로 퀘스트 정의를 조회
	const UNUQuestDefinition* FindDefinition(FName QuestId) const;

	bool HasDefinition(FName QuestId) const;

	// 보상 수령 가능 상태에서 목표 아래 붙는 안내 줄. 보고 NPC와의 대화에서 바로 수령하는 퀘스트는 빈 글자를 돌려준다.
	FText GetTurnInDisplayText(FName QuestId) const;
	TArray<FName> GetAutoAcceptQuestIds() const;

	// 서버에서 사용하는 숫자 Quest_ID로 퀘스트 정의를 조회
	const UNUQuestDefinition* FindDefinitionByNumericId(int64 QuestNumericId) const;

	// 런타임 QuestId를 숫자 Quest_ID로 변환
	bool TryGetNumericId(FName QuestId, int64& OutQuestNumericId) const;

	// 해당 ActorId의 NPC가 제안하는 퀘스트 목록
	const TArray<FName>& GetOfferedQuestsForActor(FName ActorId) const;

	// 해당 ActorId의 NPC에게 보고할 수 있는 퀘스트 목록
	const TArray<FName>& GetTurnInQuestsForActor(FName ActorId) const;

	// 검증을 통과한 연출 ID를 표 순서대로 준다. 같은 순간에 조건이 참이 된 연출은 이 순서로 재생한다.
	const TArray<FName>& GetSceneIds() const;
	const FNUQuestSceneRow* FindScene(FName SceneId) const;

	// 한 액터(PresenceId)의 등장 행 이름을 StepOrder 오름차순으로 준다. 검증을 통과한 행만 들어 있다.
	const TArray<FName>& GetActorPresenceSteps(FName PresenceId) const;
	const FNUActorPresenceRow* FindActorPresence(FName RowName) const;

private:
	// 퀘스트 DataTable 에셋을 로드
	void LoadQuestDefinitionTable();
	void LoadQuestObjectiveTable();

	// DataTable의 각 행을 검증 및 UNUQuestDefinition으로 변환
	void LoadDefinitionsFromTable();

	void BuildActorIndex();

	// 연출을 읽고, 없는 퀘스트·목표·대사를 가리키는 행은 제외한다.
	void LoadSceneTable();

	// 등장 표를 읽고, 없는 퀘스트를 가리키거나 전투의 목표가 그 자리의 대상·목표 타입과 다른 행은 제외한다.
	void LoadActorPresenceTable();

	// CSV 자체가 아니라 CSV를 임포트해 생성한 DataTable 에셋을 런타임에 보관
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> QuestDefinitionTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> QuestObjectiveTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> QuestSceneTable = nullptr;

	// 검증을 통과한 연출 행 이름(표 순서)
	TArray<FName> SceneIds;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ActorPresenceTable = nullptr;

	// PresenceId -> 검증을 통과한 등장 행 이름(StepOrder 오름차순)
	TMap<FName, TArray<FName>> ActorPresenceSteps;

	// QuestId(RowName) -> 런타임 퀘스트 정의
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UNUQuestDefinition>> QuestDefinitions;

	// 숫자 Quest_ID -> QuestId. 서버 패킷의 숫자 ID를 런타임 정의로 연결할 때 사용
	TMap<int64, FName> NumericIdToQuestId;

	// NPC 상호작용 시 전체 퀘스트를 순회하지 않도록
	TMap<FName, TArray<FName>> ActorToOfferedQuests;
	TMap<FName, TArray<FName>> ActorToTurnInQuests;
};
