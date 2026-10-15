// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// 애정도 - 누적 포인트를 단계(0~15)로 바꾸는 표
// 저장 단위는 0.1 포인트다 저장값 1253 = 125.3 포인트
namespace NUNaiaAffection
{
    // 저장값 = 기획서 포인트 × 이 값
    constexpr int32 PointScale = 10;

    // 상수표에 적힌 최대 애정도
    constexpr int32 MaxTier = 15;

    // 누적 포인트를 단계로 변환, 음수는 0, 상한을 넘으면 MaxTier
    PROJECTM_API int32 ResolveTier(int32 StoredPoints);

    // 그 단계에 도달하는 데 필요한 누적 포인트, 0단계는 0
    // 단계 진척 바를 그릴 때 이 값과 현재 포인트로 비율을 냄
    PROJECTM_API int32 GetTierThreshold(int32 Tier);
}
