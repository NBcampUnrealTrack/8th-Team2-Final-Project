// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "Styling/SlateBrush.h"
#include "Tools/VFXViewer/NUVFXViewerTypes.h"
#include "NUVFXViewerDirector.generated.h"

class ANUBattleAvatar;
class SNUVFXViewerPanel;
class UNiagaraSystem;
class UTexture2D;

// 전투 VFX 뷰어의 진행 담당, 무대를 올리고 전투원을 세우고 스킬 연출을 재생함
// 연출 규칙은 넷 전투와 같은 함수(NUBattlePresentation)를 써서 게임과 어긋나지 않게 함
// 로그인 · 서버 · 필드 없이 돎 (ANUVFXViewerGameMode 가 스폰)
UCLASS(NotPlaceable)
class PROJECTM_API ANUVFXViewerDirector : public AActor
{
	GENERATED_BODY()

public:
	ANUVFXViewerDirector();

	virtual void Tick(float DeltaSeconds) override;

	/* 패널 · 단축키가 부르는 조작 */

	// 처음부터 틂, 멈춰 있었으면 멈춘 채로 처음에 섬 (설정을 바꿀 때마다 불림)
	void Play();

	// 처음부터 틀고 멈춰 있었으면 풂 (R)
	void Replay();

	void SetSpeed(float InSpeed);

	// 재생 · 일시정지 (Space), 끝난 뒤에 누르면 처음부터
	void TogglePause();

	// 사용자가 멈춰 둔 상태인지, 감는 동안 월드가 잠깐 도는 것은 치지 않음
	bool IsPaused() const;

	/* 재생 위치 (3D 화면 아래 진행바) */

	// 재생 시작 기준 지금 시각(초), 감는 중이면 가려는 시각
	float GetPlaybackTime() const;

	// 진행바 끝, 행동이 끝난 뒤 이펙트가 사라질 때까지 (한 번 끝까지 재생하면 잰 값을 씀)
	float GetTimelineLength() const;

	const FNUVFXViewerTimeline& GetTimeline() const { return Timeline; }

	// 그 시각으로 감음, 연출이 월드 시간으로만 흘러 거꾸로 돌릴 수 없으므로
	// 앞으로는 이어서 흘리고 뒤로는 처음부터 다시 흘려 그 시각에 멈춤 (FrameStep 간격, 자동 캡처와 같은 조건)
	void SeekTo(float Time);

	// 한 프레임씩, 누르면 멈춘 채로 옮김
	void StepFrames(int32 FrameCount);

	// 초 단위, 재생 중이면 옮긴 뒤 이어서 재생
	void StepSeconds(float Seconds);

	// 진행바를 끄는 동안은 멈춰 두고, 놓으면 끌기 전 상태(재생 · 일시정지)로 돌아감
	void BeginScrub();
	void EndScrub();

	bool IsSeeking() const;

	// 멀리 감는 동안 3D 칸을 덮는 직전 화면, 그동안은 월드를 안 그려 빨리 감음
	bool IsFreezeFrameVisible() const { return bFreezeVisible; }
	const FSlateBrush* GetFreezeFrameBrush() const { return &FreezeBrush; }

	// 3D 화면 칸 크기(Slate 단위), 패널이 진행바와 덮개를 이 칸에 맞춰 둠
	FVector2D GetViewAreaSlateSize() const { return ViewAreaSlateSize; }

	// 바뀐 에셋을 다시 읽음, -game 에서는 메모리에서 내린 뒤 디스크에서 다시 불러옴
	void RequestReload();

	void TogglePanel();
	bool IsPanelVisible() const { return bPanelVisible; }

	// 무대 · 전투원 구성이 바뀌는 설정, 전투원을 다시 세운 뒤 재생함
	void SetStage(int32 InStageIndex);
	void SetCaster(ENUVFXViewerCaster InCaster);
	void SetJob(int32 InJobId);
	void SetGender(int32 InGender);
	void SetHair(int32 InHairId);

	// 이 성별 · 직업의 전투 그림 몸 줄이 있는지 (패널이 직업 버튼을 거를 때)
	bool HasJobChoice(int32 InJobId, int32 InGender) const { return FindJobChoice(InJobId, InGender) != nullptr; }
	void SetAllyNaia(int32 InNaiaId);
	void SetEnemyNaia(int32 InNaiaId);
	void SetEnemyCount(int32 InCount);
	void SetTargetIndex(int32 InIndex);

	// 재생 내용만 바뀌는 설정
	void SetMode(ENUVFXViewerMode InMode);
	void SelectSkill(int32 InSkillId);
	void SetCustomSlot(int32 SlotIndex, const FSoftObjectPath& Path);
	void SetCustomColor(const FLinearColor& InColor);
	void SetCustomMotion(ENUBattleActionMotion InMotion);
	void SetCustomHitCount(int32 InCount);
	void SetPlayHitCue(bool bInPlayHitCue);
	void SetLoop(bool bInLoop);

	// 고른 스킬의 표 행을 직접 지정 칸으로 옮김, 한 칸만 바꿔 비교할 때 씀
	void CopySkillToCustom();

	// 임팩트만 고르면 게임에서도 안 뜸, 게임이 쓰는 보이지 않는 운반체 투사체를 채움
	void FillCarrierProjectile();

	/* 후보 비교 (형식은 Tools/VFXViewer/README.md "후보 비교")
	 * 후보는 공용 폴더(Tools/VFXViewer/Candidates/*.json, 커밋) · 개인 파일(Saved/VFXViewer/Candidates.json) ·
	 * -VFXCompare 로 준 파일을 모두 합쳐 읽음, 고른 후보(적용 대기)는 개인 것(Saved/VFXViewer/Pending.json) */

	// 그 파일을 후보 원본에 더하고 모든 원본을 다시 읽음, 그 파일에 후보가 있으면 true
	bool LoadCompareFile(const FString& Path);

	// 후보는 스킬 표 모드에서 고른 스킬에 딸림, 첫 칸(0)은 늘 표 그대로
	bool HasCandidates() const { return !CandidateSkillOrder.IsEmpty(); }
	int32 GetCandidateCount(int32 SkillId) const;
	FString GetCandidateLabel(int32 SkillId, int32 Index) const;
	FString GetCandidateNote(int32 SkillId, int32 Index) const;

	// 지금 스킬에서 틀고 있는 후보, 적용 대기에 담은 후보 (없으면 INDEX_NONE)
	int32 GetCandidateIndex() const { return CandidateIndex; }
	int32 GetChosenCandidate(int32 SkillId) const;

	// 지금 스킬의 그 후보를 스킬 표 행 위에 덮어 재생함
	void SelectCandidate(int32 Index);

	// 앞 · 뒤 후보, 끝에서 처음으로 돎
	void StepCandidate(int32 Delta);

	// 후보가 있는 앞 · 뒤 스킬로 (후보 파일 순서), 그 스킬에서 고른 후보가 있으면 그것부터
	void StepCandidateSkill(int32 Delta);

	// 지금 후보를 적용 대기에 담음 (스킬마다 하나), "지금"(표 그대로)이면 그 스킬을 대기에서 뺌
	void ChooseCandidate();

	// 재생이 끝날 때마다 다음 후보로 넘어가며 계속 틂
	void SetCompareCycle(bool bInCycle);
	bool IsCompareCycle() const { return bCompareCycle; }

	const FString& GetCompareMessage() const { return CompareMessage; }

	/* 적용 대기 (Saved/VFXViewer/Pending.json, 3단계 [적용] 이 이 목록을 DT 에 씀) */

	const TArray<FNUVFXViewerPendingChange>& GetPendingChanges() const { return PendingChanges; }

	// 목록이 바뀔 때마다 오름, 패널이 이걸 보고 다시 그림
	int32 GetPendingVersion() const { return PendingVersion; }

	// 칸별 "지금 → 바꿀 값" 과 알림 (표에 행 없음 · 후보 파일에서 못 찾음), 지금은 뷰어가 시작할 때 읽은 표
	TArray<FString> DescribePendingChange(const FNUVFXViewerPendingChange& Change) const;

	// 스킬 목록의 이름 ("3002  발현"), 목록에 없으면 "스킬 3002"
	FString GetSkillLabel(int32 SkillId) const;

	// 그 스킬로 가서 담은 후보를 틂
	void ViewPendingChange(int32 SkillId);
	void RemovePendingChange(int32 SkillId);
	void ClearPendingChanges();

	/* 적용 (원본 CSV → DT_SkillVFX, CSV 경로는 DT 가 기억하는 원본 파일 / 기본 공격은 DefaultGame.ini 의 JobAttackVFX) */

	// 적용 대기를 원본 CSV 에 씀: 확인 창 → 백업 · 이력 → CSV → 뷰어의 표 → DT 다시 채우기
	// 에디터가 켜져 있으면 CSV 만 쓰고 안내 창을 띄움 (에디터가 같은 DT 를 들고 있어 뒤에서 저장하면 서로 덮어씀)
	// 기본 공격(스킬 0) 후보는 담을 때 보던 직업의 ini 항목에 씀 (DT 다시 채우기 없음, 뷰어 안의 설정도 바로 바꿈)
	// bAskUser = false 는 창 없이 (콘솔 nu.VFX.Apply yes, 사람이 이미 확인한 뒤 스크립트 · AI 가 부를 때), 내용은 로그로
	void ApplyPendingChanges(bool bAskUser = true);

	// 마지막 적용(되돌리지 않은 것)을 그 전 값으로 되돌림, 그 뒤에 다시 바뀐 행이 있으면 확인 창에 알림
	void RevertLastApply(bool bAskUser = true);
	bool CanRevertLastApply() const { return !LastApplyHistory.IsEmpty(); }
	const FString& GetLastApplySummary() const { return LastApplySummary; }

	// 원본 CSV 로 DT_SkillVFX 를 다시 채워 저장함 (화면 없는 에디터로 커맨드렛, 1~2분)
	void ReimportSkillVFXTable();
	bool IsReimportingTable() const { return ReimportProcess.IsValid(); }

	// DT 가 기억하는 원본 MD5 와 지금 CSV 가 다름 (적용 뒤 아직 안 채움 · 받기만 하고 안 채움 · 손으로 고침)
	bool NeedsTableReimport() const { return bTableNeedsReimport; }

	// 적용 · 되돌리기 · 다시 채우기 상태 한 줄
	const FString& GetTableStatus() const { return TableStatus; }

	/* 패널이 읽는 상태 */

	const FNUVFXViewerSetup& GetSetup() const { return Setup; }
	const FNUVFXViewerPlayReport& GetReport() const { return Report; }
	const TArray<TSharedPtr<FNUVFXViewerSkillItem>>& GetSkillItems() const { return SkillItems; }
	const TArray<TSharedPtr<FNUVFXViewerAssetItem>>& GetAssetItems() const { return AssetItems; }
	const TArray<FNUVFXViewerChoice>& GetStageChoices() const { return StageChoices; }
	const TArray<FNUVFXViewerChoice>& GetJobChoices() const { return JobChoices; }
	const TArray<FNUVFXViewerChoice>& GetNaiaChoices() const { return NaiaChoices; }
	FString GetChoiceLabel(const TArray<FNUVFXViewerChoice>& Choices, int32 Id) const;

	// 상단 상태 줄 (준비 중 · 재생 시간 · 새로고침 결과)
	FText GetStatusText() const;

	// 정보 칸 (세 칸 경로 · 색 · 이동 · 시간표 · 경고)
	FText GetReportText() const;

	// 패널 폭(Slate 단위), 3D 화면을 그 옆에 둠
	static constexpr float PanelWidth = 430.f;

	// 3D 화면 아래 진행바 줄 높이(Slate 단위)
	static constexpr float TransportHeight = 52.f;

	// 감기 · 프레임 넘기기 한 칸 (자동 캡처의 고정 60fps 와 같음)
	static constexpr float FrameStep = 1.f / 60.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class EState : uint8
	{
		LoadingStage,
		Ready,
		Reloading
	};

	/* 준비 */

	void BuildChoiceLists();
	void BuildSkillItems();
	void BuildAssetItems();
	void LoadSettings();
	void SaveSettings() const;
	void ParseCommandLine();

	void BeginStageLoad();
	void FinishStageLoad();
	void CreatePanel();
	void RemovePanel();

	// 3D 화면을 패널 오른쪽 16:9 칸으로 두고 그 아래에 진행바 줄을 남김, 패널을 숨기면 화면 전체
	void UpdateViewLayout();

	// 창(제목 표시줄 포함)이 작업 영역을 넘으면 줄여서 안에 둠
	// -game 은 DPI 를 모르는 프로세스라 150% 배율에서 요청 해상도가 작업 영역에 맞춰 줄면서 창 안쪽이 맨 위에 붙어
	// 제목 표시줄이 화면 밖으로 나감 (엔진은 창 안쪽 크기만 작업 영역에 맞춤 - SWindow::Construct)
	bool FitWindowToWorkArea();

	/* 전투원 */

	void SpawnAvatars();
	void DestroyAvatars();
	ANUBattleAvatar* SpawnAvatar(ENUVFXViewerCaster Kind, int32 SlotIndex, int32 SlotCount, int32 CombatantId);
	ANUBattleAvatar* GetCaster() const;

	// 직업 선택지는 (직업, 성별) 쌍, 같은 직업이 남 · 여 두 줄일 수 있음
	const FNUVFXViewerChoice* FindJobChoice(int32 InJobId, int32 InGender) const;

	// 지금 성별에 그 직업이 없으면 그 성별의 첫 직업으로, 성별에 직업이 하나도 없으면 다른 성별로
	void ValidateJobChoice();
	ANUBattleAvatar* GetTarget() const;
	void ClampTargetIndex();

	/* 재생 */

	// 넷 전투(UNUBattleNetActionQueueSubsystem::BuildMotionSpec)와 같은 순서로 스펙을 만듦
	FNUBattleActionMotionSpec BuildSpec(int32& OutHitCount, bool& bOutDealsDamage, bool& bOutHasProfile) const;

	// 무대를 깨끗이 한 뒤 처음부터 한 번 흘림, Play · 반복 · 되감기가 같이 씀
	void StartPlayback();

	// 남은 이펙트 · 투사체 · 피격 반응 · 모션 · 화면 흔들림 · 암전을 지움 (되감은 화면에 앞 재생이 섞이지 않게)
	void ResetPlaybackScene();

	void BuildTimeline();
	double GetElapsed() const;

	// 묶음 캡처가 기준으로 삼는 첫 타격, 화면에 보이는 타격 모션 시각 (제자리 행동은 게임의 타격 처리 시각보다 이름)
	float GetShotAnchorTime() const;
	void StopPlayback();
	void PlayHitCue();
	void CollectWarnings(const FNUBattleActionMotionSpec& Resolved);
	bool HasLiveEffects() const;
	void DestroyLiveEffects();

	// -game 에서 불러온 패키지의 파일 핸들을 놓음, 안 놓으면 에디터 저장이 오류 32 로 실패함
	void ReleaseFileHandles();

	void TickReload();
	void TickAutomation();
	void RequestShot(int32 ShotIndex);

	/* 감기 */

	// 직전 화면 찍기 → 고정 간격으로 흘리기
	enum class ESeekPhase : uint8
	{
		None,
		Capture,
		Run
	};

	void TickSeek();
	void FinishSeek();

	// 설정이 바뀌어 처음부터 다시 틀 때, 감던 것을 버리고 시간 · 그리기 설정을 되돌림
	// bApplyPauseIntent 면 감는 동안 잠깐 풀어 둔 월드를 사용자가 바란 상태(재생 · 일시정지)로 둠
	void CancelSeek(bool bApplyPauseIntent = true);

	// 다음 프레임의 월드 시간을 정확히 Step 초 흘림 (엔진 고정 시간 간격을 잠깐 빌림)
	void StepWorldNextFrame(float Step);
	void RestoreTimeStep();

	// 바라는 시간 간격을 프레임 시작에 엔진에 넣음 (FCoreDelegates::OnBeginFrame)
	void HandleBeginFrame();
	void ApplyTimeStep();
	void SetWorldRenderingEnabled(bool bEnabled);
	void SetWorldPaused(bool bPaused);

	// 지금 그리는 3D 칸을 화면 캡처 신호로 받아 덮개 그림으로 만듦
	void RequestFreezeCapture();
	void HandleFreezeCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors);
	void ShowFreezeFrame();
	void HideFreezeFrame();

public:
	// 지금 화면을 한 장 찍음 (콘솔 nu.VFX.Shot), 경로를 돌려줌
	FString TakeShotNow();

private:
	void HandleScreenshotProcessed();
	void WriteAutomationReport() const;
	TSharedRef<class FJsonObject> BuildReportJson() const;

	// -VFXSkills 묶음 캡처, 스킬마다 예열 한 번 → 본 재생 → 첫 타격 기준 시각에 찍기
	void TickSkillQueue();

	/* 자동 캡처 점검 기록 (report.json) */

	// 지금 뜬 나이아가라를 에셋별로 모음 (처음 · 마지막으로 보인 시각, 입자 수, 처음 화면 위치)
	void SampleEffects();

	// 월드 위치를 3D 칸 안 비율(0~1)로, 화면 뒤면 (-1, -1)
	FVector2D ProjectToView(const FVector& WorldLocation) const;

	// 아직 안 끝난 나이아가라와 그 이미터 (입자 수 · 남은 입자의 수명), 잔상 대기 상한에 걸렸을 때 씀
	TArray<FString> DescribeLingeringEffects() const;

	static FString DescribeAsset(const TSoftObjectPtr<UNiagaraSystem>& Asset);

private:
	EState State = EState::LoadingStage;
	FNUVFXViewerSetup Setup;
	FNUVFXViewerPlayReport Report;

	TArray<FNUVFXViewerChoice> StageChoices;
	TArray<FNUVFXViewerChoice> JobChoices;
	TArray<FNUVFXViewerChoice> NaiaChoices;
	TArray<TSharedPtr<FNUVFXViewerSkillItem>> SkillItems;
	TArray<TSharedPtr<FNUVFXViewerAssetItem>> AssetItems;

	UPROPERTY(Transient)
	TObjectPtr<ANUBattleAvatar> PlayerAvatar;

	UPROPERTY(Transient)
	TObjectPtr<ANUBattleAvatar> AllyNaiaAvatar;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANUBattleAvatar>> EnemyAvatars;

	TSharedPtr<SNUVFXViewerPanel> Panel;
	bool bPanelVisible = true;

	// 무대 레벨이 보일 때까지 기다리는 중 재생 요청이 오면 끝난 뒤 재생
	bool bPlayQueued = false;

	// 재생 시각은 월드 시간(속도 · 일시정지 반영)
	bool bPlaying = false;
	double PlayStartTime = 0.0;
	int32 RemainingHitCues = 0;
	FTimerHandle HitCueTimer;

	// 진행바
	FNUVFXViewerTimeline Timeline;

	// 이펙트까지 다 사라진 시각, 같은 연출(PlaybackSignature)을 다시 틀면 유지
	float MeasuredEndTime = 0.f;
	FString PlaybackSignature;

	// 재생이 끝난 뒤에도 진행바가 가리킬 마지막 시각
	float LastElapsed = 0.f;

	/* 감기 */

	ESeekPhase SeekPhase = ESeekPhase::None;
	float SeekTarget = 0.f;

	// 다 감은 뒤 이어서 재생할지 (아니면 그 시각에 멈춤)
	bool bResumeAfterSeek = false;

	// 진행바를 끄는 중, 놓을 때 되돌릴 상태
	bool bScrubbing = false;
	bool bResumeAfterScrub = false;

	// 다음 프레임에 쓸 고정 시간 간격 (감는 동안만)
	bool bWantFixedTimeStep = false;
	double WantedFixedDeltaTime = 0.0;
	FDelegateHandle BeginFrameHandle;

	// 빌린 고정 시간 간격을 돌려줄 값 (자동 캡처는 원래 고정 60fps)
	bool bTimeStepOverridden = false;
	bool bSavedUseFixedTimeStep = false;
	double SavedFixedDeltaTime = 0.0;

	bool bWorldRenderingDisabled = false;

	// 덮개 그림
	FDelegateHandle FreezeCaptureHandle;
	TArray<FColor> FreezePixels;
	FIntPoint FreezeSize = FIntPoint::ZeroValue;
	bool bFreezeCaptured = false;
	bool bFreezeVisible = false;
	int32 FreezeWaitFrames = 0;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FreezeTexture;

	FSlateBrush FreezeBrush;

	// 3D 칸 (UpdateViewLayout 이 정함)
	FIntRect ViewAreaPixels;
	FVector2D ViewAreaSlateSize = FVector2D::ZeroVector;

	// 창 맞추기는 처음 한 번만 (사용자가 키운 창은 그대로 둠)
	bool bWindowFitDone = false;

	// 새로고침 진행, 가비지 컬렉션이 끝날 때까지 몇 프레임 기다림
	int32 ReloadFramesLeft = 0;
	TArray<FSoftObjectPath> ReloadWatchList;
	FString ReloadMessage;

	/* 자동 캡처 (-VFXShotAt) */

	bool bAutomation = false;
	bool bAutomationQuit = false;

	// -VFXCmds="명령1;명령2": 준비된 뒤 1초 간격으로 콘솔 명령을 차례로 실행 (스크립트 · AI 가 후보 담기 · 적용 · 되돌리기를 시험)
	// -VFXQuit 를 같이 주면 다 돌고 DT 다시 채우기까지 끝난 뒤 종료 (자동 캡처가 있으면 캡처 쪽이 종료)
	TArray<FString> ReadyCommands;
	double NextReadyCommandTime = 0.0;
	bool bQuitAfterReadyCommands = false;
	void TickReadyCommands();
	bool bShotWithUI = false;

	// -VFXShotReload: 다 찍은 뒤 새로고침하고 같은 시각을 한 번 더 찍음 (새로고침 확인용)
	bool bShotReloadPass = false;
	bool bReloadPassDone = false;

	/* 첫 사용 끊김 측정 (게임과 같은 조건으로 재기 위한 스위치) */

	// -VFXNoPrepare: 뷰어가 미리 불러 두거나 컴파일을 기다리지 않음 (게임의 첫 시전과 같은 조건)
	bool bNoPrepare = false;

	// -VFXAsyncPreload: 시작할 때 전투 VFX 미리 불러오기(UNUBattleVFXPreloadSubsystem)로 고른 스킬을 부름
	bool bAsyncPreload = false;

	// -VFXNoWarmUp: 예열 재생 없이 첫 재생을 바로 찍음
	bool bNoWarmUp = false;

	// -VFXHitchLog: 40ms 넘는 프레임을 재생 시각과 함께 로그로 남김
	bool bHitchLog = false;
	double LastTickRealTime = 0.0;

	// -VFXSeekShots: 진행바처럼 감아서 찍음 (감기가 재생과 같은 화면을 내는지 확인)
	bool bSeekShots = false;
	bool bSeekShotStarted = false;
	int32 SeekShotSettleFrames = 0;

	// 감기 한 번에 걸린 실제 시간 · 흘린 칸 수 (로그)
	double SeekStartRealTime = 0.0;
	int32 SeekStepCount = 0;

	/* 묶음 캡처 (-VFXSkills=스킬[:시전자],... -VFXShotAtHit=초,...) */

	// 시전자: 비우면 지금 설정, 숫자 = 그 직업 캐릭터(뒤에 f · m 을 붙이면 그 성별), n<번호> = 그 아군 나이아, e<번호> = 그 적 나이아 (번호를 빼면 지금 나이아)
	struct FQueueEntry
	{
		int32 SkillId = 0;
		bool bCasterGiven = false;
		ENUVFXViewerCaster Caster = ENUVFXViewerCaster::Player;
		int32 JobId = INDEX_NONE;

		// 직업 뒤 f · m 으로 고른 성별 (1 = 여, 0 = 남), 없으면 남캐부터
		int32 Gender = INDEX_NONE;
		int32 NaiaId = INDEX_NONE;

		// 후보 비교 항목이면 그 묶음 · 후보 번호
		int32 CompareSet = INDEX_NONE;
		int32 CompareVariant = INDEX_NONE;
	};

	// "스킬:시전자" 의 시전자 표기를 읽음 (숫자 = 직업, n<번호> = 아군 나이아, e<번호> = 적 나이아, 비우면 지금 설정)
	static void ParseQueueCaster(const FString& CasterText, FQueueEntry& OutEntry);

	// 항목의 시전자 · 직업 · 나이아를 설정에 넣음, 무대가 서 있으면 바뀐 전투원을 다시 세움
	void ApplyQueueEntry(const FQueueEntry& Entry);

	TArray<FQueueEntry> SkillQueue;
	int32 SkillQueueIndex = 0;
	int32 SkillQueuePhase = 0;

	// 첫 타격(GetShotAnchorTime) 기준 상대 초, 스킬마다 첫 타격 시각이 달라 절대 시각 대신 씀
	TArray<float> ShotOffsetsFromHit;

	// 묶음 캡처 중인 스킬, 파일 이름 앞에 붙음
	int32 QueueSkillId = INDEX_NONE;

	// -VFXShotUI 와 같이 주면 패널을 끝까지 내려 찍음 (-VFXShotPanelEnd)
	bool bShotPanelEnd = false;

	// 묶음 캡처 중인 비교 후보(후보 파일의 묶음 · 후보 번호), 파일 이름에 _v<번호> 로 붙음
	int32 QueueCompareSet = INDEX_NONE;
	int32 QueueVariantIndex = INDEX_NONE;

	/* 후보 비교 */

	// 지금 재생에 덮을 후보, 스킬 표 모드에서 후보가 있는 스킬을 틀 때만
	const FNUVFXViewerVariant* GetActiveVariant() const;
	const FNUVFXViewerCandidate* FindCandidate(int32 SkillId, int32 Index) const;

	// 후보 파일의 묶음 · 후보가 그 스킬 후보 목록의 몇 번째인지, 표 그대로인 후보는 첫 칸
	int32 FindCandidateIndex(int32 SkillId, int32 SetIndex, int32 VariantIndex) const;

	// 후보를 적은 묶음의 시전자로 맞춤 (같은 스킬이면 후보끼리 조건이 같게)
	void ApplyCandidateCaster(int32 SkillId);

	// 후보 파일이 바뀌면(AI 가 후보를 더하거나 고침) 다시 읽고 지금 후보를 다시 틂
	void TickCompareFileWatch();

	// 후보 원본 파일 목록 (-VFXCompare 파일 → 공용 폴더 이름순 → 개인 파일), 있는 파일만
	TArray<FString> GatherCompareFiles() const;

	// 원본을 모두 다시 읽어 합치고 스킬별 후보 · 고른 기록을 다시 만듦
	bool ReloadCompareSources();

	// 후보 파일 하나를 읽어 묶음을 더함, 못 읽으면 OutError
	bool ParseCompareFile(const FString& FullPath, TArray<FNUVFXViewerCompareSet>& OutSets,
		TArray<FString>& OutMissingNames, FString& OutError) const;

	// 후보 · 적용 대기가 같이 쓰는 칸 값 (cast · projectile · impact · color), 후보는 label · note 도
	void ReadVariantFields(const TSharedPtr<class FJsonObject>& Object, FNUVFXViewerVariant& OutVariant,
		TArray<FString>& OutMissingNames) const;
	static TSharedRef<class FJsonObject> WriteVariantOverrides(const FNUVFXViewerVariant& Variant);

	// 적용 대기 한 건이 지금 읽은 후보의 몇 번째인지 (같은 파일 · 같은 이름 먼저, 없으면 같은 이름)
	int32 FindPendingCandidate(const FNUVFXViewerPendingChange& Change) const;

	// 지금 표 행과 적용 뒤 행, 표에 행이 없으면 bOutHasRow = false (빈 행 위에 덮음)
	void BuildPendingRows(const FNUVFXViewerPendingChange& Change, struct FNUSkillVFXProfile& OutBefore,
		struct FNUSkillVFXProfile& OutAfter, bool& bOutHasRow) const;

	/* 기본 공격 (스킬 0) - DT 가 아니라 DefaultGame.ini 의 JobAttackVFX, 직업마다 한 항목 */

	FString GetJobAttackIniPath() const;

	// 게임이 지금 쓰는 직업 기본 공격 연출 (뷰어가 시작할 때 읽은 ini, 적용 · 되돌리기가 고침), 항목이 없으면 false
	bool FindJobAttackRow(int32 JobId, struct FNUVFXViewerTableRow& OutRow) const;

	// 뷰어 안의 설정을 바꿈 (적용 · 되돌리기 뒤 바로 그 값으로 틂), Row 가 없으면 그 직업 항목을 지움
	void SetJobAttackRow(int32 JobId, const struct FNUVFXViewerTableRow* Row);

	// 기본 공격 적용 대기 한 건의 설명 (직업 항목의 칸별 "지금 -> 바꿀 값")
	TArray<FString> DescribeJobAttackChange(const FNUVFXViewerPendingChange& Change) const;

	FString GetPendingPath() const;
	void LoadPendingChanges();
	void SavePendingChanges() const;

	/* 적용 */

	class UDataTable* GetSkillVFXTable() const;
	FString GetSkillVFXCsvPath() const;

	// 되돌릴 수 있는 마지막 적용 이력과 DT · CSV 가 맞는지 다시 봄 (시작할 때 · 적용 · 되돌리기 뒤)
	void RefreshTableState();

	// 뷰어가 들고 있는 DT 를 CSV 내용으로 바꿈 (지금 값 · 스킬 목록이 바로 따라옴, 디스크의 DT 는 다시 채우기가 씀)
	bool UpdateViewerTable(const class FNUVFXViewerTableFile& File, FString& OutError);

	// 새 행 이름, 기존 행과 같은 규칙 (직업 Job_<번호> · 보스 Boss_<번호> · 나이아 <번호>)
	FString MakeNewRowName(int32 SkillId) const;

	// CSV · ini 를 Saved/VFXViewer/Backups 에 복사하고 그 경로를 돌려줌
	FString BackupFile(const FString& SourcePath, const FString& Stamp) const;

	// 이 프로젝트를 연 에디터 창이 있는지 (창 제목 "<프로젝트> - Unreal Editor")
	bool IsProjectEditorOpen() const;

	// 적용 · 되돌리기 뒤 DT 다시 채우기, 에디터가 켜져 있으면 안내 창만
	void FinishTableWrite(const FString& HistoryPath, const FString& Summary, bool bAskUser);

	// 창을 띄우거나(bAskUser) 로그만 남김, 창이 없으면 확인은 늘 승인
	bool AskOrLog(bool bAskUser, EAppMsgType::Type Type, const FString& Message) const;

	void TickTableReimport();
	void SetHistoryField(const FString& HistoryPath, const FString& Field, const FString& Value) const;

	// 전체 경로(/Game/...) · 이름(NS_X) 어느 쪽이든 받아 에셋 경로로, 이름은 /Game/VFX 목록에서 찾음
	FSoftObjectPath ResolveVariantAsset(const FString& Text) const;

	TArray<FNUVFXViewerCompareSet> CompareSets;

	// -VFXCompare · nu.VFX.Compare 로 준 파일, 공용 · 개인 원본은 찍기가 아닐 때만 읽음
	TArray<FString> CompareExplicitFiles;
	bool bUseDefaultCompareSources = false;

	// 마지막으로 읽은 원본 파일과 수정 시각, 하나라도 바뀌거나 늘거나 줄면 다시 읽음
	TMap<FString, FDateTime> CompareFileStamps;
	double NextCompareWatchTime = 0.0;

	// 스킬별 후보 (모든 묶음에서 모음, 첫 칸은 표 그대로), 후보가 있는 스킬은 후보 파일에 나온 순서
	TMap<int32, TArray<FNUVFXViewerCandidate>> CandidatesBySkill;
	TArray<int32> CandidateSkillOrder;

	// 지금 스킬에서 틀고 있는 후보, 다른 스킬을 고르면 표 그대로(0)로 돌아감
	int32 CandidateIndex = 0;

	// 적용 대기, 담은 순서대로 (Saved/VFXViewer/Pending.json)
	TArray<FNUVFXViewerPendingChange> PendingChanges;
	int32 PendingVersion = 0;

	// 되돌릴 수 있는 마지막 적용 이력 (Saved/VFXViewer/History/Apply_<시각>.json) 과 그 요약
	FString LastApplyHistory;
	FString LastApplySummary;

	bool bTableNeedsReimport = false;

	// DT 다시 채우기 커맨드렛 프로세스, 그 결과를 남길 이력 파일과 시작 시각
	FProcHandle ReimportProcess;
	FString ReimportHistory;
	double ReimportStartTime = 0.0;

	FString TableStatus;

	bool bCompareCycle = false;

	// 패널에 보이는 마지막 비교 소식 (읽음 · 못 읽음 · 고름)
	FString CompareMessage;

	// 스킬마다 남긴 보고, 끝에 report.json 의 entries 로 씀
	TArray<TSharedPtr<class FJsonValue>> QueueReports;

	/* 점검 기록 — 자동 캡처의 재생마다 새로 잼 */

	struct FEffectSample
	{
		float FirstSeen = -1.f;
		float LastSeen = -1.f;
		int32 MaxParticles = 0;
		FVector2D FirstScreen = FVector2D(-1.0, -1.0);

		// 처음 보인 순간의 시전자 위치 (앞으로 나가 시전하는 행동은 제자리와 다름)
		FVector2D CasterScreenAtFirst = FVector2D(-1.0, -1.0);
	};

	// 에셋 경로 → 기록
	TMap<FString, FEffectSample> EffectSamples;

	// 시전자 · 대상의 화면 위치 (3D 칸 안 비율), 이펙트 위치와 견줌
	FVector2D CasterScreen = FVector2D(-1.0, -1.0);
	FVector2D TargetScreen = FVector2D(-1.0, -1.0);

	// 재생이 끝난 시각과, 잔상 대기 상한에 걸려 끝났는지 (끝나지 않는 이미터)
	float PlaybackEndTime = -1.f;
	bool bEndedAtTailCap = false;
	TArray<FString> LingeringEffects;
	TArray<float> ShotTimes;
	FString ShotDirectory;
	int32 NextShotIndex = 0;
	bool bShotPending = false;
	bool bWarmUpStarted = false;
	bool bWarmUpDone = false;

	// 무대가 뜬 뒤 기다린 프레임 수
	int32 AutomationSettleFrames = 0;
	TArray<FString> WrittenShots;
	FDelegateHandle ScreenshotProcessedHandle;
};
