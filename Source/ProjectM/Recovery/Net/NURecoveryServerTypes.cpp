#include "Recovery/Net/NURecoveryServerTypes.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool NURecoveryServer::ParseResponse(ENURecoveryRequest Request, const FString& Json, FNURecoveryServerState& OutState)
{
	// int64 잔액과 ID를 double로 변환하지 않도록 JSON 숫자의 원문을 보존한다.
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root, FJsonSerializer::EFlags::StoreNumbersAsStrings) || !Root) { return false; }
	FNURecoveryServerState Parsed;
	if (!Root->TryGetNumberField(TEXT("PlayerHp"), Parsed.PlayerHp) || !Root->TryGetNumberField(TEXT("PlayerMaxHp"), Parsed.PlayerMaxHp)
		|| Parsed.PlayerHp < 0 || Parsed.PlayerMaxHp <= 0 || Parsed.PlayerHp > Parsed.PlayerMaxHp) { return false; }
	if (Request == ENURecoveryRequest::Heal || Request == ENURecoveryRequest::Rest)
	{
		if (!Root->TryGetStringField(TEXT("MutationId"), Parsed.MutationId) || Parsed.MutationId.IsEmpty()) { return false; }
	}

	// 191에는 잔액이 없으므로 해당 응답으로 골드를 덮어쓰지 않는다.
	if (Request == ENURecoveryRequest::Heal || Request == ENURecoveryRequest::Status || Request == ENURecoveryRequest::RegisterPoint)
	{
		if (!Root->TryGetNumberField(TEXT("Gold"), Parsed.Gold)) { return false; }
		Parsed.bHasGold = true;
	}
	if (Request == ENURecoveryRequest::Heal)
	{
		if (!Root->TryGetNumberField(TEXT("ChargedGold"), Parsed.ChargedGold) || Parsed.ChargedGold < 0) { return false; }
	}
	else if (Request == ENURecoveryRequest::Rest || Request == ENURecoveryRequest::Status || Request == ENURecoveryRequest::RegisterPoint)
	{
		if (!Root->TryGetNumberField(TEXT("ServerTime"), Parsed.ServerTime) || Parsed.ServerTime <= 0
			|| !Root->TryGetNumberField(TEXT("RecoveryId"), Parsed.RecoveryId) || Parsed.RecoveryId < 0
			|| !Root->TryGetNumberField(TEXT("RecoveryState"), Parsed.RecoveryState) || Parsed.RecoveryState < 0 || Parsed.RecoveryState > 2
			|| !Root->TryGetNumberField(TEXT("FullRecoverySeconds"), Parsed.FullRecoverySeconds) || Parsed.FullRecoverySeconds <= 0) { return false; }
	}
	else { return false; }
	// 193 응답은 192와 동일한 확정 상태다.
	if (Request == ENURecoveryRequest::Status || Request == ENURecoveryRequest::RegisterPoint)
	{
		if (!Root->TryGetNumberField(TEXT("CharacterId"), Parsed.CharacterId) || Parsed.CharacterId <= 0
			|| !Root->TryGetNumberField(TEXT("RegisteredRecoveryPointId"), Parsed.RegisteredPointId) || Parsed.RegisteredPointId < 0 || Parsed.RegisteredPointId > MAX_int32
			|| !Root->TryGetNumberField(TEXT("EffectiveRecoveryPointId"), Parsed.EffectivePointId) || Parsed.EffectivePointId <= 0 || Parsed.EffectivePointId > MAX_int32
			|| !Root->TryGetNumberField(TEXT("RespawnState"), Parsed.RespawnState) || (Parsed.RespawnState != 0 && Parsed.RespawnState != 2)
			|| !Root->TryGetNumberField(TEXT("DeathId"), Parsed.DeathId) || Parsed.DeathId < 0
			|| !Root->TryGetNumberField(TEXT("DeathChargedGold"), Parsed.ChargedGold) || Parsed.ChargedGold < 0) { return false; }
	}

	// 부분적으로 파싱된 응답이 기존 상태를 덮어쓰지 않게 마지막에 교체한다.
	OutState = MoveTemp(Parsed);
	return true;
}

float NURecoveryServer::InterpolateHealth(float Health, float MaxHealth, double ElapsedSeconds, int32 DurationSeconds)
{
	if (Health <= 0.0f || MaxHealth <= 0.0f || DurationSeconds <= 0) { return Health; }
	return FMath::Min(MaxHealth, Health + MaxHealth * static_cast<float>(FMath::Max(0.0, ElapsedSeconds) / DurationSeconds));
}
