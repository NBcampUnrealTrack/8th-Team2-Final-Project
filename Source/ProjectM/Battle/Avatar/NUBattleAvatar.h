// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Battle/NUBattleTypes.h"
#include "Field/NUFieldTypes.h"
#include "NUGAS/Interface/NUCombatantInterface.h"
#include "GameFramework/Actor.h"
#include "NUBattleAvatar.generated.h"


struct FGameplayAttribute;
struct FGameplayTag;
struct FNUStatusEffectView;
struct FNUJobResourceView;
struct FNUNaiaDefinition;
class ANUBattleEncounter;
class UNiagaraSystem;
class UNUBattleGameplayAbility;

enum class ENUAvatarMotionPhase : uint8
{
	Idle,
	Telegraphing,	// 적 공격 예고, 제자리에서 대기만 함
	Approaching,
	Acting,
	Returning
};

// 공격 모션이 소리를 내는 때: 준비(모션 시작) / 휘두름 · 발사(타격 직전) / 닿음
enum class ENUBattleAttackSFX : uint8
{
	Windup,
	Swing,
	Impact
};

UCLASS()
class PROJECTM_API ANUBattleAvatar : public AActor, public IAbilitySystemInterface, public INUCombatantInterface
{
	GENERATED_BODY()

public:
	ANUBattleAvatar();
	
	virtual void Tick(float DeltaSeconds) override;
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UNUAbilitySystemComponent* GetNUASC() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	void InitializeBattleAvatar(AActor* InAbilitySystemOwner, int32 InCombatantId);

	// 넷 전투 전용 초기화 - ASC를 잡지 않고 외형·진영만 세팅함 (서버가 스탯을 들고 있어 바인딩이 필요 X)
	// bIsPlayerVisual은 ApplyCombatantVisual/ApplyBattleFacing이 캐릭터인지 가릴 때 씀
	void InitializeNetAvatar(int32 InCombatantId, int32 InNaiaId, bool bIsPlayerVisual, EFieldFacing InFacing);

	// 서버 스냅샷의 HP/SP를 위젯에 직접 반영함, ASC 어트리뷰트 델리게이트가 없는 넷 전투 아바타 전용
	void SetVitals(int32 Health, int32 MaxHealth, int32 SP, int32 MaxSP);

	// 머리 위 이름/레벨은 서버 스냅샷에서, 표시할 진영은 기존 bIsAlly에서 가져온다.
	void SetCombatantIdentity(const FText& Name, int32 Level);

	// 서버 스냅샷의 상태이상을 위젯에 직접 반영함, SetVitals 와 같은 이유로 넷 전투 아바타 전용
	// 서버 자료구조를 아바타까지 끌고 오지 않으려고 표시용 뷰로 바꾼 뒤에 받음
	void SetStatusViews(const TArray<FNUStatusEffectView>& InViews);

	// 서버 스냅샷의 직업 자원을 위젯에 직접 반영함, SetStatusViews 와 같은 이유로 넷 전투 아바타 전용
	// 게이지 · 아이콘 목록은 NUJobResourceView 규칙으로 만든 뒤에 받음
	void SetJobResourceViews(const TArray<FNUJobResourceView>& InGauges, const TArray<FNUJobResourceView>& InBadges);

	// 서버 Death 이벤트로 직접 호출함, ASC의 State.Dead 태그 델리게이트가 없는 넷 전투 아바타 전용
	void SetDeadVisual(bool bDead);

	// 전투 중 나이아 교체용, 전투 아바타는 두고 ASC 소유자만 갈아끼움
	// 델리게이트 해제 -> 대입 -> 재바인딩 순서를 지켜야 함
	void SetAbilitySystemOwner(AActor* InAbilitySystemOwner);

	// 이전 나이아의 연출 상태를 지움, 교체 시 함께 불러야 함
	void ResetPresentationState();

	USceneComponent* GetTargetIndicatorAnchor() const;
	USceneComponent* GetCommandUIAnchor() const;

	int32 GetCombatantId() const;

	// 아군 진영인지, 연출 블루프린트가 편을 가를 때 씀
	UFUNCTION(BlueprintPure, Category = "NU|Battle")
	bool IsAllyCombatant() const;

	// 스폰 직후 인카운터·넷 무대가 지정함, 전투 중에는 바뀌지 않음
	void SetIsAlly(bool bInIsAlly);

	// 이 아바타의 나이아 Id, 캐릭터면 INDEX_NONE (전투 BGM 이 보스전을 가릴 때 씀)
	int32 GetNaiaId() const { return NaiaId; }

	// 전투 아바타의 연출을 알리는 Multicast
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayAction(const FNUBattleActionMotionSpec& Spec);

	// 인정 연출, Once 를 한 번 튼 뒤 Loop 를 전투가 끝날 때까지 반복함 (둘 다 비어도 됨)
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayConcession(UPaperZDAnimSequence* Once, UPaperZDAnimSequence* Loop);

	// 이 전투원이 쓸 행동 연출(어빌리티 기본값 · 스킬 표 · 직업 기본 공격)을 미리 불러 붙잡아 둠
	// 처음 쓸 때 불러오면 전투 도중 동기 로드로 끊김, 인카운터 · 넷 무대가 아바타를 세울 때 부름
	void PreloadActionVFX(const TArray<int32>& SkillIds, const TArray<const UNUBattleGameplayAbility*>& Abilities);

	// 포획 흔들림, 굴린 흔들림 횟수만큼 대상을 흔들고 끝에 결과 이펙트와 팝업을 띄움
	// 성공이면 그 뒤에 사망 처리가 이어지므로 연출이 끝날 때까지 숨기지 않음
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayCaptureShakes(int32 ShakeCount, bool bSuccess);

	// 위 멀티캐스트의 실제 동작, 넷 전투는 아바타가 로컬 전용 액터라 이걸 직접 부름
	void PlayCaptureShakes(int32 ShakeCount, bool bSuccess);

	// 흔들림이 끝날 때까지 남은 시간, 인카운터가 화면 전환을 미룰 때 물어봄
	float GetCaptureShakeRemainingSeconds() const;

	// 머리 위 팝업 글자, 파티원도 남의 결과를 봐야 하므로 소유자 한정이 아니라 멀티캐스트
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowCombatPopup(const FNUCombatPopupData& PopupData);

	// 체력/SP 바에 자기 전투원을 다시 지정함
	// 위젯이 인카운터에서 ASCOwner 를 찾으므로, 참가자 정보와 초기 스탯이
	// 모두 정해진 뒤에 인카운터가 한 번 더 불러줘야 함
	void RefreshVitalWidget();

	// 화면 전환 중 체력/마나 바만 숨기도록
	void SetVitalBarHidden(bool NewHidden);
	
	// 커맨드 컴포넌트가 ←/→ 로 투입 SP를 조절할 때마다 호출함
	// 아직 서버에 보내지 않은 값이라 로컬 표시 전용
	void SetPendingSPSpend(float InPendingSPSpend);
	
	void TickAnimation(float DeltaSeconds);

	// 예고가 끝난 뒤 실제 공격 모션으로 진입함, 예고가 없으면 곧바로 불림
	void BeginActionMotion();

	// StartPosition 은 모션 안의 시작 위치(초), 반복 타격이 앞부분을 건너뛸 때만 씀
	void PlayActionAnimation(float StartPosition = 0.f);

	// 이 행동의 행동 시간 · 타격 지점 · 시전 지점을 스펙에 채움
	// 공격 모션을 재생할 수 있으면 모션 길이와 모션에 찍힌 Hit·Cast 표시, 못 틀거나 표시가 없으면 스펙에 적힌 값 그대로
	void ResolveMotionTiming(FNUBattleActionMotionSpec& InOutSpec) const;
	
	void HandleActionAnimationEnded(bool bCompleted);

	// 인정 시퀀스가 끝날 때마다 불림, 반복 시퀀스를 다시 틈
	void HandleConcessionAnimationEnded(bool bCompleted);
	
	// 피격시 밀려남 처리, GameplayCue가 호출함
	// FromLocation으로 공격자 위치, 밀려난 방향을 정함
	UFUNCTION(BlueprintCallable, Category = "NU|Battle")
	void PlayHitReaction(FVector FromLocation);

	// 피격 점등, GameplayCue가 호출함
	UFUNCTION(BlueprintCallable, Category = "NU|Battle")
	void PlayHitFlash();
	
	// 서버가 스폰 직후 호출, 클라는 OnRep으로 반영
	void SetNaiaId(int32 InNaiaId);

	// 전투 중 바라볼 방향, 서버가 스폰 직후 ResolveBattleFacing 으로 정해 넣음
	void SetBattleFacing(EFieldFacing InFacing);
	
	// 진영과 누구인지로 전투 방향을 정함
	// 아군은 카메라 가까이 왼쪽 아래에서 등을 보이고, 적은 멀리 오른쪽 위에서 카메라를 바라봄
	// 플레이어는 4방향 애님이라 Up 이 뒷모습, 나이아·적은 좌우 반전만 되므로 서로를 향하게 가름
	static EFieldFacing ResolveBattleFacing(bool bInIsAlly, bool bInIsPlayerVisual);
	
	// 캐릭터 전투원인지 (나이아·적이 아닌지)
	bool IsPlayerVisual() const;

	// 행동 뒤 돌아올 자리, 스폰한 위치
	const FTransform& GetHomeTransform() const { return HomeTransform; }

	// 지금(또는 마지막) 행동의 스펙, ResolveActionVFX 가 직업 기본 공격으로 바꾼 뒤의 값
	const FNUBattleActionMotionSpec& GetActiveSpec() const { return ActiveSpec; }

	// 진행 중인 행동을 버리고 제자리에 세움, VFX 뷰어가 같은 행동을 처음부터 다시 틀 때 씀
	void ResetActionMotion();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	void BindDeathState();
	void BindVitalAttributes();

	// 현재 ASC에서 델리게이트를 떼고 핸들을 비움
	// 핸들을 비우지 않으면 Bind* 의 중복 방지 가드에 걸려 재바인딩이 실패함
	void UnbindCombatantDelegates();

	UFUNCTION()
	void OnRep_AbilitySystemOwner();

	UFUNCTION()
	void OnRep_VitalBarHidden();
	
	UFUNCTION()
	void OnRep_NaiaId();

	UFUNCTION()
	void OnRep_BattleFacing();

	// 애님 BP가 다시 만들어지면 방향이 기본값으로 돌아가므로 적용 뒤에 항상 호출해야 함
	void ApplyBattleFacing();

	void HandleDeadTagChanged(const FGameplayTag, const int32 NewCount);
	void HandleVitalAttributeChanged(const struct FOnAttributeChangeData& Data);

	void SetVitalsToWidget();
	class UNU_CombatantStatusWidget* GetVitalWidget() const;
	
	// 전투원 종류에 맞는 외형을 적용하고 방향을 다시 넣음
	void ApplyCombatantVisual();

	// 외형 적용, 기본은 나이아 정의를 따라감
	// 캐릭터는 몸·장비·외모가 여러 장으로 겹쳐 그려져 ANUBattlePlayerAvatar가 덮어씀
	virtual void ApplyCombatantAppearance();

	// 이 행동에 틀 액션 시퀀스, 기본은 어빌리티에 물린 것
	// 나이아 · 적은 그것을 못 틀면(애님 소스가 다름) 나이아 정의의 공격 시퀀스, 제자리 행동은 제외
	virtual UPaperZDAnimSequence* ResolveActionAnimation(const FNUBattleActionMotionSpec& Spec) const;

	// 스킬 표(DT_SkillVFX)가 정하지 않은 행동의 이펙트를 이 아바타 기준으로 바꿈
	// 나이아 · 적은 종의 속성별 기본 공격 연출(NUBattleDataSettings::NaiaElementAttackVFX), 캐릭터는 ANUBattlePlayerAvatar 가 직업의 기본 공격 연출로 바꿈
	virtual void ResolveActionVFX(FNUBattleActionMotionSpec& InOutSpec) const;

	// ResolveActionVFX 가 바꿔 넣을 수 있는 이펙트를 모음, 미리 불러 둘 때 씀
	virtual void CollectResolvedActionVFX(TArray<TSoftObjectPtr<UNiagaraSystem>>& OutVFX) const;

	// 이펙트에 걸린 소리가 없을 때 이 아바타의 공격이 내는 효과음 태그, 기본은 나이아 · 적이 몸으로 부딪치는 소리
	// 캐릭터는 ANUBattlePlayerAvatar 가 직업별로 바꿈 (준비 소리는 공격 모션 그림이 있는 캐릭터에만 있음)
	virtual FGameplayTag GetAttackSFX(ENUBattleAttackSFX Moment) const;

	// 그 시퀀스가 이 아바타에서 실제로 재생되는 길이, 기본은 시퀀스 원본의 길이
	// 스킨이 플립북을 갈아끼우는 캐릭터는 ANUBattlePlayerAvatar 가 스킨의 플립북 길이로 변경함
	virtual float GetActionAnimationDuration(const UPaperZDAnimSequence* Sequence) const;

	// 이 아바타의 애님 BP 와 애님 소스가 같은지 확인
	bool CanPlayActionAnimation(const UPaperZDAnimSequence* Sequence) const;

	// 파생 클래스가 ASC 소유자(로컬 전투의 나이아·캐릭터)를 읽을 때 사용
	AActor* GetAbilitySystemOwner() const;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_AbilitySystemOwner)
	TObjectPtr<AActor> AbilitySystemOwner;
	
	// ApplyCombatantVisual/ApplyBattleFacing이 캐릭터인지 나이아·적인지 가리는 값
	// 로컬은 AbilitySystemOwner를 Cast해서 채우고, 넷은 ASC가 없어 InitializeNetAvatar가 직접 넣음
	bool bIsPlayerVisual = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<class UPaperFlipbookComponent> Sprite;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Animation")
	TObjectPtr<class UPaperZDAnimationComponent> Animation;

	// 머티리얼·빌보드 담당, 필드 계열과 달리 LookAtCamera 방식을 사용함
	// 애님BP는 전투원 종류마다 다르므로 이 클래스가 직접 지정해야함
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<class UNUSpriteVisualComponent> SpriteVisual;

	
	// --------------------
	//		UI 관련
	// --------------------
	
	// 타겟팅 표시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<USceneComponent> TargetIndicatorAnchor;
	
	// 명령 UI 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|UI")
	TObjectPtr<USceneComponent> CommandUIAnchor;
	
	// Vital 위젯 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|UI")
	TObjectPtr<class UWidgetComponent> VitalBarWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|UI")
	bool bShowSPBar;

	// 전환 연출 중에만 true. 서버가 지정하고 클라가 OnRep으로 반영
	UPROPERTY(ReplicatedUsing = OnRep_VitalBarHidden)
	bool bVitalBarHidden;
	
	
	// ----------------------------
	//		애니메이션 관련
	// ----------------------------
	
	// 애니메이션 이후 돌아올 위치
	FTransform HomeTransform;
	
	ENUAvatarMotionPhase MotionPhase;
	FNUBattleActionMotionSpec ActiveSpec;
	float MotionElapsed;
	
	// 전투원 Id
	UPROPERTY(Replicated)
	int32 CombatantId;
	
	// State.Dead 태그 변화 델리게이트
	FDelegateHandle DeadTagDelegateHandle;
	
	// HP/MP 어트리뷰트 변경 델리게이트
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> VitalDelegateHandles;
	
	// 밀려나는 거리
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle", meta = (ClampMin = "0"))
	float HitReactionDistance;
	
	// 밀려나는 시간
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle", meta = (ClampMin = "0.01"))
	float HitReactionOutTime;
	
	// 제자리로 돌아오는 시간
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle", meta = (ClampMin = "0.01"))
	float HitReactionReturnTime;
	
	// 피격 시 점등할 색
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle")
	FLinearColor HitFlashColor;
	
	// 점등이 유지되는 시간, 이 시간이 지나면 원래 색으로 돌림
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle", meta = (ClampMin = "0.01"))
	float HitFlashDuration;
	
	// 포획 흔들림 한 번의 좌우 진폭
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Capture", meta = (ClampMin = "0"))
	float CaptureShakeDistance = 28.f;
	
	// 흔들림 한 번의 길이
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Capture", meta = (ClampMin = "0.01"))
	float CaptureShakeDuration = 0.25f;
	
	// 흔들림 사이의 정지, 여기서 멈춰야 몇 번 흔들렸는지 보임
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Capture", meta = (ClampMin = "0"))
	float CaptureShakeInterval = 0.15f;
	
	// 흔들림 한 번에 좌우를 몇 번 왕복할지
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Capture", meta = (ClampMin = "0.5"))
	float CaptureShakeCycles = 2.f;
	
	// 결과 이펙트와 글자가 화면에 남는 시간
	// 이 시간도 연출 길이에 포함된다 - 성공하면 곧바로 전투가 끝나 무대가 철거되므로,
	// 여기서 잡아두지 않으면 팝업이 뜬 프레임에 페이드아웃이 시작되어 안보임
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Capture", meta = (ClampMin = "0"))
	float CaptureResultHoldSeconds = 0.8f;


	// ----------------------------
	//			사망 이펙트
	// ----------------------------

	// 스프라이트가 완전히 사라지기까지의 시간
	// 사망 나이아가라의 버스트 타이밍이 이 길이에 맞춰져 있으므로 같이 바꿔야 함
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Down", meta = (ClampMin = "0.05"))
	float DeathDissolveSeconds = 0.35f;
	
	
	// ----------------------------
	//			쓰러진 아군 표시
	// ----------------------------
	
	// 아군은 쓰러져도 아바타를 남기므로 사망한 아군은 톤다운으로 구분
	// 이후에 별도 애니메이션 출력할수도 있음
	UPROPERTY(EditDefaultsOnly, Category = "NU|Battle|Down")
	FLinearColor DownedColor = FLinearColor(0.32f, 0.32f, 0.38f, 1.f);
	
	
	// ----------------------------
	//			식별용
	// ----------------------------
	
	UPROPERTY(ReplicatedUsing = OnRep_NaiaId)
	int32 NaiaId = INDEX_NONE;

	// 진형 방향, 단방향 스프라이트(나이아)는 이 값을 무시함
	UPROPERTY(ReplicatedUsing = OnRep_BattleFacing)
	EFieldFacing BattleFacing = EFieldFacing::Down;

	// 아군이면 true, 스폰한 쪽이 넣어줘야 함(기본값은 적)
	UPROPERTY(Replicated)
	bool bIsAlly = false;

private:
	// 나이아 정의에 피격 시퀀스가 있으면 한 번 재생함, 다른 행동 모션 중이면 끊지 않음
	void PlayHitAnimation();

	// 이 아바타의 나이아 정의, 캐릭터거나 NaiaId 가 없으면 nullptr
	const FNUNaiaDefinition* FindNaiaDefinition() const;

	// 나이아 정의가 제자리 공격(bAttackInPlace)인지
	bool ShouldAttackInPlace() const;

	void TickHitReaction(float DeltaSeconds);

	// 스프라이트를 몇 프레임 아주 조금 움직였다 되돌림, 나이아 외형을 입힐 때(전투 시작 암전 · 교체) 한 번 돎
	void TickMotionWarmUp();
	void TickHitFlash(float DeltaSeconds);
	void TickCaptureShake(float DeltaSeconds);

	// 적이 죽을 때 스프라이트를 아래에서 위로 침식시킴, 끝나면 CommitDeadVisual 로 숨김
	void TickDeathDissolve(float DeltaSeconds);

	// 디졸브를 시작함, 머티리얼이 없거나 이미 도는 중이면 false 를 돌려주고,
	// 그때는 부른 쪽이 예전처럼 곧바로 숨겨야 함
	bool BeginDeathDissolve();

	// 사망 파티클 게임플레이큐를 띄움
	void PlayDeathCue();

	// 사망 표시를 실제로 반영하는 한 곳
	// 로컬은 GAS 사망 태그(HandleDeadTagChanged), 넷은 액션 큐(SetDeadVisual)
	// 포획 흔들림 중이면 흔들림이 끝날 때까지 미룬다
	void ApplyDeadVisual(bool bDead);

	// 미루지 않고 지금 반영함, 미뤄둔 것을 흔들림 끝에서 처리할 때만 직접 호출
	void CommitDeadVisual(bool bDead);

	// 쓰러진 표시를 현재 상태에 맞게 다시 그림
	// 점등이 끝날 때도 이 함수를 지나야 함
	void RefreshDownedVisual();

	// 흔들림이 끝난 뒤 결과 이펙트와 팝업을 띄우고, 미뤄둔 사망 처리를 반영함
	void FinishCaptureShakes();

	// 화면 기준 좌우를 액터 로컬 벡터로 돌려줌
	// 진형이 어느 축으로 늘어서 있든 카메라에서 보면 좌우로 흔들려야 함
	FVector ResolveScreenRightLocal() const;

	// ActiveSpec 의 CastVFX/ProjectileVFX 를 재생함, TickCastVFX 가 모션마다 시전 지점에서 부름
	// (PlayActionAnimation 자체 안에는 넣지 않음, 애님 소스 불일치로 나이아는 건너뛰기 때문)
	// FlightTime 은 탄환이 대상에 닿기까지의 시간 - 그 모션의 시전 위치부터 Hit 까지
	void PlaySkillVFX(float FlightTime);

private:
	// 스프라이트의 상대위치 원점, 밀려난 뒤 돌아오는 위치
	FVector SpriteOriginRelativeLocation;
	
	// 액터 로컬 기준 밀려나는 방향
	FVector HitReactionDirection;
	
	// 재생한 공격 애니메이션 횟수
	int32 PlayedHitAnimations;

	// 낸 시전 연출 횟수, 모션마다 한 번씩 그 모션의 Cast 지점에 냄
	int32 PlayedCastVFX = 0;

	// 반복 타격의 두 번째 모션부터 틀기 시작하는 모션 안 위치(초) = Hit - HitInterval
	// 피해는 HitInterval 마다 들어가는데 모션은 그보다 길어서, 처음부터 다시 틀면 Hit 프레임에 닿기 전에 또 끊김
	// 직전 타격 순간에 이 위치부터 틀면 매 모션이 자기 피해 순간에 Hit 프레임에 닿음. 모션을 안 트는 아바타는 0
	float RepeatMotionStart = 0.f;

	// 나이아 정의의 공격 · 피격 시퀀스(AttackAnimation · HitAnimation), 외형을 입힐 때 미리 불러 둠
	// 애님 BP 가 참조하지 않는 시퀀스라 첫 공격 · 첫 피격 때 처음 불러오면, 에디터에선 그 그림 텍스처가 아직 준비 중이라
	// 한동안 기본 텍스처로 그려져 스프라이트 칸이 검은 사각형으로 보임, 패키지 빌드에서도 전투 도중 동기 로드로 끊김
	UPROPERTY(Transient)
	TObjectPtr<UPaperZDAnimSequence> NaiaAttackAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UPaperZDAnimSequence> NaiaHitAnimation;

	// 첫 움직임 예열에 남은 프레임 수 (TickMotionWarmUp)
	int32 MotionWarmUpFramesLeft = 0;

	// 인정 반복 시퀀스, MulticastPlayConcession 이 넣음
	UPROPERTY(Transient)
	TObjectPtr<UPaperZDAnimSequence> ConcessionLoopAnimation;

	// 인정 반복 시퀀스를 처음부터 틈, 틀었으면 true
	bool PlayConcessionLoop();

	// PreloadActionVFX 가 불러 둔 행동 연출, 가비지 컬렉션에 안 걸리게 아바타가 사라질 때까지 들고 있음
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraSystem>> PreloadedActionVFX;

	// 재생한 모션 중 시전 지점이 지난 것의 연출을 냄
	void TickCastVFX();
	
	// 밀려나는 중에 다시 맞으면 0에서 시작
	float HitReactionElapsed;
	bool bHitReactionActive;
	
	float HitFlashElapsed;
	bool bHitFlashActive;
	
	// 포획 흔들림 진행 상태
	int32 CaptureShakeTotal = 0;
	float CaptureShakeElapsed = 0.f;
	bool bCaptureShakeActive = false;
	bool bCaptureShakeSucceeded = false;
	
	// 결과를 이미 띄웠는지, 흔들림이 끝난 뒤 유지 구간에서 두 번 띄우지 않기 위함
	bool bCaptureResultShown = false;
	
	// 흔들 방향(액터 로컬), 시작할 때 카메라 기준으로 한 번 구함
	FVector CaptureShakeAxis = FVector::ZeroVector;
	
	// 흔들림 도중에 들어온 사망 처리를 미뤄뒀는지
	bool bDeadVisualPendingShake = false;

	// 마지막으로 실제 적용한 사망 표시, 같은 값이 또 오면 아무것도 하지 않으려고 들고 있음
	bool bDeadVisualApplied = false;
	bool bLastDeadVisual = false;

	// 쓰러진 표시가 켜져 있는지, 색을 되돌릴 때 필요
	bool bDownedVisualActive = false;

	// 사망 디졸브 진행 상태
	float DeathDissolveElapsed = 0.f;
	bool bDeathDissolveActive = false;
};
