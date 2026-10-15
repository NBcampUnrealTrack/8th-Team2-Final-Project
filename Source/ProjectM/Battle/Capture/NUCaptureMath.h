// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Naia/Data/NUNaiaTypes.h"

// 포획 확률 계산, 기획 문서 나이아 포획 시스템의 공식을 그대로 옮긴 것
//   P = 기본 포획률 x HP 보정 x 상태이상 보정 x 교감석 배율 x 누적 보정   (상한 0.95)
// 상태이상 보정만 여기 없다 - 대상 ASC 에서 읽어야 해서 호출부가 구해 넘김
// 순수 함수로 둔 이유: 넷 전투가 확률을 미리 보여주게 되면 같은 값을 재사용해야 함
namespace NUCapture
{
	// 흔들림 횟수, 서버 계약(Capture 이벤트의 shakes) 과 같은 값이어야 함
	constexpr int32 ShakeCount = 3;

	// 등급·진화 단계별 기본 포획률
	// 등급이 미지정이거나 진화 단계가 없는 나이아는 일반 1단계로 본다
	PROJECTM_API float GetBaseChance(ENUNaiaGrade Grade, ENUNaiaEvolutionStage Stage);

	// HP 보정 = (3 x maxHP - 2 x HP) / (3 x maxHP)
	// 만피 0.333배, 빈사 1.0배. 문서의 상한 1.5 는 이 식으로 불가능함
	PROJECTM_API float GetHealthBonus(float Health, float MaxHealth);

	// 교감석 배율 = 아이템 표 UseMagnitude / 5   (5/10/20/50 -> x1/x2/x4/x10)
	PROJECTM_API float GetStoneMultiplier(float UseMagnitude);

	// 누적 보정 = 1 + 시도 횟수 x 0.05, 상한 2.0
	PROJECTM_API float GetAttemptBonus(int32 AttemptCount);

	// 위 값들을 곱하고 0.95 에서 자른다
	PROJECTM_API float ResolveChance(float BaseChance, float HealthBonus, float StatusBonus,
		float StoneMultiplier, float AttemptBonus);

	// 통과한 흔들림 수 = 화면에서 실제로 흔들 횟수
	// 서버는 3칸을 항상 채워 보내고(굴리지 못한 칸도 false), 로컬은 굴린 것까지만 담음 —
	// 앞에서부터 true 인 것만 세면 됨
	// 한 번도 통과하지 못하면 0 이고, 그때는 흔들림 없이 곧바로 열림
	PROJECTM_API int32 CountPassedShakes(const TArray<bool>& Shakes);

	// 흔들림 한 번의 통과 확률은 P 의 세제곱근, 3회 모두 통과하면 포획
	// OutShakes 는 실제로 굴린 것까지만 담는다 - 실패한 흔들림이 마지막 항목
	PROJECTM_API bool RollShakes(float Chance, TArray<bool>& OutShakes);
}
