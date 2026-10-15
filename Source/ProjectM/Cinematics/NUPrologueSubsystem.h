#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NUPrologueSubsystem.generated.h"

class ANUPlayerController;
class ANUStorySequenceCue;
class SWidget;
class UGameViewportClient;
class USoundMix;
struct FStreamableHandle;

// 새 캐릭터가 처음 필드에 들어오면 게임 시작 프롤로그(UNUPrologueSettings::PrologueData)를 한 번 재생한다.
// 대상이면 로딩 화면이 떠 있는 동안 그 아래에 검은 화면을 깔고 입력·HUD·레벨 BGM 을 막고 에셋을 미리 읽는다.
// 그래서 로딩 화면이 걷히면 필드가 비치지 않고 곧바로 프롤로그로 이어진다(시퀀스도 검은 화면에서 시작한다).
// 로딩·서버 입장 확정이 끝나면 시작하고, 끝나면 입력·HUD를 돌려주고 검은 화면에서 밝아진다.
// 본 기록은 이 PC의 세이브 슬롯에 캐릭터별로 남긴다. 콘솔 nu.Prologue.Play 로 언제든 다시 볼 수 있다.
UCLASS()
class PROJECTM_API UNUPrologueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// 조건과 본 기록을 따지지 않고 지금 재생한다. 메뉴의 '프롤로그 다시 보기'도 이걸 부르면 된다.
	UFUNCTION(BlueprintCallable, Category = "NU|Prologue")
	bool PlayPrologue();

	UFUNCTION(BlueprintPure, Category = "NU|Prologue")
	bool IsPlaying() const { return Cue != nullptr; }

	// 이 PC의 본 기록을 지운다(시험용).
	static void ResetSeenRecords();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UFUNCTION()
	void HandlePrologueFinished(bool bCompleted);

	enum class ECandidate : uint8
	{
		NotYet,	// 아직 판단할 수 없음(컨트롤러·선택 캐릭터 없음)
		No,		// 자동 재생 대상이 아님
		Yes,
	};

	// 이 캐릭터가 자동 재생 대상인지. 로딩 중에도 판단한다.
	ECandidate CheckCandidate(ANUPlayerController*& OutController, FString& OutCharacterKey);
	// 로딩이 끝나고 서버 입장이 확정되고 에셋을 다 읽어 지금 시작할 수 있는지
	bool IsReadyToStart(const ANUPlayerController* Controller) const;

	// 시작 전 준비: 검은 화면을 덮고 입력·HUD·레벨 BGM을 막고 에셋을 미리 읽는다.
	void PrepareAutoPlay(ANUPlayerController* Controller);
	// 준비를 거둔다(입장 실패, 시작 실패, 월드 종료).
	void CancelAutoPlay();
	void StartPreload();
	bool IsPreloaded() const;
	void PushEarlyMute(USoundMix* Mix);
	void PopEarlyMute();
	void ShowCover();
	void HideCover();

	void ReleasePlayer(bool bFadeIn);

	static bool HasSeen(const FString& CharacterKey);
	static void MarkSeen(const FString& CharacterKey);

	UPROPERTY(Transient)
	TObjectPtr<ANUStorySequenceCue> Cue;

	TWeakObjectPtr<ANUPlayerController> PlayingController;

	// 끝까지 보거나 스킵하면 이 키를 본 기록에 남긴다. 다시 보기는 비어 있다.
	FString PlayingCharacterKey;
	FString PendingCharacterKey;

	// 이 월드에서 자동 재생 판단을 끝냈다(재생했거나 대상이 아님).
	bool bAutoPlayResolved = false;
	float ReadySeconds = 0.0f;

	// 준비(검은 화면·입력 막기)를 한 컨트롤러. 시작하면 재생 쪽으로 넘어간다.
	TWeakObjectPtr<ANUPlayerController> PreparedController;
	bool bPrepared = false;
	// 로딩이 끝난 뒤 시작 조건을 기다린 시간. 너무 길면 포기하고 화면을 돌려준다.
	float WaitAfterLoadingSeconds = 0.0f;

	TSharedPtr<FStreamableHandle> DataHandle;
	TSharedPtr<FStreamableHandle> SequenceHandle;

	// 로딩 화면 뒤에서부터 레벨 BGM 을 꺼 두는 믹스. 시퀀스가 같은 믹스를 걸면 내려놓는다.
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> EarlyMuteMix;

	TSharedPtr<SWidget> Cover;
	TWeakObjectPtr<UGameViewportClient> CoverViewport;
	// 프롤로그 화면이 한 번 그려진 뒤 검은 덮개를 걷기까지 남은 시간
	float CoverHideCountdown = -1.0f;
};
