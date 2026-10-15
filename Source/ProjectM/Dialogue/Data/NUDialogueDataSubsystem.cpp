#include "Dialogue/Data/NUDialogueDataSubsystem.h"

#include "Dialogue/Data/NUDialogueDataSettings.h"
#include "Dialogue/Data/NUDialogueRow.h"
#include "Field/NPC/Data/NUNPCDataSubsystem.h"

#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUDialogueData, Log, All);

void UNUDialogueDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UNUNPCDataSubsystem>();
	const UNUDialogueDataSettings* Settings = GetDefault<UNUDialogueDataSettings>();
	auto LoadTables = [this](const TArray<TSoftObjectPtr<UDataTable>>& Sources, const UScriptStruct* Expected)
	{
		for (const TSoftObjectPtr<UDataTable>& Source : Sources)
		{
			UDataTable* Table = Source.LoadSynchronous();
			if (!IsValid(Table) || Table->GetRowStruct() != Expected)
			{
				UE_LOG(LogNUDialogueData, Error, TEXT("대사 테이블 로드 또는 행 형식 오류: %s"), *Source.ToString());
				continue;
			}
			if (!Tables.Contains(Table))
			{
				Tables.Add(Table);
				Table->OnDataTableChanged().AddUObject(this, &ThisClass::RebuildIndex);
			}
		}
	};
	LoadTables(Settings->DialogueTables, FNUDialogueRow::StaticStruct());
	RebuildIndex();
}

void UNUDialogueDataSubsystem::Deinitialize()
{
	for (UDataTable* Table : Tables)
	{
		if (IsValid(Table)) { Table->OnDataTableChanged().RemoveAll(this); }
	}
	Tables.Reset();
	Dialogues.Reset();
	NpcDialogues.Reset();
	EventDialogues.Reset();
	Super::Deinitialize();
}

void UNUDialogueDataSubsystem::RebuildIndex()
{
	Dialogues.Reset();
	NpcDialogues.Reset();
	EventDialogues.Reset();
	const UNUNPCDataSubsystem* NPCData = GetGameInstance()->GetSubsystem<UNUNPCDataSubsystem>();
	TMap<FName, FName> RowOwners;
	int32 LineCount = 0;
	for (UDataTable* Table : Tables)
	{
		for (const auto& Pair : Table->GetRowMap())
		{
			const FNUDialogueRow& Row = *reinterpret_cast<const FNUDialogueRow*>(Pair.Value);
			if (Row.DialogueId.IsNone()) { continue; }
			const bool bNewDialogue = !Dialogues.Contains(Row.DialogueId);
			FDialogue& Dialogue = Dialogues.FindOrAdd(Row.DialogueId);
			if (bNewDialogue)
			{
				Dialogue.NpcId = Row.NpcId;
				Dialogue.QuestId = Row.QuestId > 0 ? FName(*LexToString(Row.QuestId)) : NAME_None;
				Dialogue.ObjectiveId = Row.ObjectiveId > 0 ? FName(*LexToString(Row.ObjectiveId)) : NAME_None;
				Dialogue.Phase = Row.Phase;
				Dialogue.EventId = Row.EventId;
			}

			// 잘못된 대화도 조회 ID는 남긴다. 대사 오류가 대사 생략·보상 진행으로 이어지면 안 된다.
			FString Error;
			if (!Row.Validate(Error) || (Row.NpcId > 0 && !NPCData->HasDefinition(Row.NpcId)) || (Row.SpeakerNpcId > 0 && !NPCData->HasDefinition(Row.SpeakerNpcId)))
			{
				Dialogue.bValid = false;
				UE_LOG(LogNUDialogueData, Error, TEXT("대사 행 또는 NPC 오류: %s %s"), *Pair.Key.ToString(), *Error);
			}
			const FName QuestId = Row.QuestId > 0 ? FName(*LexToString(Row.QuestId)) : NAME_None;
			const FName ObjectiveId = Row.ObjectiveId > 0 ? FName(*LexToString(Row.ObjectiveId)) : NAME_None;
			if (Dialogue.NpcId != Row.NpcId || Dialogue.QuestId != QuestId || Dialogue.ObjectiveId != ObjectiveId || Dialogue.Phase != Row.Phase || Dialogue.EventId != Row.EventId || Dialogue.Lines.Contains(Row.LineIndex))
			{
				Dialogue.bValid = false;
				UE_LOG(LogNUDialogueData, Error, TEXT("같은 대화의 조건이 다르거나 문장 순서가 중복됩니다: %s"), *Row.DialogueId.ToString());
			}
			if (const FName* Previous = RowOwners.Find(Pair.Key))
			{
				Dialogues.FindChecked(*Previous).bValid = false;
				Dialogue.bValid = false;
				UE_LOG(LogNUDialogueData, Error, TEXT("중복 대사 행 이름: %s"), *Pair.Key.ToString());
			}
			RowOwners.Add(Pair.Key, Row.DialogueId);
			FDataTableRowHandle Handle;
			Handle.DataTable = Table;
			Handle.RowName = Pair.Key;
			Dialogue.Lines.Add(Row.LineIndex, Handle);
			++LineCount;
		}
	}

	// 조건별 ID를 미리 색인하므로 대화할 때 전체 테이블을 탐색하지 않는다.
	// 연출 표가 부르는 대화는 조건이 없어 서로 겹치지 않으므로 색인하지 않는다.
	TMap<FString, FName> Contexts;
	for (auto& Pair : Dialogues)
	{
		FDialogue& Dialogue = Pair.Value;
		if (Dialogue.IsSceneDialogue())
		{
			continue;
		}

		const FString Context = FString::Printf(TEXT("%d|%s|%s|%d|%s"), Dialogue.NpcId, *Dialogue.QuestId.ToString(), *Dialogue.ObjectiveId.ToString(), static_cast<int32>(Dialogue.Phase), *Dialogue.EventId.ToString());
		const FName* Previous = Contexts.Find(Context);
		const FName* PreviousEvent = Dialogue.EventId.IsNone() ? nullptr : EventDialogues.Find(Dialogue.EventId);
		if (Previous != nullptr || PreviousEvent != nullptr)
		{
			Dialogue.bValid = false;
			if (Previous != nullptr) { Dialogues.FindChecked(*Previous).bValid = false; }
			if (PreviousEvent != nullptr) { Dialogues.FindChecked(*PreviousEvent).bValid = false; }
			UE_LOG(LogNUDialogueData, Error, TEXT("동일한 조건에 여러 대화가 등록되었습니다: %s"), *Pair.Key.ToString());
		}
		Contexts.Add(Context, Pair.Key);
		NpcDialogues.FindOrAdd(Dialogue.NpcId).Add(Pair.Key);
		if (!Dialogue.EventId.IsNone()) { EventDialogues.Add(Dialogue.EventId, Pair.Key); }
	}

	UE_LOG(LogNUDialogueData, Log, TEXT("대사 인덱스 준비: Lines=%d Dialogues=%d NPCs=%d"), LineCount, Dialogues.Num(), NpcDialogues.Num());
}

FName UNUDialogueDataSubsystem::FindQuestDialogue(int32 NpcId, const FNUQuestInteractionOption& Option) const
{
	const TArray<FName>* Ids = NpcDialogues.Find(NpcId);
	if (Ids == nullptr) { return NAME_None; }

	FName Fallback;
	for (FName Id : *Ids)
	{
		const FDialogue& Row = Dialogues.FindChecked(Id);
		if (!Row.EventId.IsNone() || Row.QuestId != Option.QuestId || Row.Phase != Option.Phase) { continue; }
		if (Row.ObjectiveId.IsNone()) { Fallback = Id; }
		else if (Row.ObjectiveId == Option.ObjectiveId) { return Id; }
	}
	return Fallback;
}

FName UNUDialogueDataSubsystem::FindEventDialogue(FName EventId) const
{
	const FName* Id = EventDialogues.Find(EventId);
	return Id != nullptr ? *Id : NAME_None;
}

bool UNUDialogueDataSubsystem::HasDialogue(FName DialogueId) const
{
	const FDialogue* Dialogue = Dialogues.Find(DialogueId);
	return Dialogue != nullptr && Dialogue->bValid && !Dialogue->Lines.IsEmpty();
}

bool UNUDialogueDataSubsystem::FDialogue::IsSceneDialogue() const
{
	return NpcId == 0 && QuestId.IsNone() && ObjectiveId.IsNone() && Phase == ENUQuestInteractionPhase::Idle && EventId.IsNone();
}

TArray<FName> UNUDialogueDataSubsystem::GetInProgressQuestIds(int32 NpcId) const
{
	TArray<FName> Result;
	const TArray<FName>* Ids = NpcDialogues.Find(NpcId);
	if (Ids != nullptr)
	{
		for (FName Id : *Ids)
		{
			const FDialogue& Row = Dialogues.FindChecked(Id);
			if (Row.EventId.IsNone() && Row.Phase == ENUQuestInteractionPhase::InProgress) { Result.AddUnique(Row.QuestId); }
		}
	}
	return Result;
}

bool UNUDialogueDataSubsystem::GetLines(FName DialogueId, TArray<FNUDialogueLine>& OutLines) const
{
	OutLines.Reset();
	const FDialogue* Dialogue = Dialogues.Find(DialogueId);
	if (Dialogue == nullptr || !Dialogue->bValid) { return false; }

	TArray<int32> Order;
	Dialogue->Lines.GetKeys(Order);
	Order.Sort();
	for (int32 Index : Order) { OutLines.AddDefaulted_GetRef().DialogueRow = Dialogue->Lines.FindChecked(Index); }
	return !OutLines.IsEmpty();
}

bool UNUDialogueDataSubsystem::GetDefinitionLines(const UNUDialogueDefinition* Definition, TArray<FNUDialogueLine>& OutLines) const
{
	OutLines.Reset();
	if (!IsValid(Definition)) { return false; }

	const FName DialogueId = Dialogues.Contains(Definition->DialogueId) ? Definition->DialogueId : FindEventDialogue(Definition->DialogueId);
	TArray<FNUDialogueLine> SourceLines;
	if (!DialogueId.IsNone())
	{
		if (!GetLines(DialogueId, SourceLines))
		{
			UE_LOG(LogNUDialogueData, Error, TEXT("대사 연결을 읽을 수 없습니다: %s"), *DialogueId.ToString());
			return false;
		}
	}
	else { SourceLines = Definition->Lines; }
	FString Error;
	if (SourceLines.IsEmpty() || !FNUDialogueLine::ResolveLines(SourceLines, OutLines, Error))
	{
		UE_LOG(LogNUDialogueData, Error, TEXT("대사 본문을 읽을 수 없습니다: %s %s"), *GetNameSafe(Definition), *Error);
		return false;
	}
	return true;
}
