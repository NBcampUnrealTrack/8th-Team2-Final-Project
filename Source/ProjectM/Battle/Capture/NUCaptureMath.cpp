// Fill out your copyright notice in the Description page of Project Settings.

#include "NUCaptureMath.h"

namespace
{
	// 기획 표를 그대로 옮긴 것, 행이 등급이고 열이 진화 단계
	// 일반 전투 포획의 확률표. 새순의 클리어 보상 포획(고정 9%)은 이 보정식을 쓰지 않는다.
	constexpr float NormalGradeChances[3] = { 0.15f, 0.12f, 0.09f };
	constexpr float RareGradeChances[3] = { 0.12f, 0.09f, 0.06f };

	constexpr float CaptureChanceCap = 0.95f;

	constexpr float HealthBonusMin = 0.333f;
	constexpr float HealthBonusMax = 1.5f;

	constexpr float AttemptBonusPerTry = 0.05f;
	constexpr float AttemptBonusCap = 2.f;

	// 아이템 표의 UseMagnitude 가 표기 포획률(%) 이라 배율로 바꿀 때 나누는 값
	constexpr float StoneMagnitudePerStep = 5.f;
}

float NUCapture::GetBaseChance(ENUNaiaGrade Grade, ENUNaiaEvolutionStage Stage)
{
	// 전설의 일반 전투 포획률은 미정. 새 등급을 일반 등급으로 잘못 계산하지 않는다.
	if (Grade == ENUNaiaGrade::Legendary)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("포획 확률: 전설 등급의 일반 전투 포획률이 미정이라 포획하지 않음"));
		return 0.f;
	}

	// 진화하지 않는 나이아는 EvolutionStage 가 None 이라 1단계로 본다
	int32 StageIndex = 0;
	switch (Stage)
	{
	case ENUNaiaEvolutionStage::Stage2:
		StageIndex = 1;
		break;

	case ENUNaiaEvolutionStage::Stage3:
		StageIndex = 2;
		break;

	default:
		StageIndex = 0;
		break;
	}

	// 등급이 미지정인 행은 시트가 덜 채워진 것이므로 일반으로 보고 로그를 남긴다
	if (Grade == ENUNaiaGrade::Unknown)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("포획 확률: 등급이 미지정인 나이아라 일반 등급으로 계산함"));
	}

	return Grade == ENUNaiaGrade::Rare
		? RareGradeChances[StageIndex]
		: NormalGradeChances[StageIndex];
}

float NUCapture::GetHealthBonus(float Health, float MaxHealth)
{
	if (MaxHealth <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	const float Bonus = (3.f * MaxHealth - 2.f * Health) / (3.f * MaxHealth);

	return FMath::Clamp(Bonus, HealthBonusMin, HealthBonusMax);
}

float NUCapture::GetStoneMultiplier(float UseMagnitude)
{
	if (UseMagnitude <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("포획 확률: 교감석의 UseMagnitude 가 비어 있어 배율 1로 계산함"));
		return 1.f;
	}

	return UseMagnitude / StoneMagnitudePerStep;
}

float NUCapture::GetAttemptBonus(int32 AttemptCount)
{
	const float Bonus = 1.f + FMath::Max(0, AttemptCount) * AttemptBonusPerTry;

	return FMath::Min(Bonus, AttemptBonusCap);
}

float NUCapture::ResolveChance(float BaseChance, float HealthBonus, float StatusBonus,
	float StoneMultiplier, float AttemptBonus)
{
	const float Chance = BaseChance * HealthBonus * StatusBonus * StoneMultiplier * AttemptBonus;

	return FMath::Clamp(Chance, 0.f, CaptureChanceCap);
}

int32 NUCapture::CountPassedShakes(const TArray<bool>& Shakes)
{
	int32 Passed = 0;

	for (const bool bShake : Shakes)
	{
		// 하나라도 실패하면 그 뒤는 굴리지 않은 칸이라 세지 않는다
		if (!bShake)
		{
			break;
		}

		++Passed;
	}

	return Passed;
}

bool NUCapture::RollShakes(float Chance, TArray<bool>& OutShakes)
{
	OutShakes.Reset();

	const float Clamped = FMath::Clamp(Chance, 0.f, 1.f);
	const float PerShake = FMath::Pow(Clamped, 1.f / static_cast<float>(ShakeCount));

	for (int32 Index = 0; Index < ShakeCount; ++Index)
	{
		const bool bPassed = FMath::FRand() < PerShake;
		OutShakes.Add(bPassed);

		// 한 번 실패하면 남은 흔들림은 굴리지 않는다, 연출도 거기서 열린다
		if (!bPassed)
		{
			return false;
		}
	}

	return true;
}
