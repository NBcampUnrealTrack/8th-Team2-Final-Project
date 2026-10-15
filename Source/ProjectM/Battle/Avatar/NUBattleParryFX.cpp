#include "Battle/Avatar/NUBattleParryFX.h"

#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/NUBattleTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace
{
	int32 GNUParryFXEnabled = 1;
	float GNUParryHitStop = 0.09f;
	float GNUParryHitStopScale = 0.05f;
	float GNUParryFlashAlpha = 0.12f;
	float GNUParryFlashSeconds = 0.12f;
	float GNUParryBurstSize = 230.f;
	float GNUParryBurstSeconds = 0.38f;
	int32 GNUDodgeFXEnabled = 1;
	float GNUDodgeSize = 240.f;
	float GNUDodgeSeconds = 0.32f;

	FAutoConsoleVariableRef CVarNUParryFXEnabled(
		TEXT("nu.VFX.Parry.Enabled"), GNUParryFXEnabled,
		TEXT("Parry success presentation (burst, hit-stop, flash): 1 on, 0 off"));
	FAutoConsoleVariableRef CVarNUParryHitStop(
		TEXT("nu.VFX.Parry.HitStop"), GNUParryHitStop,
		TEXT("Real seconds the battle world freezes on a successful parry (default 0.09, 0 = none)"));
	FAutoConsoleVariableRef CVarNUParryHitStopScale(
		TEXT("nu.VFX.Parry.HitStopScale"), GNUParryHitStopScale,
		TEXT("World time scale during the parry hit-stop (default 0.05)"));
	FAutoConsoleVariableRef CVarNUParryFlashAlpha(
		TEXT("nu.VFX.Parry.FlashAlpha"), GNUParryFlashAlpha,
		TEXT("Peak opacity of the warm screen flash on a successful parry (0 - 1, default 0.12)"));
	FAutoConsoleVariableRef CVarNUParryFlashSeconds(
		TEXT("nu.VFX.Parry.FlashSeconds"), GNUParryFlashSeconds,
		TEXT("Seconds the parry flash takes to fall back to nothing (default 0.12)"));
	FAutoConsoleVariableRef CVarNUParryBurstSize(
		TEXT("nu.VFX.Parry.BurstSize"), GNUParryBurstSize,
		TEXT("Width in cm of the parry burst card: cross flare, ring and sparks (default 230)"));
	FAutoConsoleVariableRef CVarNUParryBurstSeconds(
		TEXT("nu.VFX.Parry.BurstSeconds"), GNUParryBurstSeconds,
		TEXT("Seconds the parry burst plays (default 0.38)"));
	FAutoConsoleVariableRef CVarNUDodgeFXEnabled(
		TEXT("nu.VFX.Dodge.Enabled"), GNUDodgeFXEnabled,
		TEXT("Dodge success presentation (wind streaks): 1 on, 0 off"));
	FAutoConsoleVariableRef CVarNUDodgeSize(
		TEXT("nu.VFX.Dodge.Size"), GNUDodgeSize,
		TEXT("Width in cm of the dodge streak card (default 240)"));
	FAutoConsoleVariableRef CVarNUDodgeSeconds(
		TEXT("nu.VFX.Dodge.Seconds"), GNUDodgeSeconds,
		TEXT("Seconds the dodge streaks play (default 0.32)"));

	FTimerHandle GNUParryHitStopHandle;
	float GNUParryPrevDilation = 1.f;
	double GNUParryFlashUntil = 0.0;

	// camera-facing cards with procedural materials that play once from their StartTime
	// (tools/ue_apply/build_parry_material.py in the NiaUnbounds repository builds both)
	const TCHAR* GNUParryBurstMaterial = TEXT("/Game/NUA/Materials/Runtime/M_NUA_ParryBurst.M_NUA_ParryBurst");
	const TCHAR* GNUDodgeStreakMaterial = TEXT("/Game/NUA/Materials/Runtime/M_NUA_DodgeStreak.M_NUA_DodgeStreak");
	const TCHAR* GNUCardMesh = TEXT("/Engine/BasicShapes/Plane.Plane");
	const FLinearColor GNUParryFlashColor(1.0f, 0.95f, 0.80f, 1.0f);

	// A one-shot card at an avatar. Offset is in screen space (right, up) cm from the avatar's chest; TowardCameraCm
	// moves it in front of the sprite (positive) or behind it (negative). The card's local X lies along the screen's
	// right, so a material can tell left from right.
	UMaterialInstanceDynamic* SpawnCard(const ANUBattleAvatar* Avatar, const APlayerCameraManager* Camera, const TCHAR* MaterialPath,
		const FVector2D& OffsetCm, const FVector2D& SizeCm, float Seconds, float TowardCameraCm)
	{
		UWorld* World = Avatar->GetWorld();
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, GNUCardMesh);
		if (Material == nullptr || Mesh == nullptr)
		{
			return nullptr;
		}
		const FRotator CameraRotation = Camera->GetCameraRotation();
		const FVector Right = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);
		const FVector Up = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Z);
		FVector Location = Avatar->GetActorLocation() + FVector(0.f, 0.f, 50.f) + Right * OffsetCm.X + Up * OffsetCm.Y;
		const FVector ToCamera = (Camera->GetCameraLocation() - Location).GetSafeNormal();
		Location += ToCamera * TowardCameraCm;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		const FRotator Facing = FRotationMatrix::MakeFromZX(ToCamera, Right).Rotator();
		AStaticMeshActor* Card = World->SpawnActor<AStaticMeshActor>(Location, Facing, Params);
		if (Card == nullptr)
		{
			return nullptr;
		}
		UStaticMeshComponent* Component = Card->GetStaticMeshComponent();
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->bAffectDynamicIndirectLighting = false;
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, Card);
		Instance->SetScalarParameterValue(TEXT("StartTime"), World->GetTimeSeconds());
		Instance->SetScalarParameterValue(TEXT("Duration"), Seconds);
		Component->SetMaterial(0, Instance);
		Card->SetActorScale3D(FVector(SizeCm.X / 100.f, SizeCm.Y / 100.f, 1.f));
		Card->SetLifeSpan(Seconds + 0.2f);
		return Instance;
	}

	APlayerCameraManager* LocalCamera(const UWorld* World)
	{
		APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
		return IsValid(Controller) ? Controller->PlayerCameraManager.Get() : nullptr;
	}

	bool CanPlayOn(const ANUBattleAvatar* Avatar)
	{
		if (!IsValid(Avatar))
		{
			return false;
		}
		const UWorld* World = Avatar->GetWorld();
		return IsValid(World) && World->GetNetMode() != NM_DedicatedServer;
	}
}

bool NUBattleParryFX::IsFlashing(const UWorld* World)
{
	return FPlatformTime::Seconds() < GNUParryFlashUntil;
}

void NUBattleParryFX::PlayForPopup(AActor* AnchorActor, ENUCombatPopupType Type)
{
	if (Type == ENUCombatPopupType::Parry || Type == ENUCombatPopupType::Dodge)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[nu.VFX.Reaction] popup %s on %s (%s)"), *UEnum::GetValueAsString(Type), *GetNameSafe(AnchorActor),
			Cast<ANUBattleAvatar>(AnchorActor) != nullptr ? TEXT("battle avatar") : TEXT("NOT a battle avatar: no presentation"));
	}
	if (Type == ENUCombatPopupType::Parry)
	{
		Play(Cast<ANUBattleAvatar>(AnchorActor));
	}
	else if (Type == ENUCombatPopupType::Dodge)
	{
		PlayDodge(Cast<ANUBattleAvatar>(AnchorActor));
	}
}

void NUBattleParryFX::Play(ANUBattleAvatar* Defender)
{
	if (GNUParryFXEnabled == 0 || !CanPlayOn(Defender))
	{
		return;
	}
	UWorld* World = Defender->GetWorld();
	APlayerCameraManager* Camera = LocalCamera(World);

	// the burst: on the attacker's side of the defender's chest (allies stand screen-left of the enemy row)
	if (IsValid(Camera) && GNUParryBurstSize > 1.f)
	{
		const float Side = Defender->IsAllyCombatant() ? 1.f : -1.f;
		UE_LOG(LogTemp, Verbose, TEXT("[nu.VFX.Parry] burst on %s at %s, side %.0f, world time %.3f"), *GetNameSafe(Defender),
			*Defender->GetActorLocation().ToCompactString(), Side, World->GetTimeSeconds());
		// 45 cm toward the attacker and 18 up: the clash point. A parry halves the damage, so the hit effect (flash, ring, sparkles)
		// plays on the body at the same moment; the burst sits beside it, with only a faint ring of its own (RingGain in the material)
		SpawnCard(Defender, Camera, GNUParryBurstMaterial, FVector2D(Side * 45.f, 18.f),
			FVector2D(GNUParryBurstSize, GNUParryBurstSize), FMath::Max(0.1f, GNUParryBurstSeconds), 35.f);
	}

	// hit-stop: the whole battle world slows for a few frames - sprites, effects and the timers that drive the action
	// (net action queue, ability WaitDelay) together. Slowing only the avatars (CustomTimeDilation) left the attacker
	// ~0.09 s behind those timers: it was still walking home when the next action read its position (impact off target),
	// and in net battles the parry's continuation piece froze it there and snapped it home
	float HitStopExtra = 0.f;
	if (GNUParryHitStop > 0.f)
	{
		const float Dilation = FMath::Clamp(GNUParryHitStopScale, 0.01f, 1.f);
		if (!World->GetTimerManager().IsTimerActive(GNUParryHitStopHandle))
		{
			GNUParryPrevDilation = UGameplayStatics::GetGlobalTimeDilation(World);
		}
		UGameplayStatics::SetGlobalTimeDilation(World, GNUParryPrevDilation * Dilation);

		// world timers run on the slowed clock: HitStop real seconds = HitStop * dilation world seconds
		TWeakObjectPtr<UWorld> WeakWorld(World);
		World->GetTimerManager().SetTimer(GNUParryHitStopHandle, [WeakWorld]()
		{
			if (WeakWorld.IsValid())
			{
				UGameplayStatics::SetGlobalTimeDilation(WeakWorld.Get(), GNUParryPrevDilation);
			}
		}, GNUParryHitStop * GNUParryPrevDilation * Dilation, false);
		HitStopExtra = GNUParryHitStop * (1.f - Dilation);
	}

	// a short warm flash over the whole frame (the fade runs on world time, so it also holds through the hit-stop)
	if (IsValid(Camera) && GNUParryFlashAlpha > 0.f)
	{
		const float Seconds = FMath::Max(0.03f, GNUParryFlashSeconds);
		Camera->StartCameraFade(FMath::Clamp(GNUParryFlashAlpha, 0.f, 1.f), 0.f, Seconds, GNUParryFlashColor, false, false);
		GNUParryFlashUntil = FPlatformTime::Seconds() + Seconds + HitStopExtra;
	}
}

void NUBattleParryFX::PlayDodge(ANUBattleAvatar* Defender)
{
	if (GNUDodgeFXEnabled == 0 || !CanPlayOn(Defender) || GNUDodgeSize <= 1.f)
	{
		return;
	}
	APlayerCameraManager* Camera = LocalCamera(Defender->GetWorld());
	if (!IsValid(Camera))
	{
		return;
	}
	// the streaks pass BEHIND the sprite (the card is depth tested and sits 30 cm beyond it) and sweep away from the attacker:
	// allies give ground toward screen-left, enemies toward screen-right
	const float Direction = Defender->IsAllyCombatant() ? -1.f : 1.f;
	if (UMaterialInstanceDynamic* Instance = SpawnCard(Defender, Camera, GNUDodgeStreakMaterial, FVector2D(Direction * 18.f, 0.f),
		FVector2D(GNUDodgeSize, GNUDodgeSize * 0.5f), FMath::Max(0.1f, GNUDodgeSeconds), -30.f))
	{
		Instance->SetScalarParameterValue(TEXT("Dir"), Direction);
	}
}

#if !UE_BUILD_SHIPPING
namespace
{
	ANUBattleAvatar* NearestAvatar(UWorld* World)
	{
		if (!IsValid(World))
		{
			return nullptr;
		}
		const APlayerCameraManager* Camera = LocalCamera(World);
		const FVector CameraLocation = IsValid(Camera) ? Camera->GetCameraLocation() : FVector::ZeroVector;
		ANUBattleAvatar* Nearest = nullptr;
		double Best = TNumericLimits<double>::Max();
		for (TActorIterator<ANUBattleAvatar> It(World); It; ++It)
		{
			const double Distance = FVector::DistSquared(It->GetActorLocation(), CameraLocation);
			if (Distance < Best)
			{
				Best = Distance;
				Nearest = *It;
			}
		}
		return Nearest;
	}
}

static FAutoConsoleCommandWithWorld GNUParryFXCmd(
	TEXT("nu.VFX.Parry"),
	TEXT("Plays the parry success presentation on the battle avatar nearest to the camera"),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		NUBattleParryFX::Play(NearestAvatar(World));
	}));

static FAutoConsoleCommandWithWorld GNUDodgeFXCmd(
	TEXT("nu.VFX.Dodge"),
	TEXT("Plays the dodge success presentation on the battle avatar nearest to the camera"),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		NUBattleParryFX::PlayDodge(NearestAvatar(World));
	}));
#endif
