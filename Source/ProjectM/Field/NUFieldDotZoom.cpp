#include "NUFieldDotZoom.h"

#include "Battle/Avatar/NUBattleAvatar.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Field/NUCameraOcclusionFadeComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbookComponent.h"
#include "UObject/UObjectIterator.h"

static TAutoConsoleVariable<float> CVarNUDotScale(
	TEXT("nu.Cam.DotScale"), 2.0f,
	TEXT("Field camera dot scale (screen pixels per sprite dot). The FOV is derived from the viewport height and the spring arm length every frame. 0 = off."));

static TAutoConsoleVariable<float> CVarNUDotLookHeight(
	TEXT("nu.Cam.DotLookHeight"), 0.0f,
	TEXT("Centimetres added to the field spring arm's target offset Z while the dot scale is on (the character sits lower in the frame)."));

static TAutoConsoleVariable<float> CVarNUBattleDotScale(
	TEXT("nu.Cam.BattleDotScale"), 2.0f,
	TEXT("Battle stage camera dot scale (screen pixels per sprite dot) at the mean avatar depth. 0 = off."));

static TAutoConsoleVariable<int32> CVarNUBattleSpriteSnap(
	TEXT("nu.Cam.BattleSpriteSnap"), 1,
	TEXT("1 = scale each battle avatar by its depth over the mean depth so every avatar renders at exactly the battle dot scale."));

static TAutoConsoleVariable<int32> CVarNUBattleFit(
	TEXT("nu.Cam.BattleFit"), 1,
	TEXT("1 = the battle camera widens past the dot scale when the line-up would not fit: every avatar (feet and the status tag above it) stays inside the screen margins. A full party or four foes otherwise fall off the screen edge."));

static TAutoConsoleVariable<float> CVarNUBattleFitMargin(
	TEXT("nu.Cam.BattleFitMargin"), 100.0f,
	TEXT("Side margin in 1080p screen pixels kept free left and right of the outermost avatars (room for their status tags)."));

static TAutoConsoleVariable<float> CVarNUBattleFitTop(
	TEXT("nu.Cam.BattleFitTop"), 150.0f,
	TEXT("Height above an avatar's feet (cm) that must stay in frame: the status tag above its head."));

static TAutoConsoleVariable<int32> CVarNUSpriteStencil(
	TEXT("nu.Sprite.Stencil"), 2,
	TEXT("Custom depth stencil value written by every Paper flipbook component (character sprites), so the post process can treat characters apart from the scenery. 0 = off."));

static TAutoConsoleVariable<int32> CVarNUOcclusionFade(
	TEXT("nu.Cam.OcclusionFade"), 1,
	TEXT("Camera occlusion fade values in MPC_NUCameraFade. 1 = written here only when the pawn's own fade component has no collection assigned, 2 = always written here, 0 = never."));

static TAutoConsoleVariable<float> CVarNUOcclusionFadeRadius(
	TEXT("nu.Cam.OcclusionFadeRadius"), 165.0f,
	TEXT("Cone radius (cm at the character) of the occlusion fade when this subsystem writes it."));

// the FOV property is the horizontal FOV at the 16:9 reference aspect; Unreal keeps the vertical FOV fixed
static constexpr float NUReferenceAspect = 1.777778f;

static float NUFovForScale(float ViewportY, float Distance, float Scale)
{
	return FMath::RadiansToDegrees(2.0f * FMath::Atan(ViewportY / (2.0f * Distance * Scale) * NUReferenceAspect));
}

bool UNUFieldDotZoom::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UNUFieldDotZoom::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNUFieldDotZoom, STATGROUP_Tickables);
}

void UNUFieldDotZoom::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// character sprites write custom depth stencil N: checked twice a second so sprites spawned later (NPCs, battle avatars) are caught
	SpriteStencilTimer += DeltaTime;
	if (SpriteStencilTimer >= 0.5f)
	{
		SpriteStencilTimer = 0.0f;
		const int32 Stencil = CVarNUSpriteStencil.GetValueOnGameThread();
		if (Stencil > 0)
		{
			for (TObjectIterator<UPaperFlipbookComponent> It; It; ++It)
			{
				if (It->GetWorld() == World && !It->bRenderCustomDepth)
				{
					It->SetRenderCustomDepth(true);
					It->SetCustomDepthStencilValue(Stencil);
				}
			}
		}
	}
	if (World->GetGameViewport() == nullptr)
	{
		return;
	}
	FVector2D ViewportSize = FVector2D::ZeroVector;
	World->GetGameViewport()->GetViewportSize(ViewportSize);
	if (ViewportSize.Y <= 0.0f)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	APawn* Pawn = PC != nullptr ? PC->GetPawn() : nullptr;
	AActor* ViewTarget = PC != nullptr ? PC->GetViewTarget() : nullptr;
	TickOcclusionFade(Pawn, ViewTarget != nullptr && ViewTarget == Pawn, DeltaTime);
	if (ViewTarget != nullptr && ViewTarget == Pawn)
	{
		TickField(Pawn, ViewportSize);
		LastBattleCamera.Reset();   // the next battle takes its field of view at once
	}
	else if (ViewTarget != nullptr)
	{
		TickBattle(ViewTarget, ViewportSize, DeltaTime);
	}
}

void UNUFieldDotZoom::TickField(APawn* Pawn, const FVector2D& ViewportSize)
{
	const float Scale = CVarNUDotScale.GetValueOnGameThread();
	if (Scale <= 0.0f)
	{
		return;
	}
	USpringArmComponent* Arm = Pawn->FindComponentByClass<USpringArmComponent>();
	UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
	if (Arm == nullptr || Camera == nullptr)
	{
		return;
	}
	const float LookHeight = CVarNUDotLookHeight.GetValueOnGameThread();
	// the sprite's depth from the camera, not the arm length: the sprite stands a little behind the look-at point, and
	// 30 cm of depth is 1.5 % of scale (126 px for 64 dots instead of 128). Lag free: the arm length plus the offset of
	// the sprite's centre from the arm origin along the view direction (the camera lags in position, not in rotation).
	float Distance = Arm->TargetArmLength;
	if (const UPaperFlipbookComponent* PawnSprite = Pawn->FindComponentByClass<UPaperFlipbookComponent>())
	{
		const FVector Origin = Arm->GetComponentLocation() + Arm->TargetOffset;
		Distance += FVector::DotProduct(PawnSprite->Bounds.Origin - Origin, Camera->GetForwardVector());
	}
	const bool bUnchanged = LastPawn.Get() == Pawn
		&& FMath::IsNearlyEqual(LastViewportY, static_cast<float>(ViewportSize.Y))
		&& FMath::IsNearlyEqual(LastArm, Distance, 0.5f)
		&& FMath::IsNearlyEqual(LastScale, Scale)
		&& FMath::IsNearlyEqual(LastLookHeight, LookHeight);
	if (bUnchanged)
	{
		return;
	}
	if (LastPawn.Get() != Pawn)
	{
		BaseLookHeight = Arm->TargetOffset.Z;
	}
	const float Fov = NUFovForScale(static_cast<float>(ViewportSize.Y), Distance, Scale);
	Camera->SetFieldOfView(Fov);
	Arm->TargetOffset.Z = BaseLookHeight + LookHeight;
	LastPawn = Pawn;
	LastViewportY = static_cast<float>(ViewportSize.Y);
	LastArm = Distance;
	LastScale = Scale;
	LastLookHeight = LookHeight;
	UE_LOG(LogTemp, Log, TEXT("[nu.Cam.DotScale] viewport %.0fx%.0f sprite depth %.0f scale %.1f -> fov %.2f, look height %.0f"),
		ViewportSize.X, ViewportSize.Y, Distance, Scale, Fov, Arm->TargetOffset.Z);
}

void UNUFieldDotZoom::TickBattle(AActor* ViewTarget, const FVector2D& ViewportSize, float DeltaTime)
{
	const float Scale = CVarNUBattleDotScale.GetValueOnGameThread();
	if (Scale <= 0.0f)
	{
		return;
	}
	UCameraComponent* Camera = ViewTarget->FindComponentByClass<UCameraComponent>();
	if (Camera == nullptr)
	{
		return;
	}
	const FVector CamLoc = Camera->GetComponentLocation();
	const FVector Fwd = Camera->GetForwardVector();

	// another camera (a new battle, or the set camera taking over from the fallback one): the depths are measured again
	const bool bNewCamera = LastBattleCamera.Get() != Camera;
	if (bNewCamera)
	{
		BattleHomeDepth.Reset();
		BattleHomeLocation.Reset();
	}

	// forget avatars that are gone
	for (auto It = BattleBaseScale.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			BattleHomeDepth.Remove(It.Key());
			BattleHomeLocation.Remove(It.Key());
			It.RemoveCurrent();
		}
	}

	// Every avatar is measured ONCE, where it is first seen: its formation slot. The field of view and the scales
	// follow those home depths, so they hold still while a combatant steps forward to attack and walks back
	// (with live depths the field of view changed on every frame of every step: the background zoomed under the sprites).
	TArray<ANUBattleAvatar*> Avatars;
	float Sum = 0.0f;
	for (TActorIterator<ANUBattleAvatar> It(GetWorld()); It; ++It)
	{
		ANUBattleAvatar* Avatar = *It;
		if (!BattleHomeDepth.Contains(Avatar))
		{
			const float Depth = FVector::DotProduct(Avatar->GetActorLocation() - CamLoc, Fwd);
			if (Depth <= 50.0f)
			{
				continue;
			}
			BattleHomeDepth.Add(Avatar, Depth);
			BattleHomeLocation.Add(Avatar, Avatar->GetActorLocation());
			if (!BattleBaseScale.Contains(Avatar))
			{
				BattleBaseScale.Add(Avatar, Avatar->GetActorScale3D());
			}
		}
		Avatars.Add(Avatar);
		Sum += BattleHomeDepth[Avatar];
	}
	if (Avatars.Num() == 0)
	{
		return;
	}
	const float MeanDepth = Sum / Avatars.Num();
	const float DotFov = NUFovForScale(static_cast<float>(ViewportSize.Y), MeanDepth, Scale);

	// The dot scale is as close as the camera gets, never closer than the line-up allows: every avatar's slot and the status
	// tag above it must stay inside the screen (side margin for the tags, top margin for the turn pill). A wide line-up
	// (a four-member party, four foes) widens the view just enough; small line-ups keep the dot scale exactly.
	float FitFov = 0.0f;
	if (CVarNUBattleFit.GetValueOnGameThread() != 0 && ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f)
	{
		const FVector Right = Camera->GetRightVector();
		const FVector Up = Camera->GetUpVector();
		const float Aspect = static_cast<float>(ViewportSize.X / ViewportSize.Y);
		const float PxScale = static_cast<float>(ViewportSize.Y) / 1080.0f;     // margins are given in 1080p pixels
		const float MarginX = FMath::Clamp(CVarNUBattleFitMargin.GetValueOnGameThread() * PxScale * 2.0f / static_cast<float>(ViewportSize.X), 0.0f, 0.5f);
		const float MarginTop = FMath::Clamp(110.0f * PxScale * 2.0f / static_cast<float>(ViewportSize.Y), 0.0f, 0.5f);
		const float MarginBottom = FMath::Clamp(40.0f * PxScale * 2.0f / static_cast<float>(ViewportSize.Y), 0.0f, 0.5f);
		const float TopCm = CVarNUBattleFitTop.GetValueOnGameThread();
		float NeedTanV = 0.0f;     // tangent of half the vertical field of view that fits everything
		for (ANUBattleAvatar* Avatar : Avatars)
		{
			const FVector Home = BattleHomeLocation.FindRef(Avatar);
			for (const FVector& Point : { Home, Home + FVector(0.0f, 0.0f, TopCm) })
			{
				const FVector ToPoint = Point - CamLoc;
				const float Z = FVector::DotProduct(ToPoint, Fwd);
				if (Z <= 50.0f)
				{
					continue;
				}
				const float X = FVector::DotProduct(ToPoint, Right) / Z;
				const float Y = FVector::DotProduct(ToPoint, Up) / Z;
				NeedTanV = FMath::Max(NeedTanV, FMath::Abs(X) / (Aspect * (1.0f - MarginX)));
				NeedTanV = FMath::Max(NeedTanV, Y > 0.0f ? Y / (1.0f - MarginTop) : -Y / (1.0f - MarginBottom));
			}
		}
		if (NeedTanV > 0.0f)
		{
			FitFov = FMath::RadiansToDegrees(2.0f * FMath::Atan(NeedTanV * NUReferenceAspect));
		}
	}
	const float TargetFov = FMath::Min(FMath::Max(DotFov, FitFov), 110.0f);

	// the target only moves when the line-up or the window changes: the first time it is taken at once, later it eases over
	const float Fov = bNewCamera ? TargetFov : FMath::FInterpTo(Camera->FieldOfView, TargetFov, DeltaTime, 6.0f);
	if (!FMath::IsNearlyEqual(Camera->FieldOfView, Fov, 0.005f))
	{
		Camera->SetFieldOfView(Fov);
	}
	LastBattleCamera = Camera;
	if (!FMath::IsNearlyEqual(LastBattleFov, TargetFov, 0.05f))
	{
		LastBattleFov = TargetFov;
		UE_LOG(LogTemp, Log, TEXT("[nu.Cam.BattleDotScale] viewport %.0fx%.0f avatars %d mean home depth %.0f scale %.1f -> dot fov %.2f, fit fov %.2f -> fov %.2f"),
			ViewportSize.X, ViewportSize.Y, Avatars.Num(), MeanDepth, Scale, DotFov, FitFov, TargetFov);
	}
	if (CVarNUBattleSpriteSnap.GetValueOnGameThread() == 0)
	{
		return;
	}
	for (ANUBattleAvatar* Avatar : Avatars)
	{
		const FVector Wanted = BattleBaseScale[Avatar] * (BattleHomeDepth[Avatar] / MeanDepth);
		if (!Avatar->GetActorScale3D().Equals(Wanted, 0.001))
		{
			Avatar->SetActorScale3D(Wanted);
		}
	}
}

void UNUFieldDotZoom::TickOcclusionFade(APawn* Pawn, bool bFieldView, float DeltaTime)
{
	const int32 Mode = CVarNUOcclusionFade.GetValueOnGameThread();
	if (Mode <= 0 || Pawn == nullptr)
	{
		// the collection keeps its last value: switching off writes strength 0 once, or the fade would stay on
		if (FadeCollection != nullptr && FadeStrength > 0.0f)
		{
			FadeStrength = 0.0f;
			UKismetMaterialLibrary::SetScalarParameterValue(this, FadeCollection, TEXT("FadeStrength"), 0.0f);
		}
		return;
	}
	if (Mode == 1)
	{
		// the team's component drives the collection when its blueprint assigned one
		if (const UNUCameraOcclusionFadeComponent* Team = Pawn->FindComponentByClass<UNUCameraOcclusionFadeComponent>())
		{
			const FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(Team->GetClass(), TEXT("FadeCollection"));
			if (Property != nullptr && !Property->GetPropertyValue_InContainer(Team).IsNull())
			{
				return;
			}
		}
	}
	if (FadeCollection == nullptr)
	{
		FadeCollection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Village/Materials/MPC_NUCameraFade.MPC_NUCameraFade"));
		if (FadeCollection == nullptr)
		{
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("[nu.Cam.OcclusionFade] the pawn's fade component has no collection: MPC_NUCameraFade is written by NUFieldDotZoom"));
	}
	const UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
	if (Camera == nullptr)
	{
		return;
	}
	const float Target = (bFieldView && !Pawn->IsHidden()) ? 1.0f : 0.0f;
	FadeStrength = FMath::FInterpTo(FadeStrength, Target, DeltaTime, 8.0f);
	const FVector Offset = Pawn->GetActorLocation() - Camera->GetComponentLocation();
	float FloorZ = static_cast<float>(Pawn->GetActorLocation().Z) + 2.0f;
	if (const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>())
	{
		FloorZ -= Capsule->GetScaledCapsuleHalfHeight();
	}
	UKismetMaterialLibrary::SetVectorParameterValue(this, FadeCollection, TEXT("CharacterOffset"),
		FLinearColor(static_cast<float>(Offset.X), static_cast<float>(Offset.Y), static_cast<float>(Offset.Z), FloorZ));
	UKismetMaterialLibrary::SetScalarParameterValue(this, FadeCollection, TEXT("FadeRadius"), CVarNUOcclusionFadeRadius.GetValueOnGameThread());
	UKismetMaterialLibrary::SetScalarParameterValue(this, FadeCollection, TEXT("FadeStrength"), FadeStrength);
}
