#pragma once

#include "CoreMinimal.h"
#include "Battle/Encounter/NUBattleEncounterTypes.h"
#include "Field/Encounter/NUSymbolEncounterSourceInterface.h"
#include "Interaction/Interface/NUInteractableInterface.h"
#include "GameFramework/Character.h"
#include "FieldEnemy.generated.h"

class ANUBattleEncounter;
class AFieldPlayerCharacter;
class UNUSpriteVisualComponent;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class USphereComponent;
class UMaterialInterface;

// 심볼 전투 시작 방식
UENUM(BlueprintType)
enum class ENUSymbolEncounterTriggerMode : uint8
{
	Automatic		UMETA(DisplayName = "자동 전투"),
	Interaction		UMETA(DisplayName = "상호작용")
};

// 승리 후 심볼 처리 방식
UENUM(BlueprintType)
enum class ENUFieldEnemyDefeatPolicy : uint8
{
	RespawnAfterDelay	UMETA(DisplayName = "일정 시간 후 재생성"),
	RemoveForSession	UMETA(DisplayName = "세션 동안 제거")
};

// 필드 심볼의 현재 상태
UENUM(BlueprintType)
enum class ENUFieldEnemyState : uint8
{
	Idle			UMETA(DisplayName = "대기"),
	Starting		UMETA(DisplayName = "전투 시작"),
	InEncounter		UMETA(DisplayName = "전투 진행"),
	Cooldown		UMETA(DisplayName = "재도전 쿨타임"),
	Respawning		UMETA(DisplayName = "리스폰 대기"),
	Defeated		UMETA(DisplayName = "처치")
};

UCLASS(Blueprintable)
class PROJECTM_API AFieldEnemy : public ACharacter , public INUInteractableInterface , public INUSymbolEncounterSourceInterface
{
	GENERATED_BODY()

public:
	AFieldEnemy();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

public:
	/* 수동 심볼 인카운터 상호작용 */
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;
	
	/* 전투 시작 및 종료 */
	virtual void HandleSymbolEncounterStarted_Implementation(AActor* Initiator) override;
	virtual void HandleSymbolEncounterCompleted_Implementation(ENUBattleResult Result) override;

	// 서버 심볼 전투의 symbol_id, 파티원 클라도 같은 레벨을 열고 있으면 같은 값이 나와 같은 심볼을 찾음
	// 레벨 패키지 이름(PIE 접두사 제외)과 처치 기록 이름으로 만듦, 0 은 "심볼 아님" 이라 나오지 않음
	int64 GetSymbolId() const;

public:
	// 감지 범위
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
	TObjectPtr<USphereComponent> EncounterTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Animation")
	TObjectPtr<UPaperZDAnimationComponent> Animation;

	// 머티리얼·애님BP·빌보드 담당
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UNUSpriteVisualComponent> SpriteVisual;

	// 전투에 등장할 적 ID 목록
	// 여러 마리 추가 가능, 필드에는 첫 번째 적의 나이아 그림으로 섬
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
	TArray<FName> EnemyPartyIds;

	// 생성할 Encounter 클래스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
	TSoftClassPtr<ANUBattleEncounter> EncounterClass;

	// 이 심볼과 싸울 때 사용할 배경 서브레벨, 비어 있으면 배경을 바꾸지 않음
	// 보스처럼 전용 배경이 필요한 심볼을 위해 존과 별도로 둠
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
	TSoftObjectPtr<UWorld> BattleStageLevel;

	// 자동 or 수동 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
	ENUSymbolEncounterTriggerMode TriggerMode = ENUSymbolEncounterTriggerMode::Automatic;

	// 승리 후 심볼 제거 or 일정 시간 뒤 리스폰
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter|Lifecycle")
	ENUFieldEnemyDefeatPolicy DefeatPolicy = ENUFieldEnemyDefeatPolicy::RespawnAfterDelay;

	// 처치 여부 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter|Lifecycle")
	FName FieldEnemyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter|Lifecycle", meta = (ClampMin = "0.0"))
	float RespawnDelay = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter|Lifecycle", meta = (ClampMin = "0.0"))
	float RetryCooldown = 1.0f;

	// 걸어 들어온 게 아니라 범위 안에 나타난 플레이어(전투 복귀 · 접속 · 포탈 · 부활 · 심볼 재등장)의 보호 시간 (자동 전투만, 0 이면 끔)
	// 멈춰 있는 동안은 줄지 않고 움직인 시간만 셈, 그 안에 범위를 벗어나면 보호 끝 · 다 쓰고도 범위 안이면 전투 시작. 보호 중엔 심볼이 깜빡임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Encounter|Lifecycle", meta = (ClampMin = "0.0"))
	float ReentryGraceSeconds = 3.0f;

	// 보호 중 깜빡임에 잠깐 입히는 반투명 머티리얼, 정점 색 알파를 불투명도로 받아야 페이드가 됨
	// 기본은 Paper2D 반투명 Unlit - 지금 그림(기본 마스크드 Unlit)과 밝기가 같음. 비우면 깜빡이지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Encounter|Lifecycle")
	TSoftObjectPtr<UMaterialInterface> ProtectionBlinkMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_EncounterState, BlueprintReadOnly, Category = "NU|Encounter")
	ENUFieldEnemyState EncounterState = ENUFieldEnemyState::Idle;

private:
	UFUNCTION()
	void HandleEncounterTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_EncounterState();

	// 전투 시작
	bool TryStartEncounter(AFieldPlayerCharacter* FieldPlayer);
	
	// 상태 변경 및 적용
	void SetEncounterState(ENUFieldEnemyState NewState);
	void ApplyEncounterState();
	
	// 쿨다운, 리스폰
	void FinishRetryCooldown();
	void FinishRespawn();
	
	// 끝난 전투의 플레이어가 필드로 돌아오기를 지켜봄, 돌아온 자리가 범위 안이면 보호
	void WaitForReturn();
	void WatchReturn();
	void StopWaitingForReturn();

	// 출현 보호 - 범위 안에 나타난 플레이어에게 빠져나갈 시간을 줌 (ReentryGraceSeconds)
	void StartProtection(AFieldPlayerCharacter* Player);
	void TickProtection(float DeltaSeconds);
	void ClearProtection();

	// UI 갱신
	void RefreshNearbyInteractionSelection() const;
	FName GetPersistentEnemyId() const;

	FTimerHandle LifecycleTimer;

	// 이 심볼과 싸운 플레이어, 끝난 뒤 필드로 돌아오기를 지켜봄
	TWeakObjectPtr<AFieldPlayerCharacter> EncounterPlayer;
	bool bWaitingForReturn = false;
	FTimerHandle ReturnWatchTimer;

	// 보호 중인 플레이어 · 보호 중 움직인 시간 · 깜빡임 시계
	TWeakObjectPtr<AFieldPlayerCharacter> ProtectedPlayer;
	bool bProtecting = false;
	float ProtectedMoveSeconds = 0.f;
	float BlinkElapsed = 0.f;

	// BeginPlay 에 불러 둔 ProtectionBlinkMaterial, 보호가 처음 걸릴 때 불러오면 그 순간 끊김
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedBlinkMaterial;
};
