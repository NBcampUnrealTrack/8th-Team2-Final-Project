#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Net/NUProgressionServerTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUProgressionNetSubsystem.generated.h"

class UEmberServerManager;
enum class EEmberConnState : uint8;

DECLARE_MULTICAST_DELEGATE(FNUOnProgressionSnapshotChanged);
DECLARE_MULTICAST_DELEGATE(FNUOnProgressionRequestStateChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FNUOnProgressionRequestFailed, int32, const FText&);

// 성장 서버 요청과 받은 상태를 관리한다.
UCLASS()
class PROJECTM_API UNUProgressionNetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

private:
	enum class ENUPendingProgressionMutation : uint8
	{
		None,
		JobSelect,
		NodeAcquire
	};

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void BeginCharacterSession(int64 CharacterIdx);

	// 서버 요청: 조회(170), 직업 선택(171), 노드 획득(172)
	bool RequestProgression();
	bool RequestInitialJobSelection(int32 JobId);
	bool RequestNodeAcquire(int32 NodeId, int32 ChoiceAdvancementId);

	// 서버 응답 처리
	void HandleProgressionResponse(int32 ResultCode, const FString& ResultMessage, const FString& ResponseData);
	void HandleJobSelectResponse(int32 ResultCode, const FString& ResultMessage, const FString& ResponseData);
	void HandleNodeAcquireResponse(int32 ResultCode, const FString& ResultMessage, const FString& ResponseData);

	bool HasSnapshot() const { return bHasSnapshot; }
	const FNUProgressionServerSnapshot& GetSnapshot() const { return Snapshot; }
	bool IsStateRequestPending() const { return bStateRequestPending; }
	bool IsMutationPending() const { return PendingMutation != ENUPendingProgressionMutation::None; }
	bool IsExternalSessionActive() const;

	FNUOnProgressionSnapshotChanged OnSnapshotChanged;
	FNUOnProgressionRequestStateChanged OnRequestStateChanged;
	FNUOnProgressionRequestFailed OnRequestFailed;

private:
	UFUNCTION()
	void HandleServerStateChanged(EEmberConnState NewState);

	UEmberServerManager* GetServer() const;
	bool ApplyResponseSnapshot(int32 ResultCode, const FString& ResponseData);
	void CompleteMutation(ENUPendingProgressionMutation ExpectedMutation, int32 ResultCode, const FString& ResultMessage, const FString& ResponseData);
	void ResetSession();

	FNUProgressionServerSnapshot Snapshot;
	int64 ActiveCharacterIdx = 0;
	ENUPendingProgressionMutation PendingMutation = ENUPendingProgressionMutation::None;
	bool bHasSnapshot = false;
	bool bStateRequestPending = false;
};
