#include "CharacterProgression/Net/NUProgressionNetSubsystem.h"

#include "CharacterProgression/Data/NUProgressionVersion.h"
#include "Net/EmberServerManager.h"
#include "Subsystems/SubsystemCollection.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUProgressionNet, Log, All);

namespace
{
	constexpr int32 ProgressionSuccess = 200;
	constexpr int32 ProgressionGraphUnavailable = 2100;
	constexpr int32 ProgressionVersionMismatch = 2101;
	constexpr int32 ProgressionRevisionConflict = 2102;

	// 어느 칸이 달라서 거부됐는지 남김
	// 무언가 다르다로만 찍히면 서버 계약 문제인지 클라 비교가 과한 것인지 알 수 없음
	FString BuildProgressionStateDiff(const FNUProgressionState& Current, const FNUProgressionState& Incoming)
	{
		TArray<FString> Diffs;

		auto AddIfDifferent = [&Diffs](const TCHAR* Field, int64 Left, int64 Right)
		{
			if (Left != Right)
			{
				Diffs.Add(FString::Printf(TEXT("%s %lld->%lld"), Field, Left, Right));
			}
		};

		AddIfDifferent(TEXT("Level"), Current.Level, Incoming.Level);
		AddIfDifferent(TEXT("TotalExperience"), Current.TotalExperience, Incoming.TotalExperience);
		AddIfDifferent(TEXT("ExperienceIntoLevel"), Current.ExperienceIntoLevel, Incoming.ExperienceIntoLevel);
		AddIfDifferent(TEXT("AvailablePoints"), Current.AvailablePoints, Incoming.AvailablePoints);
		AddIfDifferent(TEXT("TotalPoints"), Current.TotalPoints, Incoming.TotalPoints);
		AddIfDifferent(TEXT("ActiveStemId"), Current.ActiveStemId, Incoming.ActiveStemId);
		AddIfDifferent(TEXT("CurrentNodeId"), Current.CurrentNodeId, Incoming.CurrentNodeId);
		AddIfDifferent(TEXT("SelectedFinalNodeId"), Current.SelectedFinalNodeId, Incoming.SelectedFinalNodeId);

		if (Current.PrimaryStats != Incoming.PrimaryStats)
		{
			Diffs.Add(TEXT("PrimaryStats"));
		}

		// 배열은 순서까지 비교되므로, 같은 집합을 다른 순서로 받아도 여기 걸림
		if (Current.AcquiredNodeIds != Incoming.AcquiredNodeIds)
		{
			Diffs.Add(FString::Printf(TEXT("AcquiredNodeIds %d->%d개"),
				Current.AcquiredNodeIds.Num(), Incoming.AcquiredNodeIds.Num()));
		}
		if (Current.CompletedStemIds != Incoming.CompletedStemIds)
		{
			Diffs.Add(TEXT("CompletedStemIds"));
		}
		if (Current.AcquiredNodeLinks != Incoming.AcquiredNodeLinks)
		{
			Diffs.Add(TEXT("AcquiredNodeLinks"));
		}

		return Diffs.IsEmpty() ? FString(TEXT("성장 상태는 같음 (JobState 차이)")) : FString::Join(Diffs, TEXT(", "));
	}
}

void UNUProgressionNetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UEmberServerManager>();
	if (UEmberServerManager* Server = GetServer())
	{
		Server->OnStateChanged.AddDynamic(this, &ThisClass::HandleServerStateChanged);
	}
}

void UNUProgressionNetSubsystem::Deinitialize()
{
	if (UEmberServerManager* Server = GetServer())
	{
		Server->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleServerStateChanged);
	}
	ResetSession();
	Super::Deinitialize();
}

void UNUProgressionNetSubsystem::BeginCharacterSession(int64 CharacterIdx)
{
	if (CharacterIdx <= 0)
	{
		UE_LOG(LogNUProgressionNet, Error, TEXT("성장 세션 시작 거부: CharacterIdx=%lld"), CharacterIdx);
		return;
	}

	ResetSession();
	ActiveCharacterIdx = CharacterIdx;
	OnSnapshotChanged.Broadcast();
	RequestProgression();
}

bool UNUProgressionNetSubsystem::RequestProgression()
{
	UEmberServerManager* Server = GetServer();
	if (ActiveCharacterIdx <= 0 || Server == nullptr || !Server->IsLoggedIn() || bStateRequestPending)
	{
		return false;
	}

	bStateRequestPending = true;
	OnRequestStateChanged.Broadcast();
	Server->SendCharacterProgression(NUProgressionGraphVersion);
	return true;
}

bool UNUProgressionNetSubsystem::RequestInitialJobSelection(int32 JobId)
{
	UEmberServerManager* Server = GetServer();
	if (JobId <= 0 || ActiveCharacterIdx <= 0 || Server == nullptr || !Server->IsLoggedIn() || !bHasSnapshot || IsMutationPending())
	{
		return false;
	}

	PendingMutation = ENUPendingProgressionMutation::JobSelect;
	const FString MutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	OnRequestStateChanged.Broadcast();
	Server->SendCharacterJobSelect(JobId, NUProgressionGraphVersion, Snapshot.ProgressionState.Revision, MutationId);
	return true;
}

bool UNUProgressionNetSubsystem::RequestNodeAcquire(int32 NodeId, int32 ChoiceAdvancementId)
{
	UEmberServerManager* Server = GetServer();
	if (NodeId <= 0 || ChoiceAdvancementId < 0 || ActiveCharacterIdx <= 0 || Server == nullptr || !Server->IsLoggedIn() || !bHasSnapshot || IsMutationPending())
	{
		return false;
	}

	PendingMutation = ENUPendingProgressionMutation::NodeAcquire;
	const FString MutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	OnRequestStateChanged.Broadcast();
	Server->SendCharacterNodeAcquire(NodeId, ChoiceAdvancementId, NUProgressionGraphVersion, Snapshot.ProgressionState.Revision, MutationId);
	return true;
}

bool UNUProgressionNetSubsystem::IsExternalSessionActive() const
{
	const UEmberServerManager* Server = GetServer();
	return ActiveCharacterIdx > 0 && Server != nullptr && Server->IsLoggedIn();
}

void UNUProgressionNetSubsystem::HandleProgressionResponse(int32 ResultCode, const FString& ResultMessage, const FString& ResponseData)
{
	bStateRequestPending = false;
	ApplyResponseSnapshot(ResultCode, ResponseData);
	OnRequestStateChanged.Broadcast();
	if (ResultCode != ProgressionSuccess)
	{
		OnRequestFailed.Broadcast(ResultCode, NUProgressionServer::GetResultMessage(ResultCode, ResultMessage));
	}
}

void UNUProgressionNetSubsystem::HandleJobSelectResponse(int32 ResultCode, const FString& ResultMessage, const FString& ResponseData)
{
	CompleteMutation(ENUPendingProgressionMutation::JobSelect, ResultCode, ResultMessage, ResponseData);
}

void UNUProgressionNetSubsystem::HandleNodeAcquireResponse(int32 ResultCode, const FString& ResultMessage, const FString& ResponseData)
{
	CompleteMutation(ENUPendingProgressionMutation::NodeAcquire, ResultCode, ResultMessage, ResponseData);
}

void UNUProgressionNetSubsystem::HandleServerStateChanged(EEmberConnState NewState)
{
	if (NewState == EEmberConnState::LoggedIn || (ActiveCharacterIdx <= 0 && !bHasSnapshot && !bStateRequestPending && !IsMutationPending()))
	{
		return;
	}

	UE_LOG(LogNUProgressionNet, Warning, TEXT("서버 연결 종료로 캐릭터 성장 세션을 초기화합니다."));
	ResetSession();
	OnSnapshotChanged.Broadcast();
	OnRequestStateChanged.Broadcast();
}

UEmberServerManager* UNUProgressionNetSubsystem::GetServer() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
}

bool UNUProgressionNetSubsystem::ApplyResponseSnapshot(int32 ResultCode, const FString& ResponseData)
{
	if (ResultCode != ProgressionSuccess && ResultCode != ProgressionRevisionConflict)
	{
		if (ResultCode == ProgressionGraphUnavailable || ResultCode == ProgressionVersionMismatch)
		{
			bHasSnapshot = false;
			OnSnapshotChanged.Broadcast();
		}
		return false;
	}

	FNUProgressionServerSnapshot ParsedSnapshot;
	FString Error;
	if (!NUProgressionServer::ParseSnapshot(ResponseData, ParsedSnapshot, Error))
	{
		bHasSnapshot = false;
		UE_LOG(LogNUProgressionNet, Error, TEXT("성장 응답 파싱 실패: %s"), *Error);
		OnSnapshotChanged.Broadcast();
		OnRequestFailed.Broadcast(ResultCode, FText::FromString(Error));
		return false;
	}
	if (ParsedSnapshot.ProgressionState.GraphVersion != NUProgressionGraphVersion)
	{
		bHasSnapshot = false;
		UE_LOG(LogNUProgressionNet, Error, TEXT("성장 그래프 버전 불일치: Client=%d Server=%d"), NUProgressionGraphVersion, ParsedSnapshot.ProgressionState.GraphVersion);
		OnSnapshotChanged.Broadcast();
		OnRequestFailed.Broadcast(ProgressionVersionMismatch, NUProgressionServer::GetResultMessage(ProgressionVersionMismatch, FString()));
		return false;
	}
	if (bHasSnapshot)
	{
		const FNUProgressionState& Current = Snapshot.ProgressionState;
		const FNUProgressionState& Incoming = ParsedSnapshot.ProgressionState;

		// 리비전이 내려가면 순서가 뒤바뀐 옛 응답이다
		bool bReject = Incoming.Revision < Current.Revision;

		if (!bReject && Incoming.Revision == Current.Revision)
		{
			// 같은 리비전 + 경험치만 다르면 정상
			// 레벨업처럼 포인트, 노드가 달라져야할때만 검사
			const bool bOnlyExperienceChanged =
				Incoming.EqualsIgnoringExperience(Current)
				&& ParsedSnapshot.JobState == Snapshot.JobState
				&& Incoming.TotalExperience >= Current.TotalExperience;

			bReject = !bOnlyExperienceChanged;
		}

		if (bReject)
		{
			UE_LOG(LogNUProgressionNet, Error,
				TEXT("이전 리비전 또는 같은 리비전의 다른 성장 상태를 거부합니다. Current=%lld Incoming=%lld 차이=[%s]"),
				Current.Revision,
				Incoming.Revision,
				*BuildProgressionStateDiff(Current, Incoming));
			OnRequestFailed.Broadcast(ResultCode, NSLOCTEXT("NUProgressionNet", "InvalidRevisionOrder", "서버 성장 상태의 리비전 순서가 올바르지 않습니다."));
			return false;
		}
	}

	Snapshot = MoveTemp(ParsedSnapshot);
	bHasSnapshot = true;
	OnSnapshotChanged.Broadcast();
	return true;
}

void UNUProgressionNetSubsystem::CompleteMutation(ENUPendingProgressionMutation ExpectedMutation, int32 ResultCode, const FString& ResultMessage, const FString& ResponseData)
{
	if (PendingMutation != ExpectedMutation)
	{
		UE_LOG(LogNUProgressionNet, Warning, TEXT("대기 중인 성장 요청과 다른 응답을 받았습니다. Pending=%d Response=%d"), static_cast<int32>(PendingMutation), static_cast<int32>(ExpectedMutation));
		return;
	}

	ApplyResponseSnapshot(ResultCode, ResponseData);
	PendingMutation = ENUPendingProgressionMutation::None;
	OnRequestStateChanged.Broadcast();
	if (ResultCode != ProgressionSuccess)
	{
		OnRequestFailed.Broadcast(ResultCode, NUProgressionServer::GetResultMessage(ResultCode, ResultMessage));
	}
}

void UNUProgressionNetSubsystem::ResetSession()
{
	Snapshot = FNUProgressionServerSnapshot();
	ActiveCharacterIdx = 0;
	PendingMutation = ENUPendingProgressionMutation::None;
	bHasSnapshot = false;
	bStateRequestPending = false;
}
