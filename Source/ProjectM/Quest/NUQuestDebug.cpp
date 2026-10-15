// 퀘스트 수동 확인용 콘솔 명령 (nu.Quest.*) — 개발 전용, Shipping 제외.
// 자동화 테스트가 아니라 실제 PIE 플레이 중 상태를 확인하고 선행 조건 값을 바꾸는 용도다.

#if !UE_BUILD_SHIPPING

#include "CharacterState/NUPlayerState.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Component/NUQuestProviderComponent.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Encounter/NUQuestEncounterSubsystem.h"
#include "Quest/World/NUQuestActionActor.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

namespace NUQuestDebug
{

static UNUQuestComponent* FindLocalQuestComponent(UWorld* World)
{
	APlayerController* PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	ANUPlayerState* PlayerState = PlayerController != nullptr
		? PlayerController->GetPlayerState<ANUPlayerState>()
		: nullptr;

	UNUQuestComponent* QuestComponent = PlayerState != nullptr ? PlayerState->GetQuestComponent() : nullptr;
	if (QuestComponent == nullptr)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest] 로컬 QuestComponent를 찾지 못했습니다."));
	}

	return QuestComponent;
}

static const UNUQuestDefinition* FindDefinition(UWorld* World, FName QuestId)
{
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUQuestDataSubsystem* QuestDataSubsystem = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUQuestDataSubsystem>()
		: nullptr;

	return QuestDataSubsystem != nullptr ? QuestDataSubsystem->FindDefinition(QuestId) : nullptr;
}

static const UNUQuestDataSubsystem* FindQuestDataSubsystem(UWorld* World)
{
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;

	return GameInstance != nullptr ? GameInstance->GetSubsystem<UNUQuestDataSubsystem>() : nullptr;
}

// CollectQuestInteractionOptions는 상호작용하는 쪽을 받아야 하므로 로컬 폰을 넘긴다
static AActor* FindLocalInteractor(UWorld* World)
{
	APlayerController* PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;

	return PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
}

static FString JoinQuestIds(const TArray<FName>& QuestIds)
{
	return QuestIds.IsEmpty()
		? FString(TEXT("없음"))
		: FString::JoinBy(QuestIds, TEXT(", "), [](FName QuestId) { return QuestId.ToString(); });
}

// CSV 퀘스트 정의에 어떤 대사 묶음이 연결돼 있는지 확인한다.
static FString DescribeQuestDialogues(const UNUQuestDataSubsystem& QuestDataSubsystem, FName QuestActorId)
{
	TArray<FName> QuestIds = QuestDataSubsystem.GetOfferedQuestsForActor(QuestActorId);
	for (const FName QuestId : QuestDataSubsystem.GetTurnInQuestsForActor(QuestActorId))
	{
		QuestIds.AddUnique(QuestId);
	}

	TArray<FString> Dialogues;
	for (const FName QuestId : QuestIds)
	{
		const UNUQuestDefinition* Definition = QuestDataSubsystem.FindDefinition(QuestId);
		if (Definition == nullptr || Definition->QuestDialogue.IsNull())
		{
			Dialogues.Add(FString::Printf(TEXT("%s=(없음)"), *QuestId.ToString()));
			continue;
		}

		Dialogues.Add(FString::Printf(
			TEXT("%s=%s"),
			*QuestId.ToString(),
			*Definition->QuestDialogue.ToSoftObjectPath().GetAssetName()));
	}

	return Dialogues.IsEmpty() ? FString(TEXT("없음")) : FString::Join(Dialogues, TEXT(", "));
}

static void LogInteractionActorList(UWorld* World)
{
	UE_LOG(LogNUQuest, Log, TEXT("[nu.Quest.Actor] 월드의 상호작용 액터"));

	int32 FoundCount = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UNUQuestProviderComponent* QuestProvider = It->FindComponentByClass<UNUQuestProviderComponent>();
		if (QuestProvider == nullptr)
		{
			continue;
		}

		++FoundCount;

		const FString ActorIdText = QuestProvider->QuestActorId.IsNone()
			? FString(TEXT("(비어 있음)"))
			: QuestProvider->QuestActorId.ToString();

		UE_LOG(LogNUQuest, Log, TEXT("  %s QuestActorId=%s"), *It->GetName(), *ActorIdText);
	}

	if (FoundCount == 0)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("  QuestProviderComponent를 가진 액터가 없습니다."));
	}
}

static void LogInteractionActorDetail(UWorld* World, FName QuestActorId)
{
	const UNUQuestDataSubsystem* QuestDataSubsystem = FindQuestDataSubsystem(World);
	if (QuestDataSubsystem == nullptr)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.Actor] QuestDataSubsystem을 찾지 못했습니다."));
		return;
	}

	UE_LOG(LogNUQuest, Log, TEXT("[nu.Quest.Actor] QuestActorId=%s"), *QuestActorId.ToString());

	// 정의에서 만들어진 역인덱스. 여기가 비어 있으면 에셋 연결 문제다
	UE_LOG(LogNUQuest, Log, TEXT("  역인덱스 Offered=%s"),
		*JoinQuestIds(QuestDataSubsystem->GetOfferedQuestsForActor(QuestActorId)));
	UE_LOG(LogNUQuest, Log, TEXT("  역인덱스 TurnIn =%s"),
		*JoinQuestIds(QuestDataSubsystem->GetTurnInQuestsForActor(QuestActorId)));

	AActor* Interactor = FindLocalInteractor(World);
	int32 FoundCount = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UNUQuestProviderComponent* QuestProvider = It->FindComponentByClass<UNUQuestProviderComponent>();
		if (QuestProvider == nullptr || QuestProvider->QuestActorId != QuestActorId)
		{
			continue;
		}

		++FoundCount;

		UE_LOG(LogNUQuest, Log, TEXT("  월드 액터=%s"), *It->GetName());

		// 역인덱스에는 있는데 여기 없으면 단계 필터에 걸린 것이다
		const TArray<FNUQuestInteractionOption> Options = QuestProvider->CollectQuestInteractionOptions(Interactor);
		if (Options.IsEmpty())
		{
			UE_LOG(LogNUQuest, Log, TEXT("    옵션 없음"));
		}

		for (const FNUQuestInteractionOption& Option : Options)
		{
			UE_LOG(LogNUQuest, Log, TEXT("    옵션 QuestId=%s Phase=%s"),
				*Option.QuestId.ToString(),
				*UEnum::GetValueAsString(Option.Phase)
			);
		}

		UE_LOG(LogNUQuest, Log, TEXT("    최종 Phase=%s 목표대상=%s ObjectiveType=%s"),
			*UEnum::GetValueAsString(QuestProvider->GetInteractionPhase(Interactor)),
			QuestProvider->IsActiveObjectiveTarget(Interactor) ? TEXT("예") : TEXT("아니오"),
			*QuestProvider->ObjectiveType.ToString()
		);

		UE_LOG(LogNUQuest, Log, TEXT("    퀘스트 대사=%s"), *DescribeQuestDialogues(*QuestDataSubsystem, QuestActorId));
	}

	if (FoundCount == 0)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("  월드에 이 QuestActorId를 가진 액터가 없습니다."));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GListCmd(
	TEXT("nu.Quest.List"),
	TEXT("로컬 퀘스트 상태 출력"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		const UNUQuestComponent* QuestComponent = FindLocalQuestComponent(World);
		if (QuestComponent == nullptr)
		{
			return;
		}

		UE_LOG(LogNUQuest, Log, TEXT("[nu.Quest.List] Level=%d Experience=%d Quests=%d"),
			QuestComponent->GetTestPlayerLevel(),
			QuestComponent->GetTestExperience(),
			QuestComponent->GetQuestProgressList().Num()
		);

		for (const FName FlagId : QuestComponent->GetTestFlags())
		{
			UE_LOG(LogNUQuest, Log, TEXT("  Flag=%s"), *FlagId.ToString());
		}

		for (const FName FeatureId : QuestComponent->GetTestUnlockedFeatures())
		{
			UE_LOG(LogNUQuest, Log, TEXT("  UnlockedFeature=%s"), *FeatureId.ToString());
		}

		for (const FNUQuestProgressState& Progress : QuestComponent->GetQuestProgressList())
		{
			const UNUQuestDefinition* Definition = FindDefinition(World, Progress.QuestId);
			UE_LOG(LogNUQuest, Log, TEXT("  QuestId=%s State=%s CompleteCount=%d"),
				*Progress.QuestId.ToString(),
				*UEnum::GetValueAsString(Progress.QuestState),
				Progress.CompletionCount
			);

			for (const FNUQuestObjectiveProgress& ObjectiveProgress : Progress.ObjectiveProgress)
			{
				const FNUQuestObjectiveSpec* Objective = Definition != nullptr
					? Definition->Objectives.FindByPredicate(
						[&ObjectiveProgress](const FNUQuestObjectiveSpec& Spec)
						{
							return Spec.ObjectiveId == ObjectiveProgress.ObjectiveId;
						})
					: nullptr;

				UE_LOG(LogNUQuest, Log, TEXT("    ObjectiveId=%s %d/%d"),
					*ObjectiveProgress.ObjectiveId.ToString(),
					ObjectiveProgress.CurrentCount,
					Objective != nullptr ? Objective->RequiredCount : 0
				);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GSetLevelCmd(
	TEXT("nu.Quest.SetLevel"),
	TEXT("더미 플레이어 레벨 설정: nu.Quest.SetLevel <Level>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1 || !Args[0].IsNumeric())
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.SetLevel] 사용법: nu.Quest.SetLevel <Level>"));
			return;
		}

		if (UNUQuestComponent* QuestComponent = FindLocalQuestComponent(World))
		{
			QuestComponent->DebugSetPlayerLevel(FCString::Atoi(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GSetFlagCmd(
	TEXT("nu.Quest.SetFlag"),
	TEXT("더미 플래그 추가: nu.Quest.SetFlag <FlagId>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.SetFlag] 사용법: nu.Quest.SetFlag <FlagId>"));
			return;
		}

		if (UNUQuestComponent* QuestComponent = FindLocalQuestComponent(World))
		{
			QuestComponent->DebugAddFlag(FName(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GAcceptCmd(
	TEXT("nu.Quest.Accept"),
	TEXT("Available 퀘스트 수락: nu.Quest.Accept <QuestId>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.Accept] 사용법: nu.Quest.Accept <QuestId>"));
			return;
		}

		if (UNUQuestComponent* QuestComponent = FindLocalQuestComponent(World))
		{
			QuestComponent->AcceptQuestById(FName(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GProgressCmd(
	TEXT("nu.Quest.Progress"),
	TEXT("목표 정상 진행: nu.Quest.Progress <QuestId> [Amount] 또는 <QuestId> <ObjectiveId> <Amount>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1 || Args.Num() > 3 || (Args.Num() == 2 && !Args[1].IsNumeric())
			|| (Args.Num() == 3 && !Args[2].IsNumeric()))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.Progress] 사용법: nu.Quest.Progress <QuestId> [Amount] 또는 <QuestId> <ObjectiveId> <Amount>"));
			return;
		}

		if (UNUQuestComponent* QuestComponent = FindLocalQuestComponent(World))
		{
			const FName QuestId(*Args[0]);
			if (Args.Num() == 1)
			{
				QuestComponent->DebugProgressQuestById(QuestId, 1);
			}
			else if (Args.Num() == 2 && Args[1].IsNumeric())
			{
				QuestComponent->DebugProgressQuestById(QuestId, FCString::Atoi(*Args[1]));
			}
			else
			{
				QuestComponent->DebugProgressObjectiveById(QuestId, FName(*Args[1]), FCString::Atoi(*Args[2]));
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GStartBattleCmd(
	TEXT("nu.Quest.StartBattle"),
	TEXT("배치 없이 퀘스트 연습 전투 시작: nu.Quest.StartBattle <QuestId> <ObjectiveId>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 2 || !Args[0].IsNumeric() || !Args[1].IsNumeric())
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.StartBattle] 사용법: nu.Quest.StartBattle <QuestId> <ObjectiveId>"));
			return;
		}

		const int32 QuestId = FCString::Atoi(*Args[0]);
		const int32 ObjectiveId = FCString::Atoi(*Args[1]);
		if (QuestId <= 0 || ObjectiveId <= 0 || World == nullptr)
		{
			return;
		}

		// 테스트 BP의 기본 전투 설정을 재사용해 레벨 배치와 상호작용 거리만 생략한다.
		const FString ClassPath = FString::Printf(TEXT("/Game/Developers/zxcv1/Quest/World/%d/BP_Quest_%d.BP_Quest_%d_C"), QuestId, ObjectiveId, ObjectiveId);
		const UClass* ActionClass = LoadClass<ANUQuestActionActor>(nullptr, *ClassPath);
		const ANUQuestActionActor* ActionDefaults = ActionClass != nullptr ? ActionClass->GetDefaultObject<ANUQuestActionActor>() : nullptr;
		const UNUQuestBattleDefinition* Battle = ActionDefaults != nullptr ? Cast<UNUQuestBattleDefinition>(ActionDefaults->EncounterRequest.BattleDefinition) : nullptr;
		if (Battle == nullptr || Battle->QuestId != FName(*Args[0]) || Battle->ObjectiveId != FName(*Args[1])
			|| !NUQuestBattle::IsPracticeKind(Battle->Kind))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.StartBattle] 연습 전투 설정을 찾지 못했습니다. QuestId=%d ObjectiveId=%d"), QuestId, ObjectiveId);
			return;
		}

		const APlayerController* Controller = World->GetFirstPlayerController();
		ANUPlayerState* PlayerState = Controller != nullptr ? Controller->GetPlayerState<ANUPlayerState>() : nullptr;
		const UNUQuestComponent* Quest = PlayerState != nullptr ? PlayerState->GetQuestComponent() : nullptr;
		UNUQuestEncounterSubsystem* Encounters = World->GetGameInstance() != nullptr ? World->GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>() : nullptr;
		if (Quest == nullptr || !Quest->IsServerBackedSession() || Encounters == nullptr
			|| !Encounters->TryStartQuestEncounter(PlayerState, ActionDefaults->EncounterRequest, true))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.StartBattle] 시작 요청 실패. 퀘스트 상태·목표·서버 연결을 확인하세요. QuestId=%d ObjectiveId=%d"), QuestId, ObjectiveId);
			return;
		}

		UE_LOG(LogNUQuest, Log, TEXT("[nu.Quest.StartBattle] 서버에 연습 전투 시작 요청 QuestId=%d ObjectiveId=%d"), QuestId, ObjectiveId);
	}));

static FAutoConsoleCommandWithWorldAndArgs GSkipCmd(
	TEXT("nu.Quest.Skip"),
	TEXT("선행 퀘스트를 건너뛰고 지정 퀘스트부터 플레이: nu.Quest.Skip <QuestId> (로컬 전투 밖에서만)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UNUQuestComponent* Quest = FindLocalQuestComponent(World);
		if (Args.Num() != 1 || Quest == nullptr)
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.Skip] 사용법: nu.Quest.Skip <시작할 QuestId>"));
			return;
		}
		FString Error;
		if (!Quest->DebugSkipLocalQuest(FName(*Args[0]), Error))
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.Skip] %s"), *Error);
			return;
		}
		UE_LOG(LogNUQuest, Log, TEXT("[nu.Quest.Skip] 선행 퀘스트 준비 완료. %s부터 플레이하세요."), *Args[0]);
	}));

static FAutoConsoleCommandWithWorldAndArgs GCompleteCmd(
	TEXT("nu.Quest.Complete"),
	TEXT("ReadyToTurnIn 퀘스트 정상 완료: nu.Quest.Complete <QuestId>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUQuest, Warning, TEXT("[nu.Quest.Complete] 사용법: nu.Quest.Complete <QuestId>"));
			return;
		}

		if (UNUQuestComponent* QuestComponent = FindLocalQuestComponent(World))
		{
			QuestComponent->DebugCompleteQuestById(FName(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GActorCmd(
	TEXT("nu.Quest.Actor"),
	TEXT("액터에 걸린 퀘스트 확인: nu.Quest.Actor [QuestActorId] (인자 없으면 목록)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}

		if (Args.Num() < 1)
		{
			LogInteractionActorList(World);
			return;
		}

		LogInteractionActorDetail(World, FName(*Args[0]));
	}));

} // namespace NUQuestDebug

#endif // !UE_BUILD_SHIPPING
