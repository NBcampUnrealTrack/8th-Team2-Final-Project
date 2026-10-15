#include "Quest/Data/NUQuestDataSubsystem.h"

#include "Quest/Data/NUQuestDataSettings.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Data/NUQuestDefinitionRow.h"
#include "Quest/Data/NUQuestObjectiveRow.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Field/Presence/NUActorPresenceRow.h"
#include "Quest/Data/NUQuestSceneRow.h"
#include "Dialogue/Data/NUDialogueDataSubsystem.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "NUGAS/NUGameplayTags.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Field/NPC/Data/NUNPCDataSubsystem.h"


void UNUQuestDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 연출 표가 가리키는 대사가 있는지 확인하려면 대사 색인이 먼저 있어야 한다.
	Collection.InitializeDependency<UNUDialogueDataSubsystem>();
	// 등장 표의 시작 계약 조건이 고를 수 있는 시작 나이아를 가리키는지 확인하려면 나이아 정의가 먼저 있어야 한다.
	Collection.InitializeDependency<UNUNaiaDataSubsystem>();
	Super::Initialize(Collection);

	// 1. 설정에 지정된 DataTable 에셋을 로드
	LoadQuestDefinitionTable();
	LoadQuestObjectiveTable();

	// 2. 임포트 행을 게임 코드가 사용하는 런타임 정의로 변환
	LoadDefinitionsFromTable();

	// 3. NPC 상호작용에서 빠르게 찾을 수 있도록 ActorId 역색인을 만든다.
	BuildActorIndex();

	// 4. 퀘스트 정의가 준비된 뒤 월드 상태 규칙·연출·자리의 퀘스트·목표·대사 참조를 검사한다.
	LoadSceneTable();
	LoadActorPresenceTable();

	UE_LOG(LogNUQuest, Log, TEXT("QuestDataSubsystem: Ready Definitions=%d NumericIds=%d OfferActors=%d TurnInActors=%d"),
		QuestDefinitions.Num(),
		NumericIdToQuestId.Num(),
		ActorToOfferedQuests.Num(),
		ActorToTurnInQuests.Num()
	);
}

void UNUQuestDataSubsystem::Deinitialize()
{
	QuestDefinitionTable = nullptr;
	QuestObjectiveTable = nullptr;
	QuestSceneTable = nullptr;
	SceneIds.Reset();
	ActorPresenceTable = nullptr;
	ActorPresenceSteps.Reset();
	QuestDefinitions.Reset();
	NumericIdToQuestId.Reset();
	ActorToOfferedQuests.Reset();
	ActorToTurnInQuests.Reset();

	Super::Deinitialize();
}

void UNUQuestDataSubsystem::LoadQuestDefinitionTable()
{
	const UNUQuestDataSettings* Settings = GetDefault<UNUQuestDataSettings>();
	if (!IsValid(Settings) || Settings->QuestDefinitionTable.IsNull())
	{
		return;
	}

	UDataTable* LoadedTable = Settings->QuestDefinitionTable.LoadSynchronous();
	if (!IsValid(LoadedTable))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: QuestDefinitionTable load failed [%s]"),
			*Settings->QuestDefinitionTable.ToString());
		return;
	}

	// 다른 RowStruct의 DataTable을 연결 오류 차단
	if (LoadedTable->GetRowStruct() != FNUQuestDefinitionRow::StaticStruct())
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: Wrong row struct Table=%s Expected=%s Actual=%s"),
			*LoadedTable->GetName(),
			*FNUQuestDefinitionRow::StaticStruct()->GetName(),
			*GetNameSafe(LoadedTable->GetRowStruct()));
		return;
	}

	QuestDefinitionTable = LoadedTable;
	UE_LOG(LogNUQuest, Log, TEXT("QuestDataSubsystem: Loaded Table=%s Rows=%d"),
		*QuestDefinitionTable->GetName(),
		QuestDefinitionTable->GetRowMap().Num());
}

void UNUQuestDataSubsystem::LoadQuestObjectiveTable()
{
	const UNUQuestDataSettings* Settings = GetDefault<UNUQuestDataSettings>();
	if (Settings->QuestObjectiveTable.IsNull())
	{
		return;
	}
	UDataTable* LoadedTable = Settings->QuestObjectiveTable.LoadSynchronous();
	if (!IsValid(LoadedTable) || LoadedTable->GetRowStruct() != FNUQuestObjectiveRow::StaticStruct())
	{
		UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Invalid QuestObjectiveTable [%s]"), *Settings->QuestObjectiveTable.ToString());
		return;
	}
	QuestObjectiveTable = LoadedTable;
}

void UNUQuestDataSubsystem::LoadDefinitionsFromTable()
{
	QuestDefinitions.Reset();
	NumericIdToQuestId.Reset();
	if (!IsValid(QuestDefinitionTable))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: No QuestDefinitionTable. No quest definitions loaded."));
		return;
	}

	const TMap<FName, uint8*>& RowMap = QuestDefinitionTable->GetRowMap();

	// 한 번만 소속별로 색인한다. 잘못된 목표가 있으면 그 퀘스트 전체를 거부한다.
	TMap<FName, TArray<TPair<FName, const FNUQuestObjectiveRow*>>> ObjectiveRowsByQuest;
	TSet<FName> InvalidObjectiveQuests;
	if (IsValid(QuestObjectiveTable) && QuestObjectiveTable->GetRowStruct() == FNUQuestObjectiveRow::StaticStruct())
	{
		for (const TPair<FName, uint8*>& Pair : QuestObjectiveTable->GetRowMap())
		{
			const FNUQuestObjectiveRow* Row = reinterpret_cast<const FNUQuestObjectiveRow*>(Pair.Value);
			if (Row == nullptr)
			{
				continue;
			}
			const FName QuestId(*LexToString(Row->QuestId));
			FString Error;
			if (!Row->ValidateRow(Pair.Key, Error))
			{
				UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Invalid objective=%s Quest=%s Error=%s"), *Pair.Key.ToString(), *QuestId.ToString(), *Error);
				InvalidObjectiveQuests.Add(QuestId);
				continue;
			}
			if (!RowMap.Contains(QuestId))
			{
				UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Orphan objective=%s Quest=%s"), *Pair.Key.ToString(), *QuestId.ToString());
				continue;
			}
			ObjectiveRowsByQuest.FindOrAdd(QuestId).Emplace(Pair.Key, Row);
		}
		// DataTable에 저장된 행 순서를 유지한다. 수행 조건은 선행 목표 ID로 판정한다.
	}

	// 선행 퀘스트가 현재 행보다 뒤에 있어도 검사할 수 있도록 원본 RowName을 먼저 수집
	TSet<FName> SourceQuestIds;
	for (const TPair<FName, uint8*>& Pair : RowMap)
	{
		SourceQuestIds.Add(Pair.Key);
	}

	TSet<FName> UsedQuestKeys;
	int32 RejectedCount = 0;

	for (const TPair<FName, uint8*>& Pair : RowMap)
	{
		const FName RowName = Pair.Key;
		const FNUQuestDefinitionRow* Row = reinterpret_cast<const FNUQuestDefinitionRow*>(Pair.Value);
		if (Row == nullptr)
		{
			++RejectedCount;
			continue;
		}

		// 각 행의 필수 데이터 검사
		FString RowError;
		if (!Row->ValidateRow(RowName, RowError))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: Invalid row Quest_ID=%s Error=%s"),
				*RowName.ToString(),
				*RowError);
			++RejectedCount;
			continue;
		}

		// 다른 행과의 중복 검사
		if (UsedQuestKeys.Contains(Row->Quest_Key))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: Duplicate Quest_Key=%s Quest_ID=%s"),
				*Row->Quest_Key.ToString(),
				*RowName.ToString());
			++RejectedCount;
			continue;
		}

		// 선행 ID는 행 순서와 무관하며, 같은 DataTable 안에 RowName으로 존재해야 함
		if (Row->PrereqQuest_ID > 0)
		{
			const FName PrerequisiteId(*LexToString(Row->PrereqQuest_ID));
			if (!SourceQuestIds.Contains(PrerequisiteId))
			{
				UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: Missing PrereqQuest_ID=%lld Quest_ID=%s"),
					static_cast<long long>(Row->PrereqQuest_ID),
					*RowName.ToString());
				++RejectedCount;
				continue;
			}
		}

		bool bMissingPrerequisite = false;
		for (const int32 Prerequisite : Row->AdditionalPrereqQuest_IDs)
		{
			if (!SourceQuestIds.Contains(FName(*LexToString(Prerequisite))))
			{
				UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Missing additional prerequisite %lld Quest=%s"), Prerequisite, *RowName.ToString());
				bMissingPrerequisite = true;
			}
		}
		if (bMissingPrerequisite)
		{
			++RejectedCount;
			continue;
		}

		// 런타임 객체로 변환
		const auto* ObjectiveRows = ObjectiveRowsByQuest.Find(RowName);
		if (InvalidObjectiveQuests.Contains(RowName) || (Row->bUseObjectiveTable && ObjectiveRows == nullptr) || (!Row->bUseObjectiveTable && ObjectiveRows != nullptr))
		{
			UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Missing, invalid or conflicting objective rows Quest=%s UseObjectiveTable=%d"), *RowName.ToString(), Row->bUseObjectiveTable);
			++RejectedCount;
			continue;
		}

		UNUQuestDefinition* Definition = NewObject<UNUQuestDefinition>(this);
		if (!IsValid(Definition))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: Runtime definition allocation failed Quest_ID=%s"),
				*RowName.ToString());
			++RejectedCount;
			continue;
		}

		// 런타임 구조로 변환
		if (!Row->ApplyTo(*Definition, RowName, RowError))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: Row apply failed Quest_ID=%s Error=%s"),
				*RowName.ToString(),
				*RowError);
			++RejectedCount;
			continue;
		}

		if (Row->bUseObjectiveTable)
		{
			for (const auto& ObjectiveRow : *ObjectiveRows)
			{
				Definition->Objectives.Add(ObjectiveRow.Value->MakeObjective(ObjectiveRow.Key));
			}
		}

		// 구현된 규칙인지 확인
		if (!Definition->ValidateDefinition(RowError))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: Unsupported definition Quest_ID=%s Error=%s"),
				*RowName.ToString(),
				*RowError);
			++RejectedCount;
			continue;
		}

		// 숫자 ID는 서버에서도 사용하므로 중복되면 어느 정의인지 결정할 수 없음
		if (NumericIdToQuestId.Contains(Definition->QuestNumericId))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("QuestDataSubsystem: Duplicate numeric Quest_ID=%lld RowName=%s"),
				static_cast<long long>(Definition->QuestNumericId),
				*RowName.ToString());
			++RejectedCount;
			continue;
		}

		// 모든 검증을 통과한 정의할 경우 캐시에 등록
		UsedQuestKeys.Add(Row->Quest_Key);
		QuestDefinitions.Add(Definition->QuestId, Definition);
		NumericIdToQuestId.Add(Definition->QuestNumericId, Definition->QuestId);
	}

	UE_LOG(LogNUQuest, Log, TEXT("QuestDataSubsystem: Built from DataTable Rows=%d Loaded=%d Rejected=%d"),
		RowMap.Num(),
		QuestDefinitions.Num(),
		RejectedCount);
}

void UNUQuestDataSubsystem::BuildActorIndex()
{
	// build전 초기화
	ActorToOfferedQuests.Reset();
	ActorToTurnInQuests.Reset();

	for (const TPair<FName, TObjectPtr<UNUQuestDefinition>>& Pair : QuestDefinitions)
	{
		const UNUQuestDefinition* Definition = Pair.Value.Get();
		if (Definition == nullptr)
		{
			continue;
		}

		// NPC -> QuestId 목록을 미리 만들면 상호작용 때 모든 정의를 순회하지 않아도 됨
		if (!Definition->OfferActorId.IsNone())
		{
			ActorToOfferedQuests.FindOrAdd(Definition->OfferActorId).AddUnique(Definition->QuestId);
		}

		if (!Definition->TurnInActorId.IsNone())
		{
			ActorToTurnInQuests.FindOrAdd(Definition->TurnInActorId).AddUnique(Definition->QuestId);
		}
	}

	// 같은 NPC가 여러 퀘스트를 가질 때 SortOrder, QuestId 순으로 항상 같은 순서를 보장
	auto SortByDefinition = [this](TArray<FName>& QuestIds)
	{
		QuestIds.Sort([this](const FName& A, const FName& B)
		{
			const UNUQuestDefinition* DefA = FindDefinition(A);
			const UNUQuestDefinition* DefB = FindDefinition(B);

			if (DefA == nullptr || DefB == nullptr)
			{
				return DefA != nullptr;
			}

			if (DefA->SortOrder != DefB->SortOrder)
			{
				return DefA->SortOrder < DefB->SortOrder;
			}

			return A.ToString() < B.ToString();
		});
	};

	for (TPair<FName, TArray<FName>>& Pair : ActorToOfferedQuests)
	{
		SortByDefinition(Pair.Value);
	}

	for (TPair<FName, TArray<FName>>& Pair : ActorToTurnInQuests)
	{
		SortByDefinition(Pair.Value);
	}
}

const TArray<FName>& UNUQuestDataSubsystem::GetOfferedQuestsForActor(FName ActorId) const
{
	static const TArray<FName> EmptyQuestIds;

	const TArray<FName>* Found = ActorToOfferedQuests.Find(ActorId);

	return Found != nullptr ? *Found : EmptyQuestIds;
}

const TArray<FName>& UNUQuestDataSubsystem::GetTurnInQuestsForActor(FName ActorId) const
{
	static const TArray<FName> EmptyQuestIds;

	const TArray<FName>* Found = ActorToTurnInQuests.Find(ActorId);

	return Found != nullptr ? *Found : EmptyQuestIds;
}

// -----------------------------------------------------------------------------
// 퀘스트 정의 조회
// -----------------------------------------------------------------------------

const UNUQuestDefinition* UNUQuestDataSubsystem::FindDefinition(FName QuestId) const
{
	if (QuestId.IsNone())
	{
		return nullptr;
	}

	const TObjectPtr<UNUQuestDefinition>* FoundDefinition = QuestDefinitions.Find(QuestId);
	if (FoundDefinition == nullptr)
	{
		return nullptr;
	}

	return FoundDefinition->Get();
}

bool UNUQuestDataSubsystem::HasDefinition(FName QuestId) const
{
	return FindDefinition(QuestId) != nullptr;
}

FText UNUQuestDataSubsystem::GetTurnInDisplayText(FName QuestId) const
{
	const UNUQuestDefinition* Definition = FindDefinition(QuestId);
	if (Definition == nullptr)
	{
		return FText::GetEmpty();
	}
	if (Definition->bAutoComplete)
	{
		return NUQuest::GetStateDisplayText(ENUQuestState::Completed);
	}

	// 보고 NPC와의 대화로 끝나는 퀘스트는 그 대화에서 바로 수령하므로 "돌아가 보고하기" 줄을 붙이지 않는다.
	if (Definition->FindTurnInTalkObjective() != nullptr)
	{
		return FText::GetEmpty();
	}

	int32 NpcId = 0;
	const UNUNPCDataSubsystem* NpcData = GetGameInstance()->GetSubsystem<UNUNPCDataSubsystem>();
	const FNUNPCDefinition* Npc = NpcData != nullptr && LexTryParseString(NpcId, *Definition->TurnInActorId.ToString()) ? NpcData->FindDefinition(NpcId) : nullptr;
	if (Npc != nullptr && !Npc->DisplayName.IsEmpty())
	{
		return FText::Format(NSLOCTEXT("NUQuest", "NamedTurnIn", "{0}에게 돌아가 보고하기"), Npc->DisplayName);
	}
	return NSLOCTEXT("NUQuest", "TurnInFallback", "의뢰인에게 돌아가 보고하기");
}

TArray<FName> UNUQuestDataSubsystem::GetAutoAcceptQuestIds() const
{
	TArray<FName> Result;
	for (const auto& Pair : QuestDefinitions)
	{
		if (IsValid(Pair.Value) && Pair.Value->bAutoAccept)
		{
			Result.Add(Pair.Key);
		}
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

const UNUQuestDefinition* UNUQuestDataSubsystem::FindDefinitionByNumericId(int64 QuestNumericId) const
{
	if (QuestNumericId <= 0)
	{
		return nullptr;
	}

	const FName* QuestId = NumericIdToQuestId.Find(QuestNumericId);
	return QuestId != nullptr ? FindDefinition(*QuestId) : nullptr;
}

bool UNUQuestDataSubsystem::TryGetNumericId(FName QuestId, int64& OutQuestNumericId) const
{
	OutQuestNumericId = 0;

	const UNUQuestDefinition* Definition = FindDefinition(QuestId);
	if (Definition == nullptr || Definition->QuestNumericId <= 0)
	{
		return false;
	}

	OutQuestNumericId = Definition->QuestNumericId;
	return true;
}

const TArray<FName>& UNUQuestDataSubsystem::GetSceneIds() const
{
	return SceneIds;
}

const FNUQuestSceneRow* UNUQuestDataSubsystem::FindScene(FName SceneId) const
{
	if (!IsValid(QuestSceneTable) || !SceneIds.Contains(SceneId))
	{
		return nullptr;
	}

	return QuestSceneTable->FindRow<FNUQuestSceneRow>(SceneId, TEXT("QuestScene"), false);
}

void UNUQuestDataSubsystem::LoadSceneTable()
{
	SceneIds.Reset();
	const UNUQuestDataSettings* Settings = GetDefault<UNUQuestDataSettings>();
	if (Settings->QuestSceneTable.IsNull())
	{
		return;
	}

	UDataTable* LoadedTable = Settings->QuestSceneTable.LoadSynchronous();
	if (!IsValid(LoadedTable) || LoadedTable->GetRowStruct() != FNUQuestSceneRow::StaticStruct())
	{
		UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Invalid QuestSceneTable [%s]"), *Settings->QuestSceneTable.ToString());
		return;
	}
	QuestSceneTable = LoadedTable;

	// 대사가 없거나 목표를 잘못 가리키는 연출은 재생하지 않는다. 연출이 빠지면 로그로 남긴다.
	const UNUDialogueDataSubsystem* DialogueData = GetGameInstance()->GetSubsystem<UNUDialogueDataSubsystem>();
	auto FindQuest = [this](FName QuestId) { return FindDefinition(QuestId); };
	for (const TPair<FName, uint8*>& Pair : QuestSceneTable->GetRowMap())
	{
		const FNUQuestSceneRow* Row = reinterpret_cast<const FNUQuestSceneRow*>(Pair.Value);
		FString Error;
		if (Row == nullptr || !Row->Validate(Error))
		{
			UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Invalid scene=%s Error=%s"), *Pair.Key.ToString(), *Error);
			continue;
		}

		if (!Row->Condition.HasTarget(FindQuest))
		{
			UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Scene=%s points at a missing quest or objective [%s]"), *Pair.Key.ToString(), *Row->Condition.ToString());
			continue;
		}

		if (!Row->DialogueId.IsNone() && (DialogueData == nullptr || !DialogueData->HasDialogue(Row->DialogueId)))
		{
			UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Scene=%s points at a missing dialogue [%s]"), *Pair.Key.ToString(), *Row->DialogueId.ToString());
			continue;
		}

		SceneIds.Add(Pair.Key);
	}

	// 함께 건너뛸 연출이 없는 ID나 자기 자신이면 스킵해도 조용히 안 넘어가므로 로그로 남긴다.
	for (const FName SceneId : SceneIds)
	{
		for (const FName SkipAlsoId : QuestSceneTable->FindRow<FNUQuestSceneRow>(SceneId, TEXT("QuestScene"), false)->SkipAlsoSceneIds)
		{
			if (SkipAlsoId == SceneId || !SceneIds.Contains(SkipAlsoId))
			{
				UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Scene=%s skips a missing or own scene [%s]"), *SceneId.ToString(), *SkipAlsoId.ToString());
			}
		}
	}
}

const TArray<FName>& UNUQuestDataSubsystem::GetActorPresenceSteps(FName PresenceId) const
{
	static const TArray<FName> Empty;
	const TArray<FName>* Steps = ActorPresenceSteps.Find(PresenceId);
	return Steps != nullptr ? *Steps : Empty;
}

const FNUActorPresenceRow* UNUQuestDataSubsystem::FindActorPresence(FName RowName) const
{
	return IsValid(ActorPresenceTable) ? ActorPresenceTable->FindRow<FNUActorPresenceRow>(RowName, TEXT("ActorPresence"), false) : nullptr;
}

void UNUQuestDataSubsystem::LoadActorPresenceTable()
{
	ActorPresenceSteps.Reset();
	const UNUQuestDataSettings* Settings = GetDefault<UNUQuestDataSettings>();
	if (Settings->ActorPresenceTable.IsNull())
	{
		return;
	}

	UDataTable* LoadedTable = Settings->ActorPresenceTable.LoadSynchronous();
	if (!IsValid(LoadedTable) || LoadedTable->GetRowStruct() != FNUActorPresenceRow::StaticStruct())
	{
		UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Invalid ActorPresenceTable [%s]"), *Settings->ActorPresenceTable.ToString());
		return;
	}
	ActorPresenceTable = LoadedTable;

	// 잘못된 행을 쓰면 액터가 엉뚱한 자리에서 엉뚱한 전투를 열므로, 참조가 맞는 행만 제공한다.
	auto FindQuest = [this](FName QuestId) { return FindDefinition(QuestId); };
	for (const TPair<FName, uint8*>& Pair : ActorPresenceTable->GetRowMap())
	{
		const FNUActorPresenceRow* Row = reinterpret_cast<const FNUActorPresenceRow*>(Pair.Value);
		FString Error;
		if (Row == nullptr || !Row->Validate(Error))
		{
			UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: Invalid actor presence=%s Error=%s"), *Pair.Key.ToString(), *Error);
			continue;
		}

		if (Row->Conditions.ContainsByPredicate([&FindQuest](const FNUQuestStateCondition& Condition) { return !Condition.HasTarget(FindQuest); }))
		{
			UE_LOG(LogNUQuest, Error, TEXT("ActorPresence: row=%s points at a missing quest or objective"), *Pair.Key.ToString());
			continue;
		}

		// 고를 수 없는 나이아를 가리키면 계약 조건이 영원히 참이 되지 않는다.
		if (Row->ContractedStarterNaiaId > 0)
		{
			const UNUNaiaDataSubsystem* NaiaData = GetGameInstance()->GetSubsystem<UNUNaiaDataSubsystem>();
			const FNUNaiaDefinition* Naia = NaiaData != nullptr ? NaiaData->FindDefinitionById(Row->ContractedStarterNaiaId) : nullptr;
			if (Naia == nullptr || !Naia->IsStarterSelectionCandidate())
			{
				UE_LOG(LogNUQuest, Error, TEXT("ActorPresence: row=%s ContractedStarterNaiaId=%d is not a selectable starter naia"), *Pair.Key.ToString(), Row->ContractedStarterNaiaId);
				continue;
			}
		}

		// 전투 결과는 전투 정의의 목표에 반영되므로, 그 목표가 이 자리의 대상·목표 타입을 가리켜야 표시와 결과가 어긋나지 않는다.
		if (!Row->Battle.IsNull())
		{
			const UNUQuestBattleDefinition* Battle = Row->Battle.LoadSynchronous();
			const UNUQuestDefinition* Quest = Battle != nullptr ? FindDefinition(Battle->QuestId) : nullptr;
			const FNUQuestObjectiveSpec* Objective = Quest != nullptr
				? Quest->Objectives.FindByPredicate([Battle](const FNUQuestObjectiveSpec& Item) { return Item.ObjectiveId == Battle->ObjectiveId; })
				: nullptr;
			if (Objective == nullptr || Objective->TargetId != FName(*LexToString(Row->QuestActorId)) || Objective->ObjectiveType != Row->ObjectiveType)
			{
				UE_LOG(LogNUQuest, Error, TEXT("QuestDataSubsystem: ActorPresence=%s battle [%s] does not target %s (%s)"),
					*Pair.Key.ToString(), *Row->Battle.ToString(), *LexToString(Row->QuestActorId), *Row->ObjectiveType.ToString());
				continue;
			}
		}

		ActorPresenceSteps.FindOrAdd(Row->PresenceId).Add(Pair.Key);
	}

	// 우선순위가 겹친 그룹은 임의의 행을 고르지 않고 전체를 제외한다.
	for (auto It = ActorPresenceSteps.CreateIterator(); It; ++It)
	{
		TArray<FName>& Steps = It.Value();
		Steps.Sort([this](FName A, FName B) { return FindActorPresence(A)->StepOrder < FindActorPresence(B)->StepOrder; });
		for (int32 Index = 1; Index < Steps.Num(); ++Index)
		{
			if (FindActorPresence(Steps[Index - 1])->StepOrder == FindActorPresence(Steps[Index])->StepOrder)
			{
				UE_LOG(LogNUQuest, Error, TEXT("ActorPresence: duplicate StepOrder for PresenceId=%s (%s, %s)"),
					*It.Key().ToString(), *Steps[Index - 1].ToString(), *Steps[Index].ToString());
				It.RemoveCurrent();
				break;
			}
		}
	}
}
