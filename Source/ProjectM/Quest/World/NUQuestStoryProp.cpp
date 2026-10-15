#include "Quest/World/NUQuestStoryProp.h"

#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "UI/NUUIManager.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "PaperSpriteComponent.h"

ANUQuestStoryProp::ANUQuestStoryProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->InitSphereRadius(65.f);
	InteractionVolume->SetCollisionProfileName(TEXT("NU_Interactable"));
	Glow = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Glow"));
	Glow->SetupAttachment(InteractionVolume);
	Glow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glow->SetGenerateOverlapEvents(false);
	Glow->SetCastShadow(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(InteractionVolume);
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(220.f);
	Light->SetLightColor(FLinearColor(1.f, 0.72f, 0.32f));
}

void ANUQuestStoryProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InteractionVolume->SetCollisionEnabled(InspectionText.IsEmpty() ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	Light->SetVisibility(LightReaction != ENUQuestStoryLight::None);
	Light->SetIntensity(IdleIntensity);
	Glow->SetVisibility(LightReaction != ENUQuestStoryLight::None);
}

void ANUQuestStoryProp::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(LightReaction != ENUQuestStoryLight::None);
	CurrentIntensity = IdleIntensity;
}

const UNUQuestComponent* ANUQuestStoryProp::FindQuest(const APawn* Pawn) const
{
	const APlayerState* State = Pawn != nullptr ? Pawn->GetPlayerState() : nullptr;
	return State != nullptr ? State->FindComponentByClass<UNUQuestComponent>() : nullptr;
}

bool ANUQuestStoryProp::IsConditionMet(const FNUQuestStateCondition& Condition, const UNUQuestComponent& Quest) const
{
	const UNUQuestDataSubsystem* Data = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>() : nullptr;
	return !Condition.IsSet() || Condition.IsMet(
		[&Quest](FName Id) { return Quest.FindQuestProgress(Id); },
		[Data](FName Id) { return Data != nullptr ? Data->FindDefinition(Id) : nullptr; });
}

bool ANUQuestStoryProp::CanInteract_Implementation(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const UNUQuestComponent* Quest = FindQuest(Pawn);
	return !InspectionText.IsEmpty() && Pawn != nullptr && Pawn->IsLocallyControlled()
		&& Quest != nullptr && Quest->IsQuestProgressReady() && IsConditionMet(ReactionCondition, *Quest)
		&& FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(220.f)
		&& GetWorld()->GetTimeSeconds() >= NextInspectTime;
}

bool ANUQuestStoryProp::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor)) { return false; }
	if (UNUUIManager* UI = UNUUIManager::Get(Cast<APlayerController>(CastChecked<APawn>(Interactor)->GetController())))
	{
		UI->ShowNotice(InspectionText);
		NextInspectTime = GetWorld()->GetTimeSeconds() + 1.f;
		return true;
	}
	return false;
}

void ANUQuestStoryProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const UNUQuestComponent* Quest = FindQuest(Pawn);
	if (Pawn == nullptr || !Pawn->IsLocallyControlled() || Quest == nullptr || !Quest->IsQuestProgressReady()) { return; }

	Elapsed += DeltaSeconds;
	const bool bOpen = OpenCondition.IsSet() && IsConditionMet(OpenCondition, *Quest);
	const bool bNear = IsConditionMet(ReactionCondition, *Quest)
		&& FVector::DistSquared2D(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(ReactionRadius)
		&& FMath::Abs(Pawn->GetActorLocation().Z - GetActorLocation().Z) < 250.f;
	float Target = IdleIntensity;
	if (bOpen)
	{
		Target = OpenIntensity;
	}
	else if (LightReaction == ENUQuestStoryLight::Breathing)
	{
		Target = (bNear ? ReactionIntensity : IdleIntensity) * (0.92f + 0.08f * FMath::Sin(Elapsed * 0.8f));
	}
	else if (LightReaction == ENUQuestStoryLight::ApproachPulse)
	{
		if (bNear && !bApproachPlayed)
		{
			bApproachPlayed = true;
			PulseElapsed = 0.f;
		}
		if (PulseElapsed >= 0.f && PulseElapsed < PulseSeconds)
		{
			PulseElapsed += DeltaSeconds;
			Target = FMath::Lerp(IdleIntensity, ReactionIntensity,
				FMath::Sin(PI * FMath::Clamp(PulseElapsed / PulseSeconds, 0.f, 1.f)));
		}
	}
	CurrentIntensity = FMath::FInterpTo(CurrentIntensity, Target, DeltaSeconds, 3.f);
	Light->SetIntensity(CurrentIntensity);
	const float Alpha = FMath::Clamp(CurrentIntensity / FMath::Max3(ReactionIntensity, OpenIntensity, 0.01f), 0.f, 1.f);
	Glow->SetSpriteColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
}
