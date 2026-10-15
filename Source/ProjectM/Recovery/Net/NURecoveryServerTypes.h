#pragma once

#include "CoreMinimal.h"

enum class ENURecoveryRequest : uint8 { None, Heal, Rest, Status, RegisterPoint };

// 서버 회복 응답으로, 패킷마다 포함되는 필드가 달라 요청 종류에 맞춰 파싱한다.
struct FNURecoveryServerState
{
	FString MutationId;
	int64 CharacterId = 0;
	int64 Gold = 0;
	int64 ChargedGold = 0;
	int64 ServerTime = 0;
	int64 RecoveryId = 0;
	int64 DeathId = 0;
	int64 RegisteredPointId = 0;
	int64 EffectivePointId = 0;
	int32 PlayerHp = 0;
	int32 PlayerMaxHp = 0;
	int32 RecoveryState = 0;
	int32 RespawnState = 0;
	int32 FullRecoverySeconds = 3600;

	bool bHasGold = false;
};

namespace NURecoveryServer
{
	bool ParseResponse(ENURecoveryRequest Request, const FString& Json, FNURecoveryServerState& OutState);
	float InterpolateHealth(float Health, float MaxHealth, double ElapsedSeconds, int32 DurationSeconds);
}
