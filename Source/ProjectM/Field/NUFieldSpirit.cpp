// Fill out your copyright notice in the Description page of Project Settings.


#include "NUFieldSpirit.h"

#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "CharacterState/NUSpiritState.h"
#include "Field/NUSpiritFollowComponent.h"
#include "Field/NUSpriteVisualComponent.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"


ANUFieldSpirit::ANUFieldSpirit()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);

	bOnlyRelevantToOwner = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(RootComponent);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	Animation->InitRenderComponent(Sprite);

	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, Animation, NUSpriteVisuals::MakeFieldSpirit());

	FollowComponent = CreateDefaultSubobject<UNUSpiritFollowComponent>(TEXT("Follow"));
	FollowComponent->Setup(NUSpiritFollow::MakeDefaults());
}

void ANUFieldSpirit::BeginPlay()
{
	Super::BeginPlay();

	ApplyNaiaVisual();
}

void ANUFieldSpirit::ApplyNaiaVisual()
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(AbilitySystemOwner))
	{
		return;
	}

	// AbilitySystemOwner = SpiritState
	SpriteVisual->ApplyNaiaAnimBlueprint(AbilitySystemOwner->GetSpiritId());
}

void ANUFieldSpirit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버도 정령 자체는 따라가게 함
	FollowComponent->UpdateFollow(FollowTarget, DeltaTime);

	// 빌보드는 카메라가 있는 클라이언트만
	if (GetNetMode() != NM_DedicatedServer)
	{
		SpriteVisual->UpdateBillboard();
	}
}

void ANUFieldSpirit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, AbilitySystemOwner);
	DOREPLIFETIME(ThisClass, FollowTarget);
}

UAbilitySystemComponent* ANUFieldSpirit::GetAbilitySystemComponent() const
{
	return GetNUASC();
}

UNUAbilitySystemComponent* ANUFieldSpirit::GetNUASC() const
{
	return IsValid(AbilitySystemOwner) ? AbilitySystemOwner->GetNUASC() : nullptr;
}

void ANUFieldSpirit::InitializeFieldSpirit(ANUSpiritState* InSpiritState, AActor* InFollowTarget)
{
	if (!HasAuthority() || !IsValid(InSpiritState) || !IsValid(InFollowTarget)) return;

	AbilitySystemOwner = InSpiritState;
	FollowTarget = InFollowTarget;

	SetActorLocation(FollowComponent->GetDesiredLocation(InFollowTarget));

	// 서버 호스트는 OnRep이 뜨지 않으므로 여기서 직접 적용해야 함
	ApplyNaiaVisual();

	ForceNetUpdate();
}

void ANUFieldSpirit::SetFollowTarget(AActor* NewFollowTarget)
{
	if (!HasAuthority() || !IsValid(NewFollowTarget)) return;

	FollowTarget = NewFollowTarget;
}

void ANUFieldSpirit::OnRep_AbilitySystemOwner()
{
	// SpiritState가 정해지면 클라이언트에서 호출되는 함수
	// 정해진 SpiritId로 외형 변경
	ApplyNaiaVisual();
}

void ANUFieldSpirit::OnRep_FollowTarget()
{
	if (!IsValid(FollowTarget)) return;

	SetActorLocation(FollowComponent->GetDesiredLocation(FollowTarget));
	FollowComponent->ResetBob();
}
