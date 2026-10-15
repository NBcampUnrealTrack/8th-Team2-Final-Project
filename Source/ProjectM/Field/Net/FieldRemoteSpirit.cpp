#include "Field/Net/FieldRemoteSpirit.h"

#include "Field/NUSpiritFollowComponent.h"
#include "Field/NUSpriteVisualComponent.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"

namespace
{
	// 전투중 톤다운
	const FLinearColor SPIRIT_COLOR_BATTLE(0.45f, 0.45f, 0.5f, 0.9f);
}

AFieldRemoteSpirit::AFieldRemoteSpirit()
{
	PrimaryActorTick.bCanEverTick = true;

	// gRPC가 전송 계층이다 — 이 액터는 각 클라가 로컬로 만든다
	bReplicates = false;
	SetReplicateMovement(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(RootComponent);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	Animation->InitRenderComponent(Sprite);

	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, Animation, NUSpriteVisuals::MakeFieldSpirit());

	FollowComponent = CreateDefaultSubobject<UNUSpiritFollowComponent>(TEXT("FollowComponent"));
	FollowComponent->Setup(NUSpiritFollow::MakeDefaults());
}

void AFieldRemoteSpirit::SetFollowTarget(AActor* InFollowTarget)
{
	FollowTarget = InFollowTarget;

	// 스폰 직후엔 즉시 위치를 변경한다
	if (IsValid(FollowTarget))
	{
		SetActorLocation(FollowComponent->GetDesiredLocation(FollowTarget));
		FollowComponent->ResetBob();
	}
}

void AFieldRemoteSpirit::SetActivity(EFieldActivity InActivity)
{
	if (InActivity == EFieldActivity::Battle)
	{
		SpriteVisual->SetColorOverride(SPIRIT_COLOR_BATTLE);
	}
	else
	{
		SpriteVisual->ClearColorOverride();
	}
}

void AFieldRemoteSpirit::SetSpiritId(int32 InSpiritId)
{
	SpiritId = InSpiritId;

	// 나이아 정의에 AnimationClass가 있으면 그 외형으로 교체함
	// 성공하면 안에서 RefreshAppearance까지 하므로 실패했을때만 직접 호출
	// 스프라이트를 교체하면 머티리얼·색이 초기화되므로 다시 입혀야함 (톤다운 유지)
	if (!SpriteVisual->ApplyNaiaAnimBlueprint(SpiritId))
	{
		SpriteVisual->RefreshAppearance();
	}
}

void AFieldRemoteSpirit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FollowComponent->UpdateFollow(FollowTarget, DeltaSeconds);
	SpriteVisual->UpdateBillboard();
}
