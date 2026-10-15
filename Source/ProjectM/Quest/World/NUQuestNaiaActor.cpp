#include "Quest/World/NUQuestNaiaActor.h"

#include "CharacterState/NUPlayerState.h"
#include "Field/NUSpriteVisualComponent.h"
#include "Field/NUSpriteVisualSettings.h"
#include "Field/Presence/NUActorPresenceRow.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestSceneComponent.h"
#include "Quest/Data/NUQuestTypes.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"

ANUQuestNaiaActor::ANUQuestNaiaActor()
{
	// 맴돌기 단계에서만 켠다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 물건용 메시 대신 나이아 스프라이트를 보여준다.
	Mesh->SetVisibility(false);
	Mesh->SetHiddenInGame(true);

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(GetRootComponent());
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// NPC와 같은 캐릭터 머티리얼이라 뒤 가림을 CustomDepth로 판정한다.
	Sprite->SetRenderCustomDepth(true);

	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	Animation->InitRenderComponent(Sprite);

	// 동행 나이아의 0.45배 대신 필드 적 나이아처럼 원본 크기로 그린다. NPC 옆에서도 알아볼 수 있고 1텍셀이 1유닛이라 깨짐이 없다.
	FNUSpriteVisualSettings Settings = NUSpriteVisuals::MakeFieldSpirit();
	Settings.RelativeScale = FVector(1.f);
	// 1텍셀이 1유닛이라 NPC와 같은 캐릭터 머티리얼(또렷함·노출 보정·가림)을 쓴다. 0.45배 동행 나이아는 축소 계단이 깨져 보여 쓰지 않는다.
	Settings.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/M_NUCharacterSprite.M_NUCharacterSprite")));

	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, Animation, Settings);
}

void ANUQuestNaiaActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 정의나 애님 BP가 없으면 프리셋 그림이 나오므로 알린다.
	if (NaiaId <= 0 || !SpriteVisual->ApplyNaiaAnimBlueprint(NaiaId))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestNaiaActor: 나이아 그림을 적용하지 못했습니다 [%s] NaiaId=%d"), *GetName(), NaiaId);
	}

	SpriteVisual->EnableAutomaticBillboardUpdates(BillboardUpdateInterval);
}

void ANUQuestNaiaActor::HandleActorPresence(const FNUActorPresenceRow* Step)
{
	Super::HandleActorPresence(Step);

	// 등장 컴포넌트는 자리로 옮긴 뒤에 알리므로 지금 위치가 돌아올 자리다.
	const bool bHover = Step != nullptr && Step->bHoverAroundPlayer && GetNetMode() != NM_DedicatedServer;
	if (bHover && !IsActorTickEnabled())
	{
		// 여러 나이아가 한 플레이어 둘레에 겹치지 않게 나이아마다 다른 각도에서 시작한다.
		HoverAngle = FMath::DegreesToRadians(static_cast<float>((NaiaId * 67) % 360));
	}
	if (bHover)
	{
		HoverHome = GetActorLocation();
	}
	SetActorTickEnabled(bHover);
}

void ANUQuestNaiaActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const ACharacter* Player = Controller != nullptr ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
	if (Player == nullptr || ShouldPauseHover(*Controller))
	{
		return;
	}

	// 가까이 온 플레이어의 발치 둘레를 천천히 돌고, 멀어지면 자리로 돌아간다.
	FVector Target = HoverHome;
	if (FVector::Dist2D(Player->GetActorLocation(), HoverHome) <= HoverLeashRadius)
	{
		HoverAngle = FMath::Fmod(HoverAngle + DeltaSeconds * 2.f * PI / HoverOrbitSeconds, 2.f * PI);
		const FVector Feet = Player->GetActorLocation() - FVector(0.f, 0.f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Target = Feet + FVector(FMath::Cos(HoverAngle), FMath::Sin(HoverAngle), 0.f) * HoverRadius;
	}
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), Target, DeltaSeconds, HoverInterpSpeed));
}

bool ANUQuestNaiaActor::ShouldPauseHover(const APlayerController& Controller) const
{
	const ANUPlayerController* NUController = Cast<ANUPlayerController>(&Controller);
	if (NUController != nullptr && (NUController->IsGameInputBlocked() || NUController->IsDialogueOpen()))
	{
		return true;
	}

	const ANUPlayerState* PlayerState = Controller.GetPlayerState<ANUPlayerState>();
	if (PlayerState != nullptr && PlayerState->GetBattleEncounter() != nullptr)
	{
		return true;
	}

	// 진정 직후의 연출(물병 내려놓기 등)은 제자리에서 보여준다.
	const UNUQuestSceneComponent* Scene = Controller.FindComponentByClass<UNUQuestSceneComponent>();
	return Scene != nullptr && (Scene->IsScenePlaying() || Scene->HasPendingCameraScene());
}
