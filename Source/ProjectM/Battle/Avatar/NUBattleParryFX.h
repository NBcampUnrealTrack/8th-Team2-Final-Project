// Reaction success presentation (NUA): parry and dodge. Both only showed a text popup.
//   Parry: a burst at the contact point (a cross flare, a ring and sparks drawn by M_NUA_ParryBurst on a camera-facing
//          card), a short hit-stop on the battle sprites and a warm screen flash.
//   Dodge: a sweep of wind streaks across the defender (M_NUA_DodgeStreak), no hit-stop and no flash: the attack missed.
// Local presentation only, nothing here touches the judgement. Called from ANUPlayerController::ShowCombatPopup, the
// one place both the local (GAS) and the network battle paths pass through.
// CVars: nu.VFX.Parry.Enabled (1), nu.VFX.Parry.HitStop (seconds, 0.09), nu.VFX.Parry.HitStopScale (0.05),
// nu.VFX.Parry.FlashAlpha (0.12), nu.VFX.Parry.FlashSeconds (0.12), nu.VFX.Parry.BurstSize (cm, 230),
// nu.VFX.Parry.BurstSeconds (0.38); nu.VFX.Dodge.Enabled (1), nu.VFX.Dodge.Size (cm, 240), nu.VFX.Dodge.Seconds (0.32).
// Console commands (non-shipping): nu.VFX.Parry / nu.VFX.Dodge play them on the avatar nearest to the camera.
#pragma once

#include "CoreMinimal.h"

class AActor;
class ANUBattleAvatar;
class UWorld;
enum class ENUCombatPopupType : uint8;

namespace NUBattleParryFX
{
	// plays the presentation that belongs to a combat popup (Parry, Dodge); every other type does nothing
	void PlayForPopup(AActor* AnchorActor, ENUCombatPopupType Type);

	// Defender = the avatar that parried
	void Play(ANUBattleAvatar* Defender);

	// Defender = the avatar that dodged
	void PlayDodge(ANUBattleAvatar* Defender);

	// true while the parry flash holds the camera fade, so the hit dim does not override it
	bool IsFlashing(const UWorld* World);
}
