// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Notifies/PaperZDAnimNotify.h"
#include "NUBattleMotionMarkerNotify.generated.h"

class UPaperZDAnimSequence;

// 전투 모션 타임라인에 찍는 종류
UENUM(BlueprintType)
enum class ENUBattleMotionMarker : uint8
{
	Hit,	// 피해가 들어가는 순간 - 피해 / 피격 반응 / 착탄 이펙트 / 패링 창
	Cast,	// 시전 이펙트가 뜨고 탄환이 출발하는 순간
};

// 전투 공격 시퀀스에 찍는 시각 표시
// 재생 중에는 아무것도 하지 않고, 아바타가 재생 전에 시각만 읽어 타격과 연출을 예약함
// 무슨 이펙트를 낼지는 스킬 쪽(DT_SkillVFX · 어빌리티)이 정함
UCLASS(meta = (DisplayName = "NU Battle Motion Marker"))
class PROJECTM_API UNUBattleMotionMarkerNotify : public UPaperZDAnimNotify
{
	GENERATED_BODY()

public:
	virtual FName GetDisplayName_Implementation() const override;

	// 시퀀스에 찍힌 표시 중 가장 이른 것의 시각(초), 없으면 false
	// 한 모션에 같은 표시가 여러 개면 첫 번째만 봄, 한 모션 안의 연속 타격은 아직 없음
	static bool FindMarkerTime(const UPaperZDAnimSequence* Sequence, ENUBattleMotionMarker InMarker, float& OutTime);
	
public:
	UPROPERTY(EditAnywhere, Category = "NU|Battle")
	ENUBattleMotionMarker Marker = ENUBattleMotionMarker::Hit;
};
