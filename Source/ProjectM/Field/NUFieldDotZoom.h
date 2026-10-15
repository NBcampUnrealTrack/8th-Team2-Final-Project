// Integer dot scale for the sprite cameras (NUA). Field: the camera FOV is derived each frame from the viewport height
// and the spring arm length so one sprite dot = N screen pixels on every window size (the formula of nu.Cam.Zoom,
// applied continuously). Battle: the stage camera FOV is derived the same way at the mean depth of the avatars'
// formation slots (latched when each avatar is first seen, so nothing moves while a combatant steps forward), and each
// avatar's actor scale is compensated by its own slot depth so every avatar sits on that pixel grid.
// CVars: nu.Cam.DotScale (field, default 2, 0 = off), nu.Cam.DotLookHeight (cm added to the arm target offset Z),
// nu.Cam.BattleDotScale (battle, default 2, 0 = off), nu.Cam.BattleSpriteSnap (1 = per-avatar depth compensation),
// nu.Sprite.Stencil (custom depth stencil written by every flipbook sprite, default 2, 0 = off),
// nu.Cam.OcclusionFade (1 = write MPC_NUCameraFade when the pawn's own fade component has no collection, 2 = always, 0 = never).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NUFieldDotZoom.generated.h"

class APlayerController;
class UCameraComponent;
class UMaterialParameterCollection;

UCLASS()
class UNUFieldDotZoom : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

private:
	void TickField(APawn* Pawn, const FVector2D& ViewportSize);
	void TickBattle(AActor* ViewTarget, const FVector2D& ViewportSize, float DeltaTime);
	void TickOcclusionFade(APawn* Pawn, bool bFieldView, float DeltaTime);

	TWeakObjectPtr<APawn> LastPawn;
	float LastViewportY = 0.0f;
	float LastArm = 0.0f;
	float LastScale = 0.0f;
	float LastLookHeight = 0.0f;
	float BaseLookHeight = 0.0f;
	float LastBattleFov = 0.0f;
	float SpriteStencilTimer = 1.0f;
	float FadeStrength = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> FadeCollection;
	TMap<TWeakObjectPtr<AActor>, FVector> BattleBaseScale;
	TMap<TWeakObjectPtr<AActor>, float> BattleHomeDepth;   // each avatar's depth at its formation slot, taken when first seen
	TMap<TWeakObjectPtr<AActor>, FVector> BattleHomeLocation;   // and where that slot is, for the framing check
	TWeakObjectPtr<UCameraComponent> LastBattleCamera;
};
