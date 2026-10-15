#pragma once

#include "CoreMinimal.h"
#include "NUCharacterStatServerTypes.generated.h"

// 외부 서버가 확정한 배분 스탯과 최종 파생 능력치 스냅샷
USTRUCT(BlueprintType)
struct PROJECTM_API FNUCharacterStatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int64 TotalExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int64 ExperienceIntoLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int64 RequiredExperienceToNext = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 MaxLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Strength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Dexterity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Intelligence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Luck = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Vitality = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 TotalPoint = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 FreePoint = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 StrengthCap = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 DexterityCap = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 IntelligenceCap = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 LuckCap = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 VitalityCap = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float MaxSP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float AttackPower = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float MagicAttackPower = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float Defense = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float CriticalRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float CriticalDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float ParryRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float DodgeRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float DamageReduce = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float HealBonus = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float EquipAttackRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float EquipMagicAttackRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float EquipMaxHealthRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float EquipDefenseFlat = 0.f;
};

namespace NUCharacterStatServer
{
	// CharacterStat 응답 JSON의 필수 필드와 값 범위를 검증해 스냅샷으로 변환
	PROJECTM_API bool ParseSnapshot(const FString& ResponseData, FNUCharacterStatSnapshot& OutSnapshot, FString& OutError);
}
