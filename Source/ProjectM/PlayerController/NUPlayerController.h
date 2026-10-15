#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/Data/NUInteractionTypes.h"
#include "Net/EmberServerManager.h"
#include "Quest/Data/NUQuestTypes.h"

#include "NUPlayerController.generated.h"

class UNUChatWidget;
class UNUChatSubsystem;
class UNUPartyListWidget;
class UNUPartyPromptWidget;
class UNUPartySubsystem;
class UNUPlayerContextMenu;
enum class ENUChatChannel : uint8;
struct FNUPlayerContextTarget;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class UNUParryCueWidget;
class UNUCombatPopupLayer;
class UNUActionBannerWidget;
enum class ENUCombatPopupType : uint8;
class UNUBattleResultWidget;
class UNUParryComponent;
struct FNUBattleResultView;
class UNU_BattleHUDWidget;
class UNU_BattleSwitcherWidget;
class UNU_FieldHUDWidget;
class UNU_BaseFieldWidget;
class ANUBattleTargetIndicator;
class ANUBattleEncounter;
class ANUPlayerState;
class UNUAbilitySystemComponent;
class UNUInteractorComponent;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
class UNUQuestWidget;
class UNUQuestJournalWidget;
class UNUDialogueDefinition;
class UNUQuestDialogueDefinition;
class UNUBattleCommandComponent;
class UNUBattleCommandComponentBase;
class UNUBattleNetCommandComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUBattleCommandUIChanged);

UCLASS()
class PROJECTM_API ANUPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(Client, Reliable)
	void ClientRecoveryNotice(const FText& Message);

	UFUNCTION(Client, Reliable)
	void ClientRecoveryFade(bool bFadeOut);
	UFUNCTION(Client, Reliable)
	void ClientHoldRecoveryFade();

	UFUNCTION(Client, Reliable)
	void ClientCompleteRecovery(FVector Destination);
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Recovery|Presentation")
	void OnRecoveryStarted();
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Recovery|Presentation")
	void OnRecoveryWaking(float DurationSeconds);

	ANUPlayerController();

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	UNUBattleCommandComponentBase* GetBattleCommandComponent() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Parry")
	UNUParryComponent* GetParryComponent() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|Command")
	ENUBattleRootAction GetSelectedRootAction() const;

	UFUNCTION(BlueprintPure, Category = "NU|Battle|UI")
	UUserWidget* GetBattleCommandWidget() const;

	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Command")
	void SelectBattleRootAction(ENUBattleRootAction Action);

	// 나이아 꺼내기/집어넣기, 지금은 블루프린트 Key 바인딩을 호출함
	UFUNCTION(BlueprintCallable, Category = "NU|Naia")
	void ToggleNaiaSummon();

	void SubmitBattleCommandFromComponent(FGuid EncounterId, const FNUBattleCommandRequest& Request);

	// 패링 관련
	// 서버가 표적의 소유자에게만 보내므로 표적이 아닌 플레이어에게는 도착하지 않음
	UFUNCTION(Client, Reliable)
	void ClientOpenParryWindow(int32 ActionSequence, float ImpactOffset, const FNUParryTimingSpec& Spec,
		int32 SPCost);
	void CloseParryWindow();

	// 패링 입력이 판정으로 가는 길을 가름
	// 넷 전투는 클라가 판정해 서버에 보고하고, 로컬 전투는 인카운터(권위)가 판정함
	// 창을 연 쪽이 어느 쪽인지 정해지므로 여기서는 열려 있는 창을 보고 고름
	void RouteParryPress(int32 ForActionSequence, float PressOffset);

	// 회피 관련, 패링과 달리 타이밍 판정이 없어 창이 열려 있는 동안 누르기만 하면 됨
	// WindowDuration이 지나면 스스로 닫힘, 패링은 UNUParryComponent의 틱이 같은 일을 함
	UFUNCTION(Client, Reliable)
	void ClientOpenDodgeWindow(int32 ActionSequence, int32 SPCost, float WindowDuration);

	void CloseDodgeWindow();

	// 회피 입력이 판정으로 가는 길을 가름, RouteParryPress 와 같은 규칙
	// 넷 전투는 클라가 확률을 굴려 서버에 보고하고, 로컬 전투는 인카운터(권위)가 굴림
	void RouteDodgePress(int32 ForActionSequence);

	// 전투 결과창을 띄움
	// 부른 쪽은 이 값이 true일때만 전투 세트 종료를 미뤄야 함
	bool ShowBattleResult(const FNUBattleResultView& InView, FSimpleDelegate OnDismissed);
	void CloseBattleResult();

	// 전투 아바타 머리 위 팝업 글자, 전투 아바타의 멀티캐스트에서 호출함
	void ShowCombatPopup(AActor* AnchorActor, const FNUCombatPopupData& PopupData);

	// 화면 상단 행동 이름, 인카운터의 멀티캐스트에서 호출함
	void ShowActionBanner(const FNUBattleActionBanner& Banner);

	UFUNCTION(Server, Reliable)
	void ServerTryParry(int32 ForActionSequence, float PressOffset);

	UFUNCTION(Server, Reliable)
	void ServerTryDodge(int32 ForActionSequence);

	// 전투 중 나이아 교체, 턴을 소비하지 않고 작동해야해서 명령 제출과 다르게 처리함
	UFUNCTION(Server, Reliable)
	void ServerRequestNaiaSwap(int32 ToPartySlot);

	// 현재 값을 서버가 읽어서 뒤집음, 클라는 값을 보내지 않음
	UFUNCTION(Server, Reliable)
	void ServerToggleNaiaSummon();

	// 교체 실패 사유를 요청자에게만 돌려줌
	UFUNCTION(Client, Reliable)
	void ClientNaiaSwapResult(ENUNaiaSwapResult Result);

	UFUNCTION(Client, Reliable)
	void ClientParryResult(bool bSuccess);
	
	// 커서를 원하는 주체를 등록/해제 하는 함수, 하나라도 남아있다면 커서가 보임
	// 인벤토리, 경매장, 채팅 등 요구하는 이유/상황을 Reason으로 넘김
	// Owner를 넘기면 객체가 파괴될때 요청이 자동으로 사라짐
	UFUNCTION(BlueprintCallable)
	void RequestCursor(FName Reason, bool bWant, const UObject* InOwner = nullptr, bool bForce = false);

	// 대화/퀘스트 제안처럼 필드 입력을 점유하는 UI를 등록
	// 요청이 하나라도 남아 있으면 이동과 지정된 필드 입력을 차단
	void RequestGameInputBlock(FName Reason, bool bBlock, const UObject* InOwner = nullptr);
	bool IsGameInputBlocked() const { return bGameInputBlockApplied || bRecoveryTransitionActive; }

	// 마지막으로 쓴 입력 장치가 패드인지. 커서 숨김·창 첫 버튼 포커스·창 열린 동안 이동 차단의 기준
	bool IsGamepadMode() const { return bGamepadMode; }


	// 출입 페이드 설정. Amount: 0은 투명, 1은 검정.
	void SetWorldTransitionFade(float Amount, float Duration);
	// 출입 연출 종료와 화면 복구.
	void ClearWorldTransitionFade();


	// 출입 시퀀스 재생. 완료와 중단을 별도 콜백으로 전달한다.
	bool PlayWorldTransitionSequence(ULevelSequence* Sequence, FSimpleDelegate OnFinished, FSimpleDelegate OnInterrupted);
	// 시퀀스와 재생 액터 정리.
	void StopWorldTransitionSequence();

	
	// 클릭 허용 설정하는 함수
	void SetWorldClickEnabled(bool bEnabled);

	// 채팅창, 필드에 진입한 뒤에만 존재함
	UNUChatWidget* GetChatWidget() const;

	// 성장 UI를 여닫는 진입점
	UFUNCTION(BlueprintCallable, Category = "NU|Character|UI")
	void ToggleCharacterStatsUI();

	UFUNCTION(BlueprintCallable, Category = "NU|Progression|UI")
	void ToggleProgressionGraphUI();

	UFUNCTION(BlueprintCallable, Category = "NU|Progression|Debug", meta = (DevelopmentOnly))
	void ToggleProgressionDebugUI();

	UFUNCTION(BlueprintCallable, Category = "NU|Skill|UI")
	void ToggleOwnedSkillsUI();

	// 서버가 검증한 NPC 상호작용의 대화를 로컬 화면에 표시한다.
	UFUNCTION(Client, Reliable)
	void ClientOpenDialogue(UNUDialogueDefinition* DialogueDefinition, AActor* SourceActor, const FNUQuestInteractionOption& QuestOption);

	// 퀘스트별 묶음 에셋에서 현재 Phase의 대사를 표시한다.
	UFUNCTION(Client, Reliable)
	void ClientOpenQuestDialogue(UNUQuestDialogueDefinition* DialogueDefinition, AActor* SourceActor, const FNUQuestInteractionOption& QuestOption);

	UFUNCTION(Client, Reliable)
	void ClientOpenQuestDialogueSequence(FName DialogueId, AActor* SourceActor, const FNUQuestInteractionOption& QuestOption);

	// 표시만 수행한다. 계약 완료 판정이나 보상 지급은 이 함수에서 처리하지 않는다.
	bool OpenDialogueEvent(FName EventId);

	// 퀘스트 후속 처리 없이 일반 대화만 표시한다.
	UFUNCTION(Client, Reliable)
	void ClientOpenStandaloneDialogue(UNUDialogueDefinition* DialogueDefinition);

	// 서버가 수집한 NPC 기능 선택지와 첫 인사를 로컬 메뉴로 표시한다.
	UFUNCTION(Client, Reliable)
	void ClientOpenInteractionConversation(UNUDialogueDefinition* OpeningDefinition, AActor* SourceActor, const TArray<FNUInteractionOption>& Options);

	// 서버에서 검증한 상점 선택지를 기존 상점 시스템의 로컬 진입점으로 전달한다.
	UFUNCTION(Client, Reliable)
	void ClientOpenShop(int32 ShopId, AActor* SourceActor = nullptr);

	/** Last successfully opened NPC dialogue; a weak, local presentation context only. */
	UFUNCTION(BlueprintPure, Category = "NU|Interaction|UI")
	AActor* GetLastDialogueSource() const { return LastDialogueSource.Get(); }

	// 서버에서 검증한 창고 선택지를 기존 인벤토리 시스템의 로컬 창고 진입점으로 전달한다.
	UFUNCTION(Client, Reliable)
	void ClientOpenStorage();

	// 서버가 전직 NPC 대화를 검증한 뒤 로컬 최초 직업 선택 단계를 시작한다.
	UFUNCTION(Client, Reliable)
	void ClientBeginInitialJobSelection();

	UFUNCTION(Client, Reliable)
	void ClientBeginStarterNaiaSelection();

	// 최초 직업 선택 완료 후 서버에서 시작 나이아 선택 권한을 연다.
	UFUNCTION(Server, Reliable)
	void ServerAuthorizeStarterNaiaSelection();

	UFUNCTION(BlueprintPure, Category = "NU|Dialogue")
	bool IsDialogueOpen() const;

	UFUNCTION(BlueprintCallable, Category = "NU|Dialogue")
	void AdvanceDialogue();

	// 퀘스트 저널과 우측 트래커가 공유하는 로컬 추적 대상
	// 트래커에 고정한 퀘스트 목록
	void TrackQuest(FName QuestId);
	void UntrackQuest(FName QuestId);
	void ToggleTrackedQuest(FName QuestId);

	bool IsQuestTracked(FName QuestId) const { return TrackedQuestIds.Contains(QuestId); }
	const TArray<FName>& GetTrackedQuestIds() const { return TrackedQuestIds; }
	bool IsTrackedQuestFull() const { return TrackedQuestIds.Num() >= MaxTrackedQuests; }

	// 트래커 갱신 중에 호출
	void RemoveTrackedQuestSilent(FName QuestId) { TrackedQuestIds.Remove(QuestId); }

	void CloseQuestJournal();
	
public:
	UPROPERTY(BlueprintAssignable, Category = "NU|Battle|UI")
	FOnNUBattleCommandUIChanged OnBattleCommandUIChanged;

	// 넷 전투 무대 전환용 공개 진입점 - PlayTransitionSequence/StopTransitionSequence를 그대로 호출함
	// ANUBattleEncounter 없이 UNUBattleNetStageSubsystem이 같은 페이드 연출을 쓰기 위함
	void PlayNetBattleTransition(class ULevelSequence* Sequence);
	void StopNetBattleTransition();

	// 넷 전투는 로컬 인카운터 진입을 하지 않으므로 연출 HUD만 별도로 준비함
	void EnsureNetBattleHUD();
	void DestroyNetBattleHUD();

protected:
	virtual void BeginPlay() override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void SetupInputComponent() override;
	virtual void BeginPlayingState() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void ServerSubmitBattleCommand(FGuid EncounterId, const FNUBattleCommandRequest& Request);

	// 클라이언트가 대사를 끝까지 확인한 경우에만 기존 퀘스트 상호작용을 계속한다.
	UFUNCTION(Server, Reliable)
	void ServerFinishDialogue(AActor* SourceActor, const FNUQuestInteractionOption& QuestOption);

	// 메뉴를 연 뒤 상태가 바뀔 수 있으므로 현재 대상과 선택지를 서버에서 다시 검증한다.
	UFUNCTION(Server, Reliable)
	void ServerExecuteInteractionOption(AActor* SourceActor, const FNUInteractionOptionHandle& Handle);

	UFUNCTION(Client, Reliable)
	void ClientBattleCommandResult(
		FGuid EncounterId,
		int32 TurnNumber,
		int32 SourceCombatantId,
		ENUBattleCommandSubmitResult Result
		);

	UFUNCTION()
	void HandleBattleCommandSelectionChanged();
	void HandleBattleCommandSessionChanged(UNUBattleCommandComponentBase* Component, bool bActive);

	// 필드 동기화 진입
	// 로그인 완료와 로컬 폰 준비 두 조건이 갖춰졌을 때 1회 진입한다.
	// 둘의 순서가 경로마다 다르므로(로그인 레벨 경유 / ember.Connect 콘솔) 양쪽에서 호출한다
	void TryEnterField();

	UFUNCTION()
	void HandleServerConnStateChanged(EEmberConnState NewState);

	class UEmberServerManager* GetServerManager() const;

	void BindBattleEncounterDelegate();
	void HandleBattleEncounterChanged(ANUBattleEncounter* NewEncounter);
	void HandleEncounterTransition(ENUTransitionCue Cue);
	
	void EnterBattleCamera(ANUBattleEncounter* Encounter);

	// 배경 세트가 들고 있는 전투 카메라, 없으면 nullptr
	AActor* ResolveBattleViewTarget() const;

	// 세트 카메라를 화면에 쓰기 전에 프로젝트 규약에 맞춤
	void AdoptStageCamera(AActor* StageCamera);
	void RestoreFieldCamera();

	void SetBattleInputActive(bool bActive);
	void ClearInputMappingContexts();

	/* 전투 관련 Action 처리 */
	
	void HandleBattleNavigate(const FInputActionValue& Value);
	void HandleBattleConfirm();
	void HandleBattleCancel();
	void HandleBattleParry();
	
	/* 마우스 관련 Action 처리 */
	
	void StartShowMouse();
	void CompleteShowMouse();

	// 좌클릭, 지금은 열려 있는 컨텍스트 메뉴를 닫는 역할만 함
	void HandleMouseClick();

	// 우클릭, 커서 아래 원격 플레이어를 찾아 컨텍스트 메뉴를 열음
	void HandleContextMenuClick();

	// ESC, 최상단 창 하나를 닫음 (닫을 창이 없으면 전체 메뉴를 열음)
	void HandleUIEscape();

	// 패드 B, 최상단 창 하나를 닫기만 함 (ESC와 달리 닫을 창이 없어도 전체 메뉴는 열지 않음 --- 그건 Start)
	void HandleUIBack();

	/* 전체 메뉴 (ESC) */

	// 창 열기 항목들을 매니저의 게임 메뉴에 등록 — 키 바인딩 핸들러를 그대로 부르므로 키와 메뉴가 항상 같은 동작.
	// 항목 추가는 여기 한 곳에서 (새 기능이 자기 코드에서 직접 등록해도 된다)
	void RegisterGameMenuEntries();

	// 하단 줄(시스템) 항목
	void HandleMenuOptions();
	void HandleMenuCharacterSelect();
	void HandleMenuQuitGame();

	// 모험 줄 --- 전체 지도 창 토글 (메뉴 [지도]와 M 키가 같은 길)
	void HandleToggleWorldMap();

	/* 인벤토리 관련 Action 처리 */
	
	void HandleToggleInventory();

	// 소지품 키 (키보드 I·패드 Y). 키보드는 HandleToggleInventory와 같고, 패드는 통합 캐릭터 메뉴를 통째로 여닫는다 ---
	// 패드는 이 메뉴의 키가 하나뿐이라 열 때는 첫 페이지(캐릭터 정보), 어느 페이지에서 눌러도 닫힌다.
	// 전체 메뉴의 [인벤토리]는 패드로 골라도 인벤토리 페이지다 (HandleToggleInventory)
	void HandleInventoryKey();
	void HandleToggleCharacterInfo();
	void HandleToggleQuestJournal();
	void HandleToggleNaiaRoster();

	// 입력을 막고 있는 것이 통합 캐릭터 메뉴뿐인지 --- 그때는 소지품·캐릭터 정보 키를 받는다 (다시 눌러 닫기·다른 페이지로 넘기기)
	bool IsBlockedOnlyByCharacterMenu() const;

	/* 위젯 생성, 파괴, 처리 */
	
	void CreateBattleCommandWidget();
	void CreateParryCueWidget();
	void CreateCombatPopupLayer();
	void CreateActionBannerWidget();
	void DestroyBattleCommandWidget();

	// 인게임 상시 HUD (전투 / 필드)
	// 전투 진입·퇴장 때마다 만들고 부수지 않는다 — 두 위젯 모두 UNU_BaseInGameWidget 계열이라
	// ASC 의 Encounter 통지를 받아 스스로 접혔다 펴진다(전투는 켜지고 필드는 꺼지는 거울상).
	// 다만 PlayerState/ASC 해석이 끝난 뒤에 만들어야 하므로 생성 시점만 여기서 통제한다.
	void EnsureInGameHUDWidgets();

	void CreateBattleHUDWidget();
	void DestroyBattleHUDWidget();

	void CreateFieldHUDWidget();
	void DestroyFieldHUDWidget();

	void CreateBattleSwitcherWidget();
	void DestroyBattleSwitcherWidget();

	void CreatePlayerContextMenuWidget();
	void DestroyPlayerContextMenuWidget();

	// 필드 원격 캐릭터 메뉴 --- 마우스 우클릭과 패드가 같이 쓴다 (전투 중인 상대는 파티 초대를 뺀다).
	// 메뉴는 그 캐릭터를 따라다닌다
	void OpenFieldPlayerContextMenu(class AFieldRemoteCharacter* Remote, const FVector2D& ScreenPos);

public:
	// 필드에서 원격 플레이어를 우클릭할 때와 파티 목록의 줄을 우클릭할 때 같이 사용하는 함수.
	// FollowActor를 주면 메뉴가 그 캐릭터를 따라다닌다 (파티 목록은 화면에 고정이라 비운다)
	void OpenPlayerContextMenu(const FNUPlayerContextTarget& Target, const FVector2D& ScreenPos, AActor* FollowActor = nullptr);

	// 열려 있으면 닫음, 좌클릭과 파티창 드래그 시작이 호출함
	void ClosePlayerContextMenu();

	// 패드는 커서가 없어 거래 가능 거리 안의 원격 캐릭터를 가까운 순으로 세워 고른다.
	// 열린 대상에서 Step만큼 옮긴 사람의 메뉴를 연다 (닫혀 있으면 가장 가까운 사람). 플레이어 메뉴의 LB/RB가 Step ±1로 부른다
	void OpenNearbyPlayerContextMenu(int32 Step);

protected:

	/* 채팅 위젯 생성 파괴 */

	void CreateChatWidget();
	void DestroyChatWidget();

	// 채팅 입력창을 열고 게임 입력을 막음 
	void HandleChatOpen();

	// 위젯이 포커스 상태를 알려준다 (Enter / Esc / 포커스 이탈 전부 여기로).
	void HandleChatFocusChanged(bool bFocusing);

	// 입력 확정, 채널에 맞는 경로로 채팅을 보냄
	void HandleChatSubmitted(ENUChatChannel Channel, const FString& Text);

	// 컨텍스트 메뉴에서 귓속말 대상을 골랐을 때, 채팅창을 귓속말로 열어줌
	void HandleWhisperTargetChanged(int64 TargetIdx, const FString& TargetName);

	UNUChatSubsystem* GetChatSubsystem() const;

	/* 파티 초대 팝업 생성 파괴 */

	void CreatePartyPromptWidget();
	void DestroyPartyPromptWidget();

	// 초대·신청을 받으면 서브시스템이 알려줌
	void HandlePartyPrompt(bool bIsInvite, const FString& FromName, int32 ExpireSec);

	// 수락·거절·만료가 전부 여기로 돌아옴, 정리는 이 한 곳에서만 함
	void HandlePartyPromptClosed();

	/* 파티원 목록 생성 파괴 */

	void CreatePartyListWidget();
	void DestroyPartyListWidget();

	UNUPartySubsystem* GetPartySubsystem() const;

	void CreateInteractionIndicatorWidget();
	void DestroyInteractionIndicatorWidget();
	void BindInteractionComponent(APawn* InPawn);
	void UnbindInteractionComponent();
	void RefreshInteractionIndicator();
	void UpdateInteractionIndicatorPosition();

	UFUNCTION()
	void HandleCurrentInteractableChanged(
		AActor* PreviousInteractable,
		AActor* CurrentInteractable);
	
	/* 전투 화면 전환 */
	
	void PlayTransitionSequence(ULevelSequence* Sequence);
	void StopTransitionSequence();


	// 재생 종료 후 다음 전환 단계 호출.
	UFUNCTION()
	void HandleWorldTransitionSequenceFinished();
	// 정상 종료와 중단을 다음 프레임에 구분한다.
	UFUNCTION()
	void HandleWorldTransitionSequenceStopped();
	void HandleWorldTransitionSequenceInterrupted();


	void TryRevealRecovery();
	void UpdateRecoveryCover();
	UPROPERTY(Transient)
	TObjectPtr<class UNUTransitionLayerWidget> RecoveryCover;

	bool bRecoveryTransitionActive = false;
	FVector RecoveryDestination = FVector::ZeroVector;
	FTimerHandle RecoveryRevealTimer;
	double RecoveryRevealStarted = 0.0;
	FText PendingRecoveryNotice;
	
	/* 퀘스트 위젯 */
	void CreateQuestWidget();
	void DestroyQuestWidget();
	void RefreshQuestWidget();
	void CreateQuestJournalWidget();
	void DestroyQuestJournalWidget();
	void RefreshQuestJournalWidget();

	void CloseDialogue(bool bCompleted = false);

	UFUNCTION()
	void HandleDialogueClosed(bool bCompleted);

	void HandleInteractionConversationOptionSelected(FNUInteractionOptionHandle Handle);
	void HandleInteractionActionDialogueClosed(bool bCompleted, TWeakObjectPtr<AActor> SourceActor, FNUInteractionOptionHandle Handle);
	void HandleInteractionConversationCancelled();
	bool TryAuthorizeStarterNaiaSelection();
	void HandleInitialJobSelectionCompleted();
	void WaitForStarterNaiaGathering();
	void CancelStarterNaiaGathering();
	void BeginStarterNaiaSelectionPresentation();
	void HandleStarterNaiaSelectionDialogueClosed(bool bCompleted);
	void OpenQuestDialogueLines(const TArray<struct FNUDialogueLine>& Lines, AActor* SourceActor, const FNUQuestInteractionOption& QuestOption);
	void OpenStarterNaiaSelectionUI();
	FTimerHandle StarterNaiaGatheringTimer;
	TWeakObjectPtr<ANUPlayerState> StarterNaiaGatheringPlayerState;
	int32 StarterNaiaGatheringDebugSerial = 0;
	bool bWaitingForStarterNaiaGathering = false;

protected:
	// 로컬 전투 입력 컴포넌트
	UPROPERTY()
	TObjectPtr<UNUBattleCommandComponent> BattleCommandComponent;

	// 넷 전투 전용 입력 컴포넌트
	UPROPERTY()
	TObjectPtr<UNUBattleNetCommandComponent> NetBattleCommandComponent;

	// 넷/로컬 전투 중 활성화중인 입력 컴포넌트
	UPROPERTY(Transient)
	TObjectPtr<UNUBattleCommandComponentBase> ActiveCommandComponent;
	
	// 패링 컴포넌트
	UPROPERTY()
	TObjectPtr<UNUParryComponent> ParryComponent;

	// 필드 IMC
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputMappingContext> FieldMappingContext;

	// 전투 IMC
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputMappingContext> BattleCommandMappingContext;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> NavigateAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> CancelAction;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ParryAction;
	
	// Alt 키를 눌러서 마우스를 보이게 하는 액션, Triggered중에만 처리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ShowMouseAction;
	
	// 마우스 클릭 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> MouseClickAction;

	// 우클릭 액션, 컨텍스트 메뉴 열기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ContextMenuAction;

	// ESC 액션, 열려 있는 창을 위에서부터 하나씩 닫음
	// 포커스를 잡는 위젯(채팅 입력·확인 창)은 자기가 먼저 ESC를 소비하므로 여기까지 오지 않는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> UIEscapeAction;

	// 패드 B 액션 --- 열려 있는 창을 위에서부터 하나씩 닫기만 한다 (전체 메뉴 열기는 Start가 ESC 액션으로)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> UIBackAction;

	// 채팅 IMC, Enter(채팅창 열기) 만 추가하면 됨
	// 닫기(Esc)와 채널 순환(Tab)은 입력창이 포커스를 갖고 있는 동안 위젯이 Preview로 직접 잡아서 처리함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputMappingContext> ChatMappingContext;

	// Enter, 채팅 입력창 열기 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ChatOpenAction;
	
	// 인벤토리 IMC
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputMappingContext> InventoryMappingContext;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	// 캐릭터 정보창(장비+스탯)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ToggleCharacterInfoAction;

	// Q, 퀘스트 목록 열기/닫기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ToggleQuestJournalAction;

	// M, 전체 지도 열기/닫기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ToggleWorldMapAction;

	// 나이아 편성창(대기 목록 + 보관함) 열기/닫기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ToggleNaiaRosterAction;

	// 스탯 창과 성장 트리 IMC
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputMappingContext> ProgressionMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ToggleCharacterStatsAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ToggleProgressionGraphAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> ToggleOwnedSkillsAction;

	// 전투 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Battle|UI")
	TSubclassOf<UUserWidget> BattleCommandWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Battle|UI")
	TObjectPtr<UUserWidget> BattleCommandWidget;
	
	// 전투 상시 HUD 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Battle|UI")
	TSubclassOf<UNU_BattleHUDWidget> BattleHUDWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Battle|UI")
	TObjectPtr<UNU_BattleHUDWidget> BattleHUDWidget;

	// HUD 는 전투 중 최하단 레이어다.
	// 같은 ZOrder 면 나중에 추가된 쪽이 위로 오는데, HUD 는 접속 직후 한 번 올라가고
	// 커맨드/패리 위젯은 전투 진입 때 올라가므로 0 으로 둬도 자연히 아래에 깔린다.
	// 나중에 Switcher(패리/교감 연출)를 붙일 때 그쪽에 더 큰 값을 주면 된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Battle|UI")
	int32 BattleHUDZOrder = 0;

	// 필드 상시 HUD 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Field|UI")
	TSubclassOf<UNU_FieldHUDWidget> FieldHUDWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Field|UI")
	TObjectPtr<UNU_FieldHUDWidget> FieldHUDWidget;

	// 전투 HUD 와 동시에 보이는 일이 없으므로(한쪽이 접히면 다른 쪽이 펴짐) 같은 층에 둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Field|UI")
	int32 FieldHUDZOrder = 0;

	// 전투 화면 라우터 클래스 (전투 개시 / 결과 등 전체 화면 연출)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Battle|UI")
	TSubclassOf<UNU_BattleSwitcherWidget> BattleSwitcherWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Battle|UI")
	TObjectPtr<UNU_BattleSwitcherWidget> BattleSwitcherWidget;

	// 라우터는 결과 화면처럼 전부 덮어야 하는 연출을 담으므로 가장 위에 둔다.
	// (상시 HUD 0, 상호작용 인디케이터 10 보다 위)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Battle|UI")
	int32 BattleSwitcherZOrder = 20;

	// 패링 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Battle|UI")
	TSubclassOf<UNUParryCueWidget> ParryCueWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Battle|UI")
	TObjectPtr<UNUParryCueWidget> ParryCueWidget;

	// 전투 팝업 글자 레이어 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Battle|UI")
	TSubclassOf<UNUCombatPopupLayer> CombatPopupLayerClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Battle|UI")
	TObjectPtr<UNUCombatPopupLayer> CombatPopupLayer;

	// 화면 상단 행동 배너
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Battle|UI")
	TSubclassOf<UNUActionBannerWidget> ActionBannerWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Battle|UI")
	TObjectPtr<UNUActionBannerWidget> ActionBannerWidget;

	// 전투 결과창, 확인해서 닫을 때까지 선투 세트 종료를 붙잡음
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI|Battle")
	TSubclassOf<UNUBattleResultWidget> BattleResultWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "NU|UI|Battle")
	int32 BattleResultZOrder = 60;

	UPROPERTY(Transient)
	TObjectPtr<UNUBattleResultWidget> BattleResultWidget;

	int32 DodgeWindowActionSequence = INDEX_NONE;

	// 화면에 띄울 SP 소모량
	int32 DodgeWindowSPCost = 0;

	// 안 누르고 넘어갔을 때 창을 닫는 타이머
	FTimerHandle DodgeWindowTimer;

	// 컨텍스트 메뉴 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Field|UI")
	TSubclassOf<UNUPlayerContextMenu> PlayerContextMenuClass;
	
	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Field|UI")
	TObjectPtr<UNUPlayerContextMenu> PlayerContextMenuWidget;

	// 채팅 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Chat|UI")
	TSubclassOf<UNUChatWidget> ChatWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Chat|UI")
	TObjectPtr<UNUChatWidget> ChatWidget;

	// 파티 초대·신청 수락 팝업 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Party|UI")
	TSubclassOf<UNUPartyPromptWidget> PartyPromptWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Party|UI")
	TObjectPtr<UNUPartyPromptWidget> PartyPromptWidget;

	// 화면 좌측 파티원 목록, 파티가 없으면 스스로 접힘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Party|UI")
	TSubclassOf<UNUPartyListWidget> PartyListWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Party|UI")
	TObjectPtr<UNUPartyListWidget> PartyListWidget;

	// 상호작용 인디케이터 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Field|UI|Interaction")
	TSubclassOf<UUserWidget> InteractionIndicatorWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Field|UI|Interaction")
	TObjectPtr<UUserWidget> InteractionIndicatorWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNUInteractorComponent> BoundInteractionComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Field|UI|Interaction")
	float InteractionIndicatorWorldPadding = 20.0f;

	// NPC 머리 위 표시가 떠 있으면 상호작용 키를 그 위에 쌓지 않고 표시의 이 자리에 붙인다 (Docs/HUD_Interact_Proposal_20261014 A).
	// 표시 위젯 판(그림 칸) 가운데에서 오른쪽·아래로 판 크기의 비율 --- 마름모 오른쪽 아래 모서리. 0,0 이면 옛 방식(표시 위에 쌓기)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Field|UI|Interaction")
	FVector2D InteractionIndicatorMarkerAnchor = FVector2D(0.27f, 0.20f);

	UPROPERTY(Transient)
	TObjectPtr<UNUAbilitySystemComponent> BoundASC;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FieldViewTargetBeforeBattle;

	UPROPERTY(Transient)
	TObjectPtr<ANUBattleEncounter> ViewedEncounter;
	
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> TransitionPlayer;
	
	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> TransitionSequenceActor;

	// 배틀과 분리된 출입 전용 시퀀스 상태.
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> WorldTransitionPlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> WorldTransitionSequenceActor;

	FSimpleDelegate WorldTransitionSequenceFinished;
	FSimpleDelegate WorldTransitionSequenceInterrupted;
	FTimerHandle WorldTransitionSequenceStopTimer;
	
	// 타겟한 적 머리위에 띄울 클래스
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Targeting")
	TSubclassOf<ANUBattleTargetIndicator> TargetIndicatorClass;
	
	// 마우스 커서를 요구하는 이유/상황 과 소유자, 소유자 등록시 파괴되면 자동 정리
	TMap<FName, TWeakObjectPtr<const UObject>> CursorRequesters; 

	// 로컬 필드 입력을 차단하는 이유와 요청 소유자
	TMap<FName, TWeakObjectPtr<const UObject>> GameInputBlockRequesters;
	
	bool bCursorModeApplied = false;
	bool bGameInputBlockApplied = false;

	// 마지막 입력 장치가 패드인지 (FNUInputDeviceWatcher가 판정). 패드면 창이 열려도 커서를 숨긴다.
	// 엔진의 InputDeviceSubsystem 델리게이트를 안 쓰는 이유: 창이 열리고 닫힐 때 엔진이 커서를 되돌리는 한 번짜리 점프를
	// 마우스 사용으로 쳐서 패드 모드가 매번 풀린다
	bool bGamepadMode = false;

	// Slate 입력 프로세서 --- 키·스틱·마우스 이벤트를 먼저 보고 장치를 판정한다. BeginPlay에서 등록, EndPlay에서 해제
	TSharedPtr<class FNUInputDeviceWatcher> InputDeviceWatcher;
	friend class FNUInputDeviceWatcher; // SetGamepadMode만 부른다

	// 패드 모드에서 창이 열려 있는 동안 스틱이 포커스 이동과 함께 캐릭터까지 움직이지 않게 이동 입력을 막았는지
	// (SetIgnoreMoveInput은 카운터라 켠 만큼만 끈다)
	bool bGamepadMoveIgnored = false;
	
	// WBP_QuestWidget 클래스. 레이아웃은 WBP, 데이터와 동작은 UNUQuestWidget이 담당
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|UI")
	TSubclassOf<UNUQuestWidget> QuestWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Quest|UI")
	TObjectPtr<UNUQuestWidget> QuestWidget;

	// 위젯이 아니라 PC가 들고 있다. 
	// 트래커 위젯 수명과 무관해야 하고,
	// 나중에 따라가기(웨이포인트, 미니맵)도 이 값을 활용
	TArray<FName> TrackedQuestIds;

	// 열려 있는 전체 지도 창. 토글이 닫을 대상을 찾기 위한 것으로, 수명은 창 시스템이 관리한다
	TWeakObjectPtr<class UNU_WorldMapWindowWidget> WorldMapWindow;

	// 트래커에 동시에 표시할 최대 개수 
	// 넘으면 새 고정 요청을 무시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|UI", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxTrackedQuests = 5;

	// WBP_QuestJournal 클래스. 전체 목록과 필터/상세 화면은 별도 위젯으로 유지한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Quest|UI")
	TSubclassOf<UNUQuestJournalWidget> QuestJournalWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Quest|UI")
	TObjectPtr<UNUQuestJournalWidget> QuestJournalWidget;

	// 대화 종료 후 서버가 퀘스트 처리를 이어갈 상호작용 대상
	UPROPERTY(Transient)
	TObjectPtr<AActor> PendingDialogueSource;

	UPROPERTY(Transient)
	FNUQuestInteractionOption PendingDialogueOption;

	// 기능 메뉴를 연 서버 검증 대상. 선택 또는 취소 시 즉시 비운다.
	UPROPERTY(Transient)
	TObjectPtr<AActor> PendingInteractionSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastDialogueSource;
	
private:
	void ApplyCursorState(bool bForce = false);
	void ApplyGameInputBlockState();

	// 입력 프로세서(FNUInputDeviceWatcher)가 판정한 장치가 바뀌면(패드 <=> 키보드·마우스) 커서·이동 차단·창 포커스를 다시 맞춘다
	void SetGamepadMode(bool bGamepad);

	// 패드 모드 + 창 열림이면 이동 입력 차단, 아니면 해제. 장치가 바뀌거나 창 목록이 바뀔 때 부른다
	void RefreshGamepadMoveBlock();
};
