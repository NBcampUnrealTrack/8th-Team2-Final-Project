// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleAvatar.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "AnimSequences/PaperZDAnimSequence.h"
#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "PaperZDAnimBPGeneratedClass.h"
#include "PaperZDAnimInstance.h"
#include "Battle/Avatar/NUBattleMotionMarkerNotify.h"
#include "Battle/Avatar/NUBattleProjectile.h"
#include "Battle/Avatar/NUBattleHitDim.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUSkillVFXProfile.h"
#include "Battle/Presentation/NUBattleActionPresentation.h"
#include "Battle/Presentation/NUBattleVFXPreloadSubsystem.h"
#include "NUGAS/Ability/NUBattleGameplayAbility.h"
#include "CharacterState/NUPlayerState.h"
#include "UI/Common/NU_CombatantStatusWidget.h"
#include "Components/WidgetComponent.h"
#include "Battle/Avatar/NUBattleEnemyAvatar.h"
#include "UI/NUUISettings.h"
#include "Engine/GameInstance.h"
#include "Field/NUSpriteVisualComponent.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "AnimSequences/Players/PaperZDAnimationPlaybackData.h"
#include "Net/UnrealNetwork.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "PlayerController/NUPlayerController.h"
#include "TimerManager.h"


namespace
{
	// 무기 · 발사 소리가 타격보다 앞서는 시간, 시전 표시(Cast)가 있는 공격 모션의 시전~타격 간격과 같음
	constexpr float SwingLeadTime = 0.15f;

	// 속성별 소리: 스킬을 쏠 때 / 스킬이 닿을 때 / 나이아 기본 공격이 닿을 때
	enum class ENUElementSFX : uint8
	{
		Launch,
		Impact,
		Strike
	};

	// 속성이 없으면 쏠 때 · 닿을 때는 빈 태그(공격자의 소리로 넘어감), 기본 공격은 몸으로 부딪치는 소리
	FGameplayTag ResolveElementSFX(const FGameplayTag& Element, ENUElementSFX Kind)
	{
		static const FGameplayTag Table[][4] =
		{
			{ NU::Element::Fire, NU::SFX::Battle::Attack::Fire, NU::SFX::Battle::Impact::Fire, NU::SFX::Battle::Strike::Fire },
			{ NU::Element::Water, NU::SFX::Battle::Attack::Water, NU::SFX::Battle::Impact::Water, NU::SFX::Battle::Strike::Water },
			{ NU::Element::Grass, NU::SFX::Battle::Attack::Grass, NU::SFX::Battle::Impact::Grass, NU::SFX::Battle::Strike::Grass },
			{ NU::Element::Earth, NU::SFX::Battle::Attack::Earth, NU::SFX::Battle::Impact::Earth, NU::SFX::Battle::Strike::Earth },
			{ NU::Element::Lightning, NU::SFX::Battle::Attack::Lightning, NU::SFX::Battle::Impact::Lightning, NU::SFX::Battle::Strike::Lightning },
			{ NU::Element::Wind, NU::SFX::Battle::Attack::Wind, NU::SFX::Battle::Impact::Wind, NU::SFX::Battle::Strike::Wind },
			{ FGameplayTag(), FGameplayTag(), FGameplayTag(), NU::SFX::Battle::Strike::Body },
		};

		for (const FGameplayTag* Row : Table)
		{
			if (Row[0] == Element || !Row[0].IsValid())
			{
				return Row[1 + static_cast<int32>(Kind)];
			}
		}

		return FGameplayTag();
	}

	// 첫 움직임 예열 (TickMotionWarmUp), 화면에 안 보일 만큼만 움직임
	constexpr int32 MotionWarmUpFrames = 4;
	constexpr float MotionWarmUpOffset = 0.1f;

	// 체력/SP UI가 확인할 어트리뷰트, 바인딩/해제 시 이 순서대로 사용
	TArray<FGameplayAttribute> GetWatchedVitalAttributes()
	{
		return{
			UNUVitalAttributeSet::GetHealthAttribute(),
			UNUVitalAttributeSet::GetMaxHealthAttribute(),
			UNUVitalAttributeSet::GetSPAttribute(),
			UNUVitalAttributeSet::GetMaxSPAttribute()
		};
	}
}

ANUBattleAvatar::ANUBattleAvatar()
{
	PrimaryActorTick.bCanEverTick = true;
	
	bReplicates = true;
	SetReplicateMovement(false);	
	
	// 아레나를 공유하므로 거리 컬링이 의미가 없어 항상 복제로 둠
	// 클라가 Standalone 이라 relevancy 가 평가되지 않고, 인카운터를 둘 이상 만드는 경로도 없음
	// UE 서버와 파티 전투가 생기면 IsNetRelevantFor 로 참가자 커넥션만 통과시켜야 함
	bAlwaysRelevant = true;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	
	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(RootComponent);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	Animation->InitRenderComponent(Sprite);

	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, Animation, NUSpriteVisuals::MakeBattleAvatar());

	TargetIndicatorAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("TargetIndicatorAnchor"));
	TargetIndicatorAnchor->SetupAttachment(Sprite);
	TargetIndicatorAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	
	CommandUIAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("CommandUIAnchor"));
	CommandUIAnchor->SetupAttachment(RootComponent);
	CommandUIAnchor->SetRelativeLocation(FVector(0.0f, 0.f, 80.0f));
	
	VitalBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("VitalBarWidget"));
	VitalBarWidget->SetupAttachment(RootComponent);
	VitalBarWidget->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	
	VitalBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	VitalBarWidget->SetDrawSize(FVector2D(120.f, 24.f));
	VitalBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	bShowSPBar = true;
	bVitalBarHidden = false;
	CombatantId = INDEX_NONE;
	
	HitReactionDistance = 24.f;
	HitReactionOutTime = 0.06f;
	HitReactionReturnTime = 0.12f;
	
	SpriteOriginRelativeLocation = FVector::ZeroVector;
	HitReactionDirection = FVector::ZeroVector;
	PlayedHitAnimations = 0;
	HitReactionElapsed = 0.f;
	bHitReactionActive = false;
	
	HitFlashColor = FLinearColor(1.f, 0.25f, 0.25f, 1.f);
	HitFlashDuration = 0.12f;
	HitFlashElapsed = 0.f;
	bHitFlashActive = false;
}

// 플레이어, 정령은 PlayerState, SpiritState에서 가져와야 함
UAbilitySystemComponent* ANUBattleAvatar::GetAbilitySystemComponent() const
{
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AbilitySystemOwner);
}

UNUAbilitySystemComponent* ANUBattleAvatar::GetNUASC() const
{
	return Cast<UNUAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AbilitySystemOwner));
}

void ANUBattleAvatar::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, AbilitySystemOwner);
	DOREPLIFETIME(ThisClass, CombatantId);
	DOREPLIFETIME(ThisClass, bVitalBarHidden);
	DOREPLIFETIME(ThisClass, NaiaId);
	DOREPLIFETIME(ThisClass, BattleFacing);
	DOREPLIFETIME(ThisClass, bIsAlly);
}

void ANUBattleAvatar::BeginPlay()
{
	// 머리 위 상태 표시 클래스 --- 프로젝트 세팅에 지정돼 있으면 그것으로. 위젯은 부모 BeginPlay(InitWidget)가 만드므로 그 전에 바꾼다
	if (GetNetMode() != NM_DedicatedServer && IsValid(VitalBarWidget))
	{
		const UNUUISettings* UISettings = GetDefault<UNUUISettings>();
		const bool bEnemy = IsA<ANUBattleEnemyAvatar>();
		if (UClass* TagClass = (bEnemy ? UISettings->BattleEnemyTagClass : UISettings->BattleAllyTagClass).LoadSynchronous())
		{
			VitalBarWidget->SetWidgetClass(TagClass);
			VitalBarWidget->SetDrawAtDesiredSize(true);
		}
	}

	Super::BeginPlay();
	
	HomeTransform = GetActorTransform();
	
	if (GetNetMode() != NM_DedicatedServer)
	{
		SetVitalsToWidget();
		
		ApplyCombatantVisual();
		
		if (IsValid(Sprite))
		{
			SpriteOriginRelativeLocation = Sprite->GetRelativeLocation();
		}
	}
}

void ANUBattleAvatar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	
	SpriteVisual->UpdateBillboard();
	TickAnimation(DeltaSeconds);

	// 피격 밀려남 · 포획 흔들림보다 앞에 둠, 같은 스프라이트 위치를 쓰므로 그 연출들이 이겨야 함
	TickMotionWarmUp();
	TickHitReaction(DeltaSeconds);
	TickHitFlash(DeltaSeconds);
	
	// 피격 밀려남보다 뒤에 둠, 둘이 같은 스프라이트 위치를 쓰므로 포획 연출이 이겨야 함
	TickCaptureShake(DeltaSeconds);

	// 위치가 아니라 색 알파를 만지므로 위 연출들과 충돌하지 않음
	TickDeathDissolve(DeltaSeconds);
}

void ANUBattleAvatar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindCombatantDelegates();

	Super::EndPlay(EndPlayReason);
}

// 현재 AbilitySystemOwner의 ASC에서 델리게이트를 떼고 핸들을 비움
// AbilitySystemOwner를 갈기 전에 반드시 먼저 호출해야함
// GetNUASC()가 AbilitySystemOwner를 거치므로, 먼저 대입하면 이전 ASC 쪽의 델리게이트 해제가 불가능함
void ANUBattleAvatar::UnbindCombatantDelegates()
{
	if (UNUAbilitySystemComponent* ASC = GetNUASC())
	{
		ASC->RegisterGameplayTagEvent(
			NU::State::Dead, EGameplayTagEventType::NewOrRemoved).Remove(DeadTagDelegateHandle);

		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : VitalDelegateHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
	}

	// BindDeathState / BindVitalAttributes 가 이 값들로 중복 방지 판정을 하므로 반드시 비워야 함
	// 안 비우면 재바인딩이 조기 반환으로 실패함
	DeadTagDelegateHandle.Reset();
	VitalDelegateHandles.Empty();
}

USceneComponent* ANUBattleAvatar::GetTargetIndicatorAnchor() const
{
	return TargetIndicatorAnchor;
}

USceneComponent* ANUBattleAvatar::GetCommandUIAnchor() const
{
	return CommandUIAnchor;
}

int32 ANUBattleAvatar::GetCombatantId() const
{
	return CombatantId;
}

void ANUBattleAvatar::BindDeathState()
{
	// 중복 방지
	if (DeadTagDelegateHandle.IsValid())
	{
		return;
	}
	
	UNUAbilitySystemComponent* ASC = GetNUASC();
	if (!IsValid(ASC))
	{
		return;
	}
	
	DeadTagDelegateHandle = ASC->RegisterGameplayTagEvent(
		NU::State::Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleDeadTagChanged);
	
	HandleDeadTagChanged(NU::State::Dead, ASC->GetTagCount(NU::State::Dead));
}

void ANUBattleAvatar::BindVitalAttributes()
{
	if (!VitalDelegateHandles.IsEmpty())
	{
		return;
	}
	
	UNUAbilitySystemComponent* ASC = GetNUASC();
	if (!IsValid(ASC))
	{
		return;
	}
	
	for (const FGameplayAttribute& Attribute : GetWatchedVitalAttributes())
	{
		FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
			.AddUObject(this, &ThisClass::HandleVitalAttributeChanged);
		
		VitalDelegateHandles.Emplace(Attribute, Handle);
	}
	
	// 바인딩 전에 복제/적용된 값 적용하기 위함
	SetVitalsToWidget();
}

void ANUBattleAvatar::HandleDeadTagChanged(const FGameplayTag, const int32 NewCount)
{
	if (!HasAuthority())
	{
		return;
	}

	// 로컬 전투의 사망 처리
	ApplyDeadVisual(NewCount > 0);
}

void ANUBattleAvatar::HandleVitalAttributeChanged(const struct FOnAttributeChangeData& Data)
{
	SetVitalsToWidget();
}

void ANUBattleAvatar::RefreshVitalWidget()
{
	SetVitalsToWidget();
}

void ANUBattleAvatar::SetVitalsToWidget()
{
	UNU_CombatantStatusWidget* Widget = GetVitalWidget();
	if (!Widget)
	{
		return;
	}

	const UNUAbilitySystemComponent* ASC = GetNUASC();
	ANUBattleEncounter* Encounter = ASC ? ASC->GetBattleEncounter() : nullptr;
	if (!Encounter)
	{
		return;
	}

	// 위젯이 자기 CombatantId 의 ASC 를 스스로 찾아 구독한다 — 여기서는 대상만 지정해주면 된다.
	Widget->SetCombatant(Encounter, CombatantId);
}

void ANUBattleAvatar::SetPendingSPSpend(float InPendingSPSpend)
{
	if (UNU_CombatantStatusWidget* Widget = GetVitalWidget())
	{
		Widget->SetPendingSpend(InPendingSPSpend);
	}
}

void ANUBattleAvatar::SetVitalBarHidden(bool NewHidden)
{
	if (!HasAuthority() || bVitalBarHidden == NewHidden)
	{
		return;
	}

	bVitalBarHidden = NewHidden;
	OnRep_VitalBarHidden();		// 리슨 서버일 경우
	ForceNetUpdate();
}

void ANUBattleAvatar::PlayHitReaction(FVector FromLocation)
{
	if (!IsValid(Sprite))
	{
		return;
	}

	// 피격 순간 화면을 잠깐 어둡게 (nu.VFX.HitDim.*; 전환 페이드 중이면 건너뜀)
	NUBattleHitDim::Play(GetWorld());

	// 도트 피해처럼 밀려날 방향이 없어도 피격 모션은 보여줌
	PlayHitAnimation();

	// 높이만 버리고 수평 성분은 그대로 두기
	FVector Direction = GetActorLocation() - FromLocation;
	Direction.Z = 0.f;
	
	// 공격자와 대상이 완전히 겹치면 밀려날 방향을 만들 수 없음
	// 도트 피해처럼 공격자가 없는 경우도 여기로 온다(호출부가 대상 자신을 넣음)
	if (Direction.IsNearlyZero())
	{
		return;
	}
	
	HitReactionDirection = 
		GetActorTransform().InverseTransformVectorNoScale(Direction.GetSafeNormal());
	
	HitReactionElapsed = 0.f;
	bHitReactionActive = true;
}

void ANUBattleAvatar::PlayHitAnimation()
{
	if (NaiaId <= 0 || !IsValid(Animation))
	{
		return;
	}

	// 외형을 입힐 때 불러 둔 것 (ApplyCombatantAppearance)
	const UPaperZDAnimSequence* HitSequence = NaiaHitAnimation;
	UPaperZDAnimInstance* AnimInstance = Animation->GetAnimInstance();
	if (!HitSequence || !AnimInstance || !CanPlayActionAnimation(HitSequence))
	{
		return;
	}

	// 공격 같은 행동 모션을 피격이 덮지 않게 함, 연달아 맞으면 피격 모션은 처음부터 다시 틈
	FPaperZDAnimationPlaybackData Playing;
	if (AnimInstance->GetAnimationOverrideDataBySlot(TEXT("DefaultSlot"), Playing)
		&& !Playing.WeightedAnimations.ContainsByPredicate([HitSequence](const FPaperZDWeightedAnimation& Weighted)
		{
			return Weighted.AnimSequencePtr.Get() == HitSequence;
		}))
	{
		return;
	}

	AnimInstance->PlayAnimationOverride(HitSequence, TEXT("DefaultSlot"));
}

void ANUBattleAvatar::PlayHitFlash()
{
	if (!IsValid(SpriteVisual))
	{
		return;
	}
	
	SpriteVisual->SetColorOverride(HitFlashColor);
	
	HitFlashElapsed = 0.f;
	bHitFlashActive = true;
}

bool ANUBattleAvatar::IsAllyCombatant() const
{
	return bIsAlly;
}

void ANUBattleAvatar::SetIsAlly(bool bInIsAlly)
{
	if (!HasAuthority())
	{
		return;
	}

	bIsAlly = bInIsAlly;
	ForceNetUpdate();
}

void ANUBattleAvatar::SetNaiaId(int32 InNaiaId)
{
	if (!HasAuthority())
	{
		return;
	}
	
	NaiaId = InNaiaId;
	
	ApplyCombatantVisual();
	ForceNetUpdate();
}

void ANUBattleAvatar::OnRep_VitalBarHidden()
{
	if (IsValid(VitalBarWidget))
	{
		VitalBarWidget->SetVisibility(!bVitalBarHidden);
	}
}

void ANUBattleAvatar::OnRep_NaiaId()
{
	ApplyCombatantVisual();
}

UNU_CombatantStatusWidget* ANUBattleAvatar::GetVitalWidget() const
{
	return IsValid(VitalBarWidget)
		? Cast<UNU_CombatantStatusWidget>(VitalBarWidget->GetUserWidgetObject())
		: nullptr;
}

void ANUBattleAvatar::ApplyCombatantVisual()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	
	ApplyCombatantAppearance();

	// 애님 BP가 새로 만들어졌으면 방향이 기본값으로 돌아가므로 항상 다시 넣어야 함
	ApplyBattleFacing();
}

void ANUBattleAvatar::ApplyCombatantAppearance()
{
	NaiaAttackAnimation = nullptr;
	NaiaHitAnimation = nullptr;

	if (!IsValid(SpriteVisual))
	{
		return;
	}

	// 캐릭터인데 여기까지 왔다면 아바타 클래스가 ANUBattlePlayerAvatar가 아니라는 뜻임
	// 그냥 두면 NaiaId가 0이라 기본 애님 BP가 그대로 남아 원인을 찾기 어려움
	if (bIsPlayerVisual)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] %s: 캐릭터 전투원인데 ANUBattlePlayerAvatar가 아님, 장비·외모가 적용되지 않음 "
				"(PlayerBattleAvatarClass / NetPlayerAvatarClass의 부모 클래스 확인)"),
			*GetNameSafe(this));
		return;
	}

	// 나이아(아군 정령, 적)는 Definition을 따라감, 필드 나이아와 원격 나이아가 같은 함수를 사용함
	SpriteVisual->ApplyNaiaAnimBlueprint(NaiaId);

	// 공격 · 피격 시퀀스도 지금 불러 둠, 첫 공격 · 첫 피격 때 처음 불러오면 그림이 준비되기 전 잠깐 검은 사각형이 보임
	const FNUNaiaDefinition* Definition = FindNaiaDefinition();
	NaiaAttackAnimation = Definition ? Definition->AttackAnimation.LoadSynchronous() : nullptr;
	NaiaHitAnimation = Definition ? Definition->HitAnimation.LoadSynchronous() : nullptr;

	// 첫 움직임을 전투 시작 암전 속에서 미리 치름 (TickMotionWarmUp)
	if (Definition)
	{
		MotionWarmUpFramesLeft = MotionWarmUpFrames;
	}
}

void ANUBattleAvatar::TickMotionWarmUp()
{
	// 에디터는 머티리얼 셰이더를 쓰일 때 그 자리에서 컴파일하고(FMaterial::TryGetShaders), 끝날 때까지 그 패스를 엔진 기본(불투명) 머티리얼로 그림
	// 나이아 스프라이트(Paper2D 마스크드)는 처음 움직일 때 움직임 정보를 깊이 패스에서 그리는 셰이더가 처음 쓰여서(r.VelocityOutputPass=0)
	// 에디터 세션마다 첫 움직임 몇 프레임 동안 칸 전체가 깊이를 막아 뒤 배경이 검게 빠짐
	// 등장 직후(암전 중)에 아주 조금 움직여 그 컴파일을 미리 시킴, 패키지 빌드는 셰이더가 다 구워져 있어 아무 일도 안 함
	if (MotionWarmUpFramesLeft <= 0 || !IsValid(Sprite) || bHitReactionActive)
	{
		return;
	}

	--MotionWarmUpFramesLeft;

	// 한 프레임씩 올렸다 되돌림, 마지막 프레임은 남은 수가 0 이라 반드시 제자리
	const FVector Offset = (MotionWarmUpFramesLeft % 2 == 1)
		? FVector(0.f, 0.f, MotionWarmUpOffset)
		: FVector::ZeroVector;

	Sprite->SetRelativeLocation(SpriteOriginRelativeLocation + Offset);
}

AActor* ANUBattleAvatar::GetAbilitySystemOwner() const
{
	return AbilitySystemOwner;
}

EFieldFacing ANUBattleAvatar::ResolveBattleFacing(bool bInIsAlly, bool bInIsPlayerVisual)
{
	// 적은 멀리 오른쪽 위에서 아군 쪽(왼쪽 아래)을 봄
	if (!bInIsAlly)
	{
		return EFieldFacing::Left;
	}
	
	// 플레이어는 카메라에 등을 보임
	// 나이아는 뒷모습 그림이 없어 옆모습으로 적 쪽(오른쪽 위)을 봄
	return bInIsPlayerVisual ? EFieldFacing::Up : EFieldFacing::Right;
}

bool ANUBattleAvatar::IsPlayerVisual() const
{
	return bIsPlayerVisual;
}

void ANUBattleAvatar::SetBattleFacing(EFieldFacing InFacing)
{
	if (!HasAuthority())
	{
		return;
	}

	BattleFacing = InFacing;

	ApplyBattleFacing();
	ForceNetUpdate();
}

void ANUBattleAvatar::OnRep_BattleFacing()
{
	ApplyBattleFacing();
}

void ANUBattleAvatar::ApplyBattleFacing()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 전투는 진형이 고정이라 이동 상태 없이 방향만 넣음
	// 4방향 애님 BP(플레이어)는 이 값으로 방향 시퀀스를 고름
	SpriteVisual->SetLocomotion(BattleFacing, false);

	// 나이아 애님 BP에는 SetDirectionality 노드가 없어 DirectionInput로 Facing 불가능
	// 나이아 그림은 왼쪽이 기준이라 오른쪽을 볼 때만 X축을 뒤집음
	// 깊이 배치에서는 아군 나이아가 오른쪽, 적이 왼쪽을 봄 (ResolveBattleFacing)
	SpriteVisual->SetHorizontalFlip(!bIsPlayerVisual && BattleFacing == EFieldFacing::Right);
}

void ANUBattleAvatar::TickAnimation(float DeltaSeconds)
{
	if (MotionPhase == ENUAvatarMotionPhase::Idle)
	{
		return;
	}
	
	MotionElapsed += DeltaSeconds;
	
	switch (MotionPhase)
	{
	case ENUAvatarMotionPhase::Telegraphing:
		// 이 동안 회피 창이 열려 있음, 아바타는 아직 움직이지 않음
		if (MotionElapsed >= ActiveSpec.TelegraphTime)
		{
			BeginActionMotion();
		}
		break;

	case ENUAvatarMotionPhase::Approaching:
		{
			float A = ActiveSpec.ApproachTime > KINDA_SMALL_NUMBER
				? FMath::Clamp(MotionElapsed / ActiveSpec.ApproachTime, 0.f, 1.f)
				: 1.f;
			
			SetActorLocation(FMath::Lerp(HomeTransform.GetLocation(), ActiveSpec.ActionLocation, A));
			
			if (A >= 1.f)
			{
				MotionPhase = ENUAvatarMotionPhase::Acting;
				MotionElapsed = 0.f;
				PlayActionAnimation();
				PlayedHitAnimations = 1;	// 위에서 재생처리 하므로 첫 타격을 처리해야 함
				PlayedCastVFX = 0;
				TickCastVFX();	// 시전 지점이 0 이면 지금 바로 나감
			}
			break;
		}
		
	case ENUAvatarMotionPhase::Acting:
		{
			int32 HitCount = FMath::Max(1, ActiveSpec.HitCount);
			float RepeatTime = (HitCount - 1) * ActiveSpec.HitInterval;
			
			if (ActiveSpec.HitInterval > KINDA_SMALL_NUMBER
				&& PlayedHitAnimations < HitCount)
			{
				// k 번째 모션은 RepeatMotionStart 위치부터 틀어 k 번째 피해(Hit + k * HitInterval)에 Hit 프레임이 닿음
				const float RepeatAt = RepeatMotionStart + PlayedHitAnimations * ActiveSpec.HitInterval;
				if (MotionElapsed >= RepeatAt)
				{
					// 틱이 늦은 만큼 앞에서 틀어 피해 타이머와 어긋나지 않게 함
					PlayActionAnimation(RepeatMotionStart + (MotionElapsed - RepeatAt));
					++PlayedHitAnimations;
				}
			}

			TickCastVFX();
			
			if (MotionElapsed >= ActiveSpec.ActionTime + RepeatTime)
			{
				MotionPhase = ENUAvatarMotionPhase::Returning;
				MotionElapsed = 0.f;
			}
			break;
		}

	case ENUAvatarMotionPhase::Returning:
		{
			float A = ActiveSpec.ReturnTime > KINDA_SMALL_NUMBER
				? FMath::Clamp(MotionElapsed / ActiveSpec.ReturnTime, 0.f, 1.f) 
				: 1.f;
			
			SetActorLocation(FMath::Lerp(ActiveSpec.ActionLocation, HomeTransform.GetLocation(), A));
			
			if (A >= 1.f)
			{
				SetActorTransform(HomeTransform); // 제자리로 돌리기
				MotionPhase = ENUAvatarMotionPhase::Idle;
			}
			break;
		}
		
	default: 
		break;
	}
}

void ANUBattleAvatar::PlayActionAnimation(float StartPosition)
{
	UPaperZDAnimSequence* Sequence = ResolveActionAnimation(ActiveSpec);
	if (!Sequence || !IsValid(Animation))
	{
		return;
	}

	UPaperZDAnimInstance* AnimInstance = Animation->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	if (!CanPlayActionAnimation(Sequence))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[NU] %s: 애님 소스가 달라 액션 애니메이션을 건너뜁니다 (NaiaId=%d)"),
			*GetNameSafe(this), NaiaId);
		return;
	}

	// 공격 모션의 준비 동작 소리(활 당기기 · 곰 변신), 반복 타격은 준비 구간을 건너뛰므로 첫 모션에만
	if (PlayedHitAnimations == 0)
	{
		UNUSFXSubsystem::Play(this, GetAttackSFX(ENUBattleAttackSFX::Windup));
	}

	FZDOnAnimationOverrideEndSignature OnEnd;
	OnEnd.BindUObject(this, &ThisClass::HandleActionAnimationEnded);

	// 재생 중에 앞 위치로 다시 틀면 PaperZD 스킨 시계는 "한 바퀴 돌았다" 로 읽고 시퀀스 길이만큼 더함
	// 스킨 플립북과 시퀀스 길이가 같으면(직업별 공격 시퀀스) 정확히 StartPosition 에 닿음
	AnimInstance->PlayAnimationOverride(
		Sequence,
		TEXT("DefaultSlot"),
		1.f,
		StartPosition,
		OnEnd
		);
}

bool ANUBattleAvatar::CanPlayActionAnimation(const UPaperZDAnimSequence* Sequence) const
{
	if (!IsValid(Sequence) || !IsValid(Animation))
	{
		return false;
	}

	UPaperZDAnimInstance* AnimInstance = Animation->GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	// 애니메이션 소스가 일치해야 재생됨
	const UPaperZDAnimBPGeneratedClass* AnimClass = GetAnimClass(AnimInstance);

	return !AnimClass || AnimClass->GetSupportedAnimationSource() == Sequence->GetAnimSource();
}

void ANUBattleAvatar::ResolveMotionTiming(FNUBattleActionMotionSpec& InOutSpec) const
{
	// 모션을 재생하지 않는 행동(나이아 · 제자리 행동 · 모션이 없는 어빌리티)은 적힌 값 그대로
	const UPaperZDAnimSequence* Sequence = ResolveActionAnimation(InOutSpec);
	if (!CanPlayActionAnimation(Sequence))
	{
		return;
	}

	const float Duration = GetActionAnimationDuration(Sequence);
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	InOutSpec.ActionTime = Duration;

	// 표시 시각은 시퀀스 타임라인 기준 초, 스킨 플립북도 시퀀스와 같은 속도로 흐르므로 그대로 나눔
	// 표시가 없으면 어빌리티·넷 프리셋의 비율과 Acting 시작 그대로
	float MarkerTime = 0.f;
	if (UNUBattleMotionMarkerNotify::FindMarkerTime(Sequence, ENUBattleMotionMarker::Hit, MarkerTime))
	{
		InOutSpec.HitTimeNormalized = FMath::Clamp(MarkerTime / Duration, 0.f, 1.f);
	}

	if (UNUBattleMotionMarkerNotify::FindMarkerTime(Sequence, ENUBattleMotionMarker::Cast, MarkerTime))
	{
		InOutSpec.CastTimeNormalized = FMath::Clamp(MarkerTime / Duration, 0.f, 1.f);
	}
}

void ANUBattleAvatar::TickCastVFX()
{
	// k 번째 모션의 위치 p 는 k * HitInterval + p 에 옴 (첫 모션은 0 부터, 반복은 RepeatMotionStart 부터 틀기 때문)
	// 반복 모션이 건너뛴 앞부분에 시전 지점이 있으면 그 모션이 시작하는 순간으로 당김
	const float CastOffset = ActiveSpec.ActionTime * FMath::Clamp(ActiveSpec.CastTimeNormalized, 0.f, 1.f);
	const float HitOffset = ActiveSpec.ActionTime * FMath::Clamp(ActiveSpec.HitTimeNormalized, 0.f, 1.f);

	while (PlayedCastVFX < PlayedHitAnimations)
	{
		const float CastPosition = PlayedCastVFX == 0 ? CastOffset : FMath::Max(CastOffset, RepeatMotionStart);
		if (MotionElapsed < PlayedCastVFX * ActiveSpec.HitInterval + CastPosition)
		{
			break;
		}

		// 탄환은 이 모션의 시전 위치에서 출발해 같은 모션의 Hit 에 닿음
		PlaySkillVFX(FMath::Max(HitOffset - CastPosition, 0.f));
		++PlayedCastVFX;
	}
}

float ANUBattleAvatar::GetActionAnimationDuration(const UPaperZDAnimSequence* Sequence) const
{
	return IsValid(Sequence) ? Sequence->GetTotalDuration() : 0.f;
}

UPaperZDAnimSequence* ANUBattleAvatar::ResolveActionAnimation(const FNUBattleActionMotionSpec& Spec) const
{
	// 어빌리티에 물린 시퀀스가 이 아바타에서 재생되면 그대로 씀
	// 제자리 행동(경고 · 아이템 · 버프 · 포획 · 도주)에는 나이아 공격 모션을 붙이지 않음
	if (bIsPlayerVisual || NaiaId <= 0 || Spec.Motion == ENUBattleActionMotion::InPlace
		|| CanPlayActionAnimation(Spec.ActionAnimation))
	{
		return Spec.ActionAnimation;
	}

	// 나이아 · 적은 어빌리티 시퀀스가 대개 캐릭터 애님 소스라 재생이 안 되므로 나이아 정의의 공격 시퀀스로 대신함
	// 외형을 입힐 때 불러 둔 것 (ApplyCombatantAppearance)
	return NaiaAttackAnimation ? NaiaAttackAnimation.Get() : Spec.ActionAnimation.Get();
}

const FNUNaiaDefinition* ANUBattleAvatar::FindNaiaDefinition() const
{
	if (bIsPlayerVisual || NaiaId <= 0)
	{
		return nullptr;
	}

	const UNUNaiaDataSubsystem* NaiaData = GetGameInstance() ? GetGameInstance()->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;

	return NaiaData ? NaiaData->FindDefinitionById(NaiaId) : nullptr;
}

bool ANUBattleAvatar::ShouldAttackInPlace() const
{
	const FNUNaiaDefinition* Definition = FindNaiaDefinition();

	return Definition && Definition->bAttackInPlace;
}

void ANUBattleAvatar::ResolveActionVFX(FNUBattleActionMotionSpec& InOutSpec) const
{
	// 스킬 표가 정한 이펙트 · 제자리 행동 · 공격이 아닌 전진 행동은 그대로 (캐릭터의 직업 기본 공격과 같은 규칙)
	if (InOutSpec.bHasSkillVFXProfile || InOutSpec.Motion == ENUBattleActionMotion::InPlace
		|| InOutSpec.bSkipAttackPresentation)
	{
		return;
	}

	// 나이아 · 적의 기본 공격은 종의 속성별 작은 투사체와 착탄, 없는 속성은 어빌리티 기본 연출(검기) 그대로
	const FNUNaiaDefinition* Definition = FindNaiaDefinition();
	const FNUJobAttackVFX* ElementVFX = Definition
		? GetDefault<UNUBattleDataSettings>()->NaiaElementAttackVFX.Find(Definition->ElementTag)
		: nullptr;
	if (!ElementVFX)
	{
		return;
	}

	// 행이 있으면 통째로 덮음, 빈 칸은 안 나감
	InOutSpec.CastVFX = ElementVFX->CastVFX;
	InOutSpec.ProjectileVFX = ElementVFX->ProjectileVFX;
	InOutSpec.ImpactVFX = ElementVFX->ImpactVFX;
	InOutSpec.EffectColor = ElementVFX->EffectColor;

	// 소리도 속성을 따라감: 쏠 때 속성 발사음, 닿을 때 속성 착탄음 (스펙의 속성은 효과음에만 쓰임)
	if (!InOutSpec.AttackElement.IsValid())
	{
		InOutSpec.AttackElement = Definition->ElementTag;
	}
}

void ANUBattleAvatar::CollectResolvedActionVFX(TArray<TSoftObjectPtr<UNiagaraSystem>>& OutVFX) const
{
	const FNUNaiaDefinition* Definition = FindNaiaDefinition();
	const FNUJobAttackVFX* ElementVFX = Definition
		? GetDefault<UNUBattleDataSettings>()->NaiaElementAttackVFX.Find(Definition->ElementTag)
		: nullptr;
	if (!ElementVFX)
	{
		return;
	}

	OutVFX.AddUnique(ElementVFX->CastVFX);
	OutVFX.AddUnique(ElementVFX->ProjectileVFX);
	OutVFX.AddUnique(ElementVFX->ImpactVFX);
}

FGameplayTag ANUBattleAvatar::GetAttackSFX(ENUBattleAttackSFX Moment) const
{
	// 나이아 · 적은 준비 동작 없이 몸으로 달려들어 부딪침, 닿는 소리는 종의 속성을 따라감
	// 제자리에서 공격하는 종(보스)은 달려들지 않으므로 닿는 소리만
	if (Moment == ENUBattleAttackSFX::Swing)
	{
		return ShouldAttackInPlace() ? FGameplayTag() : FGameplayTag(NU::SFX::Battle::Attack::Lunge);
	}

	if (Moment == ENUBattleAttackSFX::Impact)
	{
		const FNUNaiaDefinition* Definition = FindNaiaDefinition();
		return ResolveElementSFX(Definition ? Definition->ElementTag : FGameplayTag(), ENUElementSFX::Strike);
	}

	return FGameplayTag();
}

void ANUBattleAvatar::HandleActionAnimationEnded(bool bCompleted)
{
	// 연출 종료 통보
}

void ANUBattleAvatar::MulticastPlayConcession_Implementation(UPaperZDAnimSequence* Once, UPaperZDAnimSequence* Loop)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ConcessionLoopAnimation = Loop;

	UPaperZDAnimInstance* AnimInstance = IsValid(Animation) ? Animation->GetAnimInstance() : nullptr;
	if (!AnimInstance || !CanPlayActionAnimation(Once) || Once->GetTotalDuration() <= KINDA_SMALL_NUMBER)
	{
		PlayConcessionLoop();
		return;
	}

	FZDOnAnimationOverrideEndSignature OnEnd;
	OnEnd.BindUObject(this, &ThisClass::HandleConcessionAnimationEnded);
	AnimInstance->PlayAnimationOverride(Once, TEXT("DefaultSlot"), 1.f, 0.f, OnEnd);
}

void ANUBattleAvatar::HandleConcessionAnimationEnded(bool bCompleted)
{
	// 다른 모션이 끊고 들어왔으면 그 모션에 맡김
	if (!bCompleted || !PlayConcessionLoop())
	{
		return;
	}

	// PaperZD 는 끝난 시퀀스를 빼고 이 콜백을 부른 뒤 새로 건 시퀀스를 다음 틱부터 슬롯에 넘겨서 이번 틱엔 대기 모션이 한 번 보임 => 이번 틱 몫을 직접 넣음
	FPaperZDAnimationPlaybackData PlaybackData;
	PlaybackData.WeightedAnimations.AddDefaulted_GetRef().AnimSequencePtr = ConcessionLoopAnimation;
	Animation->GetAnimInstance()->SetAnimationOverrideDataBySlot(TEXT("DefaultSlot"), PlaybackData, true);
}

bool ANUBattleAvatar::PlayConcessionLoop()
{
	UPaperZDAnimInstance* AnimInstance = IsValid(Animation) ? Animation->GetAnimInstance() : nullptr;

	// 길이가 0 이면 끝나자마자 다시 불려 멈추지 않으므로 틀지 않음
	if (!AnimInstance || !CanPlayActionAnimation(ConcessionLoopAnimation)
		|| ConcessionLoopAnimation->GetTotalDuration() <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FZDOnAnimationOverrideEndSignature OnEnd;
	OnEnd.BindUObject(this, &ThisClass::HandleConcessionAnimationEnded);

	return AnimInstance->PlayAnimationOverride(ConcessionLoopAnimation, TEXT("DefaultSlot"), 1.f, 0.f, OnEnd);
}

void ANUBattleAvatar::PreloadActionVFX(const TArray<int32>& SkillIds,
	const TArray<const UNUBattleGameplayAbility*>& Abilities)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 빈 칸도 같이 모이지만 불러올 때 걸러짐
	TArray<TSoftObjectPtr<UNiagaraSystem>> SoftVFX;

	// 어빌리티 기본값 (기본 공격의 검기 등), 스킬 표 행이 없는 행동은 이것으로 나감
	for (const UNUBattleGameplayAbility* Ability : Abilities)
	{
		if (!IsValid(Ability))
		{
			continue;
		}

		FNUBattleActionMotionSpec Defaults;
		Ability->ApplyPresentationDefaults(Defaults);
		SoftVFX.AddUnique(Defaults.CastVFX);
		SoftVFX.AddUnique(Defaults.ProjectileVFX);
		SoftVFX.AddUnique(Defaults.ImpactVFX);
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleDataSubsystem* DataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	for (int32 SkillId : SkillIds)
	{
		const FNUSkillVFXProfile* Profile =
			IsValid(DataSubsystem) ? DataSubsystem->FindSkillVFXProfile(SkillId) : nullptr;
		if (!Profile)
		{
			continue;
		}

		SoftVFX.AddUnique(Profile->CastVFX);
		SoftVFX.AddUnique(Profile->ProjectileVFX);
		SoftVFX.AddUnique(Profile->ImpactVFX);
	}

	// 캐릭터의 직업 기본 공격 연출
	CollectResolvedActionVFX(SoftVFX);

	// 자기 임팩트가 없는 공격이 쓰는 대체 임팩트
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	SoftVFX.AddUnique(Settings->FallbackImpactVFX);
	SoftVFX.AddUnique(Settings->FallbackProjectileVFX);

	// 보유 스킬 것은 게임 중에 이미 비동기로 불러 둠(UNUBattleVFXPreloadSubsystem), 메모리에 있으면 다시 읽지 않음
	// 여기서 새로 읽는 것은 적 패턴 스킬처럼 전투에 들어와야 아는 것, 암전 중이라 끊김이 안 보임
	PreloadedActionVFX.Reset();
	for (const TSoftObjectPtr<UNiagaraSystem>& VFX : SoftVFX)
	{
		if (UNiagaraSystem* System = VFX.LoadSynchronous())
		{
			PreloadedActionVFX.AddUnique(System);
		}
	}

	// 처음 띄울 때의 준비 비용을 암전 중에 미리 치르게 넘김, 이미 예열한 것은 건너뜀
	if (UNUBattleVFXPreloadSubsystem* Preload =
		GameInstance ? GameInstance->GetSubsystem<UNUBattleVFXPreloadSubsystem>() : nullptr)
	{
		Preload->AdoptLoaded(ObjectPtrDecay(PreloadedActionVFX));
	}

	UE_LOG(LogTemp, Log, TEXT("[NU] %s: 행동 연출 %d개를 미리 불러 둠 (어빌리티 %d · 스킬 %d)"),
		*GetNameSafe(this), PreloadedActionVFX.Num(), Abilities.Num(), SkillIds.Num());
}

void ANUBattleAvatar::PlaySkillVFX(float FlightTime)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 시전 시각의 소리는 화면에 뜨는 것을 따라간다: 날아가는 이펙트에 걸린 소리 > 시전 이펙트에 걸린 소리 (효과음 표의 SourceEvents)
	// 둘 다 없으면 속성 스킬은 속성별 발사음, 그 밖의 공격은 그 캐릭터의 휘두르는 소리, 제자리 스킬은 버프음
	// 제자리 행동(아이템 · 포획 · 도주)과 공격이 아닌 전진 행동에는 공격음이 없음
	if (!ActiveSpec.bSkipAttackPresentation)
	{
		// 날아가는 것 · 휘두르는 것의 소리는 타격 SwingLeadTime 앞에서 냄, 시전 표시가 없는 모션은 시전 시각이 모션 시작이라
		// 그대로 내면 활을 들기도 전에 시위 소리가 남. 시전 이펙트의 소리(마법진 · 포효 · 버프)와 속성탄은 이펙트가 뜨는 지금 냄
		FGameplayTag CastSFX = UNUSFXSubsystem::FindSourceEvent(this, ActiveSpec.ProjectileVFX.ToSoftObjectPath());
		bool bBeforeHit = CastSFX.IsValid();
		const FGameplayTag CastVFXEvent = UNUSFXSubsystem::FindSourceEvent(this, ActiveSpec.CastVFX.ToSoftObjectPath());
		if (!CastSFX.IsValid())
		{
			CastSFX = CastVFXEvent;
		}
		else if (CastVFXEvent == NU::SFX::Battle::Attack::Ascend)
		{
			// 필살기의 기 모으기는 날아가는 이펙트가 따로 있어도 냄 (마법진은 구체 발사음과 같은 순간이라 구체 소리만)
			UNUSFXSubsystem::Play(this, CastVFXEvent);
		}
		if (!CastSFX.IsValid() && ActiveSpec.Motion == ENUBattleActionMotion::InPlace)
		{
			CastSFX = ActiveSpec.bHasSkillVFXProfile ? FGameplayTag(NU::SFX::Battle::CastBuff) : FGameplayTag();
		}
		else if (!CastSFX.IsValid())
		{
			CastSFX = ResolveElementSFX(ActiveSpec.AttackElement, ENUElementSFX::Launch);
			if (!CastSFX.IsValid())
			{
				CastSFX = GetAttackSFX(ENUBattleAttackSFX::Swing);
				bBeforeHit = true;
			}
		}

		const float Delay = bBeforeHit ? FlightTime - SwingLeadTime : 0.f;
		if (Delay > KINDA_SMALL_NUMBER)
		{
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, CastSFX]()
			{
				UNUSFXSubsystem::Play(this, CastSFX);
			}), Delay, false);
		}
		else
		{
			UNUSFXSubsystem::Play(this, CastSFX);
		}
	}

	if (UNiagaraSystem* CastSystem = ActiveSpec.CastVFX.LoadSynchronous())
	{
		// 좌우반전, 이펙트 그림은 왼쪽으로 쏘는 것이 기준
		// 깊이 배치에서 아군은 오른쪽 위(적 쪽)를 향해 쏨 — 플레이어는 등을 보여 방향이 Up 이라
		// BattleFacing 으로 가르면 플레이어만 반대로 나감, 그래서 진영으로 가름
		const float EffectMirror = bIsAlly ? -1.f : 1.f;

		// 캐릭터에 붙이지 않고 스폰 시점 위치에 고정함
		UNiagaraComponent* CastComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			CastSystem,
			GetActorLocation() + FVector(0.f, 0.f, 60.f)
			);

		if (IsValid(CastComponent))
		{
			CastComponent->SetColorParameter(TEXT("EffectColor"), ActiveSpec.EffectColor);
			CastComponent->SetFloatParameter(TEXT("EffectMirror"), EffectMirror);
		}
	}

	UNiagaraSystem* ProjectileSystem = ActiveSpec.ProjectileVFX.LoadSynchronous();
	if (!ProjectileSystem)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// 판정에 관여하지 않는 액터라 로컬로만 스폰함
	ANUBattleProjectile* Projectile = World->SpawnActor<ANUBattleProjectile>(
		GetActorLocation(), 
		FRotator::ZeroRotator
		);

	if (IsValid(Projectile))
	{
		// 닿을 때의 소리도 착탄 이펙트에 걸린 소리가 먼저, 없으면 속성 스킬은 속성별 · 그 밖은 그 캐릭터의 타격음
		// 공격이 아닌 전진 행동(달래기 · 돌 옮기기)에도 대체 임팩트는 뜨지만 타격음은 내지 않음
		FGameplayTag ImpactSFX;
		if (!ActiveSpec.bSkipAttackPresentation)
		{
			ImpactSFX = UNUSFXSubsystem::FindSourceEvent(this, ActiveSpec.ImpactVFX.ToSoftObjectPath());
			if (!ImpactSFX.IsValid())
			{
				ImpactSFX = ResolveElementSFX(ActiveSpec.AttackElement, ENUElementSFX::Impact);
			}
			if (!ImpactSFX.IsValid())
			{
				ImpactSFX = GetAttackSFX(ENUBattleAttackSFX::Impact);
			}
		}

		// 비행 시간은 부르는 쪽(TickCastVFX)이 그 모션의 시전 위치에서 Hit 까지로 계산해 넘김
		Projectile->Launch(
			ProjectileSystem,
			ActiveSpec.ImpactVFX,
			GetActorLocation(),
			ActiveSpec.ProjectileTargetLocation, 
			FlightTime,
			ActiveSpec.EffectColor,
			ImpactSFX
			);
	}
}

void ANUBattleAvatar::TickHitReaction(float DeltaSeconds)
{
	if (!bHitReactionActive || !IsValid(Sprite))
	{
		return;
	}
	
	HitReactionElapsed += DeltaSeconds;
	
	float TotalTime = HitReactionOutTime + HitReactionReturnTime;
	if (TotalTime <= HitReactionElapsed)
	{
		Sprite->SetRelativeLocation(SpriteOriginRelativeLocation);
		bHitReactionActive = false;
		return;
	}
	
	float Alpha = 0.f;
	if (HitReactionElapsed < HitReactionOutTime)
	{
		Alpha = HitReactionElapsed / HitReactionOutTime;
	}
	else
	{
		float ReturnElapsed = HitReactionElapsed - HitReactionOutTime;
		Alpha = 1.f - ReturnElapsed / HitReactionReturnTime;
	}
	
	Sprite->SetRelativeLocation(SpriteOriginRelativeLocation + 
		HitReactionDirection * HitReactionDistance * Alpha);
}

void ANUBattleAvatar::TickHitFlash(float DeltaSeconds)
{
	if (!bHitFlashActive || !IsValid(SpriteVisual))
	{
		return;
	}
	
	HitFlashElapsed += DeltaSeconds;
	if (HitFlashElapsed < HitFlashDuration)
	{
		return;
	}
	
	// 쓰러진 아군이 맞으면 점등 뒤에 다시 쓰러진 색으로 돌아가야 함
	bHitFlashActive = false;
	RefreshDownedVisual();
}

void ANUBattleAvatar::MulticastPlayCaptureShakes_Implementation(int32 ShakeCount, bool bSuccess)
{
	PlayCaptureShakes(ShakeCount, bSuccess);
}

void ANUBattleAvatar::PlayCaptureShakes(int32 ShakeCount, bool bSuccess)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	bCaptureShakeSucceeded = bSuccess;

	UNUSFXSubsystem::Play(this, NU::SFX::Battle::CaptureThrow);

	// 흔들 대상이 없으면 결과만 띄운다
	if (!IsValid(Sprite))
	{
		FinishCaptureShakes();
		return;
	}

	// 0회면 흔들림 없이 곧바로 열림
	// 그 경우도 결과 유지 시간을 똑같이 갖게 함
	CaptureShakeTotal = FMath::Max(0, ShakeCount);
	CaptureShakeElapsed = 0.f;
	bCaptureResultShown = false;
	CaptureShakeAxis = ResolveScreenRightLocal();
	bCaptureShakeActive = true;
}

float ANUBattleAvatar::GetCaptureShakeRemainingSeconds() const
{
	if (!bCaptureShakeActive)
	{
		return 0.f;
	}

	// 결과가 화면에 남는 시간까지 연출로 봄, 이 값으로 다음 행동과 무대 철거가 미뤄짐
	const float Total = CaptureShakeTotal * (CaptureShakeDuration + CaptureShakeInterval)
		+ CaptureResultHoldSeconds;

	return FMath::Max(0.f, Total - CaptureShakeElapsed);
}

FVector ANUBattleAvatar::ResolveScreenRightLocal() const
{
	// 진형이 X축으로 늘어서 있어서 월드 Y를 좌우로 쓰면 안 됨
	// 카메라의 오른쪽을 쓰면 진형 축과 무관하게 화면 좌우가 됨
	FVector ScreenRight = FVector::YAxisVector;

	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector CameraRight =
			FRotationMatrix(Camera->GetCameraRotation()).GetScaledAxis(EAxis::Y);

		const FVector Flat = FVector(CameraRight.X, CameraRight.Y, 0.f);
		if (!Flat.IsNearlyZero())
		{
			ScreenRight = Flat.GetSafeNormal();
		}
	}

	// 스프라이트의 상대 위치는 루트 기준이라 월드 방향을 액터 로컬로 바꿔서 넘김
	return GetActorTransform().InverseTransformVectorNoScale(ScreenRight);
}

void ANUBattleAvatar::TickCaptureShake(float DeltaSeconds)
{
	if (!bCaptureShakeActive || !IsValid(Sprite))
	{
		return;
	}

	CaptureShakeElapsed += DeltaSeconds;

	const float Span = CaptureShakeDuration + CaptureShakeInterval;
	const float ShakeTime = CaptureShakeTotal * Span;

	if (CaptureShakeElapsed >= ShakeTime)
	{
		Sprite->SetRelativeLocation(SpriteOriginRelativeLocation);

		// 흔들림이 끝나면 결과를 한 번 띄우고, 그것이 화면에 남는 동안 연출을 유지
		// 여기서 바로 끝내면 성공한 경우 팝업이 뜬 프레임에 무대 철거가 시작됨
		if (!bCaptureResultShown)
		{
			bCaptureResultShown = true;
			FinishCaptureShakes();
		}

		if (CaptureShakeElapsed >= ShakeTime + CaptureResultHoldSeconds)
		{
			bCaptureShakeActive = false;
		}

		return;
	}

	// 흔들림 한 번이 시작되는 틱마다 한 번, 통과한 횟수만큼 반복됨
	if (FMath::CeilToInt(CaptureShakeElapsed / Span) != FMath::CeilToInt((CaptureShakeElapsed - DeltaSeconds) / Span))
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Battle::CaptureShake);
	}

	const float InSpan = FMath::Fmod(CaptureShakeElapsed, Span);
	if (InSpan > CaptureShakeDuration)
	{
		// 흔들림 사이의 정지 구간, 제자리에 둬야 몇 번 흔들렸는지 알 수 있음
		Sprite->SetRelativeLocation(SpriteOriginRelativeLocation);
		return;
	}

	// 좌우 왕복, 한 번의 흔들림 안에서 진폭이 줄어들어 잦아드는 것처럼 보임
	const float Alpha = InSpan / CaptureShakeDuration;
	const float Offset = FMath::Sin(Alpha * 2.f * PI * CaptureShakeCycles)
		* CaptureShakeDistance
		* (1.f - Alpha);

	Sprite->SetRelativeLocation(SpriteOriginRelativeLocation + CaptureShakeAxis * Offset);
}

bool ANUBattleAvatar::BeginDeathDissolve()
{
	if (bDeathDissolveActive || !IsValid(SpriteVisual) || DeathDissolveSeconds <= 0.f)
	{
		return false;
	}

	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	UMaterialInterface* DissolveMaterial = Settings
		? Settings->SpriteDissolveMaterial.LoadSynchronous()
		: nullptr;

	if (!DissolveMaterial)
	{
		// 머티리얼이 없으면 침식할 방법이 없음, 부른 쪽이 예전처럼 곧바로 숨기게 false 를 돌려줌
		UE_LOG(LogTemp, Log,
			TEXT("사망 디졸브 생략 - SpriteDissolveMaterial 이 비어 있음 (NUBattleDataSettings)"));

		return false;
	}

	// 스프라이트가 쓰는 머티리얼은 라이팅 분기 때문에 통일하지 않았으므로 여기서만 덮어씀
	// 적은 이 뒤에 무대와 함께 사라지므로 되돌릴 필요가 없음
	SpriteVisual->OverrideMaterial(DissolveMaterial);
	SpriteVisual->SetDissolveAmount(0.f);

	bDeathDissolveActive = true;
	DeathDissolveElapsed = 0.f;

	// 가루 파티클은 침식이 시작될 때 같이 터져야 부스러지는 자리에서 나오는 것으로 보임
	PlayDeathCue();

	return true;
}

void ANUBattleAvatar::TickDeathDissolve(float DeltaSeconds)
{
	if (!bDeathDissolveActive)
	{
		return;
	}

	DeathDissolveElapsed += DeltaSeconds;

	const float Amount = FMath::Clamp(DeathDissolveElapsed / DeathDissolveSeconds, 0.f, 1.f);
	if (IsValid(SpriteVisual))
	{
		SpriteVisual->SetDissolveAmount(Amount);
	}

	if (Amount < 1.f)
	{
		return;
	}

	// 침식이 끝나면 미뤄둔 사망 표시를 처리함
	bDeathDissolveActive = false;

	CommitDeadVisual(true);
}

void ANUBattleAvatar::PlayDeathCue()
{
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!IsValid(CueManager))
	{
		return;
	}

	// 넷 아바타는 ASC 가 없어서 GE 로 큐를 띄울 수 없음
	// HandleGameplayCue 는 대상 액터만 받으므로 로컬·넷 양쪽에서 같은 경로로 처리됨
	FGameplayCueParameters Parameters;
	Parameters.Instigator = this;
	Parameters.EffectCauser = this;
	Parameters.Location = GetActorLocation();

	CueManager->HandleGameplayCue(
		this, 
		GameplayCue::Battle::Death, 
		EGameplayCueEvent::Executed, 
		Parameters
		);
}

void ANUBattleAvatar::FinishCaptureShakes()
{
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	const TSoftObjectPtr<UNiagaraSystem>& SoftSystem = bCaptureShakeSucceeded
		? Settings->CaptureSuccessVFX
		: Settings->CaptureFailVFX;

	if (UNiagaraSystem* System = SoftSystem.LoadSynchronous())
	{
		// 성공이면 곧바로 숨겨지는 액터라 붙이지 않고 위치에만 스폰함
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			System,
			GetActorLocation() + FVector(0.f, 0.f, 60.f)
			);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("포획 %s 이펙트가 지정되지 않음, NUBattleDataSettings 확인 필요"),
			bCaptureShakeSucceeded ? TEXT("성공") : TEXT("실패"));
	}

	// 결과 글자는 흔들림이 끝난 뒤에 띄움, 먼저 뜨면 흔들림을 볼 이유가 없어짐
	// 이 함수는 이미 각 클라에서 돌고 있으므로 멀티캐스트를 다시 타지 않음
	if (ANUPlayerController* PC = Cast<ANUPlayerController>(
		UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->ShowCombatPopup(this, FNUCombatPopupData(bCaptureShakeSucceeded
			? ENUCombatPopupType::CaptureSuccess
			: ENUCombatPopupType::CaptureFail));
	}

	if (bDeadVisualPendingShake)
	{
		bDeadVisualPendingShake = false;
		CommitDeadVisual(true);
	}
}

void ANUBattleAvatar::MulticastShowCombatPopup_Implementation(const FNUCombatPopupData& PopupData)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 로컬, 리슨서버에선 호스트 기준으로 처리
	if (ANUPlayerController* PC = Cast<ANUPlayerController>(
		UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->ShowCombatPopup(this, PopupData);
	}
}

void ANUBattleAvatar::MulticastPlayAction_Implementation(const FNUBattleActionMotionSpec& Spec)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ActiveSpec = Spec;
	MotionElapsed = 0.f;

	// 자리를 지키는 나이아(보스 등)는 앞으로 나가는 행동도 제자리에서 함
	// 접근 · 복귀 단계는 그대로 둬서 피해 · 반응 창 시각이 바뀌지 않음, 로컬 · 넷 모두 여기를 지남
	if (ActiveSpec.Motion != ENUBattleActionMotion::InPlace && ShouldAttackInPlace())
	{
		ActiveSpec.ActionLocation = HomeTransform.GetLocation();
	}

	ResolveActionVFX(ActiveSpec);

	// 자기 임팩트가 없는 공격(기본 공격 등)은 대체 임팩트, 직업 기본 공격으로 바꾼 뒤라 직업 임팩트는 그대로
	NUBattlePresentation::ApplyFallbackImpact(ActiveSpec);

	// 예고가 있으면 제자리에서 기다렸다가 접근을 시작함
	if (Spec.TelegraphTime > KINDA_SMALL_NUMBER)
	{
		MotionPhase = ENUAvatarMotionPhase::Telegraphing;
	}
	else
	{
		BeginActionMotion();
	}
}

void ANUBattleAvatar::ResetActionMotion()
{
	SetActorTransform(HomeTransform);
	MotionPhase = ENUAvatarMotionPhase::Idle;
	MotionElapsed = 0.f;
}

void ANUBattleAvatar::BeginActionMotion()
{
	MotionElapsed = 0.f;
	MotionPhase = ActiveSpec.Motion == ENUBattleActionMotion::InPlace
		? ENUAvatarMotionPhase::Acting
		: ENUAvatarMotionPhase::Approaching;

	PlayedHitAnimations = 0;

	// 반복 타격의 두 번째 모션부터 틀 위치, 모션을 안 트는 아바타(나이아)는 연출 간격이 예전 그대로이게 0
	RepeatMotionStart = 0.f;
	if (ActiveSpec.HitInterval > KINDA_SMALL_NUMBER
		&& CanPlayActionAnimation(ResolveActionAnimation(ActiveSpec)))
	{
		const float HitOffset = ActiveSpec.ActionTime * FMath::Clamp(ActiveSpec.HitTimeNormalized, 0.f, 1.f);
		RepeatMotionStart = FMath::Max(HitOffset - ActiveSpec.HitInterval, 0.f);
	}

	// 제자리면 접근을 건너뛰고 바로 Acting으로
	if (MotionPhase == ENUAvatarMotionPhase::Acting)
	{
		PlayActionAnimation();
		PlayedHitAnimations = 1;	// 위에서 재생처리 하므로 첫 타격을 처리해야 함
		PlayedCastVFX = 0;
		TickCastVFX();	// 시전 지점이 0 이면 지금 바로 나감
	}
}

void ANUBattleAvatar::InitializeBattleAvatar(AActor* InAbilitySystemOwner, int32 InCombatantId)
{
	if (!HasAuthority() || !IsValid(InAbilitySystemOwner) || InCombatantId == INDEX_NONE)
	{
		return;
	}
	
	AbilitySystemOwner = InAbilitySystemOwner;
	CombatantId = InCombatantId;
	bIsPlayerVisual = Cast<ANUPlayerState>(AbilitySystemOwner) != nullptr;

	BindDeathState();
	BindVitalAttributes();

	// 이 시점에서 AbilitySystemOwner가 정해짐, BeginPlay에선 null
	// 서버 호스트는 여기서 처리
	ApplyCombatantVisual();
}

void ANUBattleAvatar::InitializeNetAvatar(int32 InCombatantId, int32 InNaiaId, bool bInIsPlayerVisual,
	EFieldFacing InFacing)
{
	if (!HasAuthority() || InCombatantId == INDEX_NONE)
	{
		return;
	}

	CombatantId = InCombatantId;
	NaiaId = InNaiaId;
	bIsPlayerVisual = bInIsPlayerVisual;
	BattleFacing = InFacing;

	// 서버에서 처리한 결과를 반영하기만 하면 되므로 ASC바인딩이 필요없음
	
	ApplyCombatantVisual();	// 내부에서 ApplyBattleFacing까지 호출함

	ForceNetUpdate();
}

void ANUBattleAvatar::SetVitals(int32 Health, int32 MaxHealth, int32 SP, int32 MaxSP)
{
	if (UNU_CombatantStatusWidget* Widget = GetVitalWidget())
	{
		Widget->SetVitalsDirect(Health, MaxHealth, SP, MaxSP);
	}
}

void ANUBattleAvatar::SetCombatantIdentity(const FText& Name, int32 Level)
{
	if (UNU_CombatantStatusWidget* Widget = GetVitalWidget())
	{
		Widget->SetIdentityDirect(Name, Level, !bIsAlly);
	}
}

void ANUBattleAvatar::SetStatusViews(const TArray<FNUStatusEffectView>& InViews)
{
	if (UNU_CombatantStatusWidget* Widget = GetVitalWidget())
	{
		Widget->SetStatusViewsDirect(InViews);
	}
}

void ANUBattleAvatar::SetJobResourceViews(const TArray<FNUJobResourceView>& InGauges,
	const TArray<FNUJobResourceView>& InBadges)
{
	if (UNU_CombatantStatusWidget* Widget = GetVitalWidget())
	{
		Widget->SetJobResourceViewsDirect(InGauges, InBadges);
	}
}

void ANUBattleAvatar::SetDeadVisual(bool bDead)
{
	if (!HasAuthority())
	{
		return;
	}

	// 넷 전투의 사망 처리
	ApplyDeadVisual(bDead);
}

void ANUBattleAvatar::ApplyDeadVisual(bool bDead)
{
	// 포획 성공은 흔들림 연출 도중에 사망 처리가 들어옴
	// 여기서 바로 숨기면 흔들림이 통째로 안 보이므로 연출이 끝날 때 반영함
	if (bDead && bCaptureShakeActive)
	{
		bDeadVisualPendingShake = true;
		return;
	}

	// 살아 있는 상태로 돌아오면 미뤄둔 것도 버림 (교체/부활)
	bDeadVisualPendingShake = false;

	if (bDeadVisualApplied && bLastDeadVisual == bDead)
	{
		return;
	}

	// 첫 적용은 스폰 · 교체 때 이미 쓰러진 상태를 맞춰 주는 것이라 소리를 내지 않음
	const bool bFirstApply = !bDeadVisualApplied;
	bDeadVisualApplied = true;
	bLastDeadVisual = bDead;

	if (!bFirstApply && bDead)
	{
		UNUSFXSubsystem::Play(this, bIsAlly ? NU::SFX::Battle::DeathAlly : NU::SFX::Battle::DeathEnemy);
	}

	if (!bDead)
	{
		// 부활·교체로 되살아났으면 침식을 되돌려야 함, 안 하면 반쯤 사라진 채로 살아남
		bDeathDissolveActive = false;
		DeathDissolveElapsed = 0.f;

		if (IsValid(SpriteVisual))
		{
			SpriteVisual->SetDissolveAmount(0.f);
			SpriteVisual->OverrideMaterial(nullptr);
		}
	}
	else if (!bIsAlly && BeginDeathDissolve())
	{
		// 적, 포획이 아닌 처치에만 디졸브 처리함
		return;
	}

	CommitDeadVisual(bDead);
}

void ANUBattleAvatar::CommitDeadVisual(bool bDead)
{
	// 아군은 쓰러져도 아바타를 남김
	// 대상 수집·명령 제출에서 빠지는 것은 체력 0 으로 이미 갈리므로 표시만 바꾸면 됨
	// 적은 그대로 사라짐
	bDownedVisualActive = bDead && bIsAlly;
	SetActorHiddenInGame(bDead && !bIsAlly);

	RefreshDownedVisual();
	ForceNetUpdate();
}

void ANUBattleAvatar::RefreshDownedVisual()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!IsValid(SpriteVisual))
	{
		return;
	}

	// 캐릭터는 쓰러지는 모션으로 넘어감
	// 색은 그 위에 그대로 덮는다
	SpriteVisual->SetAnimDeadState(bDownedVisualActive);
	
	if (bDownedVisualActive)
	{
		SpriteVisual->SetColorOverride(DownedColor);
	}
	else
	{
		SpriteVisual->ClearColorOverride();
	}
}

void ANUBattleAvatar::SetAbilitySystemOwner(AActor* InAbilitySystemOwner)
{
	if (!HasAuthority() || !IsValid(InAbilitySystemOwner))
	{
		return;
	}
	if (AbilitySystemOwner == InAbilitySystemOwner)
	{
		return;
	}

	// 델리게이트 해제
	UnbindCombatantDelegates();

	// ASC Owner 교체
	AbilitySystemOwner = InAbilitySystemOwner;
	bIsPlayerVisual = Cast<ANUPlayerState>(AbilitySystemOwner) != nullptr;
	
	// 나이아 칸이 플레이어로 바뀌기도 해서 방향도 새 소유자에 맞춰 다시 고름
	// 플레이어는 등을 보이고 나이아는 옆을 보므로 스폰 때 넣은 값으로는 틀어짐
	// 아래 ApplyCombatantVisual 이 이 값으로 다시 그림
	BattleFacing = ResolveBattleFacing(bIsAlly, bIsPlayerVisual);

	// 재바인딩
	// 서버에선 OnRep이 뜨지 않으므로 직접 바인딩함, InitializeBattleAvatar와 같이 처리
	BindDeathState();
	BindVitalAttributes();

	// 소유자가 바뀌면 외형이 소유자를 따라가야 함, 나이아 교체 시 SetNaiaId가 뒤이어 오지만
	// 플레이어로 바뀌는 경우엔 그게 없으므로 여기서도 적용함
	ApplyCombatantVisual();

	ForceNetUpdate();
}

void ANUBattleAvatar::OnRep_AbilitySystemOwner()
{
	// 교체로 소유자가 바뀐 경우 이전 ASC의 델리게이트를 먼저 해제해야 함
	// 첫 도착이면 상관 X
	UnbindCombatantDelegates();

	bIsPlayerVisual = Cast<ANUPlayerState>(AbilitySystemOwner) != nullptr;

	BindDeathState();
	BindVitalAttributes();	// HP/SP 바 바인딩

	// BeginPlay 시점에 오지 않은 플레이어는 여기서 처리, 플레이어 변경 시점을 여기서 알 수 있음
	// 나이아는 NaiaId 가 따로 오므로 OnRep_NaiaId 에서 처리됨
	ApplyCombatantVisual();
}

// 아바타를 재사용하므로 이전 나이아의 연출 상태가 남으면 새 나이아에 이어져 보임
// 점등이 남으면 계속 빨갛고, 밀려남이 남으면 그 자리에 굳고, 모션이 남으면 이어서 재생됨
void ANUBattleAvatar::ResetPresentationState()
{
	MotionPhase = ENUAvatarMotionPhase::Idle;
	MotionElapsed = 0.f;
	PlayedHitAnimations = 0;
	PlayedCastVFX = 0;
	RepeatMotionStart = 0.f;
	ActiveSpec = FNUBattleActionMotionSpec();

	bHitReactionActive = false;
	HitReactionElapsed = 0.f;
	HitReactionDirection = FVector::ZeroVector;

	bHitFlashActive = false;
	HitFlashElapsed = 0.f;

	bCaptureShakeActive = false;
	CaptureShakeElapsed = 0.f;
	CaptureShakeTotal = 0;
	bCaptureResultShown = false;
	bDeadVisualPendingShake = false;

	// 아바타를 재사용하면 새 전투원의 사망 표시를 처음부터 다시 넣어야 함
	bDeadVisualApplied = false;

	// 쓰러진 나이아를 교체하면 이 아바타에 새 나이아가 들어옴, 표시를 안지우면 다른 나이아가 쓰러진 색으로 나옴
	bDownedVisualActive = false;

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 서버엔 스프라이트 연출이 없으므로 여기부터는 클라만
	if (IsValid(Sprite))
	{
		Sprite->SetRelativeLocation(SpriteOriginRelativeLocation);
	}
	RefreshDownedVisual();

	SetActorTransform(HomeTransform);
	SetPendingSPSpend(0.f);
}

